// The Wuffs API through the module alone (no Wuffs header in this file):
// every upstream package on data whose decoding is known — the image
// decoders (PNG, GIF, BMP, TGA, WBMP, NIE) by wuffs_aux::DecodeImage and by
// the C API with the adapted static inline helpers, the compression decoders
// (DEFLATE, zlib, gzip, bzip2) by transform_io, the hashers, JSON and CBOR by
// wuffs_aux, base's number, UTF-8 and base-16/64 functions — and the error
// paths: a wrong WUFFS_VERSION, a bad checksum and a truncated image return
// the very status-message objects the package exports.
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.wuffs;

using namespace mcpplibs::mwrap::wuffs;          // the static inline API, adapted
using namespace mcpplibs::mwrap::wuffs::macros;  // WUFFS_VERSION, WUFFS_BASE__FOURCC__PNG …

namespace {

constexpr std::uint8_t png_2x2[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a,
    0x73, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0xf8, 0xcf, 0xc0, 0xc0,
    0x00, 0xc2, 0x0c, 0xff, 0x81, 0x00, 0x00, 0x1f, 0xee, 0x05, 0xfb, 0xf1, 0xab, 0xba, 0x77, 0x00,
    0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};
constexpr std::uint8_t bmp_2x1[] = {
    0x42, 0x4d, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x13, 0x0b, 0x00, 0x00, 0x13, 0x0b, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00
};
constexpr std::uint8_t tga_2x1[] = {
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00,
    0x18, 0x20, 0x00, 0xff, 0x00, 0xff, 0xff, 0xff
};
constexpr std::uint8_t wbmp_8x1[] = {
    0x00, 0x00, 0x08, 0x01, 0xa0
};
constexpr std::uint8_t nie_1x1[] = {
    0x6e, 0xc3, 0xaf, 0x45, 0xff, 0x62, 0x6e, 0x34, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x0a, 0x14, 0x1e, 0xff
};
constexpr std::uint8_t gif_1x1[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44,
    0x01, 0x00, 0x3b
};
constexpr std::uint8_t deflate_text[] = {
    0x0b, 0x2f, 0x4d, 0x4b, 0x2b, 0x56, 0x48, 0x49, 0x4d, 0xce, 0x4f, 0x49, 0x2d, 0x56, 0x28, 0xcd,
    0x2b, 0x29, 0x2a, 0x2d, 0x2e, 0x49, 0x4d, 0x51, 0x48, 0x49, 0x2c, 0x49, 0xd4, 0xe3, 0x0a, 0xa7,
    0x99, 0x2c, 0x00
};
constexpr std::uint8_t zlib_text[] = {
    0x78, 0xda, 0x0b, 0x2f, 0x4d, 0x4b, 0x2b, 0x56, 0x48, 0x49, 0x4d, 0xce, 0x4f, 0x49, 0x2d, 0x56,
    0x28, 0xcd, 0x2b, 0x29, 0x2a, 0x2d, 0x2e, 0x49, 0x4d, 0x51, 0x48, 0x49, 0x2c, 0x49, 0xd4, 0xe3,
    0x0a, 0xa7, 0x99, 0x2c, 0x00, 0x85, 0x66, 0x2c, 0x09
};
constexpr std::uint8_t zlib_text_bad_checksum[] = {
    0x78, 0xda, 0x0b, 0x2f, 0x4d, 0x4b, 0x2b, 0x56, 0x48, 0x49, 0x4d, 0xce, 0x4f, 0x49, 0x2d, 0x56,
    0x28, 0xcd, 0x2b, 0x29, 0x2a, 0x2d, 0x2e, 0x49, 0x4d, 0x51, 0x48, 0x49, 0x2c, 0x49, 0xd4, 0xe3,
    0x0a, 0xa7, 0x99, 0x2c, 0x00, 0x85, 0x66, 0x2c, 0xf6
};
constexpr std::uint8_t gzip_text[] = {
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x0b, 0x2f, 0x4d, 0x4b, 0x2b, 0x56,
    0x48, 0x49, 0x4d, 0xce, 0x4f, 0x49, 0x2d, 0x56, 0x28, 0xcd, 0x2b, 0x29, 0x2a, 0x2d, 0x2e, 0x49,
    0x4d, 0x51, 0x48, 0x49, 0x2c, 0x49, 0xd4, 0xe3, 0x0a, 0xa7, 0x99, 0x2c, 0x00, 0x67, 0x4e, 0x26,
    0x2d, 0x78, 0x00, 0x00, 0x00
};
constexpr std::uint8_t bzip2_text[] = {
    0x42, 0x5a, 0x68, 0x39, 0x31, 0x41, 0x59, 0x26, 0x53, 0x59, 0x92, 0x54, 0x60, 0x13, 0x00, 0x00,
    0x0b, 0xd3, 0x80, 0x00, 0x10, 0x40, 0x01, 0x00, 0x80, 0x2f, 0x01, 0x9e, 0x00, 0x20, 0x00, 0x50,
    0xa6, 0x00, 0x00, 0x2a, 0xa9, 0xa6, 0xd4, 0xc9, 0xa6, 0xc9, 0x3b, 0x30, 0x48, 0xc1, 0x05, 0x10,
    0x68, 0x64, 0x49, 0x72, 0x0b, 0x1c, 0x24, 0xb1, 0x24, 0x17, 0x32, 0x41, 0xa3, 0xe2, 0x8a, 0x36,
    0x70, 0xfc, 0x5d, 0xc9, 0x14, 0xe1, 0x42, 0x42, 0x49, 0x51, 0x80, 0x4c
};
constexpr std::uint8_t cbor_doc[] = {
    0xa2, 0x61, 0x61, 0x01, 0x61, 0x62, 0x82, 0xf5, 0xf6
};
const std::string text = [] {
    std::string s;
    for (int i = 0; i < 4; ++i) s += "Wuffs decodes untrusted data.\n";
    return s;
}();

wuffs_base__slice_u8 slice(const std::uint8_t* p, std::size_t n) {
    return wuffs_base__make_slice_u8(const_cast<std::uint8_t*>(p), n);
}

// Pixels as BGRA (premultiplied, Wuffs' default for DecodeImage).
struct Image {
    std::string error;
    std::uint32_t width = 0, height = 0;
    std::vector<std::array<std::uint8_t, 4>> pixels;
};

Image decode(const std::uint8_t* p, std::size_t n) {
    wuffs_aux::DecodeImageCallbacks callbacks;
    wuffs_aux::sync_io::MemoryInput input(p, n);
    wuffs_aux::DecodeImageResult res = wuffs_aux::DecodeImage(callbacks, input);
    Image img;
    img.error = res.error_message;
    if (!img.error.empty()) return img;
    img.width = res.pixbuf.pixcfg.width();
    img.height = res.pixbuf.pixcfg.height();
    wuffs_base__table_u8 plane = res.pixbuf.plane(0);
    for (std::uint32_t y = 0; y < img.height; ++y)
        for (std::uint32_t x = 0; x < img.width; ++x) {
            const std::uint8_t* q = plane.ptr + y * plane.stride + x * 4;
            img.pixels.push_back({q[0], q[1], q[2], q[3]});
        }
    return img;
}

using BGRA = std::array<std::uint8_t, 4>;
constexpr BGRA red{0, 0, 255, 255}, green{0, 255, 0, 255}, blue{255, 0, 0, 255};
constexpr BGRA white{255, 255, 255, 255}, black{0, 0, 0, 255};

// One transform_io pass over a whole input into a large enough output.
template <typename Decoder>
std::pair<std::string, const char*> transform(Decoder& dec, const std::uint8_t* p, std::size_t n) {
    std::vector<std::uint8_t> out(4096);
    std::vector<std::uint8_t> work(dec.workbuf_len().max_incl + 1);
    wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(const_cast<std::uint8_t*>(p), n, true);
    wuffs_base__io_buffer dst = wuffs_base__ptr_u8__writer(out.data(), out.size());
    wuffs_base__status status = dec.transform_io(&dst, &src, wuffs_base__make_slice_u8(work.data(), work.size() - 1));
    return {std::string(reinterpret_cast<const char*>(out.data()), dst.meta.wi), status.repr};
}

}  // namespace

