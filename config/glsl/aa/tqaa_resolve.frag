uniform vec4 quincunx;
uniform sampler2DRect tex0, tex1;
uniform mat4 reprojectmatrix;
uniform vec2 maxvelocity;
uniform vec2 colorweight;

in vec2 texcoord0;

#if GATHER_ENABLED
    in vec2 texcoord1;
#endif

layout(location = 0) out vec4 fragcolor;

void main(void)
{
    float depth = gdepth(texcoord0);

#if GDEPTH_LINEAR
    vec4 prevtc = reprojectmatrix * vec4(depth*texcoord0, depth, 1.0);
#else
    vec4 prevtc = reprojectmatrix * vec4(texcoord0, depth, 1.0);
#endif

    vec2 vel = prevtc.xy/prevtc.w - texcoord0;
    float scale = clamp(maxvelocity.x*inversesqrt(dot(vel, vel) + 1e-6), 0.0, 1.0);

    float mask = 1.0 - texture(tex0, texcoord0 + quincunx.xy).a;
    vec4 color = texture(tex0, texcoord0 + mask*quincunx.xy);
    vec4 prevcolor = texture(tex1, texcoord0 + mask*(quincunx.zw + vel*scale));

#if GATHER_ENABLED
    vec4 l0 = textureGather(tex0, texcoord1, 1);
    vec4 l1 = textureGatherOffset(tex0, texcoord1, ivec2(1, 1), 1);
    float l2 = textureOffset(tex0, texcoord0, ivec2(1, -1)).g;
    float l3 = textureOffset(tex0, texcoord0, ivec2(-1, 1)).g;
    vec4 l01min = min(l0, l1), l01max = max(l0, l1);
    l01min.xy = min(l01min.xy, l01min.zw);
    l01max.xy = max(l01max.xy, l01max.zw);
    float lmin = min(min(l01min.x, l01min.y), min(l2, l3));
    float lmax = max(max(l01max.x, l01max.y), max(l2, l3));
#else
    float l0 = texture(tex0, texcoord0 + vec2(-1.0, -0.5)).g;
    float l1 = texture(tex0, texcoord0 + vec2( 0.5, -1.0)).g;
    float l2 = texture(tex0, texcoord0 + vec2( 1.0,  0.5)).g;
    float l3 = texture(tex0, texcoord0 + vec2(-0.5,  1.0)).g;
    float lmin = min(color.g, min(min(l0, l1), min(l2, l3)));
    float lmax = max(color.g, max(max(l0, l1), max(l2, l3)));
#endif

    float weight = 0.5 - 0.5*clamp((colorweight.x*max(prevcolor.g - lmax, lmin - prevcolor.g) + colorweight.y) / (lmax - lmin + 1e-4), 0.0, 1.0);
    weight *= clamp(1.0 - 2.0*(prevcolor.a - color.a), 0.0, 1.0);
    fragcolor.rgb = mix(color.rgb, prevcolor.rgb, weight);
    fragcolor.a = color.a;
}
