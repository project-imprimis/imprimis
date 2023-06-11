in vec4 vvertex;

#if AO_REDUCED
    uniform vec4 screentexcoord0;
    out vec2 texcoord0;
#endif

#if AO_UPSCALED
    uniform vec4 screentexcoord1;
    out vec2 texcoord1;
#endif

void main(void)
{
    gl_Position = vvertex;

#if AO_REDUCED
    texcoord0 = screencoord(vvertex.xy, screentexcoord0);
#endif

#if AO_UPSCALED
    texcoord1 = screencoord(vvertex.xy, screentexcoord1);
#endif
}
