#pragma once
#include "nexus/domain/error.hpp"
#include "nexus/domain/predicate.hpp"
#include <cctype>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexus::criteria {

// Supported grammar (small, deliberate subset for the MVP):
//   expr    := term (OR term)*
//   term    := factor (AND factor)*
//   factor  := IDENT OP value
//   OP      := '=' | '!=' | '>' | '>=' | '<' | '<='
//   value   := number | 'string' | true | false
//
// Cold path: runs once per subscribe call, never on the hot insert/notify path.

namespace detail {

enum class TokenKind { Ident, Op, Number, String, Bool, And, Or, End };

struct Token {
    TokenKind kind;
    std::string text;
};

inline std::vector<Token> tokenize(std::string_view input) {
    std::vector<Token> tokens;
    std::size_t i = 0;

    auto skip_ws = [&] {
        while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i]))) ++i;
    };

    while (true) {
        skip_ws();
        if (i >= input.size()) {
            tokens.push_back({TokenKind::End, ""});
            break;
        }

        const char c = input[i];

        if (c == '\'') {
            std::size_t start = ++i;
            while (i < input.size() && input[i] != '\'') ++i;
            tokens.push_back({TokenKind::String, std::string(input.substr(start, i - start))});
            if (i < input.size()) ++i; // closing quote
            continue;
        }

        if (c == '>' || c == '<' || c == '=' || c == '!') {
            std::size_t start = i++;
            if (i < input.size() && input[i] == '=') ++i;
            tokens.push_back({TokenKind::Op, std::string(input.substr(start, i - start))});
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '-' && i + 1 < input.size() && std::isdigit(static_cast<unsigned char>(input[i + 1])))) {
            std::size_t start = i++;
            while (i < input.size() &&
                   (std::isdigit(static_cast<unsigned char>(input[i])) || input[i] == '.'))
                ++i;
            tokens.push_back({TokenKind::Number, std::string(input.substr(start, i - start))});
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::size_t start = i++;
            while (i < input.size() &&
                   (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_'))
                ++i;
            std::string word(input.substr(start, i - start));

            if (word == "AND") tokens.push_back({TokenKind::And, word});
            else if (word == "OR") tokens.push_back({TokenKind::Or, word});
            else if (word == "true" || word == "false") tokens.push_back({TokenKind::Bool, word});
            else tokens.push_back({TokenKind::Ident, word});
            continue;
        }

        ++i; // skip unrecognised character (e.g. stray whitespace variant)
    }

    return tokens;
}

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    domain::Expected<domain::PredicatePtr> parse() {
        auto expr = parse_expr();
        if (!expr) return std::unexpected(expr.error());
        if (peek().kind != TokenKind::End) return std::unexpected(domain::Error::ParseError);
        return expr;
    }

private:
    [[nodiscard]] const Token& peek() const { return tokens_[pos_]; }
    Token advance() { return tokens_[pos_++]; }

    domain::Expected<domain::PredicatePtr> parse_expr() {
        auto lhs = parse_term();
        if (!lhs) return lhs;

        while (peek().kind == TokenKind::Or) {
            advance();
            auto rhs = parse_term();
            if (!rhs) return rhs;
            lhs = std::make_shared<const domain::Predicate>(
                domain::Predicate{.node = domain::Or{*lhs, *rhs}});
        }
        return lhs;
    }

    domain::Expected<domain::PredicatePtr> parse_term() {
        auto lhs = parse_factor();
        if (!lhs) return lhs;

        while (peek().kind == TokenKind::And) {
            advance();
            auto rhs = parse_factor();
            if (!rhs) return rhs;
            lhs = std::make_shared<const domain::Predicate>(
                domain::Predicate{.node = domain::And{*lhs, *rhs}});
        }
        return lhs;
    }

    domain::Expected<domain::PredicatePtr> parse_factor() {
        if (peek().kind != TokenKind::Ident) return std::unexpected(domain::Error::ParseError);
        std::string field = advance().text;

        if (peek().kind != TokenKind::Op) return std::unexpected(domain::Error::ParseError);
        auto op = parse_op(advance().text);
        if (!op) return std::unexpected(domain::Error::ParseError);

        auto value = parse_value();
        if (!value) return std::unexpected(value.error());

        return std::make_shared<const domain::Predicate>(domain::Predicate{
            .node = domain::Comparison{.field = std::move(field), .op = *op, .value = *value}});
    }

    static std::optional<domain::CompareOp> parse_op(const std::string& text) {
        if (text == "=") return domain::CompareOp::Eq;
        if (text == "!=") return domain::CompareOp::Ne;
        if (text == ">") return domain::CompareOp::Gt;
        if (text == ">=") return domain::CompareOp::Ge;
        if (text == "<") return domain::CompareOp::Lt;
        if (text == "<=") return domain::CompareOp::Le;
        return std::nullopt;
    }

    domain::Expected<domain::Value> parse_value() {
        const Token token = advance();
        switch (token.kind) {
            case TokenKind::Number: {
                if (token.text.find('.') != std::string::npos) {
                    double d = 0;
                    std::from_chars(token.text.data(), token.text.data() + token.text.size(), d);
                    return domain::Value{d};
                }
                std::int64_t n = 0;
                std::from_chars(token.text.data(), token.text.data() + token.text.size(), n);
                return domain::Value{n};
            }
            case TokenKind::String:
                return domain::Value{token.text};
            case TokenKind::Bool:
                return domain::Value{token.text == "true"};
            default:
                return std::unexpected(domain::Error::ParseError);
        }
    }

    std::vector<Token> tokens_;
    std::size_t pos_ = 0;
};

} // namespace detail

// Parses a WHERE-clause string (e.g. "amount > 50 AND currency = 'USD'")
// into a Predicate AST. Runs once per subscribe call — never on the hot path.
[[nodiscard]] inline domain::Expected<domain::PredicatePtr> parse(std::string_view input) {
    if (input.empty()) return domain::Predicate::always_true();
    detail::Parser parser(detail::tokenize(input));
    return parser.parse();
}

} // namespace nexus::criteria
