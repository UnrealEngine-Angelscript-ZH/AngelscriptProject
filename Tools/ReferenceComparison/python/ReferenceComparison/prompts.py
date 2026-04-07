"""Prompt templates for iterative exploration rounds."""

from __future__ import annotations

from pathlib import Path
from typing import List

from .config import Dimension, RepoConfig

# ---------------------------------------------------------------------------
# Shared sections injected into every first-round prompt
# ---------------------------------------------------------------------------

_WORK_MODE_SECTION = """\
## Work Mode — Write Incrementally, No Concurrency

You MUST create the output file early and write to it as you go.
**ABSOLUTELY NO CONCURRENCY** — this is the single most important constraint.

1. **First action**: Create the output file and write all section headings as a skeleton.
2. After scanning each dimension, immediately append findings to the corresponding section.
3. Do NOT accumulate results in memory. The same section can be appended to multiple times.
4. If you are running low on budget, STOP gathering evidence and finalize what you have already written.
5. A complete document with partial evidence is far better than exhaustive evidence with no output file.
6. **NEVER launch parallel sub-agents, background agents, Explore Agents, or concurrent tasks.** Work strictly sequentially: read one source file → write findings to document → move to next source file. Any form of concurrency or parallel dispatch will cause the session to terminate prematurely without writing the output file.
7. Do NOT spend time reading skill files, creating TodoLists, or planning. Start scanning source code immediately.
8. Do NOT create todo items. Do NOT use the TodoWrite tool.
9. Do NOT output `<promise>DONE</promise>` until ALL requested output files exist on disk AND each contains substantive content. If any requested file has not been written, you are FORBIDDEN from outputting `<promise>DONE</promise>`. Verify every file before claiming completion."""


def build_repo_analysis_prompt(
    repo: RepoConfig,
    dimensions: List[Dimension],
    rule_path: Path,
    output_path: Path,
    angelscript_plugin_path: str = "Plugins/Angelscript",
) -> str:
    dim_list = "\n".join(
        f"- **{d.id} {d.name_zh}**：{d.focus}" for d in dimensions
    )

    if repo.key == "hazelight":
        return _build_hazelight_prompt(
            repo, dimensions, dim_list, rule_path, output_path, angelscript_plugin_path
        )

    return f"""\
Strictly follow the rules defined in {rule_path.name} (located at Documents/Rules/{rule_path.name}).

## Task

Perform a deep source-code analysis of the **{repo.display_name}** plugin (language: {repo.language}).
The source code is located at: Reference/{repo.default_subdir}/
Source hints (key directories to explore): {', '.join(repo.source_hints)}

Write a comprehensive per-repo analysis document covering the following dimensions:

{dim_list}

## Comparison Baseline

The current Angelscript plugin is at {angelscript_plugin_path}/.
For each dimension, include a concrete comparison with the Angelscript plugin's current implementation.

## Output

Write the complete analysis document to: {output_path}
Create parent directories if they do not exist.

{_WORK_MODE_SECTION}

## Critical Requirements

1. Every dimension MUST include at least one ASCII architecture/call-chain diagram.
2. Every dimension MUST reference specific source files with paths and annotated code snippets.
3. The document MUST end with a "与 Angelscript 差异速查" comparison table.
4. Do NOT generate any TodoList section.
5. Follow the document structure defined in the rule document (纵向分析文档 format).
6. All prose and code comments in Chinese; code and technical terms in English.
"""


