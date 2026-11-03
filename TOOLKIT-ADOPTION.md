# Toolkit standards adoption

Which entries of the wheel toolkit's standards ledger
(`E:\Source\toolkits\dbce-wheel-mod-toolkit\STANDARDS.md`) this mod has brought in.
Update a row in the same commit that adopts it. Statuses: `adopted`, `partial`,
`pending`, `n/a` (say why), `unchecked` (nobody has looked yet).
Seeded 2026-10-04 from what was verified that day; `unchecked` rows need a look.

| Standard | Title | Status | Notes |
|---|---|---|---|
| STD-001 | One mod per game | unchecked |  |
| STD-002 | Recording and playback from launch | unchecked |  |
| STD-003 | Normalized FFB strength | unchecked |  |
| STD-004 | Consistent settings UX | unchecked |  |
| STD-005 | Camera numpad layout 8/2 9/3 4/6 7/1 +/- 5 | unchecked |  |
| STD-006 | Camera step sizes are settings | unchecked |  |
| STD-007 | Triple screens in one wide window | unchecked |  |
| STD-008 | Display changes: game applies once | unchecked |  |
| STD-009 | Dashboard telemetry matches the HUD | unchecked |  |
| STD-010 | Install the latest build for testing | unchecked |  |
| STD-034 | Closed-loop observation and session control | partial | Private-key ordinary consumer uses per-frame nonce/expiry/fresh parent admission and guest-poll/cache correlation;74 actual consumer/guard/MMIO assertions passed CPU503880. Runtime controls pending; no Rig Profile Apply writer or physical qualification. |
| STD-035 | Profile writers and test isolation | partial | Explicit private state and independent environment no-force latch;26 fake-SDL/private assertions and neutral-attract R1/R2 + exact owner verification passed GO503291. No profile writer, physical device injection or physical acceptance claimed. |
| STD-043 | Held direct gear wins | partial | Production sampler prioritizes a held direct gate over simultaneous sequential shift; actual inert consumer regression passed CPU503880, including ignored-shift edge/release. No physical gearbox acceptance. |
