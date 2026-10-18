#pragma once

#include <cstdint>

#include "ext/zygisk.hpp"

namespace hma_scope {

// Connects to the root companion, which loads the UID set HMA hides from this
// package and then waits for a seccomp user-notification listener fd.
int prepare(zygisk::Api* api, const char* package, uint32_t uid);

// Cheap probe used before touching prctl: true only when this package has a
// non-empty process-hiding scope. Non-targets must stay untouched so other
// modules (e.g. UID) can own the platform filter without a second hook.
bool is_target(zygisk::Api* api, const char* package, uint32_t uid);

// Hands the listener to the companion.
bool send_listener(int socket, int listener);

// Implemented by uid_broker.cpp.
void companion(int socket);

} // namespace hma_scope
