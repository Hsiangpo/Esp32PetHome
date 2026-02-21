#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""代码门禁：限制单文件和单函数体积。"""

from __future__ import annotations

import pathlib
import re
import sys
from dataclasses import dataclass
from typing import Iterable

MAX_FILE_LINES = 1000
MAX_FUNCTION_LINES = 200

CODE_EXTENSIONS = {".cpp", ".cc", ".c", ".h", ".hpp", ".ts", ".ets", ".js"}
SKIP_DIR_NAMES = {
    ".git",
    ".pio",
    "build",
    "node_modules",
    "oh_modules",
    ".hvigor",
}


@dataclass
class Violation:
    path: pathlib.Path
    message: str


def iter_code_files(root: pathlib.Path) -> Iterable[pathlib.Path]:
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() not in CODE_EXTENSIONS:
            continue
        if any(part in SKIP_DIR_NAMES for part in path.parts):
            continue
        yield path


def read_lines(path: pathlib.Path) -> list[str]:
    content = path.read_text(encoding="utf-8")
    return content.splitlines()


def looks_like_function_start(line: str) -> bool:
    text = line.strip()
    if not text.endswith("{"):
        return False
    if "(" not in text or ")" not in text:
        return False

    skip_prefix = ("if", "for", "while", "switch", "catch", "else", "do")
    if text.startswith(skip_prefix):
        return False
    if text.startswith("//") or text.startswith("*"):
        return False
    return True


def count_function_block(lines: list[str], start_idx: int) -> int:
    depth = 0
    started = False
    for idx in range(start_idx, len(lines)):
        line = lines[idx]
        open_braces = line.count("{")
        close_braces = line.count("}")
        if open_braces > 0:
            started = True
        depth += open_braces
        depth -= close_braces
        if started and depth <= 0:
            return idx - start_idx + 1
    return len(lines) - start_idx


def check_file(path: pathlib.Path) -> list[Violation]:
    violations: list[Violation] = []
    lines = read_lines(path)

    if len(lines) > MAX_FILE_LINES:
        violations.append(
            Violation(
                path=path,
                message=f"文件行数 {len(lines)} 超过限制 {MAX_FILE_LINES}",
            )
        )

    for idx, line in enumerate(lines):
        if not looks_like_function_start(line):
            continue
        func_lines = count_function_block(lines, idx)
        if func_lines > MAX_FUNCTION_LINES:
            snippet = line.strip()[:80]
            violations.append(
                Violation(
                    path=path,
                    message=(
                        f"函数体行数 {func_lines} 超过限制 {MAX_FUNCTION_LINES} "
                        f"(约在第 {idx + 1} 行, {snippet})"
                    ),
                )
            )
    return violations


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    all_violations: list[Violation] = []
    for file_path in iter_code_files(root):
        all_violations.extend(check_file(file_path))

    if not all_violations:
        print("code_guard: PASS")
        return 0

    print("code_guard: FAIL")
    for item in all_violations:
        rel = item.path.relative_to(root)
        print(f"- {rel}: {item.message}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
