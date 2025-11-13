#if DECAL_FIRST_PASS
    layout(location = 0) out vec4 gcolor;
    layout(location = 0, index = 1) out vec4 gcolorblend;
#elif DECAL_SECOND_PASS
    #if GMAT_NORMAL_PACK_DATA
        layout(location = 0) out vec4 gnormal;
    #else
        layout(location = 0) out vec4 gnormal;
        layout(location = 0, index = 1) out vec4 gnormalblend;
    #endif // GMAT_NORMAL_PACK_DATA
#else
    layout(location = 0) out vec4 gcolor;
    layout(location = 0) vec4 gnormal;
#endif

uniform sampler2D diffusemap;
uniform vec4 colorparams;
in vec4 texcoord0;

#if DECAL_NORMALMAP
    uniform sampler2D normalmap;
    in mat3 world;
#else
    in vec3 nvec;
    #define bumpblend vec4(1.0)
#endif // DECAL_NORMALMAP

#if DECAL_PARALLAX
    in vec3 camvec;
#endif // DECAL_PARALLAX

#if (DECAL_GLOW || DECAL_SPECMAP)
    uniform sampler2D glowmap;
#endif // DECAL_GLOW || DECAL_SPECMAP

#if DECAL_GLOW_PULSE
    flat in float pulse;
#endif // DECAL_GLOW_PULSE

vec2 gettexcoord()
{
#if (DECAL_NORMALMAP && DECAL_PARALLAX)
    float height = texture(normalmap, texcoord0.xy).a;
    vec3 camvecn = normalize(camvec);
    return texcoord0.xy + (camvecn * world).xy*(height*parallaxscale.x + parallaxscale.y);
#else
    return texcoord0.xy;
#endif // DECAL_NORMALMAP && DECAL_PARALLAX
}

vec3 getnormal()
{
#if (DECAL_NORMALMAP && !DECAL_FIRST_PASS)
    vec3 bump = texture(normalmap, dtc).rgb*2.0 - 1.0;
    return bumpw = world * bump;
#else
    return nvec;
#endif // DECAL_NORMALMAP && !DECAL_FIRST_PASS
}

void main(void)
{
    vec2 dtc = gettexcoord();
    vec3 normal = getnormal();
    vec4 diffuse = texture(diffusemap, dtc);

#if DECAL_GLOW
    vec4 glowspec = texture(glowmap, dtc);
    #define glow glowspec.rgb
    #define spec glowspec.a

    #if DECAL_GLOW_PULSE
        glow *= mix(glowcolor.xyz, pulseglowcolor.xyz, pulse);
    #else
        glow *= glowcolor.xyz;
    #endif // DECAL_GLOW_PULSE

    glow *= diffuse.a;
#endif // DECAL_GLOW

#if DECAL_FIRST_PASS
    #if (DECAL_SPECMAP && !DECAL_GLOW)
        #if (!DECAL_NORMALMAP || DECAL_PARALLAX)
            float spec = texture(glowmap, dtc).r;
        #else
            float spec = texture(normalmap, dtc).a;
        #endif // !DECAL_NORMALMAP || DECAL_PARALLAX
    #endif // DECAL_SPECMAP && !DECAL_GLOW

    #if (DECAL_SPEC && DECAL_SPECMAP)
        gcolor.a = gspecpack(gloss.x, spec * specscale.x);
    #elif DECAL_SPEC
        gcolor.a = gspecpack(gloss.x, specscale.x);
    #endif // DECAL_SPEC && DECAL_SPECMAP
#endif // DECAL_FIRST_PASS

#if DECAL_SECOND_PASS
    #if DECAL_GLOW
        vec3 gcolor = diffuse.rgb*colorparams.rgb;
    #endif
#else
    gcolor.rgb = diffuse.rgb*colorparams.rgb;
#endif // DECAL_SECOND_PASS

#if !DECAL_FIRST_PASS
    gnormal = vec4(normalize(nvec), 0);

    #if DECAL_GLOW
        float packnorm = gglowpack(gcolor.rgb, glow);
        gnormpack(gnormal, packnorm);
    #else
        gnormpack(gnormal);
    #endif // DECAL_GLOW
#else
    #if DECAL_GLOW
        gglowpack(gcolor.rgb, glow);
    #endif // DECAL_GLOW
#endif // !DECAL_FIRST_PASS

    float inside = clamp(texcoord0.z, 0.0, 1.0) * clamp(texcoord0.w, 0.0, 1.0);
    float alpha = inside * diffuse.a;

#if DECAL_FIRST_PASS
    gcolor.rgb *= inside;
    gcolor.a *= alpha;
    gcolorblend = vec4(alpha);
#elif DECAL_SECOND_PASS
    #if GMAT_NORMAL_PACK_DATA
        gnormal.a = alpha * bumpblend.x;
    #else
        gnormalblend = vec4(alpha * bumpblend.x);
    #endif // GMAT_NORMAL_PACK_DATA
#else
    gcolor.rgb *= inside;
    gcolor.a = alpha;
    #if DECAL_KEEP_NORMALS
        gnormal = vec4(0.0);
    #else
        gnormal.rgb *= alpha * bumpblend.x;
        gnormal.a = alpha * bumpblend.x;
    #endif // DECAL_KEEP_NORMALS
#endif
}
