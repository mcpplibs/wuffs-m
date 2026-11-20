// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ----------------

// convert-to-nia, Wuffs 0.3.6's example/convert-to-nia/convert-to-nia.c
// ported to C++23 against `import mcpplibs.mwrap.wuffs;` (no Wuffs header):
// it converts an image from stdin (BMP, GIF, NIE, PNG, TGA, WBMP) to stdout
// in the NIA format (NIE with -first-frame-only). tools/run_conformance.py
// runs it and upstream's own program on every file of the release's test
// data and requires the same bytes, the same message and the same exit code.
//
// What the port changes, and only that:
//   - I/O is std::fread / std::fwrite on binary stdin / stdout (upstream:
//     POSIX read / write), so it also runs on Windows;
//   - the decoders are allocated by Wuffs' alloc_as functions (upstream: one
//     static union of them; Wuffs' C++ structs cannot be default-constructed);
//   - the 256 MiB buffers are calloc'd (upstream: static arrays);
//   - no seccomp sandbox (upstream's Linux-only -fail-if-unsandboxed is
//     accepted and then refused, as on any unsandboxed platform).
// Everything that shapes the output, including the quirk of print_nie_frame's
// stride branch, is upstream's.

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

import mcpplibs.mwrap.wuffs;

using namespace mcpplibs::mwrap::wuffs;          // the static inline API, adapted
using namespace mcpplibs::mwrap::wuffs::macros;  // WUFFS_BASE__FOURCC__PNG, WUFFS_VERSION …

#define TRY(error_msg)         \
  do {                         \
    const char* z = error_msg; \
    if (z) {                   \
      return z;                \
    }                          \
  } while (false)

static const char* g_usage =
    "Usage: convert-to-nia -flags < src.img > dst.nia\n"
    "\n"
    "Flags:\n"
    "    -1      -first-frame-only\n"
    "            -fail-if-unsandboxed\n"
    "\n"
    "convert-to-nia converts an image from stdin (e.g. in the BMP, GIF, JPEG\n"
    "or PNG format) to stdout (in the NIA format, or in the NIE format if\n"
    "the -first-frame-only flag is given).\n"
    "\n"
    "NIA/NIE is a trivial animated/still image file format, specified at\n"
    "https://github.com/google/wuffs/blob/main/doc/spec/nie-spec.md\n"
    "\n"
    "The -fail-if-unsandboxed flag causes the program to exit if it does not\n"
    "self-impose a sandbox. On Linux, it self-imposes a SECCOMP_MODE_STRICT\n"
    "sandbox, regardless of whether this flag was set.";

// ----

namespace {

bool g_sandboxed = false;

wuffs_base__pixel_buffer g_pixbuf = {};
wuffs_base__slice_u8 g_pixbuf_slice = {};
wuffs_base__slice_u8 g_pixbuf_backup_slice = {};
wuffs_base__io_buffer g_src = {};
wuffs_base__slice_u8 g_workbuf_slice = {};

wuffs_base__image_config g_image_config = {};
wuffs_base__frame_config g_frame_config = {};
int32_t g_fourcc = 0;
uint32_t g_width = 0;
uint32_t g_height = 0;
uint32_t g_num_animation_loops = 0;
uint64_t g_num_printed_frames = 0;

wuffs_base__image_decoder* g_image_decoder = nullptr;

// ----

constexpr uint32_t BYTES_PER_PIXEL = 4;
constexpr uint32_t MAX_DIMENSION = 65535;
constexpr size_t SRC_BUFFER_ARRAY_SIZE = 64 * 1024;
constexpr size_t WORKBUF_ARRAY_SIZE = 256 * 1024 * 1024;
constexpr size_t PIXBUF_ARRAY_SIZE = 256 * 1024 * 1024;

uint8_t* g_src_buffer_array = nullptr;
uint8_t* g_workbuf_array = nullptr;
uint8_t* g_pixbuf_array = nullptr;

// ----

struct {
  int remaining_argc;
  char** remaining_argv;

