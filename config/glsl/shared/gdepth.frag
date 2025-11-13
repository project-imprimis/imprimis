#if GDEPTH_MULTISAMPLE
    uniform sampler2DMS GDEPTH_TEXTURE;
#else
    uniform sampler2DRect GDEPTH_TEXTURE;
#endif

uniform vec3 gdepthscale;
uniform vec3 gdepthunpackparams;
uniform vec3 gdepthpackparams;

#if GDEPTH_MULTISAMPLE
    vec4 gdepth_fetch(vec2 texcoord)
    {
        return texelFetch(GDEPTH_TEXTURE, ivec2(texcoord), 0);
    }

    vec4 gdepth_fetch_offset(vec2 texcoord, ivec2 offset)
    {
        return texelFetch(GDEPTH_TEXTURE, ivec2(texcoord) + offset, 0);
    }
#else
    vec4 gdepth_fetch(vec2 texcoord)
    {
        return texture(GDEPTH_TEXTURE, texcoord);
    }

    vec4 gdepth_fetch_offset(vec2 texcoord, ivec2 offset)
    {
        return textureOffset(GDEPTH_TEXTURE, texcoord, offset);
    }
#endif

float gdepthunpack(vec3 fragment)
{
    return dot(fragment, gdepthunpackparams);
}

vec3 gdepthpack(float depth)
{
    vec3 packed_val = depth * gdepthpackparams;
    packed_val = vec3(packed_val.x, fract(packed_val.yz));
    packed_val.xy -= packed_val.yz * (1.0/255.0);

    return packed_val;
}

#if GDEPTH_PACKED
    float gdepth_decode(vec4 fragment)
    {
        return gdepthunpack(fragment.rgb);
    }
#else
    float gdepth_decode(vec4 fragment)
    {
        return fragment.r;
    }
#endif

float gdepth(vec2 texcoord)
{
    vec4 depth_data = gdepth_fetch(texcoord);
    return gdepth_decode(depth_data);
}

float gdepth(vec2 texcoord, out vec4 depth_data)
{
    depth_data = gdepth_fetch(texcoord);
    return gdepth_decode(depth_data);
}

float gdepth_offset(vec2 texcoord, ivec2 offset)
{
    vec4 depth_data = gdepth_fetch_offset(texcoord, offset);
    return gdepth_decode(depth_data);
}

float gdepth_offset(vec2 texcoord, ivec2 offset, out vec4 depth_data)
{
    depth_data = gdepth_fetch_offset(texcoord, offset);
    return gdepth_decode(depth_data);
}

#if GDEPTH_LINEAR
    float gdepth_linear(vec2 texcoord)
    {
        return gdepth(texcoord);
    }

    float gdepth_linear(vec2 texcoord, out vec4 depth_data)
    {
        return gdepth(texcoord, depth_data);
    }

    float gdepth_linear_offset(vec2 texcoord, ivec2 offset)
    {
        return gdepth_offset(texcoord, offset);
    }

    float gdepth_linear_offset(vec2 texcoord, ivec2 offset, out vec4 depth_data)
    {
        return gdepth_offset(texcoord, offset, depth_data);
    }
#else
    float gdepth_linear(vec2 texcoord)
    {
        float logdepth = gdepth(texcoord);
        return gdepthscale.x / (logdepth * gdepthscale.y + gdepthscale.z);
    }

    float gdepth_linear(vec2 texcoord, out vec4 depth_data)
    {
        float logdepth = gdepth(texcoord, depth_data);
        return gdepthscale.x / (logdepth * gdepthscale.y + gdepthscale.z);
    }

    float gdepth_linear_offset(vec2 texcoord, ivec2 offset)
    {
        float logdepth = gdepth_offset(texcoord, offset);
        return gdepthscale.x / (logdepth * gdepthscale.y + gdepthscale.z);
    }

    float gdepth_linear_offset(vec2 texcoord, ivec2 offset, out vec4 depth_data)
    {
        float logdepth = gdepth_offset(texcoord, offset, depth_data);
        return gdepthscale.x / (logdepth * gdepthscale.y + gdepthscale.z);
    }
#endif
