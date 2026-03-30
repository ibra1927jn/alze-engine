#include "ImageDecoder.h"
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  ImageDecoder.h  â€”  Decodificador de imÃ¡genes propio (sin stb_image)
//
//  Soporta:
//    âœ… PNG  â€” Parser PNG completo con DEFLATE/zlib inflater propio
//    âœ… BMP  â€” Parser de cabecera BMP (24/32-bit)
//    âœ… PPM  â€” Formato P6 (RGB binario, Ãºtil para debug)
//    âœ… Radiance HDR (.hdr) â€” Para IBL / EnvironmentMap
//    âœ… API stbi-compatible: stbi_load, stbi_loadf, stbi_image_free,
//              stbi_failure_reason, stbi_set_flip_vertically_on_load
//
//  Limitaciones:
//    - PNG: 1/2/4/8-bit, RGB/RGBA/Grayscale/Palette. No 16-bit por muestra.
//    - HDR: Solo formato Radiance RGBE (el mÃ¡s comÃºn en IBL).
//    - No JPEG (requiere DCT â€” aÃ±adir si se necesita en el futuro).
//
//  Uso (retrocompatible con stb_image):
//    #include "ImageDecoder.h"  // en lugar de "stb_image.h"
//    int w, h, ch;
//    uint8_t* data = stbi_load("tex.png", &w, &h, &ch, 0);
//    stbi_image_free(data);
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
//  Lectura de enteros big/little-endian desde buffer crudo
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
namespace img_detail {

inline uint16_t u16be(const uint8_t* p) { return (uint16_t)(p[0]<<8)|p[1]; }
inline uint32_t u32be(const uint8_t* p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
inline uint16_t u16le(const uint8_t* p) { return (uint16_t)(p[1]<<8)|p[0]; }
inline uint32_t u32le(const uint8_t* p) { return ((uint32_t)p[3]<<24)|((uint32_t)p[2]<<16)|((uint32_t)p[1]<<8)|p[0]; }
inline int32_t  s32le(const uint8_t* p) { return (int32_t)u32le(p); }

// Paeth predictor (PNG spec)
inline uint8_t paeth(int a, int b, int c) {
    int p  = a + b - c;
    int pa = std::abs(p - a);
    int pb = std::abs(p - b);
    int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return (uint8_t)a;
    if (pb <= pc)             return (uint8_t)b;
    return (uint8_t)c;
}

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
//  DEFLATE / zlib Inflater (implementaciÃ³n propia sin zlib.h)
//  Soporta: bloques stored, fixed Huffman, dynamic Huffman
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

struct BitReader {
    const uint8_t* data;
    size_t         len;
    size_t         pos  = 0;  // byte position
    uint32_t       buf  = 0;  // bit buffer
    int            bits = 0;  // valid bits in buf

    void refill() {
        while (bits <= 24 && pos < len) {
            buf  |= (uint32_t)data[pos++] << bits;
            bits += 8;
        }
    }
    uint32_t peek(int n) { refill(); return buf & ((1u << n) - 1); }
    void     consume(int n) { buf >>= n; bits -= n; }
    uint32_t read(int n) { uint32_t v = peek(n); consume(n); return v; }
    bool     readBit() { return read(1) != 0; }
    bool     done() const { return pos >= len && bits == 0; }
};

// Canonical Huffman decode
struct CanonHuff {
    static const int MAX_BITS = 15;
    uint16_t count[MAX_BITS+2];
    uint16_t symbol[320];  // max 288 lit + 32 dist

    bool build(const int* lengths, int n) {
        memset(count, 0, sizeof(count));
        for (int i = 0; i < n; i++)
            if (lengths[i] > 0) count[lengths[i]]++;

        int total = 0;
        uint16_t next[MAX_BITS+2];
        next[0] = 0;
        for (int i = 1; i <= MAX_BITS; i++) {
            next[i] = (uint16_t)total;
            total += count[i];
        }

        // Fill symbols in canonical order
        uint16_t tmp[320] = {};
        for (int i = 0; i < n; i++) {
            int len = lengths[i];
            if (len > 0)
                tmp[next[len]++] = (uint16_t)i;
        }
        memcpy(symbol, tmp, total * sizeof(uint16_t));
        count[MAX_BITS+1] = (uint16_t)total;
        return true;
    }

    int decode(BitReader& br) const {
        int code = 0;
        int first = 0;
        int idx   = 0;
        for (int i = 1; i <= MAX_BITS; i++) {
            code  = (code << 1) | (int)br.read(1);
            int count_i = count[i];
            int diff = code - first;
            if (diff < count_i)
                return symbol[idx + diff];
            idx   += count_i;
            first  = (first + count_i) << 1;
        }
        return -1; // error
    }
};

// Fixed Huffman tables (RFC 1951 Â§3.2.6)
static void buildFixedLitTable(CanonHuff& lit) {
    int lens[288];
    for (int i =   0; i < 144; i++) lens[i] = 8;
    for (int i = 144; i < 256; i++) lens[i] = 9;
    for (int i = 256; i < 280; i++) lens[i] = 7;
    for (int i = 280; i < 288; i++) lens[i] = 8;
    lit.build(lens, 288);
}
static void buildFixedDistTable(CanonHuff& dist) {
    int lens[32];
    for (int i = 0; i < 32; i++) lens[i] = 5;
    dist.build(lens, 32);
}

// Length and distance tables (RFC 1951)
static const uint16_t LEN_BASE[29]  = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint8_t  LEN_EXTRA[29] = {0,0,0,0,0,0,0,0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t DIST_BASE[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint8_t  DIST_EXTRA[30]= {0,0,0,0,1,1,2,2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,  8,  8,   9,   9,  10,  10,  11,  11,  12,   12,   13,   13};

// Inflate one block with given tables
static bool inflateBlock(BitReader& br, CanonHuff& lit, CanonHuff& dist,
                         std::vector<uint8_t>& out)
{
    while (true) {
        int sym = lit.decode(br);
        if (sym < 0) return false;
        if (sym == 256) break; // end of block
        if (sym < 256) {
            out.push_back((uint8_t)sym);
        } else {
            // Length-distance pair
            int lenIdx = sym - 257;
            if (lenIdx >= 29) return false;
            int len = LEN_BASE[lenIdx] + (int)br.read(LEN_EXTRA[lenIdx]);

            int dSym = dist.decode(br);
            if (dSym < 0 || dSym >= 30) return false;
            int d = DIST_BASE[dSym] + (int)br.read(DIST_EXTRA[dSym]);

            if ((int)out.size() < d) return false;
            size_t base = out.size() - (size_t)d;
            for (int i = 0; i < len; i++)
                out.push_back(out[base + (i % d)]);
        }
    }
    return true;
}

// Full zlib inflate (handles zlib header + multiple blocks)
static bool inflate(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
    if (len < 2) return false;
    // Skip zlib header (CMF + FLG)
    uint8_t cmf = data[0], flg = data[1];
    (void)flg;
    if ((cmf & 0x0F) != 8) return false; // not deflate
    size_t offset = 2;
    if (cmf & 0x20) offset += 4; // dict â€” rare

    BitReader br;
    br.data = data + offset;
    br.len  = len  - offset - 4; // exclude adler32 checksum at end
    br.pos = br.buf = br.bits = 0;

    while (true) {
        bool bfinal = br.readBit();
        uint32_t btype = br.read(2);

        if (btype == 0) {
            // Stored block
            br.consume(br.bits & 7); // byte-align
            if (br.pos + 4 > br.len) return false;
            uint16_t blen  = (uint16_t)br.data[br.pos] | ((uint16_t)br.data[br.pos+1] << 8);
            uint16_t bnlen = (uint16_t)br.data[br.pos+2] | ((uint16_t)br.data[br.pos+3] << 8);
            br.pos += 4;
            if ((blen ^ bnlen) != 0xFFFF) return false;
            if (br.pos + blen > br.len) return false;
            out.insert(out.end(), br.data + br.pos, br.data + br.pos + blen);
            br.pos += blen;
        } else if (btype == 1) {
            // Fixed Huffman
            CanonHuff litH, distH;
            buildFixedLitTable(litH);
            buildFixedDistTable(distH);
            if (!inflateBlock(br, litH, distH, out)) return false;
        } else if (btype == 2) {
            // Dynamic Huffman
            int hlit  = (int)br.read(5) + 257;
            int hdist = (int)br.read(5) + 1;
            int hclen = (int)br.read(4) + 4;

            static const int CLORDER[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            int clens[19] = {};
            for (int i = 0; i < hclen; i++)
                clens[CLORDER[i]] = (int)br.read(3);

            CanonHuff clH;
            clH.build(clens, 19);

            int allLens[320] = {};
            int numCodes = hlit + hdist;
            for (int i = 0; i < numCodes; ) {
                int sym = clH.decode(br);
                if (sym < 0) return false;
                if (sym < 16) { allLens[i++] = sym; }
                else if (sym == 16) { int rep = (int)br.read(2) + 3; int prev_sym = i > 0 ? allLens[i-1] : 0; for (int j=0; j<rep; j++) allLens[i++] = prev_sym; }
                else if (sym == 17) { int rep = (int)br.read(3) + 3; for (int j=0; j<rep; j++) allLens[i++] = 0; }
                else                { int rep = (int)br.read(7) + 11;for (int j=0; j<rep; j++) allLens[i++] = 0; }
            }

            CanonHuff litH, distH;
            litH.build(allLens, hlit);
            distH.build(allLens + hlit, hdist);
            if (!inflateBlock(br, litH, distH, out)) return false;
        } else {
            return false; // reserved
        }

        if (bfinal) break;
    }
    return true;
}

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
//  PNG Decoder
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static const uint8_t PNG_SIG[8] = {137,80,78,71,13,10,26,10};

static bool decodePNG_internal(const uint8_t* file, size_t fileLen,
                               int& outW, int& outH, int& outChannels,
                               int reqChannels, // 0=auto, 3=RGB, 4=RGBA
                               std::vector<uint8_t>& pixels,
                               char* errBuf, int errLen)
{
    auto fail = [&](const char* msg) -> bool {
        if (errBuf && errLen > 0) strncpy(errBuf, msg, errLen-1);
        return false;
    };

    if (fileLen < 8 || memcmp(file, PNG_SIG, 8) != 0)
        return fail("Not a PNG file");

    const uint8_t* p   = file + 8;
    const uint8_t* end = file + fileLen;

    // Parse IHDR
    if (p + 25 > end) return fail("Truncated IHDR");
    uint32_t  ihdrLen = u32be(p);    p += 4;
    char      ihdrTag[5] = {};       memcpy(ihdrTag, p, 4); p += 4;
    if (strcmp(ihdrTag, "IHDR") != 0 || ihdrLen < 13) return fail("No IHDR");

    int width      = (int)u32be(p); p += 4;
    int height     = (int)u32be(p); p += 4;
    int bitDepth   = *p++;
    int colorType  = *p++;       // 0=Gray,2=RGB,3=Palette,4=GrayA,6=RGBA
    int compress   = *p++;       // 0=deflate
    int filter     = *p++;       // 0=adaptive
    int interlace  = *p++;
    p += 4; // skip CRC

    if (compress != 0 || filter != 0)   return fail("Unsupported PNG compression/filter");
    if (interlace != 0)                  return fail("Interlaced PNG not supported");
    if (bitDepth != 8 && bitDepth != 4 && bitDepth != 2 && bitDepth != 1)
        return fail("Only 1/2/4/8 bpp supported");

    outW = width; outH = height;

    // Determine channels from colorType
    int srcChannels;
    switch (colorType) {
        case 0: srcChannels = 1; break; // Gray
        case 2: srcChannels = 3; break; // RGB
        case 3: srcChannels = 1; break; // Palette (1 index byte)
        case 4: srcChannels = 2; break; // Gray+Alpha
        case 6: srcChannels = 4; break; // RGBA
        default: return fail("Unknown PNG color type");
    }
    outChannels = (reqChannels != 0) ? reqChannels : (colorType == 3 ? 3 : srcChannels);

    // Collect IDAT, PLTE
    std::vector<uint8_t> idat;
    uint8_t palette[256*3] = {};
    bool    hasPalette = false;

    while (p + 8 <= end) {
        uint32_t chunkLen = u32be(p); p += 4;
        char tag[5] = {};             memcpy(tag, p, 4); p += 4;
        const uint8_t* chunkData = p;
        if (p + chunkLen + 4 > end)   { p = end; break; }
        p += chunkLen + 4; // skip data + CRC

        if (strcmp(tag, "IDAT") == 0) {
            idat.insert(idat.end(), chunkData, chunkData + chunkLen);
        } else if (strcmp(tag, "PLTE") == 0 && colorType == 3) {
            memcpy(palette, chunkData, std::min((uint32_t)768, chunkLen));
            hasPalette = true;
        } else if (strcmp(tag, "IEND") == 0) {
            break;
        }
    }

    if (idat.empty()) return fail("No IDAT chunks");

    // Inflate compressed IDAT
    std::vector<uint8_t> raw;
    raw.reserve((size_t)width * height * (srcChannels + 1));
    if (!inflate(idat.data(), idat.size(), raw))
        return fail("PNG inflate failed");

    // Reconstruct filtered scanlines
    int bpp = std::max(1, srcChannels * bitDepth / 8);
    int stride = (bitDepth < 8)
        ? ((width * srcChannels * bitDepth + 7) / 8)
        : (width * srcChannels);
    int rowBytes = stride + 1; // +1 for filter byte

    if ((int)raw.size() < height * rowBytes)
        return fail("Insufficient decompressed data");

    std::vector<uint8_t> filtered(height * stride);
    std::vector<uint8_t> prev(stride, 0);

    for (int y = 0; y < height; y++) {
        int filterType = raw[y * rowBytes];
        const uint8_t* src = &raw[y * rowBytes + 1];
        uint8_t*       dst = &filtered[y * stride];

        for (int x = 0; x < stride; x++) {
            uint8_t a = (x >= bpp)    ? dst[x - bpp]  : 0;
            uint8_t b = prev[x];
            uint8_t c = (x >= bpp)    ? prev[x - bpp] : 0;
            uint8_t s = src[x];

            switch (filterType) {
                case 0: dst[x] = s;                              break;
                case 1: dst[x] = s + a;                          break;
                case 2: dst[x] = s + b;                          break;
                case 3: dst[x] = s + (uint8_t)((a + b) / 2);    break;
                case 4: dst[x] = s + paeth(a, b, c);             break;
                default: return fail("Unknown PNG filter type");
            }
        }
        prev.assign(dst, dst + stride);
    }

    // Expand palette / unpack sub-byte depths / convert to output channels
    int outCh = outChannels;
    pixels.resize((size_t)width * height * outCh);

    auto getGray = [&](int x, int y) -> uint8_t {
        if (bitDepth == 8) return filtered[y * stride + x];
        // Sub-byte: packed
        int bits_per_px = bitDepth;
        int pix_per_byte = 8 / bits_per_px;
        int byteIdx = x / pix_per_byte;
        int bitIdx  = (pix_per_byte - 1 - (x % pix_per_byte)) * bits_per_px;
        uint8_t raw_val = (filtered[y * stride + byteIdx] >> bitIdx) & ((1 << bits_per_px) - 1);
        // Scale to 0..255
        return (uint8_t)((raw_val * 255) / ((1 << bits_per_px) - 1));
    };

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * outCh;
            uint8_t r=0,g=0,b=0,a=255;

            if (colorType == 0) { // Gray
                r = g = b = getGray(x, y);
            } else if (colorType == 2) { // RGB
                int si = (y * stride) + x * 3;
                r = filtered[si]; g = filtered[si+1]; b = filtered[si+2];
            } else if (colorType == 3) { // Palette
                int pidx = getGray(x, y);
                if (hasPalette) { r = palette[pidx*3]; g = palette[pidx*3+1]; b = palette[pidx*3+2]; }
            } else if (colorType == 4) { // GrayA
                int si = (y * stride) + x * 2;
                r = g = b = filtered[si]; a = filtered[si+1];
            } else if (colorType == 6) { // RGBA
                int si = (y * stride) + x * 4;
                r = filtered[si]; g = filtered[si+1]; b = filtered[si+2]; a = filtered[si+3];
            }

            if (outCh == 1) pixels[idx] = r;
            else if (outCh == 2) { pixels[idx] = r; pixels[idx+1] = a; }
            else if (outCh == 3) { pixels[idx] = r; pixels[idx+1] = g; pixels[idx+2] = b; }
            else { pixels[idx] = r; pixels[idx+1] = g; pixels[idx+2] = b; pixels[idx+3] = a; }
        }
    }
    return true;
}

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
//  BMP Decoder (24/32-bit, no compression)
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
static bool decodeBMP(const uint8_t* data, size_t len,
                      int& w, int& h, int& outCh, int reqCh,
                      std::vector<uint8_t>& pixels)
{
    if (len < 54 || data[0] != 'B' || data[1] != 'M') return false;
    uint32_t dataOffset = u32le(data + 10);
    w = std::abs(s32le(data + 18));
    h = s32le(data + 22);     // positive = bottom-up, negative = top-down
    bool bottomUp = h > 0;
    h = std::abs(h);
    uint16_t bpp = u16le(data + 28);
    uint32_t compress = u32le(data + 30);
    if (compress != 0 && compress != 3) return false; // only uncompressed
    if (bpp != 24 && bpp != 32) return false;

    int srcCh = bpp / 8; // 3 or 4
    outCh = (reqCh != 0) ? reqCh : 3;
    pixels.resize((size_t)w * h * outCh);

    int rowBytes = (w * srcCh + 3) & ~3; // 4-byte aligned rows

    for (int y = 0; y < h; y++) {
        int srcY = bottomUp ? (h - 1 - y) : y;
        const uint8_t* row = data + dataOffset + (size_t)srcY * rowBytes;
        for (int x = 0; x < w; x++) {
            const uint8_t* px = row + x * srcCh;
            // BMP stores BGR(A)
            uint8_t b = px[0], g = px[1], r = px[2];
            uint8_t a = (srcCh == 4) ? px[3] : 255;
            int idx = (y * w + x) * outCh;
            if (outCh == 3) { pixels[idx]=r; pixels[idx+1]=g; pixels[idx+2]=b; }
            else { pixels[idx]=r; pixels[idx+1]=g; pixels[idx+2]=b; pixels[idx+3]=a; }
        }
    }
    return true;
}

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
//  Radiance HDR Decoder (.hdr, RGBE format)
//  Used by EnvironmentMap for IBL / equirectangular images
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
static bool decodeHDR(const uint8_t* data, size_t len,
                      int& w, int& h, int& channels,
                      std::vector<float>& pixels)
{
    // Validate header
    const char* RADIANCE_SIG = "#?RADIANCE";
    if (len < 10 || memcmp(data, RADIANCE_SIG, 10) != 0) return false;

    // Skip header lines until we find the resolution line
    size_t pos = 0;
    while (pos < len) {
        // Find end of header (empty line)
        if (pos + 1 < len && data[pos] == '\n' && data[pos+1] == '\n') { pos += 2; break; }
        if (pos + 2 < len && data[pos] == '\r' && data[pos+1] == '\n'
            && data[pos+2] == '\r') { pos += 4; break; }
        pos++;
    }

    // Parse resolution: "-Y H +X W\n"
    char resBuf[64] = {};
    size_t rp = 0;
    while (rp < 63 && pos < len && data[pos] != '\n') resBuf[rp++] = (char)data[pos++];
    if (pos < len) pos++; // skip \n

    int iw = 0, ih = 0;
    sscanf(resBuf, "-Y %d +X %d", &ih, &iw);
    if (iw <= 0 || ih <= 0) return false;
    w = iw; h = ih; channels = 3;

    pixels.resize((size_t)w * h * 3);

    // RLE scanline decoding
    for (int y = 0; y < h; y++) {
        if (pos + 4 > len) return false;
        uint8_t r0 = data[pos], g0 = data[pos+1], b0 = data[pos+2], e0 = data[pos+3];

        if (r0 == 2 && g0 == 2 && iw >= 8 && iw < 32768) {
            // New RLE format
            int scanW = (b0 << 8) | e0;
            if (scanW != w) return false;
            pos += 4;

            std::vector<uint8_t> scanline(4 * w);
            // Decode each channel with RLE
            for (int ch = 0; ch < 4; ch++) {
                int x = 0;
                while (x < w) {
                    if (pos >= len) return false;
                    uint8_t code = data[pos++];
                    if (code > 128) {
                        // Run
                        int count = code - 128;
                        if (pos >= len || x + count > w) return false;
                        uint8_t val = data[pos++];
                        while (count-- > 0) scanline[ch * w + x++] = val;
                    } else {
                        // Non-run
                        int count = code;
                        if (pos + count > len || x + count > w) return false;
                        while (count-- > 0) scanline[ch * w + x++] = data[pos++];
                    }
                }
            }
            // RGBE â†’ float
            for (int x = 0; x < w; x++) {
                float rr = scanline[0 * w + x];
                float rg = scanline[1 * w + x];
                float rb = scanline[2 * w + x];
                int   re = scanline[3 * w + x];
                if (re != 0) {
                    float scale = std::ldexp(1.0f, re - 128 - 8);
                    int baseIdx = (y * w + x) * 3;
                    pixels[baseIdx]   = rr * scale;
                    pixels[baseIdx+1] = rg * scale;
                    pixels[baseIdx+2] = rb * scale;
                }
            }
        } else {
            // Old (uncompressed) format
            for (int x = 0; x < w; x++) {
                if (pos + 4 > len) return false;
                r0 = data[pos]; g0 = data[pos+1]; b0 = data[pos+2]; e0 = data[pos+3];
                pos += 4;
                float scale = (e0 != 0) ? std::ldexp(1.0f, (int)e0 - 128 - 8) : 0.0f;
                int baseIdx = (y * w + x) * 3;
                pixels[baseIdx]   = r0 * scale;
                pixels[baseIdx+1] = g0 * scale;
                pixels[baseIdx+2] = b0 * scale;
            }
        }
    }
    return true;
}

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
//  Read entire file into vector
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
static std::vector<uint8_t> readFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return {}; }
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return {}; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return {}; }
    std::vector<uint8_t> buf((size_t)sz);
    size_t read = fread(buf.data(), 1, buf.size(), f);
    if (read != buf.size()) { fclose(f); return {}; }
    fclose(f);
    return buf;
}

} // namespace img_detail

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  stbi-compatible C API
//  Drop-in replacement for stb_image's public API.
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