// ── release identity ─────────────────────────────────────────────────────

TEST(Release, Version) {
    EXPECT_EQ(WUFFS_VERSION, 0x000030006ull);
    EXPECT_EQ(WUFFS_VERSION_MAJOR, 0);
    EXPECT_EQ(WUFFS_VERSION_MINOR, 3);
    EXPECT_EQ(WUFFS_VERSION_PATCH, 6);
    EXPECT_EQ(std::string_view(WUFFS_VERSION_STRING), "0.3.6+3407.20261020");
}

// ── images ───────────────────────────────────────────────────────────────

TEST(Images, Png) {
    Image img = decode(png_2x2, sizeof png_2x2);
    ASSERT_EQ(img.error, "");
    EXPECT_EQ(img.width, 2u);
    EXPECT_EQ(img.height, 2u);
    EXPECT_EQ(img.pixels, (std::vector<BGRA>{red, green, blue, white}));
}

TEST(Images, Gif) {
    Image img = decode(gif_1x1, sizeof gif_1x1);
    ASSERT_EQ(img.error, "");
    EXPECT_EQ(img.pixels, (std::vector<BGRA>{red}));
}

TEST(Images, Bmp) {
    Image img = decode(bmp_2x1, sizeof bmp_2x1);
    ASSERT_EQ(img.error, "");
    EXPECT_EQ(img.pixels, (std::vector<BGRA>{blue, red}));
}

