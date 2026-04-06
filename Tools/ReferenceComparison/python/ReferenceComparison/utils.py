"""File I/O, logging setup, and markdown helpers."""

from __future__ import annotations

import logging
import sys
from pathlib import Path
from typing import Optional


def setup_logging(verbose: bool = False, log_file: Optional[Path] = None) -> None:
    level = logging.DEBUG if verbose else logging.INFO
    fmt = "%(asctime)s [%(levelname)s] %(name)s: %(message)s"

    root = logging.getLogger()
    root.setLevel(level)

    console = logging.StreamHandler(sys.stderr)
    console.setLevel(level)
    console.setFormatter(logging.Formatter(fmt))
    root.addHandler(console)

    if log_file is not None:
        ensure_dir(log_file.parent)
        fh = logging.FileHandler(str(log_file), encoding="utf-8")
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(logging.Formatter(fmt))
        root.addHandler(fh)


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def read_file_safe(path: Path) -> Optional[str]:
    try:
        return path.read_text(encoding="utf-8")
    except (FileNotFoundError, PermissionError, OSError):
        return None


def write_file(path: Path, content: str) -> None:
    ensure_dir(path.parent)
    path.write_text(content, encoding="utf-8")
    logging.getLogger(__name__).info("Wrote: %s (%d chars)", path, len(content))


def repo_analysis_filename(repo_key: str, display_name: str) -> str:
    idx = {
        "hazelight": "00",
        "unrealcsharp": "01",
        "unlua": "02",
        "puerts": "03",
        "sluaunreal": "04",
    }
    prefix = idx.get(repo_key, "99")
    return f"{prefix}_{display_name}_Analysis.md"


def cross_comparison_filename(dimension_id: str, dimension_name_en: str) -> str:
    base_index = int(dimension_id.replace("D", "")) + 4
    return f"{base_index:02d}_CrossComparison_{dimension_name_en}.md"


def gap_analysis_filename() -> str:
    return "Summary_GapAnalysis.md"