  bool fail_if_unsandboxed;
  bool first_frame_only;
} g_flags = {};

const char* parse_flags(int argc, char** argv) {
  int c = (argc > 0) ? 1 : 0;  // Skip argv[0], the program name.
  for (; c < argc; c++) {
    char* arg = argv[c];
    if (*arg++ != '-') {
      break;
    }

    // A double-dash "--foo" is equivalent to a single-dash "-foo". As special
    // cases, a bare "-" is not a flag (some programs may interpret it as
    // stdin) and a bare "--" means to stop parsing flags.
    if (*arg == '\x00') {
      break;
    } else if (*arg == '-') {
      arg++;
      if (*arg == '\x00') {
        c++;
        break;
      }
    }

    if (!std::strcmp(arg, "fail-if-unsandboxed")) {
      g_flags.fail_if_unsandboxed = true;
      continue;
    }
    if (!std::strcmp(arg, "1") || !std::strcmp(arg, "first-frame-only")) {
      g_flags.first_frame_only = true;
      continue;
    }

    return g_usage;
  }

  g_flags.remaining_argc = argc - c;
  g_flags.remaining_argv = argv + c;
  return nullptr;
}

// ----

void write_out(std::FILE* f, const void* ptr, size_t len) {
  std::fwrite(ptr, 1, len, f);
}

const char* read_more_src() {
  if (g_src.meta.closed) {
    return "main: unexpected end of file";
  }
  wuffs_base__io_buffer__compact(&g_src);
  size_t n = std::fread(g_src.data.ptr + g_src.meta.wi, 1, g_src.data.len - g_src.meta.wi, stdin);
  if (n > 0) {
    g_src.meta.wi += n;
  } else if (std::ferror(stdin)) {
    return std::strerror(errno);
  } else {
    g_src.meta.closed = true;
  }
  return nullptr;
}

const char* load_image_type() {
  g_fourcc = 0;
  while (true) {
    g_fourcc = wuffs_base__magic_number_guess_fourcc(
        wuffs_base__io_buffer__reader_slice(&g_src), g_src.meta.closed);
    if ((g_fourcc >= 0) ||
        (wuffs_base__io_buffer__reader_length(&g_src) == g_src.data.len)) {
      break;
    }
    TRY(read_more_src());
  }
  return nullptr;
}

// Upstream initializes one decoder of a static union and upcasts it; Wuffs'
// alloc_as functions allocate and initialize one (WUFFS_VERSION,
// WUFFS_INITIALIZE__DEFAULT_OPTIONS) and return the same upcast pointer.
const char* initialize_image_decoder() {
  std::free(g_image_decoder);
  g_image_decoder = nullptr;
  switch (g_fourcc) {
    case WUFFS_BASE__FOURCC__BMP:
      g_image_decoder = wuffs_bmp__decoder__alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__GIF:
      g_image_decoder = wuffs_gif__decoder__alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__NIE:
      g_image_decoder = wuffs_nie__decoder__alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__PNG:
      g_image_decoder = wuffs_png__decoder__alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__TGA:
      g_image_decoder = wuffs_tga__decoder__alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__WBMP:
      g_image_decoder = wuffs_wbmp__decoder__alloc_as__wuffs_base__image_decoder();
      break;
    default:
      return "main: unsupported file format";
  }
  return g_image_decoder ? nullptr : "main: out of memory";
}

const char* advance_for_redirect() {
  wuffs_base__io_buffer empty = wuffs_base__empty_io_buffer();
  wuffs_base__more_information minfo = wuffs_base__empty_more_information();
  wuffs_base__status status = wuffs_base__image_decoder__tell_me_more(
      g_image_decoder, &empty, &minfo, &g_src);
  if (status.repr != nullptr) {
    return wuffs_base__status__message(&status);
  } else if (minfo.flavor != WUFFS_BASE__MORE_INFORMATION__FLAVOR__IO_REDIRECT) {
    return "main: unsupported file format";
  }
  g_fourcc = (int32_t)(wuffs_base__more_information__io_redirect__fourcc(&minfo));
  if (g_fourcc <= 0) {
    return "main: unsupported file format";
  }

  // Advance g_src's reader_position to pos.
  uint64_t pos = wuffs_base__more_information__io_redirect__range(&minfo).min_incl;
  if (pos < wuffs_base__io_buffer__reader_position(&g_src)) {
    // Redirects must go forward.
    return "main: unsupported file format";
  }
  while (true) {
    uint64_t relative_pos = pos - wuffs_base__io_buffer__reader_position(&g_src);
    if (relative_pos <= (g_src.meta.wi - g_src.meta.ri)) {
      g_src.meta.ri += relative_pos;
      break;
    }
    g_src.meta.ri = g_src.meta.wi;
    TRY(read_more_src());
  }
  return nullptr;
}

const char* load_image_config() {
  bool redirected = false;
redirect:
  TRY(initialize_image_decoder());

  // Decode the wuffs_base__image_config.
  while (true) {
    wuffs_base__status status = wuffs_base__image_decoder__decode_image_config(
        g_image_decoder, &g_image_config, &g_src);
    if (status.repr == nullptr) {
      break;
    } else if (status.repr == wuffs_base__note__i_o_redirect) {
      if (redirected) {
        return "main: unsupported file format";
      }
      redirected = true;
      TRY(advance_for_redirect());
      goto redirect;
    } else if (status.repr != wuffs_base__suspension__short_read) {
      return wuffs_base__status__message(&status);
    }
    TRY(read_more_src());
  }

  // Read the dimensions.
  uint32_t w = wuffs_base__pixel_config__width(&g_image_config.pixcfg);
  uint32_t h = wuffs_base__pixel_config__height(&g_image_config.pixcfg);
  if ((w > MAX_DIMENSION) || (h > MAX_DIMENSION)) {
    return "main: image is too large";
  }
  g_width = w;
  g_height = h;

  // Override the image's native pixel format to be BGRA_NONPREMUL.
  wuffs_base__pixel_config__set(&g_image_config.pixcfg,
                                WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL,
                                WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, w, h);

  // Configure the work buffer.
  uint64_t workbuf_len = wuffs_base__image_decoder__workbuf_len(g_image_decoder).max_incl;
  if (workbuf_len > WORKBUF_ARRAY_SIZE) {
    return "main: image is too large (to configure work buffer)";
  }
  g_workbuf_slice.ptr = &g_workbuf_array[0];
  g_workbuf_slice.len = workbuf_len;

  // Configure the pixel buffer and (if there's capacity) its backup buffer.
  uint64_t num_pixels = ((uint64_t)w) * ((uint64_t)h);
  if (num_pixels > (PIXBUF_ARRAY_SIZE / BYTES_PER_PIXEL)) {
    return "main: image is too large (to configure pixel buffer)";
  }
  g_pixbuf_slice.ptr = &g_pixbuf_array[0];
  g_pixbuf_slice.len = num_pixels * BYTES_PER_PIXEL;
  size_t pixbuf_array_remaining = PIXBUF_ARRAY_SIZE - g_pixbuf_slice.len;
  if (pixbuf_array_remaining >= g_pixbuf_slice.len) {
    g_pixbuf_backup_slice.ptr = &g_pixbuf_array[g_pixbuf_slice.len];
    g_pixbuf_backup_slice.len = g_pixbuf_slice.len;
  }

  // Configure the wuffs_base__pixel_buffer struct.
  wuffs_base__status status = wuffs_base__pixel_buffer__set_from_slice(
      &g_pixbuf, &g_image_config.pixcfg, g_pixbuf_slice);
  TRY(wuffs_base__status__message(&status));

  wuffs_base__table_u8 tab = wuffs_base__pixel_buffer__plane(&g_pixbuf, 0);
  if ((tab.width != (g_width * BYTES_PER_PIXEL)) || (tab.height != g_height)) {
    return "main: inconsistent pixel buffer dimensions";
  }

  return nullptr;
}

void fill_rectangle(wuffs_base__rect_ie_u32 rect, wuffs_base__color_u32_argb_premul color) {
  if (rect.max_excl_x > g_width) {
    rect.max_excl_x = g_width;
  }
  if (rect.max_excl_y > g_height) {
    rect.max_excl_y = g_height;
  }
  uint32_t nonpremul = wuffs_base__color_u32_argb_premul__as__color_u32_argb_nonpremul(color);
  wuffs_base__table_u8 tab = wuffs_base__pixel_buffer__plane(&g_pixbuf, 0);

  for (uint32_t y = rect.min_incl_y; y < rect.max_excl_y; y++) {
    uint8_t* p = tab.ptr + (y * tab.stride) + (rect.min_incl_x * BYTES_PER_PIXEL);
    for (uint32_t x = rect.min_incl_x; x < rect.max_excl_x; x++) {
      wuffs_base__poke_u32le__no_bounds_check(p, nonpremul);
      p += BYTES_PER_PIXEL;
    }
  }
}

void print_nix_header(uint32_t magic_u32le) {
  static const uint32_t version1_bn4_u32le = 0x346E62FF;
  uint8_t data[16];
  wuffs_base__poke_u32le__no_bounds_check(data + 0x00, magic_u32le);
  wuffs_base__poke_u32le__no_bounds_check(data + 0x04, version1_bn4_u32le);
  wuffs_base__poke_u32le__no_bounds_check(data + 0x08, g_width);
  wuffs_base__poke_u32le__no_bounds_check(data + 0x0C, g_height);
  write_out(stdout, &data[0], 16);
}

void print_nia_duration(wuffs_base__flicks duration) {
  uint8_t data[8];
  wuffs_base__poke_u64le__no_bounds_check(data + 0x00, duration);
  write_out(stdout, &data[0], 8);
}

void print_nie_frame() {
  g_num_printed_frames++;
  print_nix_header(0x45AFC36E);  // "nïE" as a u32le.
  wuffs_base__table_u8 tab = wuffs_base__pixel_buffer__plane(&g_pixbuf, 0);
  if (tab.width == tab.stride) {
    write_out(stdout, tab.ptr, tab.width * tab.height);
  } else {
    for (size_t y = 0; y < tab.height; y++) {
      write_out(stdout, tab.ptr + (y * tab.stride), tab.width);
      break;  // upstream's
    }
  }
}

void print_nia_padding() {
  if (g_width & g_height & 1) {
    uint8_t data[4];
    wuffs_base__poke_u32le__no_bounds_check(data + 0x00, 0);
    write_out(stdout, &data[0], 4);
  }
}

void print_nia_footer() {
  // Still images' "number of animation loops" is canonicalized to 0 (see
  // upstream's comment: either 0 or 1 is valid, 0 makes outputs comparable).
  uint32_t n = g_num_animation_loops;
  if (g_num_printed_frames <= 1) {
    n = 0;
  }

  uint8_t data[8];
  wuffs_base__poke_u32le__no_bounds_check(data + 0x00, n);
  wuffs_base__poke_u32le__no_bounds_check(data + 0x04, 0x80000000);
  write_out(stdout, &data[0], 8);
}

const char* convert_frames() {
  wuffs_base__flicks total_duration = 0;
  while (true) {
    // Decode the wuffs_base__frame_config.
    while (true) {
      wuffs_base__status dfc_status = wuffs_base__image_decoder__decode_frame_config(
          g_image_decoder, &g_frame_config, &g_src);
      if (dfc_status.repr == nullptr) {
        break;
      } else if (dfc_status.repr == wuffs_base__note__end_of_data) {
        return nullptr;
      } else if (dfc_status.repr != wuffs_base__suspension__short_read) {
        return wuffs_base__status__message(&dfc_status);
      }
      TRY(read_more_src());
    }

    wuffs_base__flicks duration = wuffs_base__frame_config__duration(&g_frame_config);
    if (duration < 0) {
      return "main: animation frame duration is negative";
    } else if (total_duration > (INT64_MAX - duration)) {
      return "main: animation frame duration overflow";
    }
    total_duration += duration;

    if (wuffs_base__frame_config__index(&g_frame_config) == 0) {
      fill_rectangle(wuffs_base__pixel_config__bounds(&g_image_config.pixcfg),
                     wuffs_base__frame_config__background_color(&g_frame_config));
    }

    if (wuffs_base__frame_config__disposal(&g_frame_config) ==
        WUFFS_BASE__ANIMATION_DISPOSAL__RESTORE_PREVIOUS) {
      if (g_pixbuf_slice.len != g_pixbuf_backup_slice.len) {
        return "main: image is too large (to configure pixel backup buffer)";
      }
      std::memcpy(g_pixbuf_backup_slice.ptr, g_pixbuf_slice.ptr, g_pixbuf_slice.len);
    }

    // Decode the frame (the pixels).
    wuffs_base__status df_status;
    const char* decode_frame_io_error_message = nullptr;
    while (true) {
      df_status = wuffs_base__image_decoder__decode_frame(
          g_image_decoder, &g_pixbuf, &g_src,
          wuffs_base__frame_config__overwrite_instead_of_blend(&g_frame_config)
              ? WUFFS_BASE__PIXEL_BLEND__SRC
              : WUFFS_BASE__PIXEL_BLEND__SRC_OVER,
          g_workbuf_slice, nullptr);
      if (df_status.repr != wuffs_base__suspension__short_read) {
        break;
      }
      decode_frame_io_error_message = read_more_src();
      if (decode_frame_io_error_message != nullptr) {
        // Neuter the "short read" df_status so that convert_frames returns the
        // I/O error message instead.
        df_status.repr = nullptr;
        break;
      }
    }

    // Updated once per frame, as upstream does (to match Chromium).
    g_num_animation_loops = wuffs_base__image_decoder__num_animation_loops(g_image_decoder);

    // Print a complete NIE frame (and surrounding bytes, for NIA).
    if (!g_flags.first_frame_only) {
      print_nia_duration(total_duration);
    }
    print_nie_frame();
    if (!g_flags.first_frame_only) {
      print_nia_padding();
    }

    // Return early if there was an error decoding the frame.
    if (df_status.repr != nullptr) {
      return wuffs_base__status__message(&df_status);
    } else if (decode_frame_io_error_message != nullptr) {
      return decode_frame_io_error_message;
    } else if (g_flags.first_frame_only) {
      return nullptr;
    }

    // Dispose the frame.
    switch (wuffs_base__frame_config__disposal(&g_frame_config)) {
      case WUFFS_BASE__ANIMATION_DISPOSAL__RESTORE_BACKGROUND: {
        fill_rectangle(wuffs_base__frame_config__bounds(&g_frame_config),
                       wuffs_base__frame_config__background_color(&g_frame_config));
        break;
      }
      case WUFFS_BASE__ANIMATION_DISPOSAL__RESTORE_PREVIOUS: {
        if (g_pixbuf_slice.len != g_pixbuf_backup_slice.len) {
          return "main: image is too large (to configure pixel backup buffer)";
        }
        std::memcpy(g_pixbuf_slice.ptr, g_pixbuf_backup_slice.ptr, g_pixbuf_slice.len);
        break;
      }
    }
  }
  return "main: unreachable";
}

const char* main1(int argc, char** argv) {
  TRY(parse_flags(argc, argv));
  if (g_flags.remaining_argc > 0) {
    return "main: bad argument: use \"program < input\", not \"program input\"";
  } else if (g_flags.fail_if_unsandboxed && !g_sandboxed) {
    return "main: unsandboxed";
  }

  g_src_buffer_array = static_cast<uint8_t*>(std::calloc(SRC_BUFFER_ARRAY_SIZE, 1));
  g_workbuf_array = static_cast<uint8_t*>(std::calloc(WORKBUF_ARRAY_SIZE, 1));
  g_pixbuf_array = static_cast<uint8_t*>(std::calloc(PIXBUF_ARRAY_SIZE, 1));
  if (!g_src_buffer_array || !g_workbuf_array || !g_pixbuf_array) {
    return "main: out of memory";
  }
  g_src.data.ptr = g_src_buffer_array;
  g_src.data.len = SRC_BUFFER_ARRAY_SIZE;

  TRY(load_image_type());
  TRY(load_image_config());
  if (!g_flags.first_frame_only) {
    print_nix_header(0x41AFC36E);  // "nïA" as a u32le.
  }
  const char* ret = convert_frames();
  if (!g_flags.first_frame_only) {
    print_nia_footer();
  }
  return ret;
}

int compute_exit_code(const char* status_msg) {
  if (!status_msg) {
    return 0;
  }
  size_t n;
  if (status_msg == g_usage) {
    n = std::strlen(status_msg);
  } else {
    n = strnlen(status_msg, 2047);
    if (n >= 2047) {
      status_msg = "main: internal error: error message is too long";
      n = strnlen(status_msg, 2047);
    }
  }
  std::fflush(stdout);
  write_out(stderr, status_msg, n);
  write_out(stderr, "\n", 1);
  // 1 for foreseen errors (bad or unsupported input), 2 for internal ones.
  return std::strstr(status_msg, "internal error:") ? 2 : 1;
}

}  // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  int exit_code = compute_exit_code(main1(argc, argv));
  std::fflush(stdout);
  return exit_code;
}
