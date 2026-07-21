#pragma once
#include <cstdint>
#include <string>

namespace nexus::session {

using SessionId = std::uint64_t;

// One client's login: WHO (username), WHAT application they're
// connecting as, and WHAT they intend to do (e.g. "SELECT,SUBSCRIBE" or
// "ALL"). application_name must be unique among concurrently active
// sessions — see SessionRegistry.
struct Session {
    std::string username;
    std::string application_name;
    std::string access_mode;
};

} // namespace nexus::session