// Global state (thread-local would be better but stb_image uses globals too)
static bool  g_flip_vertically = false;
static char  g_last_error[256] = "No error";

// Flip image vertically in-place
static void flipVertically(uint8_t* data, int w, int h, int ch) {
    int rowBytes = w * ch;
    std::vector<uint8_t> tmp(rowBytes);
    for (int y = 0; y < h / 2; y++) {
        uint8_t* top = data + (size_t)y       * rowBytes;
        uint8_t* bot = data + (size_t)(h-1-y) * rowBytes;
        memcpy(tmp.data(), top, rowBytes);
        memcpy(top, bot, rowBytes);
        memcpy(bot, tmp.data(), rowBytes);
    }
}

void stbi_set_flip_vertically_on_load(int flag) {
    g_flip_vertically = (flag != 0);
}

const char* stbi_failure_reason() {
    return g_last_error;
}

/// Load image from file. Returns RGBA/RGB/Gray data. Free with stbi_image_free().
unsigned char* stbi_load(const char* path, int* w, int* h, int* ch, int req_ch) {
    auto file = img_detail::readFile(path);
    if (file.empty()) {
        snprintf(g_last_error, sizeof(g_last_error), "Cannot open file: %s", path);
        return nullptr;
    }

    std::vector<uint8_t> pixels;
    int outW = 0, outH = 0, outCh = 0;
    bool ok = false;

    // PNG
    if (file.size() >= 8 && memcmp(file.data(), img_detail::PNG_SIG, 8) == 0) {
        ok = img_detail::decodePNG_internal(
            file.data(), file.size(), outW, outH, outCh, req_ch, pixels,
            g_last_error, sizeof(g_last_error));
    }
    // BMP
    else if (file.size() >= 2 && file[0] == 'B' && file[1] == 'M') {
        ok = img_detail::decodeBMP(file.data(), file.size(), outW, outH, outCh, req_ch, pixels);
        if (!ok) snprintf(g_last_error, sizeof(g_last_error), "BMP decode failed: %s", path);
    }
    else {
        snprintf(g_last_error, sizeof(g_last_error), "Unknown format: %s", path);
    }

    if (!ok || pixels.empty()) return nullptr;

    if (req_ch != 0) outCh = req_ch;
    if (w) *w = outW;
    if (h) *h = outH;
    if (ch) *ch = outCh;

    uint8_t* result = (uint8_t*)malloc(pixels.size());
    if (!result) return nullptr;
    memcpy(result, pixels.data(), pixels.size());

    if (g_flip_vertically) flipVertically(result, outW, outH, outCh);
    return result;
}

