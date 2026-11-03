// Explicit, bounded neutral-input diagnostic. No owner pref-path access.
#pragma once
#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace app {
struct Readback {
    std::string dir, session;
    unsigned frames = 0;
    std::ofstream log;
    unsigned sequence = 0;
    explicit operator bool() const { return !dir.empty(); }
    static unsigned budget(std::string_view value) {
        unsigned n = 0;
        const auto r = std::from_chars(value.data(), value.data() + value.size(), n);
        if (r.ec != std::errc{} || r.ptr != value.data() + value.size() || n < 600 || n > 3600)
            throw std::runtime_error("readback requires 600..3600 whole frames");
        return n;
    }
    void validate(bool environment_mute) {
        if (!*this && session.empty()) return;
        if (!*this || !environment_mute || session.size() != 32 ||
            !std::all_of(session.begin(), session.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
            throw std::runtime_error("readback requires a private absolute directory, 32 lowercase hex session and DBCE_FFB_MUTE=1");
        budget(std::to_string(frames));
        const std::filesystem::path p(dir);
        if (!p.is_absolute() || !std::filesystem::is_directory(p) || std::filesystem::is_symlink(p) ||
            std::filesystem::exists(p / "readback.jsonl"))
            throw std::runtime_error("readback requires a fresh existing private absolute directory");
        dir = p.generic_string() + "/";
        log.open(p / "readback.jsonl", std::ios::out | std::ios::binary);
        if (!log) throw std::runtime_error("cannot create private readback log");
    }
    void write(const char *phase, uint64_t frame, uint64_t presents, bool hardware) {
        if (!*this) return;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        log << "{\"session\":\"" << session << "\",\"sequence\":" << ++sequence
            << ",\"unixMs\":" << ms << ",\"phase\":\"" << phase << "\",\"frames\":" << frame
            << ",\"presents\":" << presents << ",\"hardware\":" << (hardware ? "true" : "false")
            << ",\"neutralInput\":true,\"forceMuted\":true,\"deviceInput\":false,\"network\":false}\n";
        log.flush();
        if (!log) throw std::runtime_error("private readback write failed");
    }
};
} // namespace app
