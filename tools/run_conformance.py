#!/usr/bin/env python3
"""Run examples/convert-to-nia, Wuffs' convert-to-nia ported to C++ against
import mcpplibs.mwrap.wuffs, on every file of the release's test data, and
compare each outcome with upstream's convert-to-nia.c itself.

The release tarball is downloaded once into target/upstream and checked
against upstream/documents.json. Two hosts are built from it:

- the port, examples/convert-to-nia (the module; no Wuffs header);
- the reference: the release's example/convert-to-nia/convert-to-nia.c,
  compiled as C (target/conformance/reference). One line differs from the
  file: its `#include "../../release/c/wuffs-unsupported-snapshot.c"` (the
  development snapshot upstream builds its examples with) names
  release/c/wuffs-v0.3.c of the same archive instead, the file compat.wuffs
  builds. It is otherwise unchanged, so it carries its own copy of Wuffs
  (WUFFS_IMPLEMENTATION, WUFFS_CONFIG__STATIC_FUNCTIONS) and, on Linux, its
  seccomp sandbox.

Every regular file under test/data (images, and the non-images it must
refuse) is fed on stdin, once as is (NIA) and once with -first-frame-only
(NIE). For each, the port's exit status, stdout bytes and stderr text must
equal the reference's. Then the CRC-32 of each NIA output (a bad file's partial frames included,
as upstream's script/print-nia-checksums.sh counts them) must equal
upstream's test/nia-checksums-of-data.txt, for every file it lists.

A second change is upstream's own later fix, backported: 0.3.6's
read_more_src tells the end of input by `errno == 0` after read() returns 0,
without clearing errno first. Where the Linux seccomp call fails (it does on
hosts that refuse strict mode: EINVAL), errno is stale, and the program
reports "Invalid argument" instead of closing the input, a step early, with
other pixels. Upstream's main branch tests `n == 0` instead; the reference
does the same (the port reads the end of input that way too).

The reference uses <unistd.h>: the comparison runs on Linux and macOS.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parent))
from inventory import fetch  # noqa: E402

REFERENCE_MANIFEST = '''[package]
name        = "convert-to-nia-reference"
version     = "0.1.0"
description = "Wuffs 0.3.6's example/convert-to-nia/convert-to-nia.c against release/c/wuffs-v0.3.c: the reference tools/run_conformance.py compares the port with"

[targets.convert-to-nia-reference]
kind = "bin"
main = "src/convert-to-nia.c"

[build]
include_dirs = [{include}]
# mcpp compiles C as ISO C11; upstream builds its examples with the compiler's
# GNU default, under which <string.h> and <unistd.h> declare strnlen and syscall.
cflags = ["-D_GNU_SOURCE"]
'''

SNAPSHOT_INCLUDE = '#include "../../release/c/wuffs-unsupported-snapshot.c"'
RELEASE_INCLUDE = '#include "wuffs-v0.3.c"'
# 0.3.6's end-of-input test, and upstream main's (google/wuffs, example/convert-to-nia).
EOF_BY_ERRNO = """  } else if (errno == 0) {
    if (n < 0) {
      return "main: unexpected negative-count read";
    }
    g_src.meta.closed = true;
  } else if (errno != EINTR) {"""
EOF_BY_COUNT = """  } else if (n == 0) {
    g_src.meta.closed = true;
  } else if (errno != EINTR) {"""


def build(project: Path, name: str, mcpp: str, toolchain: str | None) -> Path:
    command = [mcpp, 'build', '--profile', 'release'] + (['--toolchain', toolchain] if toolchain else [])
    print('+', ' '.join(command), f'({project.name})', flush=True)
    subprocess.run(command, cwd=project, check=True)
    exe = name + ('.exe' if os.name == 'nt' else '')
    found = sorted((p for p in (project / 'target').rglob(exe) if p.parent.name == 'bin'),
                   key=lambda p: p.stat().st_mtime)
    if not found:
        sys.exit(f'{project} was not built')
    return found[-1]


def run(exe: Path, args: list[str], data: bytes) -> tuple[int, bytes, str]:
    out = subprocess.run([str(exe), *args], input=data, capture_output=True, timeout=600)
    return out.returncode, out.stdout, out.stderr.decode('utf-8', 'replace').replace('\r\n', '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mcpp', default='mcpp')
    parser.add_argument('--toolchain')
    args = parser.parse_args()
    if os.name == 'nt':
        print('upstream convert-to-nia.c needs <unistd.h>: the comparison runs on Linux and macOS')
        return
    root = Path(__file__).resolve().parents[1]
    upstream = fetch(root)

    reference_dir = root / 'target/conformance/reference'
    (reference_dir / 'src').mkdir(parents=True, exist_ok=True)
    include = '"' + (upstream / 'release/c').as_posix() + '"'
    (reference_dir / 'mcpp.toml').write_text(REFERENCE_MANIFEST.format(include=include), encoding='utf-8')
    source = (upstream / 'example/convert-to-nia/convert-to-nia.c').read_text(encoding='utf-8')
    assert source.count(SNAPSHOT_INCLUDE) == 1, 'upstream include line changed'
    assert source.count(EOF_BY_ERRNO) == 1, 'upstream read_more_src changed'
    source = source.replace(SNAPSHOT_INCLUDE, RELEASE_INCLUDE).replace(EOF_BY_ERRNO, EOF_BY_COUNT)
    (reference_dir / 'src/convert-to-nia.c').write_text(source, encoding='utf-8')

    port = build(root / 'examples/convert-to-nia', 'convert-to-nia', args.mcpp, args.toolchain)
    reference = build(reference_dir, 'convert-to-nia-reference', args.mcpp, args.toolchain)

    files = sorted(p for p in (upstream / 'test/data').rglob('*') if p.is_file())
    failures, decoded, refused = [], 0, 0
    nia_crc: dict[str, str] = {}
    for path in files:
        name = path.relative_to(upstream).as_posix()
        data = path.read_bytes()
        for flags in ([], ['-1']):
            mine = run(port, flags, data)
            ref = run(reference, flags, data)
            label = f'{name} {" ".join(flags)}'.strip()
            if mine != ref:
                failures.append(label)
                print(f'FAIL  {label}: port exit {mine[0]} ({len(mine[1])} bytes, {mine[2].strip()!r}) '
                      f'vs upstream exit {ref[0]} ({len(ref[1])} bytes, {ref[2].strip()!r})', flush=True)
                continue
            if not flags:
                decoded += ref[0] == 0
                refused += ref[0] != 0
                if ref[1]:  # upstream's script lists any output, partial frames of a bad file too
                    nia_crc[name] = f'{zlib.crc32(ref[1]):08x}'
    print(f'{len(files)} files of test/data, each as NIA and NIE: {decoded} decoded, '
          f'{refused} refused, every outcome equal to upstream convert-to-nia.c', flush=True)

    # Upstream's own list (regenerated against the development snapshot).
    listed = {}
    for line in (upstream / 'test/nia-checksums-of-data.txt').read_text(encoding='utf-8').splitlines():
        if line and not line.startswith('#'):
            crc, name = line.split(None, 1)
            listed[name] = crc
    agree = sorted(n for n in listed if nia_crc.get(n) == listed[n])
    differ = sorted(n for n in listed if n in nia_crc and nia_crc[n] != listed[n])
    absent = sorted(n for n in listed if n not in nia_crc)
    print(f'test/nia-checksums-of-data.txt: {len(agree)} of {len(listed)} equal'
          + (f'; differ: {differ}' if differ else '') + (f'; no output here: {absent}' if absent else ''))
    if differ or absent:
        failures.append('test/nia-checksums-of-data.txt')
    if failures:
        sys.exit(f'{len(failures)} outcomes differ from upstream')


if __name__ == '__main__':
    main()