TEST(Images, Tga) {
    Image img = decode(tga_2x1, sizeof tga_2x1);
    ASSERT_EQ(img.error, "");
    EXPECT_EQ(img.pixels, (std::vector<BGRA>{green, white}));
}

TEST(Images, Wbmp) {
    Image img = decode(wbmp_8x1, sizeof wbmp_8x1);
    ASSERT_EQ(img.error, "");
    ASSERT_EQ(img.width, 8u);
    EXPECT_EQ(img.pixels, (std::vector<BGRA>{white, black, white, black, black, black, black, black}));
}

TEST(Images, Nie) {
    Image img = decode(nie_1x1, sizeof nie_1x1);
    ASSERT_EQ(img.error, "");
    EXPECT_EQ(img.pixels, (std::vector<BGRA>{BGRA{10, 20, 30, 255}}));
}

TEST(Images, TruncatedIsRefused) {
    Image img = decode(png_2x2, 40);
    EXPECT_EQ(img.error, "png: truncated input");
}

// The C API, as upstream's examples use it: the decoder by its alloc
// function, configuration and frame through the image_decoder interface.
TEST(Images, PngThroughTheCApi) {
    std::unique_ptr<wuffs_base__image_decoder, decltype(&std::free)> dec(
        wuffs_png__decoder__alloc_as__wuffs_base__image_decoder(), &std::free);
    ASSERT_NE(dec, nullptr);
    EXPECT_EQ(wuffs_base__magic_number_guess_fourcc(slice(png_2x2, sizeof png_2x2), true), WUFFS_BASE__FOURCC__PNG);

    wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(const_cast<std::uint8_t*>(png_2x2), sizeof png_2x2, true);
    wuffs_base__image_config ic{};
    wuffs_base__status status = wuffs_base__image_decoder__decode_image_config(dec.get(), &ic, &src);
    ASSERT_EQ(status.repr, nullptr) << wuffs_base__status__message(&status);
    EXPECT_EQ(wuffs_base__pixel_config__width(&ic.pixcfg), 2u);
    EXPECT_EQ(wuffs_base__pixel_config__height(&ic.pixcfg), 2u);

    wuffs_base__pixel_config__set(&ic.pixcfg, WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL,
                                  WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, 2, 2);
    wuffs_base__pixel_format format = wuffs_base__pixel_config__pixel_format(&ic.pixcfg);
    EXPECT_EQ(wuffs_base__pixel_format__bits_per_pixel(&format), 32u);
    std::array<std::uint8_t, 16> pixels{};
    wuffs_base__pixel_buffer pb{};
    status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg, slice(pixels.data(), pixels.size()));
    ASSERT_EQ(status.repr, nullptr);
    std::vector<std::uint8_t> work(wuffs_base__image_decoder__workbuf_len(dec.get()).max_incl + 1);
    status = wuffs_base__image_decoder__decode_frame(dec.get(), &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC,
                                                     slice(work.data(), work.size() - 1), nullptr);
    ASSERT_EQ(status.repr, nullptr) << wuffs_base__status__message(&status);
    EXPECT_EQ((std::array<std::uint8_t, 4>{pixels[12], pixels[13], pixels[14], pixels[15]}), white);
    EXPECT_EQ((std::array<std::uint8_t, 4>{pixels[0], pixels[1], pixels[2], pixels[3]}), red);
}

