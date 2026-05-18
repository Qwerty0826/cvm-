#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <functional>
#include <unordered_map>
#include <optional>
#include <cassert>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>

// ── project-wide version ──────────────────────────────────────────────────────
constexpr int CVM_VERSION_MAJOR = 1;
constexpr int CVM_VERSION_MINOR = 0;
constexpr int CVM_VERSION_PATCH = 0;

// ── VM limits ─────────────────────────────────────────────────────────────────
constexpr int STACK_MAX       = 512;
constexpr int FRAMES_MAX      = 64;
constexpr int MAX_CONSTANTS   = 65536;

// ── ANSI colours (gracefully disabled when not a tty) ────────────────────────
namespace Color {
    inline bool enabled = true;
    inline const char* RED    () { return enabled ? "\033[1;31m" : ""; }
    inline const char* YELLOW () { return enabled ? "\033[1;33m" : ""; }
    inline const char* CYAN   () { return enabled ? "\033[1;36m" : ""; }
    inline const char* GREEN  () { return enabled ? "\033[1;32m" : ""; }
    inline const char* BOLD   () { return enabled ? "\033[1m"    : ""; }
    inline const char* RESET  () { return enabled ? "\033[0m"    : ""; }
}
