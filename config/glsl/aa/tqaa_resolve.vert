in vec4 vvertex;
uniform vec4 screentexcoord0;
out vec2 texcoord0;

#if GATHER_ENABLED
    out vec2 texcoord1;
#endif

void main(void)
{
    gl_Position = vvertex;
    texcoord0 = screencoord(vvertex.xy, screentexcoord0);

#if GATHER_ENABLED
    texcoord1 = texcoord0 - 0.5;
#endif
}
