# Private diagnostic audio and mute evidence

The two retained Windows readback candidates skip application SDL audio
initialization and both playback-device open branches. This is source-backed
diagnostic exclusion, separate from an ordinary saved mute/volume setting.
There is no retained measured audio-subsystem/device-open counter or physical
silence qualification. No new runtime, import scan, build or device test was
performed for this note.

Qualified runtime source/evidence remains at
`E:/Source/_work/remi-daytona-provider-free-20261103`, branch
`remi/daytona-provider-free-20261103`, handoff commit
`a2c0ec95331a64d8542c742b1fa5f4fe431dc03c`. Local main now includes that qualified
native/harness history; the accompanying change is documentation only.

## Actual consumer path

In [main.cpp](../src/app/main.cpp), validated readback overrides the private
configuration after argument/config loading: `native_audio=false`, `mute=true`,
windowed hardware renderer and link disabled. The normal settings are not saved
over by this override.

The initial `SDL_Init` includes VIDEO and excludes GAMEPAD for readback; it does
not include AUDIO. `audio_initialized = !readback && SDL_InitSubSystem(SDL_INIT_AUDIO)`
short-circuits before the audio call. `start_game` sets `native_active` from the
overridden native-audio selection. Consequently the native branch is not entered,
and `else if (audio_initialized)` also skips reference `Audio::open`. That leaves
`have_audio=false`, so the reference `audio.push` call is skipped. The reference
guest sound board still exists and advances; guest synthesis is not host playback.

The ordinary reference open calls `SDL_OpenAudioDevice` before binding its streams.
The [native open](../src/app/native_audio.h) calls `SDL_OpenAudioDeviceStream`;
`available()` checks its stream, so subsequent native sync/resume returns with no
stream in the diagnostic. Neither backend is replaced by a dummy-driver request.
These paths remain present for ordinary launches. Saved mute/gain would alone
leave device acquisition possible; `DBCE_FFB_MUTE` is a force-output interlock,
not an audio-subsystem observation.

Both compiled-source revisions below were checked for these same initialization
and open guards. Their `native_audio.h` Git blob is
`835edf60685e14c0fcf31ad82543fc3730410320`. Current `main.cpp` matches the ordinary
candidate source blob `3668acdc11ea241df300c2d0f069fcc2bb60e9f5`; the neutral
candidate used `4220a57a2ff42ad976967627dc184808664863af`.

## Pinned retained candidates

All paths in this table are relative to the retained runtime checkout above.
Hashes are SHA-256. Neither archived executable was rebuilt or modified.

| Item | Neutral attract, GO503291 / attempt-02 | Ordinary controls, GO503981 / attempt-03 |
|---|---|---|
| Compiled source | `ea8ed4b7a0b9c7f6a20dc960b6fb4cdd7d1a9158` | `0e65386fbcedeb3e7639bacd3a10cc947b162c0f` |
| Frozen runner source | `a7edd8fac17268a92fdd3e39a847fbb3f440aa52` | `8c5d1c8c886a55dd6117ca329f8c6e45ab5717f0` |
| `frozen-payload/daytona.exe` | `CF3BB506CB98C793D177DDB5EB15F05980781E820023CE1A64D88C9A20D83B5A` | `AE93D5F4CC9EFF13C9EE54D4048671D5D0E9E552D661DC1DF0B74C7AB56ECAA2` |
| `prepared.json` | `AAA8C605310448F61433875D458DD81369AC40522806AFA74817351762A4C386` | `99EC3AC023AAC5003DFE60DE404E44D63DB3171F617DC185168CE7992AB638AC` |
| `frozen-payload/archive.json` | `5C7C69C7AFBC6940C080D6E9E2C6923AC179F4F28544F1347CBD104569625FD9` | `FF68CEE4DE403BA66E48E90C8E16F9828139853F445015AAD4C7A2FC82918F16` |
| `qualification.json` | `645976C72D7845210084538B469572E0E591D86075C5DB8A9A162C7A1AF0101E` | `8C6C318CCC6CBE79FEAFC710CD28E8FF6BAC3ACAF627F947C2D41D12446A9599` |
| Frozen payload count | 22 files | 24 files |

Evidence directories:
[neutral attempt-02](E:/Source/_work/remi-daytona-provider-free-20261103/build/readback/attempt-02/)
and [ordinary attempt-03](E:/Source/_work/remi-daytona-provider-free-20261103/build/readback/attempt-03/).

## What the records establish

Neutral readback has 63 lifecycle rows, ending at 1800 guest frames / 5634
successful presents. Ordinary readback has 202 rows, ending at 6000 frames /
18677 presents. Their fields include force/input/network declarations but no
audio-initialized or measured device-open field. These rows cannot independently
prove zero audio-subsystem calls. Accepted scene/control/cleanup evidence is
described in [HANDOFF.md](../HANDOFF.md); this note does not expand it.

Both parent `live/game-stdout.txt` files are empty. Attempt-02 retained no private
daytona.log. The actual attempt-03 witness is
`live/state/daytona.log`, 122 bytes, SHA-256
`CC1930B64B120F89651514CA0490383C1D20F43914216A821653B4D1F6937CCD`.
Its four lines report Direct3D12, `audio backend reference`, hardware rendering
and 6000 frames. The backend line is printed after constructing the guest game
from `native_active`; it does not depend on `have_audio` or prove a host device
opened. The log contains no `audio driver` or `no audio output` line. Absence of
those lines is not a measured audio-subsystem/device-open witness.

## Existing import audit and limits

The prior read-only full PE tables are retained in Burnout1's runtime evidence:
[audio-audit directory](E:/Source/_work/remi-burnout1-provider-free-20261103/build/readback/audio-audit/).
`daytona-02-imports.json` has 563 entries, SHA-256
`4A1F84487BFA42E167236A5D5B60F2C32C470085878E45D554BAD4F618749695`;
`daytona-03-imports.json` has 573, SHA-256
`3302BC5CE8B7AAFAE866C7A880265733FF870036B56C5E5DCB0E0C5C8F153CA1`.
These existing inventories were read, not regenerated. Their WINMM imports are
only `timeBeginPeriod` and `timeEndPeriod`, with no direct waveOut/waveIn entries.
Dynamic loading and `CoCreateInstance` remain. An import table cannot exclude
dynamic backend/device activity or prove physical silence. Its `forbidden`
array lists eight input/cursor signals; it is not an audio resolution catalog.

This is a pinned application-consumer argument, not a whole-process or vendor
library audio-absence claim. SDL VIDEO/EVENTS and rendering still run. Physical
silence, a changed/failing audio path, normal player audio, physical controls/FFB,
triples, pacing and owner acceptance remain separate gates. No listening, loopback,
speaker/device state, global mute or owner setting was inspected or changed.

## Standards and landing scope

STD-011 is recorded in TOOLKIT-ADOPTION: qualified native/harness history landed
on local main before this docs-only companion. STD001–010 remain unchecked;
034/035/043 retain their existing partial scopes. Ledger comparison also found
missing 012–033, 036–042 and 044–049 rows. This notes task does not implement or
qualify those unrelated standards. No new family standard is introduced.
