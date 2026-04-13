in vec4 vvertex;
uniform vec4 screentexcoord0;
out vec2 texcoord0;

void main(void)
{
    gl_Position = vvertex;
    texcoord0 = screencoord(vvertex.xy, screentexcoord0);
}
