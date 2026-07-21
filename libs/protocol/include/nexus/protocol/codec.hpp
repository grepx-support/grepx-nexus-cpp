#pragma once
#include "nexus/domain/change_event.hpp"
#include "nexus/domain/error.hpp"
#include "nexus/domain/row.hpp"
#include "nexus/protocol/message.hpp"
#include <charconv>
#include <cstdint>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>

namespace nexus::protocol {
    // Wire format (newline-delimited, one message per line):
    //
    //   SELECT     <table> <criteria...>
    //   INSERT     <table> <key> <field=type:value;field=type:value;...>
    //   SUBSCRIBE  <id> <table> <criteria...>
    //   TRANSITION <table> <key> <from> <to> <expected_version> <fields>
    //
    //   OK
    //   OK    <detail>            (TRANSITION success: <detail> is the new version)
    //   ERR   <message>
    //   ROW   <key> <fields>
    //   END
    //   EVENT <table> <key> <fields>
    //
    // Value types: i:<int64> d:<double> b:<true|false> s:<string, no ';' or '='> n:<null>

    namespace detail {
        inline std::string encode_value(const domain::Value &value) {
            return std::visit(
                [](const auto &v) -> std::string {
                    using V = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<V, std::monostate>) return "n:";
                    else if constexpr (std::is_same_v<V, std::int64_t>) return "i:" + std::to_string(v);
                    else if constexpr (std::is_same_v<V, double>) return "d:" + std::to_string(v);
                    else if constexpr (std::is_same_v<V, bool>) return v ? "b:true" : "b:false";
                    else if constexpr (std::is_same_v<V, std::string>) return "s:" + v;
                },
                value);
        }

        inline domain::Expected<domain::Value> parse_value(const std::string &text) {
            if (text.size() < 2 || text[1] != ':') return std::unexpected(domain::Error::ParseError);
            const char tag = text[0];
            const std::string body = text.substr(2);

            switch (tag) {
                case 'n': return domain::Value{std::monostate{}};
                case 'i': {
                    std::int64_t n = 0;
                    std::from_chars(body.data(), body.data() + body.size(), n);
                    return domain::Value{n};
                }
                case 'd': {
                    double d = 0;
                    std::from_chars(body.data(), body.data() + body.size(), d);
                    return domain::Value{d};
                }
                case 'b': return domain::Value{body == "true"};
                case 's': return domain::Value{body};
                default: return std::unexpected(domain::Error::ParseError);
            }
        }

        inline std::string encode_fields(const std::unordered_map<std::string, domain::Value> &fields) {
            std::string out;
            bool first = true;
            for (const auto &[name, value]: fields) {
                if (!first) out += ';';
                out += name + "=" + encode_value(value);
                first = false;
            }
            return out;
        }

        inline domain::Expected<std::unordered_map<std::string, domain::Value> >
        parse_fields(const std::string &text) {
            std::unordered_map<std::string, domain::Value> fields;
            if (text.empty()) return fields;

            std::stringstream ss(text);
            std::string pair;
            while (std::getline(ss, pair, ';')) {
                auto eq = pair.find('=');
                if (eq == std::string::npos) return std::unexpected(domain::Error::ParseError);
                auto value = parse_value(pair.substr(eq + 1));
                if (!value) return std::unexpected(value.error());
                fields[pair.substr(0, eq)] = *value;
            }
            return fields;
        }