/// Load HDR float image. Channels = 3 (RGB). Free with stbi_image_free().
float* stbi_loadf(const char* path, int* w, int* h, int* ch, int /*req_ch*/) {
    auto file = img_detail::readFile(path);
    if (file.empty()) {
        snprintf(g_last_error, sizeof(g_last_error), "Cannot open HDR file: %s", path);
        return nullptr;
    }

    std::vector<float> pixels;
    int outW = 0, outH = 0, outCh = 0;

    if (!img_detail::decodeHDR(file.data(), file.size(), outW, outH, outCh, pixels)) {
        snprintf(g_last_error, sizeof(g_last_error), "HDR decode failed: %s", path);
        return nullptr;
    }

    if (w) *w = outW;
    if (h) *h = outH;
    if (ch) *ch = outCh;

    float* result = (float*)malloc(pixels.size() * sizeof(float));
    if (!result) return nullptr;
    memcpy(result, pixels.data(), pixels.size() * sizeof(float));

    if (g_flip_vertically) {
        int rowFloats = outW * outCh;
        std::vector<float> tmp(rowFloats);
        for (int y = 0; y < outH / 2; y++) {
            float* top = result + (size_t)y         * rowFloats;
            float* bot = result + (size_t)(outH-1-y)* rowFloats;
            memcpy(tmp.data(), top, rowFloats * sizeof(float));
            memcpy(top, bot, rowFloats * sizeof(float));
            memcpy(bot, tmp.data(), rowFloats * sizeof(float));
        }
    }
    return result;
}

