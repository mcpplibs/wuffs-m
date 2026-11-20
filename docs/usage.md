# Using mcpplibs.mwrap.wuffs

`import mcpplibs.mwrap.wuffs;` declares Wuffs 0.3.6's API as `<wuffs-v0.3.c>` (the release file, without `WUFFS_IMPLEMENTATION`) does. Types, struct tags, functions and status messages are the upstream globals, the very same entities as the header's, so a translation unit may include the header too. The library is `compat.wuffs` 0.3.6: the tag archive's `release/c/wuffs-v0.3.c` compiled once, as C++, with every Wuffs package and the `wuffs_aux` C++ API.

## Modules

One module per upstream package; each re-exports `base`, and the root re-exports all of them.

| Module | What it holds |
|---|---|
| `mcpplibs.mwrap.wuffs` | every module below |
| `mcpplibs.mwrap.wuffs.base` | `wuffs_base__*`: status, slices and tables, I/O buffers and tokens, pixel formats, configs and buffers, the image decoder / io transformer / token decoder / hasher interfaces, numbers (parse, render), UTF-8, base-16 / base-64, magic numbers; 285 static inline functions, adapted |
| `.adler32`, `.crc32` | `wuffs_adler32__hasher`, `wuffs_crc32__ieee_hasher` |
| `.deflate`, `.zlib`, `.gzip`, `.bzip2`, `.lzw` | the decompressors (`io_transformer`s) |
| `.png`, `.gif`, `.bmp`, `.tga`, `.wbmp`, `.nie` | the image decoders |
| `.json`, `.cbor` | the token decoders |
| `mcpplibs.mwrap.wuffs.aux` | `wuffs_aux`: `DecodeImage`, `DecodeJson`, `DecodeCbor`, their callbacks, arguments and results, `sync_io::Input`, `MemoryInput`, `FileInput`, `DynIOBuffer` |
| `mcpplibs.mwrap.wuffs.macros` | the macros, in `mcpplibs::mwrap::wuffs::macros` |

The source is `src/<package>.cppm`, `src/wuffs_aux.cppm` (not `aux.cppm`: AUX is a reserved file name on Windows) and `src/macros.cppm`, all written by [tools/gen_exports.py](../tools/gen_exports.py) from clang's AST of the release file.

## The static inline API

Wuffs declares its small helpers `static inline`: `wuffs_base__make_slice_u8`, `wuffs_base__ptr_u8__reader`, `wuffs_base__io_buffer__reader_length`, `wuffs_base__pixel_config__width`, `wuffs_base__peek_u32le__no_bounds_check`, and each decoder's `…__alloc_as__wuffs_base__image_decoder` and `…__upcast_as__…` (315 in all). A function with internal linkage cannot be exported, so each is adapted in `namespace mcpplibs::mwrap::wuffs` under the same name and signature, calling upstream's own:

```cpp
import mcpplibs.mwrap.wuffs;
using namespace mcpplibs::mwrap::wuffs;

wuffs_base__image_decoder* dec = wuffs_png__decoder__alloc_as__wuffs_base__image_decoder();
wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(data, size, true);
```

The adapters are not `inline` (an inline body would expose the internal function to importers); an optimising build inlines upstream's body into each. In a translation unit that also includes the header, `::wuffs_base__make_slice_u8` is the header's and `mcpplibs::mwrap::wuffs::wuffs_base__make_slice_u8` the module's: qualify, or do not bring both into one scope. `tests/surface_test.cpp` checks every adapter's type against the header's function at compile time.

The C++ member functions Wuffs defines inside its structs (`status.is_ok()`, `pixbuf.plane(0)`, `decoder->transform_io(...)`, `wuffs_png__decoder::alloc()`) come with the structs and need nothing.

## Decoder objects in C++

