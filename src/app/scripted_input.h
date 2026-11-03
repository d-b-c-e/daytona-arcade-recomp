// Explicit diagnostic key-state provider. It never sends operating-system input.
#pragma once
#include "app/controls.h"
#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace app {
std::string read_shared_bounded(const std::string &path, size_t limit);
struct InputGuard {
    std::string session;
    double unix_seconds = 0;
    bool ok = false, pending = true, cleanup = true, adopted = false;
    static InputGuard parse(std::string_view text);
};
class ScriptedInput {
public:
    struct Command { uint64_t first, last; Action action; };
    std::vector<Command> commands;
    std::string session, guard_path, reason;
    uint64_t expiry_ms = 0, frames = 0;
    bool armed = false, revoked = false;
    void load(std::string_view text, const std::string &expected_session, uint64_t expected_frames, uint64_t now_ms);
    std::array<bool, SDL_SCANCODE_COUNT> keys(uint64_t frame, const Controls &controls) const;
    // Production policy, separately testable without opening a device or initializing SDL.
    bool admit(const InputGuard &guard, uint64_t now_ms, bool wants_input);
    void revoke(const std::string &why) { if (!revoked) reason = why; revoked = true; }
    std::array<bool, SDL_SCANCODE_COUNT> tick(uint64_t frame, const Controls &controls, uint64_t now_ms);
    void write_trace(std::ostream &out, uint64_t frame, const rt::Inputs &offered, const Controls &controls,
                     rt::M2Board &board) const;
};
} // namespace app
