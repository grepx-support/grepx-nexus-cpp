#pragma once
#include "nexus/session/session_registry.hpp"
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

namespace nexus::session {

// The uniqueness enforcement itself: one mutex-guarded hashmap, no I/O.
// login() is a single lookup + insert; logout() a single lookup + erase.
//
// Keyed on (application_name, access_mode) rather than application_name
// alone: a real client legitimately needs up to two concurrent
// connections under one application_name — one persistent connection
// for request/response calls (SELECT/INSERT/TRANSITION) and one
// dedicated connection for SUBSCRIBE (which blocks on incoming EVENT
// pushes and can't share a socket with synchronous calls). Those two
// naturally carry different access_mode strings, so they don't collide;
// two connections trying to claim the *same* role under the same
// application_name still get rejected.
class InMemorySessionRegistry {
public:
    std::optional<SessionId> login(const Session& s) {
        std::lock_guard lock(mutex_);
        Key key{s.application_name, s.access_mode};
        if (active_by_key_.contains(key)) return std::nullopt;
        SessionId id = next_id_++;
        active_by_key_.emplace(key, id);
        key_by_id_.emplace(id, key);
        return id;
    }

    void logout(SessionId id) {
        std::lock_guard lock(mutex_);
        auto it = key_by_id_.find(id);
        if (it == key_by_id_.end()) return;
        active_by_key_.erase(it->second);
        key_by_id_.erase(it);
    }

private:
    using Key = std::pair<std::string, std::string>; // (application_name, access_mode)

    struct KeyHash {
        std::size_t operator()(const Key& k) const noexcept {
            return std::hash<std::string>{}(k.first) ^ (std::hash<std::string>{}(k.second) << 1);
        }
    };

    std::mutex mutex_;
    std::unordered_map<Key, SessionId, KeyHash> active_by_key_;
    std::unordered_map<SessionId, Key> key_by_id_;
    SessionId next_id_ = 1;
};

static_assert(SessionRegistry<InMemorySessionRegistry>);

} // namespace nexus::session
