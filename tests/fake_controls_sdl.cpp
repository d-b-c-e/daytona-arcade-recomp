// Link-time inert SDL layer for the production controls consumer. Never link SDL.
#include <SDL3/SDL.h>
unsigned fake_sdl_calls = 0;
const char * SDLCALL SDL_GetGamepadStringForButton(SDL_GamepadButton) { ++fake_sdl_calls; return "fake"; }
const char * SDLCALL SDL_GetGamepadStringForAxis(SDL_GamepadAxis) { ++fake_sdl_calls; return "fake"; }
SDL_GamepadButton SDLCALL SDL_GetGamepadButtonFromString(const char *) { ++fake_sdl_calls; return SDL_GAMEPAD_BUTTON_INVALID; }
SDL_GamepadAxis SDLCALL SDL_GetGamepadAxisFromString(const char *) { ++fake_sdl_calls; return SDL_GAMEPAD_AXIS_INVALID; }
SDL_Joystick * SDLCALL SDL_OpenJoystick(SDL_JoystickID) { ++fake_sdl_calls; return nullptr; }
void SDLCALL SDL_CloseJoystick(SDL_Joystick *) { ++fake_sdl_calls; }
SDL_JoystickID SDLCALL SDL_GetJoystickID(SDL_Joystick *) { ++fake_sdl_calls; return 0; }
SDL_Gamepad * SDLCALL SDL_OpenGamepad(SDL_JoystickID) { ++fake_sdl_calls; return nullptr; }
void SDLCALL SDL_CloseGamepad(SDL_Gamepad *) { ++fake_sdl_calls; }
SDL_JoystickID SDLCALL SDL_GetGamepadID(SDL_Gamepad *) { ++fake_sdl_calls; return 0; }
void SDLCALL SDL_GUIDToString(SDL_GUID, char *out, int n) { ++fake_sdl_calls; if (n) out[0] = 0; }
SDL_GUID SDLCALL SDL_GetJoystickGUID(SDL_Joystick *) { ++fake_sdl_calls; return {}; }
bool SDLCALL SDL_GetJoystickButton(SDL_Joystick *, int) { ++fake_sdl_calls; return false; }
Uint8 SDLCALL SDL_GetJoystickHat(SDL_Joystick *, int) { ++fake_sdl_calls; return 0; }
Sint16 SDLCALL SDL_GetJoystickAxis(SDL_Joystick *, int) { ++fake_sdl_calls; return 0; }
const char * SDLCALL SDL_GetJoystickName(SDL_Joystick *) { ++fake_sdl_calls; return "fake"; }
bool SDLCALL SDL_GetGamepadButton(SDL_Gamepad *, SDL_GamepadButton) { ++fake_sdl_calls; return false; }
Sint16 SDLCALL SDL_GetGamepadAxis(SDL_Gamepad *, SDL_GamepadAxis) { ++fake_sdl_calls; return 0; }
