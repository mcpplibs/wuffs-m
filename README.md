# Wuffs

> Wuffs, Google's memory-safe decoders for untrusted file formats, through C++23 modules.

**English** · [简体中文](README.zh.md) · [Upstream](https://github.com/google/wuffs) · [Releases](https://github.com/mcpplibs/wuffs-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [Index](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/wuffs-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/wuffs-m/actions/workflows/ci.yml)

The upstream API is unchanged: `wuffs_png__decoder`, `wuffs_base__image_decoder__decode_frame`,
`wuffs_aux::DecodeImage` are the names of `<wuffs-v0.3.c>`. The library is `compat.wuffs`
0.3.6, the release file compiled as C++, which also builds the `wuffs_aux` C++ API.
Wuffs declares about 300 helpers `static inline`, which a module cannot export; this
package adapts each one in `mcpplibs::mwrap::wuffs` with the same name and signature,
and maps the macros to constants. Upstream's `convert-to-nia` runs on every CI run, on
a port written against the module, with every output equal to upstream's.

## Modules

- `import mcpplibs.mwrap.wuffs;`: everything below
- `import mcpplibs.mwrap.wuffs.base;`: `wuffs_base__*`: status, slices, I/O buffers, pixel configs, numbers, UTF-8
- `import mcpplibs.mwrap.wuffs.png;`: `wuffs_png__decoder` (likewise `.gif`, `.bmp`, `.tga`, `.wbmp`, `.nie`)
- `import mcpplibs.mwrap.wuffs.gzip;`: `wuffs_gzip__decoder` (likewise `.deflate`, `.zlib`, `.bzip2`, `.lzw`)
- `import mcpplibs.mwrap.wuffs.json;`: `wuffs_json__decoder` (likewise `.cbor`; hashes: `.crc32`, `.adler32`)
- `import mcpplibs.mwrap.wuffs.aux;`: `wuffs_aux::DecodeImage`, `DecodeJson`, `DecodeCbor`, `sync_io::MemoryInput`
- `import mcpplibs.mwrap.wuffs.macros;`: `WUFFS_VERSION`, `WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL` … in `mcpplibs::mwrap::wuffs::macros`

> More Modules API -> [Usage and API](docs/usage.md)

## Getting started

```sh
mcpp new untrusted --template mcpplibs.mwrap.wuffs@0.3.6
cd untrusted
mcpp run
```

The default template decodes a bundled PNG and GIF, inflates a gzip member, walks a
JSON document and shows a truncated PNG refused; `mcpp run -- photo.png` describes any
BMP, GIF, NIE, PNG, TGA or WBMP image.

```toml
[dependencies]
mcpplibs.mwrap.wuffs = "0.3.6"
```

```cpp
import std;
import mcpplibs.mwrap.wuffs;

std::string decode(std::span<const std::uint8_t> file) {
    wuffs_aux::DecodeImageCallbacks callbacks;
    wuffs_aux::sync_io::MemoryInput input(file.data(), file.size());
    wuffs_aux::DecodeImageResult res = wuffs_aux::DecodeImage(callbacks, input);
    if (!res.error_message.empty()) return res.error_message;
    return std::format("{} x {}", res.pixbuf.pixcfg.width(), res.pixbuf.pixcfg.height());
}
```

## The static inline API and the macros

`wuffs_base__make_slice_u8`, `wuffs_base__io_buffer__reader_length`, the
`wuffs_<pkg>__decoder__alloc_as__…` functions and the rest of Wuffs' `static inline`
API are adapted in `mcpplibs::mwrap::wuffs`; write `using namespace
mcpplibs::mwrap::wuffs;` and call them as upstream's examples do. Each adapter calls
upstream's own; a test checks every signature against the header at compile time.

Constants keep their macros' types (`WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL`, `0x82008888`,
is an `unsigned int`; `WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS` a `uint32_t`); the
macros that name a function
(`wuffs_base__load_u32le__no_bounds_check` …) are pointers to it.
`<mwrap/wuffs/macros.hpp>` (`<wuffs-v0.3.c>`) is for `#if` tests and the type-building
`WUFFS_BASE__SLICE(T)`; include it before the imports. Details: [docs/usage.md](docs/usage.md).

In C++, Wuffs' decoder structs cannot be default-constructed: allocate them with
`wuffs_png__decoder::alloc()` or the C `…__alloc` functions, as upstream documents.

## Examples

- [examples/convert-to-nia](examples/convert-to-nia): upstream's `convert-to-nia`, ported to C++ against the module;
  [tools/run_conformance.py](tools/run_conformance.py) runs it on the release's test data.

## Build and testing

Linux, macOS and Windows CI build the modules with GCC and Clang and run the tests: the
API through the module alone (every package, on data with known decodings, and the error
paths), and a header-mixing test that checks every adapter's signature and every mapped
macro at compile time. The export audit ([tools/check_exports.py](tools/check_exports.py))
reads clang's AST of the release file: every declaration exported, every macro mapped or
listed with its reason. The conformance run feeds each of the 246 files of `test/data` to
the port and to upstream's `convert-to-nia.c` and requires the same bytes, message and
exit code, and the NIA checksums of upstream's `test/nia-checksums-of-data.txt`. A
separate job scaffolds the default template as an installed package and runs it.

## License

[Apache-2.0](LICENSE) for the wrapper, by mcpp-community ([NOTICE](NOTICE)). Wuffs
remains under the Apache License 2.0 ([upstream/wuffs/LICENSE](upstream/wuffs/LICENSE),
[README](upstream/wuffs/README.md), [AUTHORS](upstream/wuffs/AUTHORS)).
