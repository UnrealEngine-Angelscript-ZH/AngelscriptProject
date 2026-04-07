"""Main entry point: CLI argument parsing and three-phase flow orchestration."""

from __future__ import annotations

import argparse
import logging
import subprocess
import sys
from pathlib import Path
from typing import List, Optional

from .config import DIMENSION_BY_ID, DIMENSIONS, REPO_BY_KEY, REPOS, Dimension, RepoConfig, RunConfig
from .critic import explore_loop
from .prompts import (
    build_cross_comparison_prompt,
    build_gap_analysis_prompt,
    build_repo_analysis_prompt,
)
from .utils import (
    cross_comparison_filename,
    ensure_dir,
    gap_analysis_filename,
    repo_analysis_filename,
    setup_logging,
)

logger = logging.getLogger(__name__)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)

    cfg = RunConfig.create(
        project_root=args.project_root,
        date_suffix=args.date_suffix,
        repo_keys=args.repos,
        dimension_ids=args.dimensions,
        max_iterations=args.max_iterations,
        timeout_seconds=args.timeout,
        dry_run=args.dry_run,
    )

    if args.preview:
        setup_logging(verbose=args.verbose)
        return _print_preview(cfg)

    if not cfg.rule_path.is_file():
        setup_logging(verbose=args.verbose)
        logger.error("Rule document not found: %s", cfg.rule_path)
        return 1

    ensure_dir(cfg.output_dir)

    log_file = cfg.output_dir / "run.log"
    setup_logging(verbose=args.verbose, log_file=log_file)

    _ensure_references_pulled(cfg)

    logger.info("=" * 60)
    logger.info("Reference Comparison Tool")
    logger.info("Output: %s", cfg.output_dir)
    logger.info("Log file: %s", log_file)
    logger.info("Prompt logs: %s", cfg.output_dir / "_prompts")
    logger.info("Output logs: %s", cfg.output_dir / "_outputs")
    logger.info("Model: %s (variant=%s)", cfg.opencode_model, cfg.opencode_variant)
    logger.info("Repos: %s", ", ".join(r.display_name for r in cfg.repos))
    logger.info("Dimensions: %s", ", ".join(d.id for d in cfg.dimensions))
    logger.info("Max rounds: %d", cfg.max_iterations)
    logger.info("=" * 60)

    phase1_ok = _run_phase1(cfg, run_log_path=log_file)
    phase2_ok = _run_phase2(cfg, run_log_path=log_file)
    phase3_ok = _run_phase3(cfg, run_log_path=log_file)

    summary = (
        f"Phase 1 (per-repo): {'OK' if phase1_ok else 'PARTIAL'}, "
        f"Phase 2 (cross-cmp): {'OK' if phase2_ok else 'PARTIAL'}, "
        f"Phase 3 (gap): {'OK' if phase3_ok else 'PARTIAL'}"
    )
    logger.info("=" * 60)
    logger.info("Complete. %s", summary)
    logger.info("Output directory: %s", cfg.output_dir)

    return 0


# ---------------------------------------------------------------------------
# Phase 1: Per-Repo Analysis
# ---------------------------------------------------------------------------

def _run_phase1(cfg: RunConfig, *, run_log_path: Path) -> bool:
    logger.info("===== Phase 1: Per-Repo Analysis =====")
    all_ok = True
    prompt_log_dir = cfg.output_dir / "_prompts"
    output_log_dir = cfg.output_dir / "_outputs"

    for repo in cfg.repos:
        repo_path = repo.resolve_path(cfg.project_root)
        if not repo_path.is_dir() and not cfg.dry_run:
            logger.warning("Reference repo not found: %s — skipping", repo_path)
            all_ok = False
            continue

        filename = repo_analysis_filename(repo.key, repo.display_name)
        output_path = cfg.output_dir / filename

        prompt = build_repo_analysis_prompt(
            repo=repo,
            dimensions=cfg.dimensions,
            rule_path=cfg.rule_path,
            output_path=output_path,
        )

        ok = explore_loop(
            initial_prompt=prompt,
            output_path=output_path,
            document_type="per-repo analysis",
            project_dir=str(cfg.project_root),
            command=cfg.opencode_command,
            model=cfg.opencode_model,
            variant=cfg.opencode_variant,
            max_iterations=cfg.max_iterations,
            timeout_seconds=cfg.timeout_seconds,
            dry_run=cfg.dry_run,
            prompt_log_dir=prompt_log_dir,
            output_log_dir=output_log_dir,
            run_log_path=run_log_path,
        )

        if not ok:
            all_ok = False

    return all_ok


# ---------------------------------------------------------------------------
# Phase 2: Cross-Repo Comparison
# ---------------------------------------------------------------------------

def _run_phase2(cfg: RunConfig, *, run_log_path: Path) -> bool:
    logger.info("===== Phase 2: Cross-Repo Comparison =====")
    all_ok = True
    prompt_log_dir = cfg.output_dir / "_prompts"
    output_log_dir = cfg.output_dir / "_outputs"

    for dim in cfg.dimensions:
        filename = cross_comparison_filename(dim.id, dim.name_en)
        output_path = cfg.output_dir / filename

        prompt = build_cross_comparison_prompt(
            dimension=dim,
            repos=cfg.repos,
            rule_path=cfg.rule_path,
            output_path=output_path,
            per_repo_docs_dir=cfg.output_dir,
        )

        ok = explore_loop(
            initial_prompt=prompt,
            output_path=output_path,
            document_type="cross-comparison",
            project_dir=str(cfg.project_root),
            command=cfg.opencode_command,
            model=cfg.opencode_model,
            variant=cfg.opencode_variant,
            max_iterations=cfg.max_iterations,
            timeout_seconds=cfg.timeout_seconds,
            dry_run=cfg.dry_run,
            prompt_log_dir=prompt_log_dir,
            output_log_dir=output_log_dir,
            run_log_path=run_log_path,
        )

        if not ok:
            all_ok = False

    return all_ok


