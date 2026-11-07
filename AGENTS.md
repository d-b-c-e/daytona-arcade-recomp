# Daytona USA — agent guide

Portfolio rules: `E:\Source\dbce-project-mgmt\HOW-WE-WORK.md` (read first; it replaces older process, lease, review and testing instructions). Current focus: `E:\Source\dbce-project-mgmt\NOW.md`. Feature status lives in the portfolio grid (`python E:\Source\dbce-project-mgmt\tools\portfolio_grid.py plan daytona`), not in this file. Grid id: `daytona`.

## Project

Static recompilation of Daytona USA (Sega Model 2): the i960 program and the TGP program are recompiled to C++, and the hardware (geometrizer, rasterizer, tilemaps, sound board) is native C++. MAME is only a test oracle. Remote `d-b-c-e/daytona-arcade-recomp` (visibility unconfirmed, so do not push). Project licence is undecided. No Sega code, ROM bytes or ROM-derived output in the repo; users supply `daytona93` (Deluxe '93) and/or `daytona` (Rev A) ROM sets.

## Build

`powershell -ExecutionPolicy Bypass -File setup.ps1` on Windows (Clang from VS Build Tools; `--msvc` for MSVC), `./setup.sh` elsewhere. It installs the toolchain, fetches pinned dependencies into `extern/`, builds and runs tests. ROMs go in `roms/daytona93.zip` and/or `roms/daytona.zip`. After changing the recompiler or seeds: `python3 scripts/recompile.py` (`--set daytona --build-dir build-daytona` for Rev A).

## Test

Setup runs the tests without a ROM set; ROM-dependent checks use MAME as the oracle (`--with-mame`, `--build-mame`). Settings, EEPROM and backup RAM are in the user data folder (`launcher.ini`); Rev A (`build-daytona`) has its own. Back them up and restore them around a run.

## Install / run on the rig

`build\Release\daytona.exe` (Visual Studio generator) or `build/daytona`; Rev A is `build-daytona/daytona`. Options: `--rom FILE.zip --autostart --gpu vulkan|direct3d12|metal --fullscreen`. Esc opens the launcher in game.

## Shared code

Toolkit adoption is tracked in `TOOLKIT-ADOPTION.md`. Change shared code in the toolkit and re-sync; never edit the vendored copy.

## Project-specific rules

The binding technical and legal rules are in `rules.md`; the essentials:
- No Sega code or assets, no leaked source, and no ROM-derived files in git (generated C++, decompiled output, MAME traces).
- Third-party components are recorded in `THIRD_PARTY.md` with upstream, commit and licence; check licence compatibility first.
- Generated code is never hand-edited; fix the recompiler or add a seed.
- FP results that can reach memory go through SoftFloat `extF80`; no `-ffast-math`; TGP results must match MAME bit for bit.
- Enhancements stay off by default; with them off the game must match MAME.
- Commits carry no assistant attribution (`rules.md`). `extern/` and build output are git-ignored.

## Key docs

- `README.md`, `docs/getting-started.md`
- `docs/daytona-usa-recomp-design.md`, `rules.md`
- `THIRD_PARTY.md`, `TOOLKIT-ADOPTION.md`


<!-- BEGIN project-file-location -->
## Project files and temporary work

Keep project-related research, evidence, scripts, packages, and other working files inside the relevant project folder whenever practical. Keep disposable or machine-specific outputs git-ignored when they do not belong in version control; never ignore files that should be committed merely to hide them. When a project folder is unsuitable, use `E:\Source\temp\<project>` instead of creating scattered drive-root folders. Record the location of material needed by later work and update references when relocating it.

Clean up temporary files and disposable copies promptly once they are no longer useful. Preserve active work, user data, settings, saves, required evidence, backups, rollback artifacts, and worktrees until their retention or replacement is established. Follow existing project, rig, and data-loss safeguards; do not remove files merely because they are old or untracked.
<!-- END project-file-location -->
