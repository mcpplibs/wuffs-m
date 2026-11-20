#!/usr/bin/env python3
"""Audit the module surface against wuffs-v0.3.c (the pinned release's).

- src/*.cppm and tests/surface_test.cpp are what tools/gen_exports.py writes,
  so every type, struct tag, function, status message and table of every
  package, and every name of wuffs_aux, is exported, and every static inline
  function adapted; tests/surface_test.cpp then checks each adapter's
  signature and each mapped macro against the header, at compile time.
- Every macro the file defines is mapped in src/macros.cppm (a constant, or a
  pointer to the function it names) or listed below with its reason; a macro
  a new release adds fails here until it is accounted for.

Reads clang's AST (tools/inventory.py): pass --clang or set CLANGXX if
clang++ is not on PATH or in the toolchain mcpp installed.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from inventory import fetch, find_clang, surface  # noqa: E402

# Defined by the file, not mappable to a constant or a function, and why.
UNMAPPED = {
    'WUFFS_INCLUDE_GUARD': 'include guard',
    'WUFFS_BASE__HAVE_EQ_DELETE': 'feature test of the compiler (C++11 `= delete`), empty',
    'WUFFS_BASE__HAVE_UNIQUE_PTR': 'feature test of the compiler (std::unique_ptr), empty',
    'WUFFS_BASE__CPU_ARCH__X86_FAMILY': 'defined on x86 targets only, empty: a test for #if (the side header)',
    'WUFFS_BASE__MAYBE_STATIC': 'declaration machinery (WUFFS_CONFIG__STATIC_FUNCTIONS), empty here',
    'WUFFS_BASE__POTENTIALLY_UNUSED': 'attribute of declarations',
    'WUFFS_BASE__WARN_UNUSED_RESULT': 'attribute of declarations',
    'WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET': 'attribute of declarations (function-like)',
    'WUFFS_BASE__RESULT': 'builds a type name by token pasting (function-like): the side header',
    'WUFFS_BASE__SLICE': 'builds a type name by token pasting (function-like): the side header',
    'WUFFS_BASE__TABLE': 'builds a type name by token pasting (function-like): the side header',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    clang = find_clang(args.clang)
    problems = []
    generated = subprocess.run([sys.executable, str(root / 'tools/gen_exports.py'), '--check', '--clang', clang],
                               capture_output=True, text=True)
    if generated.returncode != 0:
        problems.append(generated.stderr.strip() or generated.stdout.strip())

    sf = surface(fetch(root), clang)
    declared, exported = set(), set()
    for pkg in sf.packages:
        declared |= set(sf.types.get(pkg, [])) | set(sf.tags.get(pkg, [])) | set(sf.functions.get(pkg, []))
        declared |= set(sf.data.get(pkg, []))
        exported |= set(re.findall(r'export using ::(\w+);', (root / f'src/{pkg}.cppm').read_text(encoding='utf-8')))
    if declared != exported:
        problems.append(f'exports differ from the declarations: missing {sorted(declared - exported)}, '
                        f'extra {sorted(exported - declared)}')

    defined = set(sf.constants) | set(sf.aliases) | set(sf.other_macros) | set(sf.function_macros)
    unaccounted = set(sf.other_macros) | set(sf.function_macros)
    if unaccounted - UNMAPPED.keys():
        problems.append('macros neither mapped nor listed: ' + ', '.join(sorted(unaccounted - UNMAPPED.keys())))
    if UNMAPPED.keys() - unaccounted:
        problems.append('listed as unmapped but mapped or gone: ' + ', '.join(sorted(UNMAPPED.keys() - unaccounted)))
    if problems:
        sys.exit('\n'.join(problems))
    inlines = sum(len(v) for v in sf.inlines.values())
    print(f'{len(sf.packages)} packages: {len(declared)} globals exported, {inlines} static inline functions adapted; '
          f'wuffs_aux: {sum(len(v) for v in sf.aux.values())} names; {len(defined)} macros: '
          f'{len(sf.constants)} constants and {len(sf.aliases)} function names mapped, {len(UNMAPPED)} listed')


if __name__ == '__main__':
    main()
