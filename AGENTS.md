# Repository working context

Read [MEMORY.md](MEMORY.md) before working in this repository. It records the
current asset layout, active entry points and validation limits after the
2026-11-02 directory refactor. Update it when these facts change.

Game code stays in `ThreeDreamGolems/`. Authoring assets and their review
projects live in `Assets/`; see [Assets/README.md](Assets/README.md).

The game is **Three Dream Golems**. Its design, dev spec and milestones are
in `ThreeDreamGolems/Docs/`; read `ThreeDreamGolems/README.md` first.
The project, its modules, classes and content were renamed from Knight to
Golem on 2026-11-12 (milestone R0, see `ThreeDreamGolems/Docs/REFACTOR_REPORT.md`).
That rename covers the game project only.

For Medieval work, also read `Assets/hda_medieval_building/AGENTS.md` and the
current function review guide it references. Follow the installed
`.agents/skills/design-hda-building/SKILL.md` and its implementation references
when changing building sources. Preserve native SOP/VEX geometry and existing
parameter/preset contracts; directory maintenance is not geometry regeneration.

Resolve paths from `__file__` or an explicit project root. Keep the asset
projects' Unreal project and module names (`TDKProps`, `TDKKingdomMap`,
`/Game/...`) unchanged when only their filesystem directories move. Do not edit third-party plugins or generated
Unreal output to repair source paths. Historical reports retain their original
paths and hashes; they are evidence of their recorded runs, not current config.

Run `python tools/audit_paths.py` after path changes. This is a static audit;
report Houdini cook/render and Unreal editor checks separately when executed.
