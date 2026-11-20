# Wuffs

> 通过 C++23 模块使用 Wuffs（Google 面向不可信文件格式的内存安全解码器）。

[English](README.md) · **简体中文** · [上游](https://github.com/google/wuffs) · [发布](https://github.com/mcpplibs/wuffs-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [索引](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/wuffs-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/wuffs-m/actions/workflows/ci.yml)

上游 API 保持不变：`wuffs_png__decoder`、`wuffs_base__image_decoder__decode_frame`、
`wuffs_aux::DecodeImage` 就是 `<wuffs-v0.3.c>` 里的名字。库本身是 `compat.wuffs`
0.3.6：发布文件按 C++ 编译，因此也包含 `wuffs_aux` C++ API。Wuffs 有约 300 个
`static inline` 辅助函数，模块无法导出它们；本包在 `mcpplibs::mwrap::wuffs` 中以同名、
同签名逐一适配，并把宏映射为常量。每次 CI 都会在只用本模块改写的移植版上运行上游的
`convert-to-nia`，每个输出都必须与上游一致。

## 模块

- `import mcpplibs.mwrap.wuffs;`：下面全部模块
- `import mcpplibs.mwrap.wuffs.base;`：`wuffs_base__*`：状态、切片、I/O 缓冲、像素配置、数字、UTF-8
- `import mcpplibs.mwrap.wuffs.png;`：`wuffs_png__decoder`（`.gif`、`.bmp`、`.tga`、`.wbmp`、`.nie` 同理）
- `import mcpplibs.mwrap.wuffs.gzip;`：`wuffs_gzip__decoder`（`.deflate`、`.zlib`、`.bzip2`、`.lzw` 同理）
- `import mcpplibs.mwrap.wuffs.json;`：`wuffs_json__decoder`（`.cbor` 同理；哈希：`.crc32`、`.adler32`）
- `import mcpplibs.mwrap.wuffs.aux;`：`wuffs_aux::DecodeImage`、`DecodeJson`、`DecodeCbor`、`sync_io::MemoryInput`
- `import mcpplibs.mwrap.wuffs.macros;`：`WUFFS_VERSION`、`WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL` 等，位于 `mcpplibs::mwrap::wuffs::macros`

> 更多模块 API -> [用法和 API](docs/usage.md)

## 快速开始

```sh
mcpp new untrusted --template mcpplibs.mwrap.wuffs@0.3.6
cd untrusted
mcpp run
```

默认模板解码内置的 PNG 和 GIF、解压一个 gzip 成员、遍历一份 JSON 文档，并演示截断的
PNG 被拒绝；`mcpp run -- photo.png` 可以描述任意 BMP、GIF、NIE、PNG、TGA 或 WBMP 图像。

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

## static inline API 与宏

`wuffs_base__make_slice_u8`、`wuffs_base__io_buffer__reader_length`、
`wuffs_<pkg>__decoder__alloc_as__…` 等 Wuffs 的 `static inline` API 都在
`mcpplibs::mwrap::wuffs` 中适配；写 `using namespace mcpplibs::mwrap::wuffs;` 后，
可以像上游示例那样直接调用。每个适配函数都调用上游自身的实现；有一个测试在编译期
逐一核对它们与头文件的签名。

常量保持宏原来的类型（`WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL` 即 `0x82008888`，是
`unsigned int`；`WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS` 是 `uint32_t`）；
指代函数名的宏（`wuffs_base__load_u32le__no_bounds_check` 等）映射为指向该函数的指针。
`<mwrap/wuffs/macros.hpp>`（即 `<wuffs-v0.3.c>`）用于 `#if` 判断和构造类型的
`WUFFS_BASE__SLICE(T)`，须在 import 之前 include。详见 [docs/usage.md](docs/usage.md)。

在 C++ 中 Wuffs 的解码器结构体不能默认构造：按上游文档，用 `wuffs_png__decoder::alloc()`
或 C 的 `…__alloc` 函数分配。

## 示例

- [examples/convert-to-nia](examples/convert-to-nia)：上游 `convert-to-nia` 用本模块改写的 C++ 版本；
  [tools/run_conformance.py](tools/run_conformance.py) 在发布包的测试数据上运行它。

## 构建与测试

Linux、macOS、Windows 的 CI 用 GCC 和 Clang 构建模块并运行测试：只经过本模块的 API 测试
（每个包都用已知解码结果的数据，以及错误路径），以及一个头文件混用测试，在编译期核对
每个适配函数的签名和每个宏映射。导出审计（[tools/check_exports.py](tools/check_exports.py)）
读取发布文件的 clang AST：每个声明都已导出，每个宏都已映射或附原因列出。一致性测试把
`test/data` 中的 246 个文件分别交给移植版和上游 `convert-to-nia.c`，要求字节、消息和退出码
完全相同，并与上游 `test/nia-checksums-of-data.txt` 的 NIA 校验和一致。另有一个任务把默认
模板当作已安装包生成项目并运行。

## 许可

封装层为 [Apache-2.0](LICENSE)，作者 mcpp-community（[NOTICE](NOTICE)）。Wuffs 仍遵循
Apache License 2.0（[upstream/wuffs/LICENSE](upstream/wuffs/LICENSE)、
[README](upstream/wuffs/README.md)、[AUTHORS](upstream/wuffs/AUTHORS)）。