        inline std::vector<std::string> split_ws(const std::string &line, int max_tokens) {
            std::vector<std::string> tokens;
            std::size_t pos = 0;
            while (pos < line.size() && static_cast<int>(tokens.size()) < max_tokens - 1) {
                std::size_t next = line.find(' ', pos);
                if (next == std::string::npos) break;
                tokens.push_back(line.substr(pos, next - pos));
                pos = next + 1;
            }
            tokens.push_back(line.substr(pos)); // remainder (may contain spaces)
            return tokens;
        }
    } // namespace detail

    // ---- requests (client -> server) ----

    inline std::string encode(const AuthRequest &req) {
        return "AUTH " + req.username + " " + req.password + " " + req.application_name + " " + req.access_mode;
    }

    inline std::string encode(const SelectRequest &req) {
        return "SELECT " + req.table.value() + " " + req.criteria;
    }

    inline std::string encode(const InsertRequest &req) {
        return "INSERT " + req.table.value() + " " + req.row->key().value() + " " +
               detail::encode_fields(req.row->fields());
    }

    inline std::string encode(const SubscribeRequest &req) {
        return "SUBSCRIBE " + req.subscription_id + " " + req.table.value() + " " + req.criteria;
    }

    inline std::string encode(const TransitionRequest &req) {
        return "TRANSITION " + req.table.value() + " " + req.key + " " + req.from_status + " " +
               req.to_status + " " + std::to_string(req.expected_version) + " " +
               detail::encode_fields(req.row->fields());
    }

    inline domain::Expected<Request> parse_request(const std::string &line) {
        auto sp = line.find(' ');
        if (sp == std::string::npos) return std::unexpected(domain::Error::ParseError);
        std::string verb = line.substr(0, sp);
        std::string rest = line.substr(sp + 1);

        if (verb == "SELECT") {
            auto tokens = detail::split_ws(rest, 2);
            if (tokens.size() < 1) return std::unexpected(domain::Error::ParseError);
            std::string table = tokens[0];
            std::string criteria = tokens.size() > 1 ? tokens[1] : "";
            return Request{SelectRequest{domain::TableName{table}, criteria}};
        }

        if (verb == "INSERT") {
            auto tokens = detail::split_ws(rest, 3);
            if (tokens.size() < 3) return std::unexpected(domain::Error::ParseError);
            auto fields = detail::parse_fields(tokens[2]);
            if (!fields) return std::unexpected(fields.error());
            auto row = std::make_shared<const domain::Row>(domain::RowKey{tokens[1]}, std::move(*fields));
            return Request{InsertRequest{domain::TableName{tokens[0]}, row}};
        }

        if (verb == "SUBSCRIBE") {
            auto tokens = detail::split_ws(rest, 3);
            if (tokens.size() < 2) return std::unexpected(domain::Error::ParseError);
            std::string criteria = tokens.size() > 2 ? tokens[2] : "";
            return Request{SubscribeRequest{tokens[0], domain::TableName{tokens[1]}, criteria}};
        }

        if (verb == "TRANSITION") {
            // table key from to expected_version fields  -> 5 fixed tokens + remainder
            auto tokens = detail::split_ws(rest, 6);
            if (tokens.size() < 6) return std::unexpected(domain::Error::ParseError);

            std::uint64_t expected_version = 0;
            auto [ptr, ec] = std::from_chars(tokens[4].data(), tokens[4].data() + tokens[4].size(),
                                             expected_version);
            if (ec != std::errc{}) return std::unexpected(domain::Error::ParseError);

            auto fields = detail::parse_fields(tokens[5]);
            if (!fields) return std::unexpected(fields.error());
            auto row = std::make_shared<const domain::Row>(domain::RowKey{tokens[1]}, std::move(*fields));

            return Request{
                TransitionRequest{
                    domain::TableName{tokens[0]}, tokens[1], tokens[2],
                    tokens[3], expected_version, row
                }
            };
        }
        if (verb == "AUTH") {
            // AUTH <username> <password> <application_name> <access_mode>
            auto tokens = detail::split_ws(rest, 4);
            if (tokens.size() < 4) return std::unexpected(domain::Error::ParseError);
            return Request{AuthRequest{tokens[0], tokens[1], tokens[2], tokens[3]}};
        }
        return std::unexpected(domain::Error::ParseError);
    }

    // ---- responses / events (server -> client) ----

    inline std::string encode_ok() { return "OK"; }
    inline std::string encode_ok(const std::string &detail) { return "OK " + detail; }
    inline std::string encode_err(const std::string &message) { return "ERR " + message; }
    inline std::string encode_end() { return "END"; }

    inline std::string encode_row(const domain::RowPtr &row) {
        return "ROW " + row->key().value() + " " + detail::encode_fields(row->fields());
    }

    inline std::string encode_event(const domain::ChangeEvent &event) {
        return "EVENT " + event.table.value() + " " + event.row->key().value() + " " +
               detail::encode_fields(event.row->fields());
    }
} // namespace nexus::protocol
