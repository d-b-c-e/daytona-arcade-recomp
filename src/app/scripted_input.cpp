#include "app/scripted_input.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace app {
std::string read_shared_bounded(const std::string &path, size_t limit) {
    std::string text(limit + 1, '\0');
#ifdef _WIN32
    const auto name = std::filesystem::path(path).wstring();
    const HANDLE h = CreateFileW(name.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) throw std::runtime_error("input request/guard open failed");
    DWORD bytes = 0;
    const bool ok = ReadFile(h, text.data(), DWORD(text.size()), &bytes, nullptr) != 0;
    const bool closed = CloseHandle(h) != 0; // close before parsing an actively replaced snapshot
    if (!ok || !closed) throw std::runtime_error("input request/guard read or close failed");
    text.resize(bytes);
#else
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("input request/guard open failed");
    f.read(text.data(), std::streamsize(text.size()));
    text.resize(size_t(f.gcount()));
    if (f.bad()) throw std::runtime_error("input request/guard read failed");
#endif
    if (text.size() > limit) throw std::runtime_error("input request/guard oversized");
    // The protocol is an ASCII subset of strict UTF-8. No BOM, NUL, escapes or alternate keys.
    for (unsigned char c : text)
        if ((c < 0x20 && c != '\n' && c != '\r' && c != '\t') || c > 0x7e)
            throw std::runtime_error("input protocol is not strict ASCII UTF-8");
    return text;
}
namespace {
uint64_t number(std::string_view v) {
    uint64_t n = 0;
    const auto result = std::from_chars(v.data(), v.data() + v.size(), n);
    if (v.empty() || result.ec != std::errc{} || result.ptr != v.data() + v.size()) throw std::runtime_error("bad input integer");
    return n;
}
bool nonce(std::string_view s) {
    return s.size() == 32 && std::all_of(s.begin(), s.end(), [](char c) {return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');});
}
}
InputGuard InputGuard::parse(std::string_view t) {
    size_t p = 0; unsigned seen = 0; InputGuard g;
    auto ws = [&] { while (p < t.size() && (t[p] == ' ' || t[p] == '\r' || t[p] == '\n' || t[p] == '\t')) ++p; };
    auto expect = [&](char c) { ws(); if (p == t.size() || t[p++] != c) throw std::runtime_error("bad input guard syntax"); };
    auto word = [&]() {
        expect('"'); const auto begin = p;
        while (p < t.size() && t[p] != '"') { if (t[p] == '\\' || uint8_t(t[p]) < 0x20 || uint8_t(t[p]) > 0x7e) throw std::runtime_error("bad input guard string"); ++p; }
        if (p == t.size()) throw std::runtime_error("truncated input guard string");
        const std::string value(t.substr(begin, p - begin)); ++p; return value;
    };
    expect('{');
    for (;;) {
        const auto key = word(); expect(':'); ws();
        unsigned bit = 0;
        if (key == "session") { bit = 1; g.session = word(); }
        else if (key == "unix") {
            bit = 2; const auto begin = p;
            while (p < t.size() && ((t[p] >= '0' && t[p] <= '9') || t[p] == '.')) ++p;
            const auto r = std::from_chars(t.data() + begin, t.data() + p, g.unix_seconds);
            if (begin == p || r.ec != std::errc{} || r.ptr != t.data() + p || !std::isfinite(g.unix_seconds)) throw std::runtime_error("bad guard timestamp");
        } else {
            bool value;
            if (t.substr(p, 4) == "true") value = true, p += 4;
            else if (t.substr(p, 5) == "false") value = false, p += 5;
            else throw std::runtime_error("bad input guard boolean");
            if (key == "ok") bit = 4, g.ok = value;
            else if (key == "pending") bit = 8, g.pending = value;
            else if (key == "cleanup") bit = 16, g.cleanup = value;
            else if (key == "adopted") bit = 32, g.adopted = value;
            else throw std::runtime_error("unknown input guard key");
        }
        if (seen & bit) throw std::runtime_error("duplicate input guard key");
        seen |= bit; ws();
        if (p < t.size() && t[p] == '}') { ++p; break; }
        expect(',');
    }
    ws(); if (p != t.size() || seen != 63 || !nonce(g.session)) throw std::runtime_error("incomplete input guard");
    return g;
}
void ScriptedInput::load(std::string_view text, const std::string &expected_session, uint64_t expected_frames, uint64_t now_ms) {
    if (text.size() > 65536) throw std::runtime_error("input program oversized");
    for (unsigned char c : text)
        if ((c < 0x20 && c != '\n' && c != '\r' && c != '\t') || c > 0x7e) throw std::runtime_error("bad input program UTF-8");
    unsigned seen = 0; std::istringstream input{std::string(text)}; std::string line;
    commands.clear(); armed = revoked = false; reason.clear();
    while (std::getline(input, line)) {
        if (line.size() > 256) throw std::runtime_error("input line oversized");
        std::istringstream tokens(line); std::string a, b, c, extra;
        if (!(tokens >> a) || a[0] == '#') continue;
        if (!(tokens >> b)) throw std::runtime_error("truncated input line");
        if (a == "session" || a == "expires_ms" || a == "frames") {
            const unsigned bit = a == "session" ? 1 : a == "expires_ms" ? 2 : 4;
            if ((seen & bit) || (tokens >> extra)) throw std::runtime_error("duplicate or malformed input header");
            seen |= bit;
            if (bit == 1) session = b;
            else if (bit == 2) expiry_ms = number(b);
            else frames = number(b);
            continue;
        }
        if (!(tokens >> c) || (tokens >> extra) || commands.size() >= 128) throw std::runtime_error("bad input command");
        const uint64_t first = number(a), last = number(b);
        Action action = Action(kNumActions);
        for (int i = 0; i < kNumActions; ++i)
            if (c == action_key(Action(i)) && i != Test && i != Service) action = Action(i);
        if (action == kNumActions || first < 1 || first > last || last > expected_frames) throw std::runtime_error("input range/action refused");
        for (const auto &prior : commands)
            if (prior.action == action && first <= prior.last && last >= prior.first) throw std::runtime_error("overlapping input commands");
        commands.push_back({first, last, action});
    }
    if (seen != 7 || !nonce(session) || session != expected_session || frames != expected_frames || frames < 600 || frames > 7200 ||
        expiry_ms <= now_ms || expiry_ms - now_ms > 300000 || commands.empty()) throw std::runtime_error("input session/budget/expiry refused");
}
std::array<bool, SDL_SCANCODE_COUNT> ScriptedInput::keys(uint64_t frame, const Controls &controls) const {
    std::array<bool, SDL_SCANCODE_COUNT> result{};
    if (revoked || frame < 1 || frame > frames) return result;
    for (const auto &command : commands) if (frame >= command.first && frame <= command.last) {
        const auto key = controls.bind[command.action].key;
        if (key == SDL_SCANCODE_UNKNOWN || key < 0 || key >= SDL_SCANCODE_COUNT) throw std::runtime_error("input action has no bounded key");
        result[size_t(key)] = true;
    }
    return result;
}
bool ScriptedInput::admit(const InputGuard &g, uint64_t now_ms, bool wants_input) {
    if (revoked) return false;
    const double age = double(now_ms) / 1000 - g.unix_seconds;
    if (g.session != session || !g.ok || g.cleanup || !(age >= 0 && age <= 2) || now_ms >= expiry_ms) {
        revoke("guard/session/expiry revoked"); return false;
    }
    if (!g.adopted || g.pending) {
        if (armed) revoke("input admission lost");
        return false;
    }
    if (wants_input) armed = true;
    return true;
}
std::array<bool, SDL_SCANCODE_COUNT> ScriptedInput::tick(uint64_t frame, const Controls &controls, uint64_t now_ms) {
    try {
        auto result = keys(frame, controls);
        const bool wants = std::any_of(result.begin(), result.end(), [](bool b) {return b;});
        if (admit(InputGuard::parse(read_shared_bounded(guard_path, 4096)), now_ms, wants)) return result;
    } catch (const std::exception &) { revoke("input guard read/parse failed"); }
    return {};
}
void ScriptedInput::write_trace(std::ostream &out, uint64_t frame, const rt::Inputs &in, const Controls &controls,
                                rt::M2Board &board) const {
    const auto &seen = board.io().observation;
    const auto utc_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    out << "{\"session\":\"" << session << "\",\"frame\":" << frame
        << ",\"unixMs\":" << utc_ms
        << ",\"source\":\"Controls::sample(private-keys)\",\"armed\":" << (armed ? "true" : "false")
        << ",\"revoked\":" << (revoked ? "true" : "false") << ",\"reason\":\"" << reason
        << "\",\"offered\":[" << int(in.steer) << ',' << int(in.accel) << ',' << int(in.brake) << ','
        << int(in.in0) << ',' << int(in.in1) << "],\"controlsGear\":" << controls.gear
        << ",\"polls\":" << seen.polls << ",\"latchFrame\":" << seen.latched_frame;
    auto fields = [&](const char *name, const auto &values) {
        out << ",\"" << name << "\":[";
        bool first = true;
        for (size_t index : {0u, 1u, 2u, 8u, 9u}) { if (!first) out << ','; first = false; out << uint64_t(values[index]); }
        out << ']';
    };
    fields("guestValues", seen.values); fields("readFrames", seen.read_frames); fields("readCounts", seen.reads);
    // Source-traced guest-written input history/filtered ADC/digital words, not injector storage.
    out << ",\"cacheAdc\":[" << unsigned(board.read_byte(0x53e000)) << ',' << unsigned(board.read_byte(0x53e004))
        << ',' << unsigned(board.read_byte(0x53e008)) << "],\"cacheAverage\":[" << board.read_word(0x53e5c0)
        << ',' << board.read_word(0x53e5c2) << ',' << board.read_word(0x53e5c4)
        << "],\"cacheButtons\":" << board.read_dword(0x53e120)
        << ",\"gameMode\":[" << unsigned(board.read_byte(0x5010a0)) << ',' << unsigned(board.read_byte(0x5010a4)) << "]}\n";
}
} // namespace app
