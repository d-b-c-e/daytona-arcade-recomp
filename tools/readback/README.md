# Neutral attract readback

The first case checks native startup, exactly 1800 neutral board frames, hardware
renderer progress, two bounded window captures, native exit and owner-state
preservation. It does not test control delivery, racing, wheel acquisition,
physical force feedback, triples, settings reachability or owner acceptance.

The opt-in command is `--readback-state ABSOLUTE_PRIVATE_DIRECTORY
--readback-session 32_LOWERCASE_HEX --frames 1800`, with `DBCE_FFB_MUTE=1`.
The directory must exist, with no previous `readback.jsonl`. It replaces SDL's
pref directory before logging/loading, so settings, EEPROM and backup RAM are
private. Defaults remain unchanged for ordinary launches. The diagnostic forces
windowed hardware rendering, disables link/audio/device subsystems, bypasses
physical keyboard/gamepad sampling with neutral `rt::Inputs`, and ignores input
events. No global input, virtual controller, activation or network output exists
in the runner. Force output has an immutable process latch; the environment
mute also applies to ordinary launches, without rewriting saved force strength.

R0: `test_readback` links the production force implementation to inert SDL
functions, proving zero calls while muted and exercising the ordinary path as a
positive control. It also checks strict budgets/private-log admission. The
separate Python fixtures call production stale/nonce/isolation/owner readers.
`Rehearse.ps1` exercises actual suspended-root job membership, quiescence and
1001 shared-delete snapshot replacements while an owned child reads them.
The existing `test_app_controls` suite attaches SDL virtual joysticks and is
deliberately outside this R0 invocation.

R1: the graphical app flushes an append-only session/frame/present/isolation log
every 30 guest frames. This is the same graphical `GameLoop::run_frame` path;
frame counters alone do not prove rendered content. At 1800 frames it publishes
`closing` and waits up to 15 seconds for the private cleanup handshake before
removing its window. The parent enters cleanup before acknowledging that file;
the bounded acknowledgment must contain the exact session nonce. Startup records
have a 90-second bound; running progress expires after three seconds. Submitted
present counts require successful SDL GPU submission. ImGui is explicitly set
to manual empty gamepads before any frame.

R2: after frames 600 and 1200, the worker uses the pinned window enumerator to
select the sole exact `SDL_app`/`Daytona USA` window for its owned PID. Each burst
is at most two seconds/three frames. The capture tool verifies PID/start time,
HWND/class/visibility every save and retains image hashes/timestamps. The worker
and all its game/capture children inherit one job established before root resume.

`Build.ps1 -Go ID` takes the named CPU lease, copies retained dependencies and
generated daytona93 C++ into the private ignored build tree, records generated
input hashes, builds with ClangCL/strict FP and two workers, runs the inert
checks, and releases. It never launches the game or starts hooks. Retained
generated C++ provenance is explicitly a local build cache, not fresh recompilation
or parity evidence. No ROM or derived/generated assets are committed.

After a clean pinned build, `Prepare.ps1 -Attempt attempt-01` records source,
candidate/helper/ROM/runner pins and read-only owner snapshots. Send the concrete
prepared manifest SHA for one live GO. Then run `Invoke.ps1 -Attempt attempt-01
-Go ID -PreparedSha256 SHA`. It requires 180 seconds of raw idle, a named lease
and the qualified low-level observer. No hook, game or capture starts at prepare.
Executable paths are canonicalized before comparison with native process image
identity; the capture host, managed DLL and its runtime payload are all pinned.

The supervisor admits only the exact owned game and bounded startup transitions;
physical/injected input, unexplained raw resets, foreign foreground, stale
heartbeat or display drift terminally stop the affected run. Parent guard files
use whole-object native replacement; readers share READ/WRITE/DELETE. The final
owner verification is a separate process after job quiescence, comparing presence,
SHA256, byte length, exact numeric creation/write UTC ticks and attributes for the
installed game and real `%APPDATA%/daytona-recomp/daytona93` tree. No restoration
write is planned: the game uses a fresh private pref tree. Unexpected drift or
unresolved cleanup retains controller/job/lease custody; no forced retry follows.
Windows may create a hidden console host even for a `CREATE_NO_WINDOW` console
root. It is admitted only by the pinned System32 path, membership in this fresh
private job, and creation in the root's epoch. This does not adopt another
console or authorize an outside process. Every observed job identity is retained
before classification failure. The inert rehearsal uses this same classifier,
records actual host command/parent metadata, and checks wrong-image/old-epoch
and additional-Python refusal cases. Earlier root-only membership rehearsals did
not test the live image gate; attempt-01 exposed that gap before game launch.

Impact: one visible 992x768 window, ordinary foreground transition only, no input
injection, no physical devices, no audio, no display change; native CPU/GPU load
and six sampled window images maximum. Startup bound is 150 seconds, guest run
1800 frames (about 31 seconds at native rate), total owned-job bound 190 seconds,
cleanup bound 45 seconds. Capture/lease/owner proof still require live execution.

Design basis: design document sections “Architecture”, “Audio, inputs, force
feedback, link play”, and “Platform layer & build”; no guest logic, rendering,
FP arithmetic or native game rate changes.
