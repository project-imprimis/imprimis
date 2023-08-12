vec2 screencoord(vec2 position, vec4 screen_params)
{
    return position * screen_params.xy + screen_params.zw;
}
