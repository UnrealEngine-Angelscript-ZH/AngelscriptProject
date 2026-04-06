"""Prompt templates for iterative exploration rounds."""

from __future__ import annotations

from pathlib import Path
from typing import List

from .config import Dimension, RepoConfig


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
{initial_prompt}

## Continuation — Round {current_round}/{max_rounds}

The document already exists from a previous round ({doc_lines} lines, {doc_size} chars). **Read it first**, then apply the deepening checklist below.

### Deepening Checklist (MUST address every item)

**A. Source-Code Grounding Audit**
- For every claim or comparison point in the document, check: is there a specific file path + line range cited?
- If a section says "X uses Y pattern" but provides no code snippet, **find the actual source file and add an annotated snippet** (5-15 lines, with Chinese comments on key lines).
- Target: every section should have at least 2 concrete source-code references from different files.

**B. ASCII Diagram Completeness**
- Count how many ASCII diagrams exist. Each dimension/section MUST have at least one.
- If a diagram is too high-level (only boxes with module names), **add a second diagram** showing internal structure: key classes, call chains, or data flow within that module.
- Preferred diagram types: module dependency graph, class inheritance tree, call sequence, data pipeline, state machine.

**C. Missing Depth — Explore Deeper Into Source**
- For each dimension, open at least 3 source files you haven't referenced yet and look for:
  - Non-obvious design patterns (e.g., template metaprogramming, CRTP, policy classes)
  - Error handling strategies and edge cases
  - Performance-critical paths (caching, lazy init, pooling)
  - Thread safety mechanisms (locks, atomics, game-thread assertions)
  - Preprocessor macros that shape the API surface
- Add findings as new subsections or expand existing ones.

**D. Comparison Precision**
- Replace vague comparisons ("similar approach", "different strategy") with specific technical differences:
  - What exact class/function does X use vs Y?
  - What is the concrete call chain difference?
  - What are the measurable implications (compile time, runtime overhead, API surface size)?

**E. Gap Severity Assessment**
- For each identified gap or difference, assign a concrete severity:
  - 🔴 Blocking: prevents a key use case
  - 🟡 Important: degrades experience but has workaround
  - 🟢 Nice-to-have: polish or optimization
- Back up severity with evidence (e.g., "blocks hot-reload of X because Y class is missing").

### Execution Rules

1. **Do NOT rewrite from scratch.** Preserve existing content and expand in place.
2. **Do NOT add TodoList or action items.** Only analysis and evidence.
3. **Prioritize depth over breadth** — it's better to make 3 sections excellent than 10 sections shallow.
4. After expanding, re-read the full document to ensure consistency and remove any duplicate content introduced.
5. The final document should read as a single coherent analysis, not as "round 1 content + round 2 additions".
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
