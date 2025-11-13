in vec4 vvertex;
uniform vec4 screentexcoord0;
uniform float offsets[8];
out vec2 texcoord0, texcoordp1, texcoordn1;

#if (BLUR_TAPS > 1)
    out vec2 texcoordp2, texcoordn2;
#endif

#if (BLUR_TAPS > 2)
    out vec2 texcoordp3, texcoordn3;
#endif

void main(void)
{
    gl_Position = vvertex;
    texcoord0 = screencoord(vvertex.xy, screentexcoord0);
    vec2 tcp = texcoord0, tcn = texcoord0;

    tcp.BLUR_DIR_NAME += offsets[1];
    tcn.BLUR_DIR_NAME -= offsets[1];

    texcoordp1 = tcp;
    texcoordn1 = tcn;

#if (BLUR_TAPS > 1)
        tcp.BLUR_DIR_NAME = texcoord0.BLUR_DIR_NAME + offsets[2];
        tcn.BLUR_DIR_NAME = texcoord0.BLUR_DIR_NAME - offsets[2];
        texcoordp2 = tcp;
        texcoordn2 = tcn;
#endif

#if (BLUR_TAPS > 2)
        tcp.BLUR_DIR_NAME = texcoord0.BLUR_DIR_NAME + offsets[3];
        tcn.BLUR_DIR_NAME = texcoord0.BLUR_DIR_NAME - offsets[3];
        texcoordp3 = tcp;
        texcoordn3 = tcn;
#endif
}
