// <mwrap/wuffs/macros.hpp> — Wuffs' macros as macros. Every macro of
// <wuffs-v0.3.c> that is a constant, or names a function, also has a mapping
// in mcpplibs.mwrap.wuffs.macros; this header is for what a mapping cannot
// do: `#if` tests, the type-building WUFFS_BASE__SLICE(T) / TABLE(T) /
// RESULT(T) macros, and code written against the macros themselves. Include
// it before `import mcpplibs.mwrap.wuffs;`: it is <wuffs-v0.3.c> (interface
// only), so every macro is defined, and while a macro is defined it expands
// even inside `mcpplibs::mwrap::wuffs::macros::NAME`.
#pragma once
#include <wuffs-v0.3.c>