Compiled as C++, Wuffs deletes its decoders' default constructors, so that they are never left uninitialised. Allocate them as upstream documents: `wuffs_png__decoder::alloc()` (a `std::unique_ptr`), the C `wuffs_png__decoder__alloc()` (`malloc`ed, release with `free`), or `malloc(sizeof__wuffs_png__decoder())` followed by `wuffs_png__decoder__initialize(p, size, WUFFS_VERSION, options)`. Wuffs accepts a caller built against the same major version and a minor version no newer than the library's.

## Macros

A named module cannot export a macro. `mcpplibs::mwrap::wuffs::macros` holds them under their original names:

- **Constants** (296) as `inline constexpr`, each of its macro's type: `WUFFS_VERSION*`, `WUFFS_INITIALIZE__*`, `WUFFS_BASE__PIXEL_FORMAT__*`, `WUFFS_BASE__PIXEL_BLEND__*`, `WUFFS_BASE__PIXEL_SUBSAMPLING__*`, `WUFFS_BASE__FOURCC__*`, `WUFFS_BASE__ANIMATION_DISPOSAL__*`, `WUFFS_BASE__MORE_INFORMATION__FLAVOR__*`, the token constants, the number and base-64 options, each package's quirks (`WUFFS_JSON__QUIRK_*`, `WUFFS_GIF__QUIRK_*` …) and limits. `WUFFS_VERSION_STRING` and `WUFFS_VERSION_PRE_RELEASE_LABEL` are `const char[]`.
- **Names of functions** (34): `wuffs_base__load_u32le__no_bounds_check` and the other `load` / `store` / `u8` aliases of the `peek` / `poke` helpers, as constexpr pointers to the function they name (the adapted one, for a static inline target).

`<mwrap/wuffs/macros.hpp>` is `<wuffs-v0.3.c>` (the interface). Include it before the imports for `#if` tests and the type-building macros below. While a macro is defined it expands even inside a qualified name, so `macros::WUFFS_VERSION` is not reachable there: use the plain macro, or set it aside with `#pragma push_macro` / `#undef`, as `tests/surface_test.cpp` does.

### Not mapped

| Macro | Why |
|---|---|
| `WUFFS_BASE__SLICE(T)`, `WUFFS_BASE__TABLE(T)`, `WUFFS_BASE__RESULT(T)` | build a type name by token pasting; the types they built are exported (`wuffs_base__slice_u8` …) |
| `WUFFS_BASE__CPU_ARCH__X86_FAMILY` | defined, empty, on x86 targets only: a test for `#if` |
| `WUFFS_BASE__HAVE_EQ_DELETE`, `WUFFS_BASE__HAVE_UNIQUE_PTR` | feature tests of the compiler, empty |
| `WUFFS_BASE__MAYBE_STATIC`, `WUFFS_BASE__POTENTIALLY_UNUSED`, `WUFFS_BASE__WARN_UNUSED_RESULT`, `WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET` | declaration machinery and attributes |
| `WUFFS_INCLUDE_GUARD` | include guard |

[tools/check_exports.py](../tools/check_exports.py) accounts for all 341 macros the file defines.

## Configuration

`compat.wuffs` builds every package (no `WUFFS_CONFIG__MODULES`), with Wuffs' own CPU detection: the SIMD paths are compiled where the target has them and chosen at run time. `WUFFS_CONFIG__STATIC_FUNCTIONS` is not defined (the library is shared by every user of the package).

## Conformance

[tools/run_conformance.py](../tools/run_conformance.py) builds [examples/convert-to-nia](../examples/convert-to-nia) (upstream's program ported against the module) and upstream's `example/convert-to-nia/convert-to-nia.c` from the same archive, feeds each of the 246 files of `test/data` to both, as NIA and with `-first-frame-only`, and requires the same stdout bytes, message and exit code; then the NIA checksums of all 82 files upstream's `test/nia-checksums-of-data.txt` lists. Two lines of the reference differ from the file: its include names `wuffs-v0.3.c` instead of the development snapshot, and its end-of-input test is upstream main's (`n == 0`) instead of 0.3.6's `errno == 0`, which misreads a stale errno where the Linux seccomp call fails. The reference uses `<unistd.h>`; the comparison runs on Linux and macOS.
