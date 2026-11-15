#include "recursos/pe_resources.h"

namespace lin {

namespace {

constexpr u32 RT_CURSOR = 1;

bool in_range(Bytes b, size_t off, size_t n) { return off <= b.size() && n <= b.size() - off; }
u32 le16(Bytes b, size_t o) { return static_cast<u32>(b[o] | (b[o + 1] << 8)); }
u32 le32(Bytes b, size_t o) { return le16(b, o) | (le16(b, o + 2) << 16); }

// RVA -> file offset through the section table; SIZE_MAX if no section contains it.
size_t rva_to_offset(Bytes pe, size_t sections, u32 count, u32 rva) {
    for (u32 i = 0; i < count; ++i) {
        const size_t s = sections + size_t{i} * 40;
        if (!in_range(pe, s, 40)) return SIZE_MAX;
        const u32 vsize = le32(pe, s + 8), va = le32(pe, s + 12), raw_size = le32(pe, s + 16),
                  raw = le32(pe, s + 20);
        const u32 span = vsize > raw_size ? vsize : raw_size;
        if (rva >= va && rva - va < span) return size_t{raw} + (rva - va);
    }
    return SIZE_MAX;
}

// Entry of a resource directory with a numeric id: returns the raw offset field (high bit = subdir).
bool dir_entry(Bytes pe, size_t dir, u32 id, u32& target) {
    if (!in_range(pe, dir, 16)) return false;
    const u32 named = le16(pe, dir + 12), ids = le16(pe, dir + 14);
    for (u32 i = named; i < named + ids; ++i) {
        const size_t e = dir + 16 + size_t{i} * 8;
        if (!in_range(pe, e, 8)) return false;
        if (le32(pe, e) == id) {
            target = le32(pe, e + 4);
            return true;
        }
    }
    return false;
}

// First entry of any kind (used for the language level).
bool first_entry(Bytes pe, size_t dir, u32& target) {
    if (!in_range(pe, dir, 24)) return false;
    if (le16(pe, dir + 12) + le16(pe, dir + 14) == 0) return false;
    target = le32(pe, dir + 20);
    return true;
}

}  // namespace

Bytes pe_find_resource(Bytes pe, u32 type_id, u32 res_id) {
    if (!in_range(pe, 0, 0x40) || pe[0] != 'M' || pe[1] != 'Z') return {};
    const size_t nt = le32(pe, 0x3C);
    if (!in_range(pe, nt, 24) || le32(pe, nt) != 0x00004550) return {};
    const u32 nsections = le16(pe, nt + 6);
    const size_t opt = nt + 24, opt_size = le16(pe, nt + 20);
    if (!in_range(pe, opt, opt_size) || opt_size < 2) return {};
    const u32 magic = le16(pe, opt);
    const size_t dirs = opt + (magic == 0x20B ? 112 : 96);  // PE32+ : PE32
    if (magic != 0x20B && magic != 0x10B) return {};
    if (!in_range(pe, dirs + 16, 8)) return {};
    const u32 rsrc_rva = le32(pe, dirs + 16);
    if (rsrc_rva == 0) return {};
    const size_t sections = opt + opt_size;
    const size_t root = rva_to_offset(pe, sections, nsections, rsrc_rva);
    if (root == SIZE_MAX) return {};

    u32 t = 0;
    if (!dir_entry(pe, root, type_id, t) || !(t & 0x80000000u)) return {};
    if (!dir_entry(pe, root + (t & 0x7FFFFFFFu), res_id, t) || !(t & 0x80000000u)) return {};
    if (!first_entry(pe, root + (t & 0x7FFFFFFFu), t) || (t & 0x80000000u)) return {};
    const size_t data_entry = root + t;
    if (!in_range(pe, data_entry, 16)) return {};
    const size_t off = rva_to_offset(pe, sections, nsections, le32(pe, data_entry));
    const size_t size = le32(pe, data_entry + 4);
    if (off == SIZE_MAX || !in_range(pe, off, size)) return {};
    return pe.subspan(off, size);
}

bool pe_load_cursor(Bytes pe, u32 res_id, PeCursor& out) {
    const Bytes r = pe_find_resource(pe, RT_CURSOR, res_id);
    if (r.size() < 4 + 40 + 8) return false;
    const int hot_x = static_cast<int>(le16(r, 0)), hot_y = static_cast<int>(le16(r, 2));
    const Bytes d = r.subspan(4);
    const u32 header = le32(d, 0);
    const int w = static_cast<int>(le32(d, 4)), h2 = static_cast<int>(le32(d, 8));  // height covers XOR + AND
    if (header != 40 || le16(d, 12) != 1 || le16(d, 14) != 1 || le32(d, 16) != 0) return false;  // 1 bpp, BI_RGB
    if (w <= 0 || w > 256 || h2 <= 0 || h2 > 512 || (h2 & 1)) return false;
    const int h = h2 / 2;
    const size_t stride = ((static_cast<size_t>(w) + 31) / 32) * 4;
    const size_t palette = 8, xor_at = header + palette, and_at = xor_at + stride * h;
    if (!in_range(d, 0, and_at + stride * h)) return false;
    // Palette entries are B,G,R,0; index -> black or white by brightness.
    PePixel color[2];
    for (int i = 0; i < 2; ++i) {
        const size_t p = header + size_t{static_cast<u32>(i)} * 4;
        color[i] = (d[p] + d[p + 1] + d[p + 2] >= 384) ? PePixel::WHITE : PePixel::BLACK;
    }
    out.width = w;
    out.height = h;
    out.hot_x = hot_x;
    out.hot_y = hot_y;
    out.pixels.assign(static_cast<size_t>(w) * h, PePixel::TRANSPARENT);
    for (int y = 0; y < h; ++y) {
        const size_t row = static_cast<size_t>(h - 1 - y);  // bottom-up
        for (int x = 0; x < w; ++x) {
            const int bit = 7 - (x & 7);
            const int xr = (d[xor_at + row * stride + x / 8] >> bit) & 1;
            const int an = (d[and_at + row * stride + x / 8] >> bit) & 1;
            PePixel p = color[xr];
            if (an) p = xr ? PePixel::INVERT : PePixel::TRANSPARENT;
            out.pixels[static_cast<size_t>(y) * w + x] = p;
        }
    }
    return true;
}

}  // namespace lin
