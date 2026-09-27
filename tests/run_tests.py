#!/usr/bin/env python3
"""Dependency-free runner for the repository's simple regression tests.

The test modules intentionally use only top-level assertions and zero-argument
functions named ``test_*``. This runner provides the release/update workflow
with a standard-library-only test gate, while remaining compatible with pytest
for developers who prefer it.
"""

from __future__ import annotations

import importlib.util
import inspect
import os
from pathlib import Path
import sys
import traceback


def load_module(path: Path, index: int):
    module_name = f"idotmatrix_regression_{index}_{path.stem}"
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load test module: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    tests_dir = Path(__file__).resolve().parent
    repository_root = tests_dir.parent
    os.chdir(repository_root)
    files = sorted(tests_dir.glob("test_*.py"))
    if not files:
        print("ERROR: no regression tests found", file=sys.stderr)
        return 2

    total = 0
    failures: list[str] = []

    for index, path in enumerate(files):
        try:
            module = load_module(path, index)
        except Exception:
            label = f"{path.name} [module import/collection]"
            failures.append(label)
            print(f"FAIL {label}")
            traceback.print_exc()
            continue

        functions = []
        for name, obj in vars(module).items():
            if not name.startswith("test_") or not inspect.isfunction(obj):
                continue
            if obj.__module__ != module.__name__:
                continue
            functions.append((name, obj))

        for name, func in sorted(functions):
            total += 1
            label = f"{path.name}::{name}"
            try:
                signature = inspect.signature(func)
                if signature.parameters:
                    raise RuntimeError(
                        "dependency-free runner supports only zero-argument test functions"
                    )
                func()
                print(f"PASS {label}")
            except Exception:
                failures.append(label)
                print(f"FAIL {label}")
                traceback.print_exc()

    if failures:
        print()
        print(f"Regression suite: FAIL ({len(failures)} failed, {total} executed)")
        for label in failures:
            print(f"  - {label}")
        return 1

    print()
    print(f"Regression suite: PASS ({total} tests)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
