////////////
// NORMAL //
////////////



void gnormpack_alpha(inout vec4 normal, float data)
{
    normal.rgb = normal.rgb * 0.5 + 0.5;
    normal.a = data;
}

void gnormpack_rgb(inout vec4 normal, float data)
{
    normal.rgb = normal.rgb * sqrt(0.25 + 0.5 * data) * 0.5 + 0.5;
}

void gnormpack(inout vec4 normal)
{
    normal.rgb = normal.rgb * 0.5 + 0.5;
}

void gnormpack(inout vec4 normal, float data)
{
#if GMAT_NORMAL_PACK_DATA
    gnormpack_rgb(normal, data);
#else
    gnormpack_alpha(normal, data);
#endif
}



//////////////
// SPECULAR //
//////////////



float gspecpack(float gloss, float spec)
{
    return (gloss * 85.0/255.0 + 0.5/255.0) + 84.0/255.0*clamp(1.5 - 1.5/(1.0 + spec), 0.0, 1.0);
}



//////////
// GLOW //
//////////



float gglowpack(inout vec3 color, float glowk, float colork)
{
    glowk /= glowk + colork + 1.0e-3;
    color.rgb = color.rgb * (1.0 - 2.0*glowk*(glowk - 1.0));
    return 1.0-glowk;
}

float gglowpack(inout vec3 color, vec3 glow)
{
    float colork = max(color.r, max(color.g, color.b)), glowk = max(glow.r, max(glow.g, glow.b));
    glowk /= glowk + colork + 1.0e-3;
    color.rgb = mix(color.rgb, glow, glowk) * (1.0 - 2.0*glowk*(glowk - 1.0));
    return 1.0-glowk;
}
