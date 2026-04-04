# AGENTS.md

## Purpose

- This file is guidance for AI agents working in `AngelscriptProject`.
- The primary goal is not to extend a regular game project, but to organize, verify, and solidify `Plugins/Angelscript` as a standalone, reusable Angelscript plugin for Unreal Engine.
- This repository serves as the host project for plugin development and validation; the real deliverable is the `Angelscript` plugin itself.

## Current Project Positioning

- `Plugins/Angelscript/` is the core workspace. The vast majority of implementation, fixes, cleanup, and tests should land here first.
- `Source/AngelscriptProject/` retains only the minimal host project content. Do not push plugin logic back into the project module unless the task explicitly requires it.

## Key Paths

- `Plugins/Angelscript/Source/AngelscriptRuntime/`: Runtime module — plugin core capabilities land here first.
- `Plugins/Angelscript/Source/AngelscriptEditor/`: Editor-related support.
- `Plugins/Angelscript/Source/AngelscriptTest/`: Plugin tests and validation.
- `Documents/Guides/`: Build, test, and lookup guides.
- `Documents/Rules/`: Git commit and other rule documents.
- `Documents/Plans/`: Multi-phase task plan documents.
- `Documents/Plans/Archives/`: Archive directory and summaries for completed or closed plans.
- `Tools/`: Local helper scripts.

## External Reference Repositories

- External reference repositories are not part of this project's committed content. They are used only for comparison, migration analysis, and architectural reference.
- This section keeps an index only; detailed descriptions, usage boundaries, and priority guidance are maintained in `Reference/README.md`.

| Name | Entry & Notes |
| --- | --- |
| AngelScript v2.38.0 | Pull with `Tools\PullReference.bat angelscript`; defaults to `Reference\angelscript-v2.38.0`. See `Reference/README.md` |
| Hazelight Angelscript | Obtained via `AgentConfig.ini` key `References.HazelightAngelscriptEngineRoot`. See `Reference/README.md` |
| UnrealCSharp | Pull with `Tools\PullReference.bat unrealcsharp`; defaults to `Reference\UnrealCSharp`. See `Reference/README.md` |

- When adding new reference repositories, update `Reference/README.md` first, then add an index entry here.

## Local Configuration

- `AgentConfig.ini` in the project root stores machine-specific paths (e.g., engine root). It is excluded via `.gitignore`.
- On first use, run `Tools\GenerateAgentConfigTemplate.bat` to generate a template, then fill in local paths.
- Engine paths in build and test commands should be read from `AgentConfig.ini` key `Paths.EngineRoot`.

## Build & Validation Principles

- Build instructions: see `Documents/Guides/Build.md`.
- Test instructions: see `Documents/Guides/Test.md`.
- If documentation conflicts with the current plugin-centric goal, update the documentation first, then continue implementation.

## Documentation Maintenance Principles

- When plugin boundaries, module responsibilities, build steps, or test entry points change, update related documentation in sync.
- Chinese documentation is updated first in `Agents_ZH.md` or the corresponding Chinese guide; avoid updating only the English version.
- If legacy project information is still valuable, summarize it as migration rules or structural notes rather than keeping it as scattered background remarks.
- When adding new external reference repositories or local reference paths, add "purpose + path + priority" to this file to reduce future lookup cost.

## Git & Commits

- Git commit format and examples: see `Documents/Rules/GitCommitRule.md`.
- All future commits must strictly follow the format in `Documents/Rules/GitCommitRule.md` (including `Scope` and `Type` usage where applicable).
- Do not append tool-generated commit trailers (for example `Made-with: Cursor`) unless explicitly requested.
- The canonical GitHub remote for this repository is `origin -> git@github.com:UnrealEngine-Angelscript-ZH/AngelscriptProject.git`.
- The default publish branch is `main`. If a local clone still uses `master`, create or switch to `main` before the first push.
- For first-time GitHub remote setup, prefer `git remote add origin git@github.com:UnrealEngine-Angelscript-ZH/AngelscriptProject.git`, then `git push -u origin main` to establish upstream tracking.
- If `origin` already exists but points to another repository, update it with `git remote set-url origin git@github.com:UnrealEngine-Angelscript-ZH/AngelscriptProject.git` instead of adding a duplicate remote.
- Do not force-push `main` unless the user explicitly requests it.

## Plans & TODO

- Tasks requiring multi-phase execution should have a Plan document under `Documents/Plans/`. Writing rules are defined in `Documents/Plans/Plan.md`.
- Completed or closed plans move from `Documents/Plans/` to `Documents/Plans/Archives/`; each archived plan must include archive status, archive date, closure rationale, and a short result summary, with indexes updated in sync.
- TODOs should be broken down around the plugin goal. Avoid lumping legacy project issues into one large task.
- When renaming, migrating modules, or adjusting public APIs, identify all affected files and documentation.
- Tests under `Plugins/Angelscript/Source/AngelscriptTest/` should be organized by concrete theme (for example `Actor`, `Blueprint`, `Interface`, `HotReload`, `Shared`) rather than accumulated under a broad catch-all `Scenarios` bucket.

## Document Navigation

| Document | Purpose |
| --- | --- |
| `AGENTS_ZH.md` | Chinese version of this guide |
| `Reference/README.md` | External reference repository index and details |
| `Documents/Guides/Build.md` | Build and command execution guide |
| `Documents/Guides/Test.md` | Test guide |
| `Documents/Guides/UE_Search_Guide.md` | UE knowledge lookup guide |
| `Documents/Rules/GitCommitRule.md` | English commit conventions |
| `Documents/Plans/Plan.md` | Plan document writing rules |
| `Documents/Plans/Archives/README.md` | Archived plan index and summaries |
| `Documents/Tools/Tool.md` | Internal tool documentation |
