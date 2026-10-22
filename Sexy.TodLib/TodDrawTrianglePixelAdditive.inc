// Included inside the textured triangle scanline for the additive variants.
{
    #include "graphics/SWTri/SWTri_GetTexel.cpp"
    #include "graphics/SWTri/SWTri_TexelARGB.cpp"

    if (alpha > 0)
    {
        unsigned int addR = (tex >> 16) & 0xff;
        unsigned int addG = (tex >> 8) & 0xff;
        unsigned int addB = tex & 0xff;
        // Bilinear sampling already premultiplies the texture and tint alpha.
        #if !defined(LINEAR_BLEND)
        addR = (addR * alpha) / 255;
        addG = (addG * alpha) / 255;
        addB = (addB * alpha) / 255;
        #endif

        const unsigned int dest = *pix;
        #if TRI0 == 1
        unsigned int red = ((dest >> 16) & 0xff) + addR;
        unsigned int green = ((dest >> 8) & 0xff) + addG;
        unsigned int blue = (dest & 0xff) + addB;
        red = red > 255 ? 255 : red;
        green = green > 255 ? 255 : green;
        blue = blue > 255 ? 255 : blue;
        *pix = 0xff000000 | (red << 16) | (green << 8) | blue;
        #elif TRI0 == 2
        unsigned int red = ((dest >> 11) & 0x1f) + (addR >> 3);
        unsigned int green = ((dest >> 5) & 0x3f) + (addG >> 2);
        unsigned int blue = (dest & 0x1f) + (addB >> 3);
        red = red > 31 ? 31 : red;
        green = green > 63 ? 63 : green;
        blue = blue > 31 ? 31 : blue;
        *pix = (red << 11) | (green << 5) | blue;
        #endif
    }
}
