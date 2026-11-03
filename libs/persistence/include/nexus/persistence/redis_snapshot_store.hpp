/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
// UNVERIFIED IN THIS SANDBOX. This sandbox has no internet access, so
// hiredis could not be installed and no Redis server could be started
// to test against — unlike sqlite_snapshot_store.hpp, which IS compiled
// and tested here (see libs/persistence/tests). This file follows the
// same shape and the same SnapshotReader/SnapshotWriter concepts, and
// the hiredis API calls below are written correctly against hiredis's
// documented interface, but you should compile and test it on your
// machine before trusting it in anything real.
//
// Install: vcpkg install hiredis   (or apt install libhiredis-dev)
// Link:    target_link_libraries(your_target PRIVATE hiredis)
//
// Data model: one Redis HASH per row, key "nexus:{table}:{row_key}",
// field values tagged the same way as the wire protocol (i:/d:/b:/s:/n:).
// SELECT uses SCAN with a MATCH pattern rather than KEYS, since KEYS
// blocks the whole Redis server on a large keyspace — SCAN is
// incremental and safe to use against a live server under load.

#include "nexus/domain/error.hpp"
#include "nexus/domain/predicate.hpp"
#include "nexus/domain/row.hpp"
#include "nexus/domain/table_name.hpp"
#include <hiredis/hiredis.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace nexus::persistence {

namespace redis_detail {

inline std::string encode_value(const domain::Value& value) {
    return std::visit(
        [](const auto& v) -> std::string {
            using V = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<V, std::monostate>) return "n:";
            else if constexpr (std::is_same_v<V, std::int64_t>) return "i:" + std::to_string(v);
            else if constexpr (std::is_same_v<V, double>) return "d:" + std::to_string(v);
            else if constexpr (std::is_same_v<V, bool>) return v ? "b:1" : "b:0";
            else if constexpr (std::is_same_v<V, std::string>) return "s:" + v;
        },
        value);
}

inline domain::Value decode_value(const std::string& text) {
    if (text.size() < 2) return domain::Value{std::monostate{}};
    switch (text[0]) {
        case 'i': return domain::Value{static_cast<std::int64_t>(std::stoll(text.substr(2)))};
        case 'd': return domain::Value{std::stod(text.substr(2))};
        case 'b': return domain::Value{text.substr(2) == "1"};
        case 's': return domain::Value{text.substr(2)};
        default: return domain::Value{std::monostate{}};
    }
}

inline std::string row_key_pattern(const domain::TableName& table) {
    return "nexus:" + table.value() + ":*";
}

inline std::string row_key(const domain::TableName& table, const domain::RowKey& key) {
    return "nexus:" + table.value() + ":" + key.value();
}

struct ReplyDeleter {
    void operator()(redisReply* r) const {
        if (r) freeReplyObject(r);
    }
};
using ReplyPtr = std::unique_ptr<redisReply, ReplyDeleter>;

} // namespace redis_detail

// Default-constructible, connecting to localhost:6379 — same pattern as
// SqliteSnapshotStore's default path. A production version would take
// host/port through config rather than hardcoding it.
class RedisSnapshotStore {
public:
    explicit RedisSnapshotStore(const std::string& host = "127.0.0.1", int port = 6379) {
        context_ = redisConnect(host.c_str(), port);
        if (!context_ || context_->err) {
            std::string message = context_ ? context_->errstr : "redisConnect returned null";
            if (context_) redisFree(context_);
            throw std::runtime_error("Redis connection failed: " + message);
        }
    }

    ~RedisSnapshotStore() {
        if (context_) redisFree(context_);
    }

    RedisSnapshotStore(const RedisSnapshotStore&) = delete;
    RedisSnapshotStore& operator=(const RedisSnapshotStore&) = delete;

    domain::Expected<void> upsert(const domain::TableName& table, domain::RowPtr row) {
        std::lock_guard lock(mutex_);
        const std::string key = redis_detail::row_key(table, row->key());

        // Build a single HSET key f1 v1 f2 v2 ... command via argv form,
        // which is the correct way to pass a variable number of
        // arguments to redisCommandArgv without building an injectable
        // command string.
        std::vector<std::string> args{"HSET", key};
        for (const auto& [field, value] : row->fields()) {
            args.push_back(field);
            args.push_back(redis_detail::encode_value(value));
        }

        std::vector<const char*> argv;
        std::vector<std::size_t> argvlen;
        for (const auto& a : args) {
            argv.push_back(a.c_str());
            argvlen.push_back(a.size());
        }

        redis_detail::ReplyPtr reply(static_cast<redisReply*>(redisCommandArgv(
            context_, static_cast<int>(argv.size()), argv.data(), argvlen.data())));

        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            return std::unexpected(domain::Error::TableNotFound);
        }
        return {};
    }

    [[nodiscard]] std::vector<domain::RowPtr>
    select(const domain::TableName& table, const domain::PredicatePtr& predicate) const {
        std::lock_guard lock(mutex_);
        std::vector<domain::RowPtr> results;
        const std::string pattern = redis_detail::row_key_pattern(table);
        const std::string prefix = "nexus:" + table.value() + ":";

        // SCAN, not KEYS — incremental cursor-based iteration so this
        // doesn't block the whole Redis server on a large keyspace.
        std::string cursor = "0";
        do {
            redis_detail::ReplyPtr scan_reply(static_cast<redisReply*>(
                redisCommand(context_, "SCAN %s MATCH %s COUNT 100", cursor.c_str(), pattern.c_str())));
            if (!scan_reply || scan_reply->type != REDIS_REPLY_ARRAY || scan_reply->elements != 2) {
                break;
            }

            cursor = scan_reply->element[0]->str;
            redisReply* keys = scan_reply->element[1];

            for (std::size_t i = 0; i < keys->elements; ++i) {
                const std::string full_key = keys->element[i]->str;
                const std::string row_key_str = full_key.substr(prefix.size());

                redis_detail::ReplyPtr hash_reply(static_cast<redisReply*>(
                    redisCommand(context_, "HGETALL %s", full_key.c_str())));
                if (!hash_reply || hash_reply->type != REDIS_REPLY_ARRAY) continue;

                std::unordered_map<std::string, domain::Value> fields;
                for (std::size_t f = 0; f + 1 < hash_reply->elements; f += 2) {
                    fields[hash_reply->element[f]->str] =
                        redis_detail::decode_value(hash_reply->element[f + 1]->str);
                }

                auto row = std::make_shared<const domain::Row>(domain::RowKey{row_key_str},
                                                                 std::move(fields));
                if (domain::matches(predicate, *row)) results.push_back(row);
            }
        } while (cursor != "0");

        return results;
    }

private:
    redisContext* context_ = nullptr;
    mutable std::mutex mutex_;
};

} // namespace nexus::persistence
