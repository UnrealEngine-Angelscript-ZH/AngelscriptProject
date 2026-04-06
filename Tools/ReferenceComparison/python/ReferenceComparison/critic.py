"""Iterative exploration loop: generate, read back, deepen, repeat."""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Optional

from .opencode_runner import run_opencode
from .prompts import build_deepen_prompt
from .utils import read_file_safe

logger = logging.getLogger(__name__)


def explore_loop(
    *,
    initial_prompt: str,
    output_path: Path,
    document_type: str,
    project_dir: str,
    command: str = "ulw-loop",
    model: str = "codez-gpt/gpt-5.4",
    agent: str = "Hephaestus",
    variant: str = "xhigh",
    max_iterations: int = 3,
    timeout_seconds: int = 600,
    dry_run: bool = False,
    prompt_log_dir: Optional[Path] = None,
    output_log_dir: Optional[Path] = None,
) -> bool:
    """Run the iterative exploration loop for a single document.

    Round 1: generate initial document from scratch.
    Round 2+: read previous output, prompt AI to deepen and supplement.

    Returns True if at least one iteration produced output.
    """

    logger.info(
        "=== Explore loop: %s (type=%s, max %d rounds) ===",
        output_path.name,
        document_type,
        max_iterations,
    )

    produced = False

    for iteration in range(1, max_iterations + 1):
        logger.info("--- Round %d/%d for %s ---", iteration, max_iterations, output_path.name)

        if iteration == 1:
            prompt = initial_prompt
        else:
            previous_doc = read_file_safe(output_path)
            if previous_doc is None:
                logger.warning("No previous output found, re-running initial prompt")
                prompt = initial_prompt
            else:
                logger.info(
                    "Previous document: %d chars, building deepen prompt", len(previous_doc)
                )
                prompt = build_deepen_prompt(
                    initial_prompt, previous_doc, iteration, max_iterations
                )

        logger.info("Prompt ready (%d chars), invoking opencode...", len(prompt))

        result = run_opencode(
            prompt,
            project_dir=project_dir,
            command=command,
            model=model,
            agent=agent,
            variant=variant,
            timeout_seconds=timeout_seconds,
            dry_run=dry_run,
            prompt_log_dir=prompt_log_dir,
            output_log_dir=output_log_dir,
        )

        if dry_run:
            logger.info("[dry-run] Round %d would write: %s", iteration, output_path)
            return True

        if not result.success:
            logger.error(
                "Round %d failed (exit=%d): %s",
                iteration,
                result.exit_code,
                result.stderr[:300],
            )
            continue

        doc_content = read_file_safe(output_path)
        if doc_content is None:
            logger.warning("Round %d did not produce output at %s", iteration, output_path)
            continue

        produced = True
        logger.info(
            "Round %d complete: %s → %d chars",
            iteration,
            output_path.name,
            len(doc_content),
        )

    if produced:
        logger.info("Explore loop finished for %s", output_path.name)
    else:
        logger.warning("No output produced for %s after %d rounds", output_path.name, max_iterations)

    return produced
