#!/usr/bin/env python3
"""CI gate: forbid NEW raw config-key string literals in src/ C++ code.

Scans the ADDED lines of a changeset (git diff). Any added quoted literal that
exactly matches a known config key name fails the gate. Existing (存量)
occurrences are out of scope; tests/ and non-C++ files are always exempt.

Key list comes from the same source as ConfigKeys.hpp: the generator module
(scripts/generate_config_keys.py) parsing PrintConfig.cpp.

Usage:
  python scripts/check_config_key_literals.py                       # diff vs origin/main
  python scripts/check_config_key_literals.py --base origin/develop # other base
  python scripts/check_config_key_literals.py --diff-file pr.diff   # raw unified diff
  python scripts/check_config_key_literals.py --warning             # report, exit 0

Exit codes: 0 = pass (or --warning), 1 = violations found / error.
Fix for a violation: use the cfg::<key> constant from src/libslic3r/ConfigKeys.hpp,
or add the file to scripts/config_key_gate_allowlist.txt after review.
"""

import argparse
import fnmatch
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BASE = "origin/main"
ALLOWLIST = Path(__file__).resolve().parent / "config_key_gate_allowlist.txt"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_config_keys import extract_keys, DEFAULT_SOURCE  # noqa: E402

# Scope: src/ C/C++ sources. tests/ and non-code files are exempt (see spec).
SCOPE_PREFIX = "src/"
SCOPE_SUFFIXES = (".cpp", ".hpp", ".h", ".c", ".cc", ".cxx", ".mm", ".m")
EXEMPT_PREFIXES = ("src/tests/",)  # defensive; tests live outside src/ anyway


def load_allowlist() -> list:
    if not ALLOWLIST.is_file():
        return []
    pats = []
    for raw in ALLOWLIST.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if line and not line.startswith("#"):
            pats.append(line.replace("\\", "/"))
    return pats


def key_pattern(keys: list) -> re.Pattern:
    """Match a quoted literal that is exactly a config key: "layer_height".

    The (?:...) grouping is essential: '"a|b"' would bind the quotes only to
    the first/last alternation branch and match bare identifiers in between.
    """
    alternation = "|".join(keys)
    return re.compile('"(?:%s)"' % alternation)


def iter_added_lines(diff_text: str):
    """Yield (file_path, line_number, added_line) from a unified diff."""
    path = None
    new_lineno = 0
    for line in diff_text.splitlines():
        if line.startswith("--- "):
            continue
        if line.startswith("+++ "):
            path = line[4:].split("\t")[0]
            if path.startswith("b/"):
                path = path[2:]
            new_lineno = 0
            continue
        if path is None:
            continue
        if line.startswith("@@"):
            m = re.search(r"\+(\d+)", line)
            new_lineno = int(m.group(1)) - 1 if m else 0
            continue
        if line.startswith("+"):
            new_lineno += 1
            yield path, new_lineno, line[1:]
        elif line.startswith("-"):
            continue
        else:
            new_lineno += 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--base", default=DEFAULT_BASE,
                        help="git ref to diff against (default: %(default)s)")
    parser.add_argument("--diff-file", type=Path, default=None,
                        help="read a unified diff from this file instead of git")
    parser.add_argument("--warning", action="store_true",
                        help="report violations as warnings and exit 0")
    args = parser.parse_args()

    if args.diff_file:
        diff_text = args.diff_file.read_text(encoding="utf-8", errors="replace")
    else:
        diff_text = subprocess.run(
            ["git", "diff", args.base, "--unified=0", "--"],
            cwd=REPO_ROOT, capture_output=True, text=True, encoding="utf-8",
            errors="replace",
        ).stdout

    keys = extract_keys(DEFAULT_SOURCE.read_text(encoding="utf-8"))
    pattern = key_pattern(keys)
    allowlist = load_allowlist()

    violations, allowed_hits = {}, []
    for path, lineno, text in iter_added_lines(diff_text):
        norm = path.replace("\\", "/")
        if not norm.startswith(SCOPE_PREFIX):
            continue
        if not norm.endswith(SCOPE_SUFFIXES):
            continue
        if any(norm.startswith(p) for p in EXEMPT_PREFIXES):
            continue
        stripped = text.strip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        hits = sorted(set(pattern.findall(text)))
        if not hits:
            continue
        if any(fnmatch.fnmatch(norm, pat) for pat in allowlist):
            allowed_hits.append((norm, lineno, hits))
            continue
        violations.setdefault(norm, []).append((lineno, hits))

    if allowed_hits:
        print("INFO: %d added line(s) with raw key literals in allowlisted files:"
              % len(allowed_hits))
        for path, lineno, hits in allowed_hits:
            print("  %s:%d %s" % (path, lineno, ",".join(hits)))

    if not violations:
        print("OK: no new raw config-key literals (scope: src/ C++; "
              "%d keys known)." % len(keys))
        return 0

    for path in sorted(violations):
        for lineno, hits in violations[path]:
            print("VIOLATION: %s:%d raw config key literal %s" % (path, lineno, hits))
    print("\nFix: use the cfg::<key> constant from src/libslic3r/ConfigKeys.hpp, "
          "or (after review) add the file to scripts/config_key_gate_allowlist.txt.")
    if args.warning:
        print("WARNING mode: gate not enforced (report only).")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