# ---------------------------------------------------------------------------
# Phase 3: Gap Analysis
# ---------------------------------------------------------------------------

def _run_phase3(cfg: RunConfig, *, run_log_path: Path) -> bool:
    logger.info("===== Phase 3: Gap Analysis =====")
    prompt_log_dir = cfg.output_dir / "_prompts"
    output_log_dir = cfg.output_dir / "_outputs"

    filename = gap_analysis_filename()
    output_path = cfg.output_dir / filename

    prompt = build_gap_analysis_prompt(
        repos=cfg.repos,
        dimensions=cfg.dimensions,
        rule_path=cfg.rule_path,
        output_path=output_path,
        comparison_docs_dir=cfg.output_dir,
    )

    return explore_loop(
        initial_prompt=prompt,
        output_path=output_path,
        document_type="gap analysis",
        project_dir=str(cfg.project_root),
        command=cfg.opencode_command,
        model=cfg.opencode_model,
        variant=cfg.opencode_variant,
        max_iterations=cfg.max_iterations,
        timeout_seconds=cfg.timeout_seconds,
        dry_run=cfg.dry_run,
        prompt_log_dir=prompt_log_dir,
        output_log_dir=output_log_dir,
        run_log_path=run_log_path,
    )


# ---------------------------------------------------------------------------
# Reference pulling
# ---------------------------------------------------------------------------

def _ensure_references_pulled(cfg: RunConfig) -> None:
    pull_bat = cfg.project_root / "Tools" / "PullReference" / "PullReference.bat"
    if not pull_bat.is_file():
        logger.warning("PullReference.bat not found, skipping auto-pull")
        return

    for repo in cfg.repos:
        if repo.agent_config_key:
            continue

        repo_path = repo.resolve_path(cfg.project_root)
        if repo_path.is_dir():
            continue

        if cfg.dry_run:
            logger.info("[dry-run] Would pull: %s", repo.key)
            continue

        logger.info("Pulling reference: %s ...", repo.key)
        try:
            subprocess.run(
                ["cmd.exe", "/c", str(pull_bat), repo.key],
                cwd=str(cfg.project_root),
                timeout=120,
                check=False,
            )
        except (subprocess.TimeoutExpired, FileNotFoundError) as exc:
            logger.warning("Failed to pull %s: %s", repo.key, exc)


# ---------------------------------------------------------------------------
# Preview
# ---------------------------------------------------------------------------

def _print_preview(cfg: RunConfig) -> int:
    print(f"Status=Preview")
    print(f"ProjectRoot={cfg.project_root}")
    print(f"OutputDir={cfg.output_dir}")
    print(f"RulePath={cfg.rule_path}")
    print(f"Model={cfg.opencode_model}")
    print(f"Variant={cfg.opencode_variant}")
    print(f"Repos={','.join(r.key for r in cfg.repos)}")
    print(f"Dimensions={','.join(d.id for d in cfg.dimensions)}")
    print(f"MaxRounds={cfg.max_iterations}")
    print(f"TimeoutSeconds={cfg.timeout_seconds}")
    print(f"DryRun={cfg.dry_run}")

    print("\nPhase 1 outputs:")
    for repo in cfg.repos:
        fn = repo_analysis_filename(repo.key, repo.display_name)
        print(f"  {cfg.output_dir / fn}")

    print("\nPhase 2 outputs:")
    for dim in cfg.dimensions:
        fn = cross_comparison_filename(dim.id, dim.name_en)
        print(f"  {cfg.output_dir / fn}")

    print(f"\nPhase 3 output:")
    print(f"  {cfg.output_dir / gap_analysis_filename()}")

    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Reference Comparison Tool — iterative AI exploration of UE script plugins",
    )
    parser.add_argument(
        "--project-root",
        default=None,
        help="Project root directory (default: auto-detect from script location)",
    )
    parser.add_argument(
        "--date-suffix",
        default=None,
        help="Date suffix for output directory (default: today's date YYYY-MM-DD)",
    )
    parser.add_argument(
        "--repos",
        nargs="*",
        default=None,
        help=f"Reference repos to analyze (choices: {', '.join(REPO_BY_KEY.keys())}; default: all)",
    )
    parser.add_argument(
        "--dimensions",
        nargs="*",
        default=None,
        help=f"Dimensions to compare (choices: {', '.join(DIMENSION_BY_ID.keys())}; default: all)",
    )
    parser.add_argument(
        "--max-iterations",
        type=int,
        default=3,
        help="Max exploration rounds per document (default: 3)",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=600,
        help="Timeout in seconds for each opencode invocation (default: 600)",
    )
    parser.add_argument(
        "--preview",
        action="store_true",
        help="Preview mode: print configuration and exit without running",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Dry-run mode: log commands without invoking opencode",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Enable debug-level logging",
    )

    return parser.parse_args(argv)


if __name__ == "__main__":
    sys.exit(main())
