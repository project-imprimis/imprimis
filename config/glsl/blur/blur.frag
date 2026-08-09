uniform float weights[8];
uniform float offsets[8];
uniform BLUR_TEX_TYPE tex0;
in vec2 texcoord0, texcoordp1, texcoordn1;

#if (BLUR_TAPS > 1)
    in vec2 texcoordp2, texcoordn2;
#endif

#if (BLUR_TAPS > 2)
    in vec2 texcoordp3, texcoordn3;
#endif

layout(location = 0) out vec4 fragcolor;

void main(void)
{
    #define texval(coords) texture(tex0, (coords))
    vec4 val = texval(texcoord0) * weights[0];

#if (BLUR_TAPS >= 1)
    val += weights[1] * (texval(texcoordp1) + texval(texcoordn1));
#endif

#if (BLUR_TAPS >= 2)
    val += weights[2] * (texval(texcoordp2) + texval(texcoordn2));
#endif

#if (BLUR_TAPS >= 3)
    val += weights[3] * (texval(texcoordp3) + texval(texcoordn3));
#endif

#if (BLUR_TAPS >= 4)
    for(int i = 4; i <= BLUR_TAPS; i++)
    {
        val += weights[i] *
    #if (BLUR_DIR == BLUR_DIR_X)
            (texval(vec2(texcoord0.x + offsets[i], texcoord0.y)) + texval(vec2(texcoord0.x - offsets[i], texcoord0.y)));
    #else
            (texval(vec2(texcoord0.x, texcoord0.y + offsets[i])) + texval(vec2(texcoord0.x, texcoord0.y - offsets[i])));
    #endif
    }
#endif

    fragcolor = val;
}