// Wuffs accepts a caller built against the same major version and a minor
// version no newer than the library's; a newer minor is refused.
TEST(Images, WrongVersionIsRefused) {
    std::unique_ptr<wuffs_png__decoder, decltype(&std::free)> dec(
        static_cast<wuffs_png__decoder*>(std::malloc(sizeof__wuffs_png__decoder())), &std::free);
    wuffs_base__status status =
        wuffs_png__decoder__initialize(dec.get(), sizeof__wuffs_png__decoder(), WUFFS_VERSION + (1ull << 16), 0);
    EXPECT_EQ(status.repr, wuffs_base__error__bad_wuffs_version);  // the exported object itself
    EXPECT_STREQ(wuffs_base__status__message(&status), "base: bad wuffs version");
}

// ── compression ──────────────────────────────────────────────────────────

TEST(Compression, Deflate) {
    auto dec = wuffs_deflate__decoder::alloc();
    auto [out, status] = transform(*dec, deflate_text, sizeof deflate_text);
    EXPECT_EQ(status, nullptr);
    EXPECT_EQ(out, text);
}

TEST(Compression, Zlib) {
    auto dec = wuffs_zlib__decoder::alloc();
    auto [out, status] = transform(*dec, zlib_text, sizeof zlib_text);
    EXPECT_EQ(status, nullptr);
    EXPECT_EQ(out, text);
}

TEST(Compression, ZlibBadChecksum) {
    auto dec = wuffs_zlib__decoder::alloc();
    auto [out, status] = transform(*dec, zlib_text_bad_checksum, sizeof zlib_text_bad_checksum);
    EXPECT_EQ(status, wuffs_zlib__error__bad_checksum);
}

TEST(Compression, Gzip) {
    auto dec = wuffs_gzip__decoder::alloc();
    auto [out, status] = transform(*dec, gzip_text, sizeof gzip_text);
    EXPECT_EQ(status, nullptr);
    EXPECT_EQ(out, text);
}

TEST(Compression, Bzip2) {
    auto dec = wuffs_bzip2__decoder::alloc();
    auto [out, status] = transform(*dec, bzip2_text, sizeof bzip2_text);
    EXPECT_EQ(status, nullptr);
    EXPECT_EQ(out, text);
}

TEST(Compression, LzwConfigures) {
    auto dec = wuffs_lzw__decoder::alloc();
    ASSERT_NE(dec, nullptr);
    dec->set_literal_width(8);
    EXPECT_EQ(wuffs_lzw__decoder__workbuf_len(dec.get()).max_incl, 0u);
}

// ── hashes ───────────────────────────────────────────────────────────────

