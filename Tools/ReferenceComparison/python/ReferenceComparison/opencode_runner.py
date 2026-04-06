"""Wrapper around the opencode CLI for invoking AI sessions."""

from __future__ import annotations

import datetime
import logging
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from .utils import ensure_dir, write_file

logger = logging.getLogger(__name__)


@dataclass
class OpenCodeResult:
    exit_code: int
    stdout: str
    stderr: str

    @property
    def success(self) -> bool:
        return self.exit_code == 0


_call_counter: int = 0


def run_opencode(
    prompt: str,
    *,
    project_dir: str,
    command: str = "ulw-loop",
    model: str = "codez-gpt/gpt-5.4",
    agent: str = "Hephaestus",
    variant: str = "xhigh",
    timeout_seconds: int = 600,
    dry_run: bool = False,
    prompt_log_dir: Optional[Path] = None,
    output_log_dir: Optional[Path] = None,
) -> OpenCodeResult:
    """Invoke ``opencode run`` with the given prompt."""

    global _call_counter
    _call_counter += 1
    call_id = _call_counter

    args = [
        "opencode",
        "run",
        "--dir",
        project_dir,
        "--agent",
        agent,
        "--model",
        model,
        "--variant",
        variant,
        "--command",
        command,
        prompt,
    ]

    if prompt_log_dir is not None:
        _save_prompt_log(prompt_log_dir, call_id, prompt, args)

    if dry_run:
        display = " ".join(_quote_arg(a) for a in args[:6]) + ' "<prompt>"'
        logger.info("[dry-run] [call#%d] %s", call_id, display)
        return OpenCodeResult(exit_code=0, stdout="[dry-run]", stderr="")

    logger.info(
        "[call#%d] Invoking opencode (timeout=%ds, prompt=%d chars) ...",
        call_id,
        timeout_seconds,
        len(prompt),
    )

    try:
        proc = subprocess.run(
            args,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout_seconds,
            cwd=project_dir,
        )
    except FileNotFoundError:
        logger.error("[call#%d] opencode command not found in PATH", call_id)
        return OpenCodeResult(exit_code=127, stdout="", stderr="opencode not found")
    except subprocess.TimeoutExpired:
        logger.error("[call#%d] opencode timed out after %ds", call_id, timeout_seconds)
        return OpenCodeResult(
            exit_code=124, stdout="", stderr=f"Timed out after {timeout_seconds}s"
        )

    logger.info(
        "[call#%d] opencode finished (exit=%d, stdout=%d chars, stderr=%d chars)",
        call_id,
        proc.returncode,
        len(proc.stdout),
        len(proc.stderr),
    )

    if proc.returncode != 0:
        logger.warning(
            "[call#%d] opencode error stderr:\n%s",
            call_id,
            proc.stderr[:2000],
        )

    if output_log_dir is not None:
        _save_output_log(output_log_dir, call_id, proc)

    if proc.stdout:
        logger.debug("[call#%d] stdout preview: %s", call_id, proc.stdout[:500])
    if proc.stderr:
        logger.debug("[call#%d] stderr preview: %s", call_id, proc.stderr[:500])

    return OpenCodeResult(
        exit_code=proc.returncode,
        stdout=proc.stdout,
        stderr=proc.stderr,
    )


def _save_prompt_log(log_dir: Path, call_id: int, prompt: str, args: list) -> None:
    ensure_dir(log_dir)
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    cmd_display = " ".join(_quote_arg(a) for a in args[:-1]) + " <prompt>"
    content = f"# Call #{call_id}\n\n**Timestamp:** {ts}\n\n## Command\n\n```\n{cmd_display}\n```\n\n## Prompt\n\n{prompt}\n"
    path = log_dir / f"call_{call_id:03d}_prompt.md"
    try:
        path.write_text(content, encoding="utf-8")
        logger.debug("[call#%d] Prompt saved to %s", call_id, path)
    except OSError as e:
        logger.warning("[call#%d] Failed to save prompt log: %s", call_id, e)


def _save_output_log(log_dir: Path, call_id: int, proc: subprocess.CompletedProcess) -> None:
    """Save the full stdout and stderr of an opencode invocation."""
    ensure_dir(log_dir)
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    parts = [f"# Call #{call_id} — Output\n\n"]
    parts.append(f"**Timestamp:** {ts}\n\n")
    parts.append(f"**Exit code:** {proc.returncode}\n\n")
    if proc.stdout:
        parts.append(f"## stdout ({len(proc.stdout)} chars)\n\n```\n{proc.stdout}\n```\n\n")
    else:
        parts.append("## stdout\n\n_(empty)_\n\n")
    if proc.stderr:
        parts.append(f"## stderr ({len(proc.stderr)} chars)\n\n```\n{proc.stderr}\n```\n\n")
    else:
        parts.append("## stderr\n\n_(empty)_\n\n")
    path = log_dir / f"call_{call_id:03d}_output.md"
    try:
        path.write_text("".join(parts), encoding="utf-8")
        logger.info("[call#%d] Full output saved to %s (%d bytes)", call_id, path,
                     len(proc.stdout) + len(proc.stderr))
    except OSError as e:
        logger.warning("[call#%d] Failed to save output log: %s", call_id, e)


def _quote_arg(value: str) -> str:
    if " " in value or '"' in value:
        return '"' + value.replace('"', '\\"') + '"'
    return value