def _build_hazelight_prompt(
    repo: RepoConfig,
    dimensions: List[Dimension],
    dim_list: str,
    rule_path: Path,
    output_path: Path,
    angelscript_plugin_path: str,
) -> str:
    return f"""\
Strictly follow the rules defined in {rule_path.name} (located at Documents/Rules/{rule_path.name}).

## Task

Perform a deep source-code analysis of the **Hazelight Angelscript** engine integration — the upstream reference that our current Angelscript plugin is derived from.

This is NOT an external competitor plugin. It is the **original implementation** we are porting and improving upon. The analysis should focus on understanding the original design decisions, identifying capabilities we have not yet ported, and mapping the architectural differences introduced during our refactoring.

The source code is located at the path configured in AgentConfig.ini (`References.HazelightAngelscriptEngineRoot`).
Source hints (key directories to explore): {', '.join(repo.source_hints)}

### Background

- Hazelight's version is an engine-integrated plugin (modules: `AngelscriptCode`, `AngelscriptEditor`, `AngelscriptLoader`)
- Our version has been refactored into: `AngelscriptRuntime` (renamed from `AngelscriptCode`), `AngelscriptEditor`, `AngelscriptTest` (new)
- `AngelscriptLoader` has been removed in our version; its responsibilities absorbed into the subsystem
- AngelScript base version: both use 2.33.0 WIP
- Hazelight has 125 Bind files; we have 123

### Analysis Dimensions

{dim_list}

### Comparison Angle

For each dimension, compare **Hazelight original → our current implementation**:
- What did Hazelight implement that we already have?
- What did Hazelight implement that we have NOT yet ported?
- What did we change/improve during refactoring, and why?
- What Hazelight capabilities are we deliberately NOT porting, and why?

## Output

Write the complete analysis document to: {output_path}
Create parent directories if they do not exist.

{_WORK_MODE_SECTION}

## Critical Requirements

1. Every dimension MUST include at least one ASCII architecture/call-chain diagram.
2. Every dimension MUST reference specific source files with paths and annotated code snippets from BOTH Hazelight and our plugin ({angelscript_plugin_path}/).
3. The document MUST end with a "移植状态速查" table showing: Feature | Hazelight | Ours | Status (Ported/Partial/NotPorted/Deliberately Skipped).
4. Identify the 2 missing Bind files (125 vs 123) and list what they cover.
5. Do NOT generate any TodoList section.
6. Follow the document structure defined in the rule document (纵向分析文档 format).
7. All prose and code comments in Chinese; code and technical terms in English.
"""


def build_cross_comparison_prompt(
    dimension: Dimension,
    repos: List[RepoConfig],
    rule_path: Path,
    output_path: Path,
    per_repo_docs_dir: Path,
    angelscript_plugin_path: str = "Plugins/Angelscript",
) -> str:
    repo_names = " / ".join(r.display_name for r in repos)
    per_repo_refs = "\n".join(
        f"- {r.display_name}: see previously generated analysis in {per_repo_docs_dir / _repo_filename(r)}"
        for r in repos
    )

    return f"""\
Strictly follow the rules defined in {rule_path.name} (located at Documents/Rules/{rule_path.name}).

## Task

Write a **cross-comparison** document for dimension **{dimension.id}: {dimension.name_zh}**.
Focus: {dimension.focus}

Compare the following plugins: {repo_names} + Angelscript (at {angelscript_plugin_path}/).

## Reference Materials

Previously generated per-repo analysis documents:
{per_repo_refs}

Also read the actual source code in Reference/ and {angelscript_plugin_path}/ for verification.

## Output

Write the cross-comparison document to: {output_path}

{_WORK_MODE_SECTION}

## Critical Requirements

1. MUST include an ASCII comparison matrix table (plugin × sub-feature).
2. MUST include at least one ASCII call-chain or architecture diagram showing the differences.
3. MUST reference specific source files from each plugin with annotated code snippets.
4. The "小结与建议" section MUST give concrete, prioritized suggestions for Angelscript.
5. Do NOT generate any TodoList section.
6. Follow the document structure defined in the rule document (横向对比文档 format).
"""


def build_gap_analysis_prompt(
    repos: List[RepoConfig],
    dimensions: List[Dimension],
    rule_path: Path,
    output_path: Path,
    comparison_docs_dir: Path,
    angelscript_plugin_path: str = "Plugins/Angelscript",
) -> str:
    repo_names = " / ".join(r.display_name for r in repos)
    dim_list = "\n".join(f"- {d.id} {d.name_zh}" for d in dimensions)

    return f"""\
Strictly follow the rules defined in {rule_path.name} (located at Documents/Rules/{rule_path.name}).

## Task

Write the final **Gap Analysis & Experience Absorption** summary document.
Synthesize all previously generated per-repo analyses and cross-comparison documents in {comparison_docs_dir}/.

## Comparison Scope

Plugins: {repo_names}
Dimensions:
{dim_list}

Angelscript plugin: {angelscript_plugin_path}/

## Output

Write to: {output_path}

{_WORK_MODE_SECTION}

## Critical Requirements

1. MUST include a gap matrix (dimension × gap severity).
2. Each dimension's gap analysis MUST follow: Current State → Gap Description → Reference Solution → Absorption Suggestion → Priority.
3. The "值得吸收的设计模式" section MUST list concrete patterns with source-code evidence.
4. The "改进路线建议" MUST be ordered by priority with effort estimates.
5. Do NOT generate any TodoList section.
6. Follow the document structure defined in the rule document (差距分析文档 format).
"""


