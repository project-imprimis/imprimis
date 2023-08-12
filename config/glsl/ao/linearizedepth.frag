in vec2 texcoord0;

layout(location = 0) out vec4 fragcolor;

void main(void)
{
#if !AO_DEPTH_FORMAT
    #if (GDEPTH_FORMAT == 1)
        fragcolor = gdepth_fetch(tex0, texcoord0);
    #else
        fragcolor = vec4(gpackdepth(gdepth_linear(texcoord0)), 1.0);
    #endif
#else
    fragcolor.r = gdepth_linear(texcoord0);
#endif
}
