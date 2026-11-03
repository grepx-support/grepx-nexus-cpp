/*
 * Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
 * This file is part of the GrepX platform and is proprietary and confidential.
 * Unauthorised copying, use, modification or distribution is strictly prohibited.
 */

#pragma once
#include "nexus/app/app_engine.hpp"

namespace nexus::app {

// The runner receives the wired engine and argc/argv, and decides what
// to actually DO — run a canned console demo, drive a network dispatch
// loop, whatever. Composition (AppEngine<T>) and behavior (Runner) are
// separate concerns; this is what lets nexus_demo and nexus_server
// share the identical trait-wiring code in their main()s while doing
// completely different things with the result.
template <typename T, typename Runner>
int launch_app(int argc, char* argv[], Runner&& runner) {
    AppEngine<T> engine;
    return runner(engine, argc, argv);
}

} // namespace nexus::app