def build_deepen_prompt(
    initial_prompt: str,
    previous_doc: str,
    current_round: int,
    max_rounds: int,
) -> str:
    """Build a follow-up prompt that reads previous output and deepens it."""

    doc_size = len(previous_doc)
    doc_lines = previous_doc.count("\n") + 1

    return f"""\
## CRITICAL INSTRUCTION — EDIT THE FILE

You are in Round {current_round}/{max_rounds} of an iterative deepening process.
The document already exists ({doc_lines} lines, {doc_size} chars).

**YOUR ONLY JOB IS TO EDIT THE EXISTING FILE.** Not to analyze. Not to plan. Not to report.
You MUST use file editing tools (StrReplace, Write, or equivalent) to modify the document.
If you finish this round without having edited the file, you have FAILED.

### Workflow (follow strictly)

1. **Read the existing document** (1 minute max — skim, don't study every line).
2. **Pick the 2-3 weakest sections** from the checklist below.
3. **Start editing immediately.** For each weak section:
   - Find 1-2 relevant source files (Grep/Glob).
   - Use StrReplace to insert annotated code snippets, ASCII diagrams, or precise comparisons directly into the document.
4. **NEVER launch parallel sub-agents, background agents, Explore Agents, or concurrent tasks.** Work strictly sequentially: find evidence → edit → next section. Any form of concurrency will cause premature session termination.
5. **Do NOT rewrite from scratch.** Only insert/replace within existing sections.
6. **Do NOT create todo items.** Do NOT use the TodoWrite tool.

### Deepening Checklist (pick 2-3 to address per round)

**A. Source-Code Grounding** — Sections that claim "X uses Y pattern" without citing a file path + code snippet. Find the actual source and insert an annotated snippet (5-15 lines, Chinese comments on key lines). Target: 2+ source references per section.

**B. ASCII Diagrams** — Sections without diagrams, or with only high-level module boxes. Add internal structure diagrams: class inheritance, call sequence, data pipeline, state machine.

**C. Deeper Source Exploration** — For each dimension, open 2-3 source files not yet referenced. Look for: non-obvious patterns (CRTP, policy classes), error handling, performance paths (caching, pooling), thread safety, preprocessor macros. Insert findings into existing sections.

**D. Comparison Precision** — Replace vague phrases ("similar approach", "different strategy") with: exact class/function names, concrete call chain differences, measurable implications (compile time, runtime overhead, API surface size).

**E. Gap Severity** — For each gap, insert a severity tag:
  - 🔴 Blocking: prevents a key use case
  - 🟡 Important: degrades experience but has workaround
  - 🟢 Nice-to-have: polish or optimization
  Back up with evidence.

### Hard Rules

1. **MUST edit the file.** No round is complete without file modifications.
2. **Do NOT add TodoList or action items.** Only analysis and evidence.
3. **Do NOT rewrite from scratch.** Preserve existing content, expand in place.
4. **Budget: spend ≤20% of your time reading, ≥80% editing.**
5. After editing, do a quick consistency pass to remove duplicates.
6. **ABSOLUTELY NO CONCURRENCY.** No parallel agents, no background tasks, no concurrent exploration.
7. Do NOT output `<promise>DONE</promise>` until you have verified the file was actually modified in this round.

---

Below is the original task description for context (do not re-execute from scratch):

{initial_prompt}
"""


def _repo_filename(repo: RepoConfig) -> str:
    idx = {
        "hazelight": "00",
        "unrealcsharp": "01",
        "unlua": "02",
        "puerts": "03",
        "sluaunreal": "04",
    }
    prefix = idx.get(repo.key, "99")
    return f"{prefix}_{repo.display_name}_Analysis.md"