TEST(Hashes, Crc32AndAdler32) {
    const std::string nine = "123456789", wiki = "Wikipedia";
    auto crc = wuffs_crc32__ieee_hasher::alloc();
    EXPECT_EQ(crc->update_u32(slice(reinterpret_cast<const std::uint8_t*>(nine.data()), nine.size())), 0xCBF43926u);
    auto adler = wuffs_adler32__hasher::alloc();
    EXPECT_EQ(wuffs_adler32__hasher__update_u32(adler.get(), slice(reinterpret_cast<const std::uint8_t*>(wiki.data()), wiki.size())),
              0x11E60398u);
}

// ── JSON and CBOR (wuffs_aux) ────────────────────────────────────────────

namespace {
struct JsonWalk : wuffs_aux::DecodeJsonCallbacks {
    std::string seen;
    std::string AppendNull() override { seen += "null "; return ""; }
    std::string AppendBool(bool v) override { seen += v ? "true " : "false "; return ""; }
    std::string AppendF64(double v) override { seen += std::format("{} ", v); return ""; }
    std::string AppendI64(std::int64_t v) override { seen += std::format("{} ", v); return ""; }
    std::string AppendTextString(std::string&& v) override { seen += '"' + v + "\" "; return ""; }
    std::string Push(std::uint32_t) override { seen += "[ "; return ""; }
    std::string Pop(std::uint32_t) override { seen += "] "; return ""; }
};

struct CborWalk : wuffs_aux::DecodeCborCallbacks {
    std::string seen;
    std::string AppendNull() override { seen += "null "; return ""; }
    std::string AppendUndefined() override { seen += "undefined "; return ""; }
    std::string AppendBool(bool v) override { seen += v ? "true " : "false "; return ""; }
    std::string AppendF64(double v) override { seen += std::format("{} ", v); return ""; }
    std::string AppendI64(std::int64_t v) override { seen += std::format("{} ", v); return ""; }
    std::string AppendU64(std::uint64_t v) override { seen += std::format("{} ", v); return ""; }
    std::string AppendByteString(std::string&&) override { seen += "bytes "; return ""; }
    std::string AppendTextString(std::string&& v) override { seen += '"' + v + "\" "; return ""; }
    std::string AppendMinus1MinusX(std::uint64_t v) override { seen += std::format("-1-{} ", v); return ""; }
    std::string AppendCborSimpleValue(std::uint8_t v) override { seen += std::format("simple{} ", v); return ""; }
    std::string AppendCborTag(std::uint64_t v) override { seen += std::format("tag{} ", v); return ""; }
    std::string Push(std::uint32_t) override { seen += "[ "; return ""; }
    std::string Pop(std::uint32_t) override { seen += "] "; return ""; }
};
}  // namespace

TEST(Formats, Json) {
    JsonWalk walk;
    const std::string doc = R"({"format":"png","sizes":[16,32.5],"alpha":true,"note":null})";
    wuffs_aux::sync_io::MemoryInput input(doc.data(), doc.size());
    wuffs_aux::DecodeJsonResult res = wuffs_aux::DecodeJson(walk, input);
    EXPECT_EQ(res.error_message, "");
    EXPECT_EQ(walk.seen, "[ \"format\" \"png\" \"sizes\" [ 16 32.5 ] \"alpha\" true \"note\" null ] ");
}

TEST(Formats, JsonPointerAndError) {
    JsonWalk walk;
    const std::string doc = R"({"a":{"b":[7,8]}})";
    wuffs_aux::sync_io::MemoryInput input(doc.data(), doc.size());
    auto res = wuffs_aux::DecodeJson(walk, input, wuffs_aux::DecodeJsonArgQuirks::DefaultValue(),
                                     wuffs_aux::DecodeJsonArgJsonPointer("/a/b/1"));
    EXPECT_EQ(res.error_message, "");
    EXPECT_EQ(walk.seen, "8 ");

    JsonWalk bad;
    const std::string broken = "[1,2";
    wuffs_aux::sync_io::MemoryInput input2(broken.data(), broken.size());
    EXPECT_NE(wuffs_aux::DecodeJson(bad, input2).error_message, "");
}

