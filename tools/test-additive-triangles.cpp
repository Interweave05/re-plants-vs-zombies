// Standalone test: compile with -I SexyAppFramework -I Sexy.TodLib.
// Exercise the same generated rasterizers used by reanimation highlights.
#include "EffectSystem.h"
#include <array>
#include <cassert>
#include <cstdio>

bool gTodTriangleDrawAdditive = true;
static int FixedFloor(int x)
{
    return x > 0 ? x & 0xffff0000 : (x & 0xffff0000) - 0x10000;
}
#include "../Sexy.TodLib/TodDrawTriangleInc.cpp"

struct Variant
{
    DrawTriFunc draw;
    bool textureAlpha, vertexTint, globalTint, bilinear;
};

#define ENTRY(F, A, M, G, B) {TodDrawTriangle_##F##_TEX1_TALPHA##A##_MOD##M##_GLOB##G##_BLEND##B, A, M, G, B}
#define PAIR(F, A, M, G) ENTRY(F, A, M, G, 0), ENTRY(F, A, M, G, 1)
#define VARIANTS(F) PAIR(F, 0, 0, 1), PAIR(F, 0, 1, 0), PAIR(F, 0, 1, 1), \
    PAIR(F, 1, 0, 0), PAIR(F, 1, 0, 1), PAIR(F, 1, 1, 0), PAIR(F, 1, 1, 1)

template<typename Pixel>
Pixel Draw(const Variant& variant, Pixel background, unsigned int texture,
    int intensity = 255, int opacity = 255, bool additive = true)
{
    std::array<Pixel, 16> pixels;
    pixels.fill(background);
    const unsigned int texels[] = {texture, texture, texture, texture};
    SWHelper::SWTextureInfo info = {texels, 1, 1, 1, 2, 4, 2};
    const int tint = intensity << 16;
    SWHelper::SWVertex vertices[] = {
        {0, 0, opacity << 16, tint, tint, tint, 0xc000, 0xc000},
        {4 << 16, 0, opacity << 16, tint, tint, tint, 0xc000, 0xc000},
        {0, 4 << 16, opacity << 16, tint, tint, tint, 0xc000, 0xc000}
    };
    SWHelper::SWDiffuse diffuse = {(unsigned)opacity, (unsigned)intensity,
        (unsigned)intensity, (unsigned)intensity};
    gTodTriangleDrawAdditive = additive;
    variant.draw(vertices, pixels.data(), 4 * sizeof(Pixel), &info, diffuse);
    assert(pixels[15] == background); // Outside the triangle.
    return pixels[0];
}

template<typename Pixel>
void CheckVariants(const Variant* variants, Pixel background)
{
    for (int i = 0; i < 14; ++i)
    {
        const auto& v = variants[i];
        assert(Draw(v, background, 0xff000000) == background);
        if (v.textureAlpha)
            assert(Draw(v, background, 0x00ffffff) == background);
        if (v.vertexTint || v.globalTint)
            assert(Draw(v, background, 0xffffffff, 255, 0) == background);

        // Low-intensity production glow must preserve every background channel.
        for (int intensity : {0, 3, 64, 128, 255})
        {
            const Pixel result = Draw(v, background, 0xff80c0ff, intensity);
            const unsigned masks32[] = {0xff0000, 0xff00, 0xff};
            const unsigned masks16[] = {0xf800, 0x07e0, 0x001f};
            const auto* masks = sizeof(Pixel) == 4 ? masks32 : masks16;
            for (int channel = 0; channel < 3; ++channel)
                assert((result & masks[channel]) >= (background & masks[channel]));
        }
        const Pixel white = sizeof(Pixel) == 4 ? Pixel(0xfffafafa) : Pixel(0xf7be);
        const Pixel saturated = Draw(v, white, 0xffffffff);
        if (saturated != Pixel(~Pixel(0)))
            std::fprintf(stderr, "saturation: bits=%zu variant=%d result=%08x\n", sizeof(Pixel) * 8, i, (unsigned)saturated);
        assert(saturated == Pixel(~Pixel(0))); // Saturate, never wrap.
    }
}

int main()
{
    static_assert(sizeof(unsigned long) == 4, "Rasterizers require 32-bit unsigned long");
    const Variant rgb[] = {VARIANTS(0888)};
    const Variant rgb565[] = {VARIANTS(0565)};
    CheckVariants(rgb, 0xff406080ul);
    CheckVariants(rgb565, (unsigned short)0x430f);
    for (int bilinear = 0; bilinear < 2; ++bilinear)
    {
        const auto& v = rgb[6 + bilinear]; // Texture alpha, no tint.
        const unsigned long pixel = Draw(v, 0xff102030ul, 0x80808080);
        for (int shift : {0, 8, 16})
        {
            const int added = ((pixel >> shift) & 255) - ((0xff102030 >> shift) & 255);
            assert(added >= 60 && added <= 65); // Half-alpha adds once, including bilinear.
        }
        // Ordinary rendering still replaces opaque pixels instead of adding.
        const unsigned long normal = Draw(v, 0xff102030ul, 0xff808080, 255, 255, false);
        for (int shift : {0, 8, 16})
            assert(((normal >> shift) & 255) >= 124 && ((normal >> shift) & 255) <= 128);
    }
    std::puts("PASS: 28 additive rasterizers preserve, brighten and saturate RGB/565 pixels; alpha and normal blending remain correct");
}
