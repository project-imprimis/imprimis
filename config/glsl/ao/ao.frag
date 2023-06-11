uniform sampler2D tex2;
uniform vec3 tapparams;
uniform vec2 contrastparams;
uniform vec4 offsetscale;
uniform float prefilterdepth;
uniform mat3 normalmatrix;

#if AO_LINEAR_DEPTH
    #define depthtc gl_FragCoord.xy
#else
    #define depthtc texcoord0
#endif

in vec2 texcoord0, texcoord1;

layout(location = 0) out vec4 fragcolor;

void main(void)
{
    vec4 depth_data;
    float depth = gdepth(depthtc, depth_data);

#if GDEPTH_LINEAR
    vec2 tapscale = tapparams.xy/depth;
#else
    float w = depth*gdepthscale.y + gdepthscale.z;
    depth = gdepthscale.x/w;
    vec2 tapscale = tapparams.xy*w;
#endif

    vec2 dpos = depthtc*offsetscale.xy + offsetscale.zw, pos = depth*dpos;
    vec3 normal = gfetch(tex1, texcoord0).rgb*2.0 - 1.0;
    float normscale = inversesqrt(dot(normal, normal));

    normal *= normscale > 0.75 ? normscale : 0.0;
    normal = normalmatrix * normal;

    vec2 noise = texture(tex2, texcoord1).rg*2.0-1.0;
    float obscure = 0.0;

    vec2 offsets[] = vec2[](
        vec2(-0.933103,  0.025116),
        vec2(-0.432784, -0.989868),
        vec2( 0.432416, -0.413800),
        vec2(-0.117770,  0.970336),
        vec2( 0.837276,  0.531114),
        vec2(-0.184912,  0.200232),
        vec2(-0.955748,  0.815118),
        vec2( 0.946166, -0.998596),
        vec2(-0.897519, -0.581102),
        vec2( 0.979248, -0.046602),
        vec2(-0.155736, -0.488204),
        vec2( 0.460310,  0.982178)
    );

    for(int i = 0; i < AO_TAPS; i++)
    {
        vec2 tap_offset = reflect(offsets[i], noise);
        tap_offset = depthtc + tapscale * tap_offset;

        float tap_depth = gdepth_linear(tap_offset);

        vec3 v = vec3(tap_depth*(tap_offset*offsetscale.xy + offsetscale.zw) - pos, tap_depth - depth);
        float dist2 = dot(v, v);
        obscure += step(dist2, tapparams.z) * max(0.0, dot(v, normal) + depth*1.0e-2) / (dist2 + 1.0e-5);
    }

    obscure = pow(clamp(1.0 - contrastparams.x*obscure, 0.0, 1.0), contrastparams.y);


    vec2 weights = step(fwidth(depth), prefilterdepth) * (2.0*fract((gl_FragCoord.xy - 0.5)*0.5) - 0.5);

    obscure -= dFdx(obscure) * weights.x;
    obscure -= dFdy(obscure) * weights.y;

#if AO_PACKED_DEPTH
    #if AO_DEPTH_FORMAT
        fragcolor.rg = vec2(obscure, depth);
    #else
        #if GDEPTH_PACKED
            depth_data = gdepthpack(depth);
        #endif
        fragcolor = vec4(depth_data, obscure);
    #endif
#else
        fragcolor = vec4(obscure, 0.0, 0.0, 1.0);
#endif
}
