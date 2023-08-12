in vec4 vvertex;

#if DECAL_NORMALMAP
    in vec4 vtangent;
    out mat3 world;
#else
    out vec3 nvec;
#endif

in vec4 vnormal;
in vec3 vtexcoord0;
uniform mat4 camprojmatrix;
out vec4 texcoord0;

#if DECAL_PARALLAX
    uniform vec3 camera;
    out vec3 camvec;
#endif

#if DECAL_GLOW_PULSE
    uniform float millis;
    flat out float pulse;
#endif

void main(void)
{
    gl_Position = camprojmatrix * vvertex;
    texcoord0.xyz = vtexcoord0;
    texcoord0.w = 3.0*vnormal.w;

#if DECAL_NORMALMAP
    vec3 bitangent = cross(vnormal.xyz, vtangent.xyz) * vtangent.w;
    // calculate tangent -> world transform
    world = mat3(vtangent.xyz, bitangent, vnormal.xyz);
#else
    nvec = vnormal.xyz;
#endif

#if DECAL_PARALLAX
    camvec = camera - vvertex.xyz;
#endif

#if DECAL_GLOW_PULSE
    pulse = abs(fract(millis*pulseglowspeed.x)*2.0 - 1.0);
#endif
}
