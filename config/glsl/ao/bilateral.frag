uniform sampler2DRect tex0;
uniform vec2 bilateralparams;

#if AO_REDUCED
    in vec2 texcoord0;
#endif

#if AO_UPSCALED
    in vec2 texcoord1;
#endif

layout(location = 0) out vec4 fragcolor;

void main(void)
{
#if AO_UPSCALED
    #define tc texcoord1
#else
    #define tc gl_FragCoord.xy
#endif

#if AO_REDUCED
    #define depthtc texcoord0
#else
    #define depthtc gl_FragCoord.xy
#endif

#if (AO_FILTER_DIR == AO_FILTER_DIR_X)
    #define tapvec(type, i) type(i, 0.0)
#else
    #define tapvec(type, i) type(0.0, i)
#endif

    #define texval(i) texture(tex0, tc + tapvec(vec2, i))
    #define texvaloffset(i) textureOffset(tex0, tc, tapvec(ivec2, i))
    #define depthval(i) gdepth_fetch(depthtc + tapvec(vec2, i))
    #define depthvaloffset(i) gdepth_fetch_offset(depthtc, tapvec(ivec2, i))

#if AO_PACKED_DEPTH
    #if AO_DEPTH_FORMAT
        vec2 vals = texture(tex0, tc).rg;
        float color = vals.x;

        #if AO_UPSCALED
            float depth = gdepth_linear(depthtc);
        #else
            float depth = vals.y;
        #endif
    #else
        vec4 vals = texture(tex0, tc);
        float color = vals.a;

        #if AO_UPSCALED
            float depth = gdepth_linear(depthtc);
        #else
            float depth = gdepthunpack(vals.rgb);
        #endif
    #endif
#elif AO_LINEAR_DEPTH
    float color = texture(tex0, tc).r;

    #if AO_DEPTH_FORMAT
        float depth = gdepth_fetch(depthtc).r;
    #else
        float depth = gdepthunpack(gdepth_fetch(depthtc).rgb);
    #endif
#else
    float color = texture(tex0, tc).r;
    float depth = gdepth_linear(depthtc);
#endif

    float weights = 1.0;
    for(int i = 0; i < AO_FILTER_TAPS*2; i++)
    {
        int curtap = i - AO_FILTER_TAPS;
        if(curtap >= 0) curtap++;
        int curtapoffset = curtap*2;
        int curdepthoffset = curtapoffset << AO_REDUCED;

        vec4 curtexval;
        if(curtapoffset >= AO_MIN_TEX_OFFSET && curtapoffset <= AO_MAX_TEX_OFFSET)
        {
            curtexval = texvaloffset(curtapoffset);
        }
        else
        {
            curtexval = texval(curtapoffset);
        }

        vec4 curdepthval;
        if(curdepthoffset >= AO_MIN_TEX_OFFSET && curdepthoffset <= AO_MAX_TEX_OFFSET)
        {
            curdepthval = depthvaloffset(curdepthoffset);
        }
        else
        {
            curdepthval = depthval(curdepthoffset);
        }

#if AO_PACKED_DEPTH
    #if AO_DEPTH_FORMAT
        vec2 vals = curtexval.rg;
        float tap_color = vals.x;
        float tap_depth = vals.y;
    #else
        vec4 vals = curtexval;
        float tap_color = vals.a;
        float tap_depth = gdepthunpack(vals.rgb);
    #endif
#elif AO_LINEAR_DEPTH
        float tap_color = curtexval.r;
    #if AO_DEPTH_FORMAT
        float tap_depth = curdepthval.r;
    #else
        float tap_depth = gdepthunpack(curdepthval.rgb);
    #endif
#else
        float tap_color = curtexval.r;
        float tap_depth = gdepth_linear(curdepthval.xy);
#endif

        tap_depth -= depth;
        float weight = exp2(-1.0*curtap*curtap*bilateralparams.x - tap_depth*tap_depth*bilateralparams.y);
        weights += weight;
        color += weight * tap_color;
    }

#if (AO_FILTER_DIR == AO_FILTER_DIR_X && AO_PACKED_DEPTH)
    #if AO_DEPTH_FORMAT
        fragcolor.rg = vec2(color / weights, depth);
    #else
        #if AO_UPSCALED
            vec3 packdepth = gdepthpack(depth);
        #else
            vec3 packdepth = vals.rgb;
        #endif

        fragcolor = vec4(packdepth, color / weights);
    #endif
#else
    fragcolor = vec4(color / weights, 0.0, 0.0, 1.0);
#endif
}
