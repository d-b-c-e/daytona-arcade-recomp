// Production force-output implementation with inert SDL entry points. No SDL library/devices.
#include "app/ffb.h"
#include "app/readback.h"
#include <cstdio>
#include <cstdlib>

static int calls = 0, effects = 0;
SDL_Joystick * SDLCALL SDL_GetGamepadJoystick(SDL_Gamepad *) { ++calls; return reinterpret_cast<SDL_Joystick *>(1); }
bool SDLCALL SDL_IsJoystickHaptic(SDL_Joystick *) { ++calls; return true; }
SDL_Haptic * SDLCALL SDL_OpenHapticFromJoystick(SDL_Joystick *) { ++calls; return reinterpret_cast<SDL_Haptic *>(2); }
SDL_PropertiesID SDLCALL SDL_GetJoystickProperties(SDL_Joystick *) { ++calls; return 1; }
bool SDLCALL SDL_GetBooleanProperty(SDL_PropertiesID, const char *, bool) { ++calls; return true; }
Uint32 SDLCALL SDL_GetHapticFeatures(SDL_Haptic *) { ++calls; return SDL_HAPTIC_SPRING | SDL_HAPTIC_FRICTION | SDL_HAPTIC_SINE | SDL_HAPTIC_CONSTANT | SDL_HAPTIC_GAIN | SDL_HAPTIC_AUTOCENTER; }
bool SDLCALL SDL_SetHapticAutocenter(SDL_Haptic *, int) { ++calls; return true; }
bool SDLCALL SDL_SetHapticGain(SDL_Haptic *, int) { ++calls; return true; }
SDL_HapticEffectID SDLCALL SDL_CreateHapticEffect(SDL_Haptic *, const SDL_HapticEffect *) { ++calls; return effects++; }
bool SDLCALL SDL_StopHapticEffects(SDL_Haptic *) { ++calls; return true; }
void SDLCALL SDL_CloseHaptic(SDL_Haptic *) { ++calls; }
bool SDLCALL SDL_RumbleJoystick(SDL_Joystick *, Uint16, Uint16, Uint32) { ++calls; return true; }
bool SDLCALL SDL_UpdateHapticEffect(SDL_Haptic *, SDL_HapticEffectID, const SDL_HapticEffect *) { ++calls; return true; }
bool SDLCALL SDL_RunHapticEffect(SDL_Haptic *, SDL_HapticEffectID, Uint32) { ++calls; return true; }
SDL_Joystick *app::Devices::find(const std::string &) const { ++calls; return reinterpret_cast<SDL_Joystick *>(1); }

static unsigned assertions = 0;
static void check(bool value) { ++assertions; if (!value) { std::fprintf(stderr, "readback assertion %u failed\n", assertions); std::exit(1); } }
template<class F> void refuses(F f) { bool failed = false; try { f(); } catch (const std::exception &) { failed = true; } check(failed); }
int main() {
    app::Devices d;
    d.pad = reinterpret_cast<SDL_Gamepad *>(1);
    app::Controls c;
    { app::ForceFeedback muted(true); for (float strength : {0.f, 0.7f, 1.f}) {
        muted.update({0, 0x10, 0x40, 0xff}, d, c, strength, true); muted.stop(); muted.close();
    } check(std::string(muted.device_kind()) == "none"); }
    check(calls == 0); // includes destruction; no acquisition, effects, rumble or zero-output call
    { app::ForceFeedback ordinary; ordinary.update({0x10, 0x40}, d, c, 1, false); check(calls > 0); check(effects == 4); ordinary.stop(); }
    check(app::Readback::budget("600") == 600);
    check(app::Readback::budget("3600") == 3600);
    for (auto bad : {"", "0", "599", "3601", "1800x", "-1", "1.5", "999999999999999999"}) refuses([&] { app::Readback::budget(bad); });
    app::Readback inactive; inactive.validate(false); check(!inactive);
    inactive.requested = true; refuses([&] { inactive.validate(true); });
    app::Readback bad; bad.dir = "."; bad.session = std::string(32, 'a'); bad.frames = 1800;
    refuses([&] { bad.validate(false); }); refuses([&] { bad.validate(true); });
    bad.dir = std::filesystem::absolute("readback-test-state").string(); bad.session = "invalid";
    refuses([&] { bad.validate(true); });
    // New path only: never remove an existing directory to make the fixture pass.
    check(std::filesystem::create_directory(bad.dir));
    bad.session = std::string(32, 'a'); bad.validate(true);
    bad.write("running", 600, 620, true); bad.log.close();
    check(!bad.cleanup_ack());
    { std::ofstream f(bad.dir + "cleanup.ok"); f << std::string(32, 'b'); } check(!bad.cleanup_ack());
    { std::ofstream f(bad.dir + "cleanup.ok"); f << bad.session; } check(bad.cleanup_ack());
    { std::ofstream f(bad.dir + "cleanup.ok"); f << bad.session << 'x'; } check(!bad.cleanup_ack());
    check(std::filesystem::file_size(std::filesystem::path(bad.dir) / "readback.jsonl") > 100);
    app::Readback duplicate; duplicate.dir = bad.dir; duplicate.session = bad.session; duplicate.frames = 1800;
    refuses([&] { duplicate.validate(true); });
    std::filesystem::remove(std::filesystem::path(bad.dir) / "readback.jsonl");
    std::filesystem::remove(std::filesystem::path(bad.dir) / "cleanup.ok"); std::filesystem::remove(bad.dir);
    std::printf("readback: %u assertions passed; fake SDL calls blocked while muted\n", assertions);
}
