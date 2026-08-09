in vec4 vvertex;
uniform vec4 screentexcoord0, screentexcoord1;
out vec2 texcoord0, texcoord1;

void main(void)
{
    gl_Position = vvertex;
    texcoord0 = screencoord(vvertex.xy, screentexcoord0);
    texcoord1 = screencoord(vvertex.xy, screentexcoord1);
}