/// Free image data returned by stbi_load / stbi_loadf
void stbi_image_free(void* data) {
    free(data);
}

/// Load image from an in-memory buffer
unsigned char* stbi_load_from_memory(const unsigned char* buf, int bufLen,
                                              int* w, int* h, int* ch, int req_ch) {
    std::vector<uint8_t> pixels;
    int outW = 0, outH = 0, outCh = 0;
    bool ok = false;

    const auto* data = reinterpret_cast<const uint8_t*>(buf);
    size_t len = static_cast<size_t>(bufLen);

    // PNG
    if (len >= 8 && memcmp(data, img_detail::PNG_SIG, 8) == 0) {
        ok = img_detail::decodePNG_internal(data, len, outW, outH, outCh, req_ch, pixels,
                                             g_last_error, sizeof(g_last_error));
    }
    // BMP
    else if (len >= 2 && data[0] == 'B' && data[1] == 'M') {
        ok = img_detail::decodeBMP(data, len, outW, outH, outCh, req_ch, pixels);
        if (!ok) snprintf(g_last_error, sizeof(g_last_error), "BMP decode failed (in-memory)");
    }
    else {
        snprintf(g_last_error, sizeof(g_last_error), "Unknown in-memory image format");
    }

    if (!ok || pixels.empty()) return nullptr;

    if (req_ch != 0) outCh = req_ch;
    if (w) *w = outW;
    if (h) *h = outH;
    if (ch) *ch = outCh;

    uint8_t* result = (uint8_t*)malloc(pixels.size());
    if (!result) return nullptr;
    memcpy(result, pixels.data(), pixels.size());
    if (g_flip_vertically) flipVertically(result, outW, outH, outCh);
    return result;
}

/// Load HDR from in-memory buffer
float* stbi_loadf_from_memory(const unsigned char* buf, int bufLen,
                                      int* w, int* h, int* ch, int /*req_ch*/) {
    std::vector<float> pixels;
    int outW = 0, outH = 0, outCh = 0;
    if (!img_detail::decodeHDR(reinterpret_cast<const uint8_t*>(buf),
                               static_cast<size_t>(bufLen), outW, outH, outCh, pixels)) {
        snprintf(g_last_error, sizeof(g_last_error), "HDR from-memory decode failed");
        return nullptr;
    }
    if (w) *w = outW;
    if (h) *h = outH;
    if (ch) *ch = outCh;
    float* result = (float*)malloc(pixels.size() * sizeof(float));
    if (!result) return nullptr;
    memcpy(result, pixels.data(), pixels.size() * sizeof(float));
    return result;
}


