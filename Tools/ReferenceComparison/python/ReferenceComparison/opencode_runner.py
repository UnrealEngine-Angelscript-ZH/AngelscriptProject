"""Wrapper around the opencode CLI for invoking AI sessions.

Streams opencode output to the terminal in real-time (mirroring the
Review tool's ``2>&1 | Tee-Object`` pattern) while capturing the full
output for log files.
"""

from __future__ import annotations

import datetime
import logging
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import IO, Any, Optional

from .utils import ensure_dir

_ANSI_ESCAPE_RE = re.compile(r"\x1b\[[0-9;]*m")

logger = logging.getLogger(__name__)


def _ensure_utf8_stdout() -> None:
    """Reconfigure sys.stdout/stderr to UTF-8 on Windows so that Chinese
    characters from opencode are not mangled by the system codepage."""
    for stream_name in ("stdout", "stderr"):
        stream = getattr(sys, stream_name)
        if hasattr(stream, "reconfigure") and getattr(stream, "encoding", "").lower() != "utf-8":
            try:
                stream.reconfigure(encoding="utf-8", errors="replace")
            except Exception:
                pass


_ensure_utf8_stdout()


@dataclass
class OpenCodeResult:
    exit_code: int
    output: str

    @property
    def success(self) -> bool:
        return self.exit_code == 0


_call_counter: int = 0


def _close_run_log_stream(
    fp: Optional[IO[str]], call_id: int, proc: Optional[Any]
) -> None:
    """Write footer and close *fp* (same role as Review's StreamWriter tail)."""
    if fp is None:
        return
    try:
        ec = -1
        if proc is not None:
            rc = proc.returncode
            if rc is None:
                rc = proc.poll()
            ec = rc if rc is not None else -1
        fp.write(f"\n--- end opencode call #{call_id} (exit={ec}) ---\n")
        fp.flush()
    except OSError:
        pass
    try:
        fp.close()
    except OSError:
        pass


def run_opencode(
    prompt: str,
    *,
    project_dir: str,
    command: Optional[str] = "ralph-loop",
    model: str = "codez-gpt/gpt-5.4",
    variant: str = "xhigh",
    timeout_seconds: int = 600,
    dry_run: bool = False,
    prompt_log_dir: Optional[Path] = None,
    output_log_dir: Optional[Path] = None,
    run_log_path: Optional[Path] = None,
) -> OpenCodeResult:
    """Invoke ``opencode run`` and stream its merged output to the terminal.

    Like the Review tool's ``2>&1 | Tee-Object``, stdout and stderr are
    merged into a single interleaved stream.  Each line is:
      - printed to the terminal in real-time (with original ANSI colours),
      - appended to *run_log_path* when set (ANSI-stripped, UTF-8), like Review's run.log,
      - accumulated for the return value and ``_outputs/`` snapshot (ANSI-stripped).
    """

    global _call_counter
    _call_counter += 1
    call_id = _call_counter

    args = [
        "opencode",
        "run",
        "--dir",
        project_dir,
        "--model",
        model,
        "--variant",
        variant,
    ]
    if command:
        args.extend(["--command", command])
    args.append(prompt)

    if prompt_log_dir is not None:
        _save_prompt_log(prompt_log_dir, call_id, prompt, args)

    if dry_run:
        display = " ".join(_quote_arg(a) for a in args[:6]) + ' "<prompt>"'
        logger.info("[dry-run] [call#%d] %s", call_id, display)
        return OpenCodeResult(exit_code=0, output="[dry-run]")

    logger.info(
        "[call#%d] Invoking opencode (timeout=%ds, prompt=%d chars) ...",
        call_id,
        timeout_seconds,
        len(prompt),
    )

    env = os.environ.copy()
    env["PYTHONIOENCODING"] = "utf-8"

    collected_lines: list[str] = []
    proc: Optional[subprocess.Popen] = None
    run_log_fp: Optional[IO[str]] = None

    try:
        if run_log_path is not None:
            try:
                ensure_dir(run_log_path.parent)
                run_log_fp = open(run_log_path, "a", encoding="utf-8", newline="")
                ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                run_log_fp.write(
                    f"\n{'=' * 80}\n"
                    f"# opencode merged stream (stdout+stderr, ANSI stripped) | "
                    f"call #{call_id} | {ts}\n"
                    f"{'=' * 80}\n"
                )
                run_log_fp.flush()
            except OSError as exc:
                logger.warning(
                    "[call#%d] Cannot append opencode stream to run.log (%s): %s",
                    call_id,
                    run_log_path,
                    exc,
                )
                run_log_fp = None

        proc = subprocess.Popen(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            cwd=project_dir,
            env=env,
        )

        assert proc.stdout is not None
        for raw_line in proc.stdout:
            sys.stdout.write(raw_line)
            sys.stdout.flush()
            collected_lines.append(raw_line)
            if run_log_fp is not None:
                run_log_fp.write(_strip_ansi(raw_line))
                run_log_fp.flush()

        proc.wait(timeout=timeout_seconds)

    except FileNotFoundError:
        logger.error("[call#%d] opencode command not found in PATH", call_id)
        _close_run_log_stream(run_log_fp, call_id, proc)
        return OpenCodeResult(exit_code=127, output="opencode not found")
    except subprocess.TimeoutExpired:
        if proc is not None:
            proc.kill()
            proc.wait(timeout=5)
        logger.error("[call#%d] opencode timed out after %ds", call_id, timeout_seconds)
        out = _strip_ansi("".join(collected_lines)) + f"\n[Timed out after {timeout_seconds}s]\n"
        _close_run_log_stream(run_log_fp, call_id, proc)
        return OpenCodeResult(exit_code=124, output=out)

    _close_run_log_stream(run_log_fp, call_id, proc)

    raw_output = "".join(collected_lines)
    clean_output = _strip_ansi(raw_output)

    logger.info(
        "[call#%d] opencode finished (exit=%d, output=%d chars)",
        call_id,
        proc.returncode,
        len(clean_output),
    )

    if proc.returncode != 0:
        logger.warning(
            "[call#%d] opencode non-zero exit, tail output:\n%s",
            call_id,
            clean_output[-2000:],
        )

    if output_log_dir is not None:
        _save_output_log(output_log_dir, call_id, proc.returncode, clean_output)

    return OpenCodeResult(exit_code=proc.returncode, output=clean_output)


def _strip_ansi(text: str) -> str:
    """Remove ANSI terminal escape sequences from text."""
    return _ANSI_ESCAPE_RE.sub("", text) if text else text


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


def _save_output_log(
    log_dir: Path, call_id: int, exit_code: int, output: str
) -> None:
    """Save the full (ANSI-stripped) merged output of an opencode invocation."""
    ensure_dir(log_dir)
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    parts = [
        f"# Call #{call_id} — Output\n\n",
        f"**Timestamp:** {ts}\n\n",
        f"**Exit code:** {exit_code}\n\n",
    ]
    if output:
        parts.append(f"## Output ({len(output)} chars)\n\n```\n{output}\n```\n\n")
    else:
        parts.append("## Output\n\n_(empty)_\n\n")
    path = log_dir / f"call_{call_id:03d}_output.md"
    try:
        path.write_text("".join(parts), encoding="utf-8")
        logger.info("[call#%d] Full output saved to %s (%d bytes)", call_id, path, len(output))
    except OSError as e:
        logger.warning("[call#%d] Failed to save output log: %s", call_id, e)


def _quote_arg(value: str) -> str:
    if " " in value or '"' in value:
        return '"' + value.replace('"', '\\"') + '"'
    return value