TEST(Formats, Cbor) {
    CborWalk walk;
    wuffs_aux::sync_io::MemoryInput input(cbor_doc, sizeof cbor_doc);
    wuffs_aux::DecodeCborResult res = wuffs_aux::DecodeCbor(walk, input);
    EXPECT_EQ(res.error_message, "");
    EXPECT_EQ(walk.seen, "[ \"a\" 1 \"b\" [ true null ] ] ");
}

// ── base ─────────────────────────────────────────────────────────────────

TEST(Base, Numbers) {
    const std::string i = "-12345", f = "6.25e2";
    wuffs_base__result_i64 ri = wuffs_base__parse_number_i64(slice(reinterpret_cast<const std::uint8_t*>(i.data()), i.size()),
                                                             WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
    EXPECT_EQ(ri.status.repr, nullptr);
    EXPECT_EQ(ri.value, -12345);
    wuffs_base__result_f64 rf = wuffs_base__parse_number_f64(slice(reinterpret_cast<const std::uint8_t*>(f.data()), f.size()),
                                                             WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
    EXPECT_EQ(rf.value, 625.0);
    std::array<std::uint8_t, 32> buf{};
    std::size_t n = wuffs_base__render_number_i64(slice(buf.data(), buf.size()), -42, WUFFS_BASE__RENDER_NUMBER_XXX__DEFAULT_OPTIONS);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(buf.data()), n), "-42");
}

TEST(Base, Utf8) {
    std::array<std::uint8_t, 4> buf{};
    std::size_t n = wuffs_base__utf_8__encode(slice(buf.data(), buf.size()), 0x1F600);
    ASSERT_EQ(n, 4u);
    wuffs_base__utf_8__next__output next = wuffs_base__utf_8__next(buf.data(), n);
    EXPECT_TRUE(next.is_valid());
    EXPECT_EQ(next.code_point, 0x1F600u);
    EXPECT_EQ(next.byte_length, 4u);
}

TEST(Base, Base64) {
    const std::string in = "Wuffs";
    std::array<std::uint8_t, 32> out{};
    wuffs_base__transform__output o = wuffs_base__base_64__encode(
        slice(out.data(), out.size()), slice(reinterpret_cast<const std::uint8_t*>(in.data()), in.size()), true,
        WUFFS_BASE__BASE_64__DEFAULT_OPTIONS);
    EXPECT_EQ(o.status.repr, nullptr);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(out.data()), o.num_dst), "V3VmZnM");  // no padding by default
    o = wuffs_base__base_64__encode(slice(out.data(), out.size()), slice(reinterpret_cast<const std::uint8_t*>(in.data()), in.size()),
                                    true, WUFFS_BASE__BASE_64__ENCODE_EMIT_PADDING);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char*>(out.data()), o.num_dst), "V3VmZnM=");
}

TEST(Base, StatusAndSlices) {
    wuffs_base__status ok = wuffs_base__make_status(nullptr);
    EXPECT_TRUE(ok.is_ok());
    wuffs_base__status err = wuffs_base__make_status(wuffs_base__error__bad_argument);
    EXPECT_TRUE(err.is_error());
    EXPECT_STREQ(err.message(), "base: bad argument");

    std::array<std::uint8_t, 6> bytes{1, 2, 3, 4, 5, 6};
    wuffs_base__slice_u8 s = slice(bytes.data(), bytes.size());
    wuffs_base__slice_u8 mid = wuffs_base__slice_u8__subslice_ij(s, 2, 5);
    EXPECT_EQ(mid.len, 3u);
    EXPECT_EQ(mid.ptr[0], 3);
    EXPECT_EQ(wuffs_base__peek_u32le__no_bounds_check(bytes.data()), 0x04030201u);
    EXPECT_EQ(wuffs_base__load_u32le__no_bounds_check(bytes.data()), 0x04030201u);  // the macro alias
}
