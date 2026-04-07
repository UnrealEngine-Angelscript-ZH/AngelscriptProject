"""Iterative exploration loop: generate, read back, deepen, repeat."""

from __future__ import annotations

import hashlib
import logging
from pathlib import Path
from typing import Optional

from .opencode_runner import run_opencode
from .prompts import build_deepen_prompt
from .utils import read_file_safe

logger = logging.getLogger(__name__)


def _content_hash(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]


def explore_loop(
    *,
    initial_prompt: str,
    output_path: Path,
    document_type: str,
    project_dir: str,
    command: str = "ralph-loop",
    model: str = "codez-gpt/gpt-5.4",
    variant: str = "xhigh",
    max_iterations: int = 3,
    timeout_seconds: int = 600,
    dry_run: bool = False,
    prompt_log_dir: Optional[Path] = None,
    output_log_dir: Optional[Path] = None,
    run_log_path: Optional[Path] = None,
) -> bool:
    """Run the iterative exploration loop for a single document.

    If the file does not exist: Round 1 creates it from the initial prompt,
    subsequent rounds deepen.  If the file already exists from a previous run,
    ALL rounds use the deepen prompt so the AI edits rather than re-creates.

    Returns True if at least one iteration produced output.
    """

    logger.info(
        "=== Explore loop: %s (type=%s, max %d rounds) ===",
        output_path.name,
        document_type,
        max_iterations,
    )

    existing_doc = read_file_safe(output_path)
    file_pre_exists = existing_doc is not None
    if file_pre_exists:
        logger.info(
            "File already exists (%d chars) — all rounds will use deepen prompt",
            len(existing_doc),
        )

    produced = False
    unchanged_streak = 0

    for iteration in range(1, max_iterations + 1):
        logger.info("--- Round %d/%d for %s ---", iteration, max_iterations, output_path.name)

        previous_doc = read_file_safe(output_path)
        pre_hash = _content_hash(previous_doc) if previous_doc else None

        use_deepen = (iteration > 1) or file_pre_exists
        if use_deepen and previous_doc is not None:
            logger.info(
                "Previous document: %d chars (hash=%s), building deepen prompt",
                len(previous_doc),
                pre_hash,
            )
            prompt = build_deepen_prompt(
                initial_prompt, previous_doc, iteration, max_iterations
            )
        else:
            if iteration > 1:
                logger.warning("No previous output found, re-running initial prompt")
            prompt = initial_prompt

        effective_command = command if not use_deepen else None
        logger.info(
            "Prompt ready (%d chars), invoking opencode (command=%s)...",
            len(prompt),
            effective_command or "(default)",
        )

        result = run_opencode(
            prompt,
            project_dir=project_dir,
            command=effective_command,
            model=model,
            variant=variant,
            timeout_seconds=timeout_seconds,
            dry_run=dry_run,
            prompt_log_dir=prompt_log_dir,
            output_log_dir=output_log_dir,
            run_log_path=run_log_path,
        )

        if dry_run:
            logger.info("[dry-run] Round %d would write: %s", iteration, output_path)
            return True

        if not result.success:
            logger.error(
                "Round %d failed (exit=%d): %s",
                iteration,
                result.exit_code,
                result.output[-300:],
            )
            continue

        doc_content = read_file_safe(output_path)
        if doc_content is None:
            logger.warning("Round %d did not produce output at %s", iteration, output_path)
            continue

        produced = True
        post_hash = _content_hash(doc_content)
        changed = pre_hash != post_hash

        if changed:
            unchanged_streak = 0
            logger.info(
                "Round %d complete: %s → %d chars (hash=%s, CHANGED +%d chars)",
                iteration,
                output_path.name,
                len(doc_content),
                post_hash,
                len(doc_content) - (len(previous_doc) if previous_doc else 0),
            )
        else:
            unchanged_streak += 1
            logger.warning(
                "Round %d complete: %s → %d chars (hash=%s, UNCHANGED — streak %d)",
                iteration,
                output_path.name,
                len(doc_content),
                post_hash,
                unchanged_streak,
            )
            if unchanged_streak >= 2:
                logger.warning(
                    "Stopping early: %d consecutive rounds with no file changes",
                    unchanged_streak,
                )
                break

    if produced:
        logger.info("Explore loop finished for %s", output_path.name)
    else:
        logger.warning("No output produced for %s after %d rounds", output_path.name, max_iterations)

    return produced
