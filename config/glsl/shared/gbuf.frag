#if GBUF_MULTISAMPLE
    #define GBUF_TEX_TYPE sampler2DMS
#else
    #define GBUF_TEX_TYPE sampler2DRect
#endif

uniform GBUF_TEX_TYPE GBUF_TEXTURES;

vec4 gfetch(GBUF_TEX_TYPE tex, vec2 texcoord)
{
#if GBUF_MULTISAMPLE
    return texelFetch(tex, ivec2(texcoord), 0);
#else
    return texture(tex, texcoord);
#endif
}
