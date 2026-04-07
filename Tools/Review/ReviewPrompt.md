## Task

Strictly follow Documents/Rules/ReviewRule_ZH.md and perform a full engineering audit of the current mainline.
Audit center is Plugins/Angelscript/; host project Source/AngelscriptProject/ is observed only as needed.

## Work Mode

Audit incrementally, write as you go.

1. Create the output file {OUTPUT_PATH} and write all section headings as a skeleton.
2. Execute the audit checklist item by item. After each scan, immediately append findings to the corresponding section in the output file.
3. Write findings as they are confirmed. Do not accumulate results in memory. The same section can be appended to multiple times.
4. After all audit items are complete, go back and fill in "Executive Summary" and "Conclusion" - these require a global view and are written last.

## Evidence Rules

- All numbers must come from your own scans (glob/grep/find). Do NOT copy numbers from AGENTS.md or any Plan document.
- Qualitative judgments must be verified by reading source code, not just by looking at filenames or directory structure.
- Every finding must include at least one file:line as evidence anchor.
- Report objectively: record both positive and negative observations without exaggeration or concealment.
- Inferences must be explicitly labeled as such, with stated reasoning.

## Deduplication Rules

If a problem is already explicitly tracked in a Plan document, write one line "Already tracked in Plan_XXX.md" and move on. Do not elaborate.
Spend time on problems the team does not already know about.

## Audit Checklist

Execute the following checks one by one. Write results to the file after each item. Do not skip any.

### A. Structure and Dependencies

A1. Read AngelscriptRuntime.Build.cs, AngelscriptEditor.Build.cs, AngelscriptTest.Build.cs. Extract PublicDependencyModuleNames and PrivateDependencyModuleNames. List the declared dependency sets.

A2. For .cpp files in AngelscriptRuntime/, AngelscriptEditor/, AngelscriptTest/, grep their #include directives. Check for:
    - Modules declared as dependencies but never included (unnecessary dependencies)
    - Modules included but not declared as dependencies (missing dependencies)
    - Runtime code including Editor headers (direction violation)
    - Test code including Private/ paths (encapsulation violation)

A3. Check whether AngelscriptRuntime.Build.cs exposes ThirdParty/angelscript/source/ in PublicIncludePaths, and count how many non-ThirdParty files directly #include "source/as_*.h".

### B. Code Quality Sampling

B1. Randomly select 5 Bind_*.cpp files. Read each one and report:
    - File line count
    - Whether null pointer / invalid reference checks exist
    - Whether there are hardcoded debug logs, scene names, or TODO/FIXME/HACK markers
    - Whether binding function naming is consistent with neighboring files

B2. Sample 10 public header files (spread across Core/, ClassGenerator/, Binds/, Debugging/, etc.). Check:
    - Whether #pragma once is present
    - Whether there are obviously unnecessary #includes (replaceable with forward declarations)
    - Whether ANGELSCRIPTRUNTIME_API export macro usage is consistent

B3. Search core public headers (AngelscriptBinds.h, AngelscriptEngine.h, AngelscriptPreprocessor.h, etc.) for TODO, FIXME, HACK, WILL-EDIT, TEMP markers. Count totals and distribution.

### C. Third-Party Code

C1. Count total //[UE++] and //[UE--] markers in ThirdParty/angelscript/source/, and how many files they span.

C2. For the 3 files with the densest markers, read the code around the markers and classify modification nature (bug fix / feature extension / UE adaptation / other).

C3. Check for code segments that appear modified but lack //[UE++] markers (e.g. commented-out original code, new #if branches).

### D. Test Coverage

D1. Count .cpp files and test macro definitions (IMPLEMENT_*_AUTOMATION_TEST, BEGIN_DEFINE_SPEC, etc.) in AngelscriptTest/ grouped by subdirectory (Actor/, Bindings/, Blueprint/, Component/, Core/, Debugger/, Delegate/, Dump/, GC/, HotReload/, Interface/, Internals/, Preprocessor/, Subsystem/, etc.).

D2. Cross-reference against AngelscriptRuntime/ functional subdirectories (Core/, ClassGenerator/, Binds/, Debugging/, StaticJIT/, CodeCoverage/, Dump/, FunctionLibraries/, Preprocessor/, BaseClasses/). Flag which have dedicated test files targeting them and which do not.

D3. Count test files in AngelscriptRuntime/Tests/ (if it exists) and describe their relationship to tests in the AngelscriptTest/ module.

### E. Script Examples

E1. List all .as files under Script/ and count them.

E2. Read each .as file and evaluate: file length, whether comments explain purpose, whether it can be independently studied, which APIs or concepts it covers.

### F. Documentation-to-Code Sync

F1. Pick 5 specific factual claims from AGENTS.md (e.g. module counts, file counts, capability descriptions). Independently verify each against source code. Record matches and mismatches.

F2. Pick 3 claims from Plan_StatusPriorityRoadmap.md about items marked "completed" or "landed". Independently verify whether the corresponding code actually exists on mainline.

F3. Check whether README.md is empty or placeholder-only. Check whether Angelscript.uplugin DocsURL, SupportURL, MarketplaceURL fields have actual values.

## Output Document Structure

Output file {OUTPUT_PATH} contains these 8 sections. Do not skip or merge any.

### Section 1 - Audit Scope
List modules, directories, and key files actually scanned in this review.

### Section 2 - Executive Summary
Audit dimension summary table (Dimension | Findings Count | Highest Risk Level), plus 3-5 headline conclusions. Write this section LAST.

### Section 3 - Mainline Progress
For runtime core capabilities, editor integration, test infrastructure, and external delivery entry points, classify as [Landed] / [Partial] / [Plan-only]. Each with evidence anchors.

### Section 4 - Key Problems
All problems found during audit, appended incrementally, no upper limit. Each must follow this format:

> **Problem N: [title]**
>
> | Field | Content |
> |-------|---------|
> | Description | [what is the problem] |
> | Affected Scope | [module/file] |
> | Evidence | [file:line] |
> | Risk Level | High / Medium / Low |
> | Recommended Action | [specific actionable step] |

### Section 5 - Risks and Blockers

| Blocker | Impact | Current Status | Resolution Path |
|---------|--------|----------------|-----------------|

### Section 6 - Documentation and Test Gaps
Two sub-tables:

Doc gaps:

| Gap | Current State | Target State | Owner Plan |
|-----|---------------|--------------|------------|

Test gaps:

| Gap | Current State | Target State | Owner Plan |
|-----|---------------|--------------|------------|

### Section 7 - Suggested Next Actions
Three priority levels: P0 (immediate), P1 (short-term), P2 (mid-term).
Each action must specify what file to change / what check to perform / what artifact to produce.

### Section 8 - Conclusion
3-5 objective summary bullets. Write this section LAST.

## Prohibitions

- Do not write generic project introductions or historical recaps.
- Do not fill space with vague praise.
- Do not treat "it's in a Plan" as "it's landed on mainline".
- Do not copy numbers from AGENTS.md or Plan documents. Always produce your own counts.
- Do not output the entire document at once at the very end.
- Do not generate TodoLists.
