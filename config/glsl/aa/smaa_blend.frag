// Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
// Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
// Copyright (C) 2013 Belen Masia (bmasia@unizar.es)
// Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)
// Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to
// do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software. As clarification, there
// is no requirement that the copyright notice and permission be included in
// binary distributions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//                  _______  ___  ___       ___           ___
//                 /       ||   \/   |     /   \         /   \
//                |   (---- |  \  /  |    /  ^  \       /  ^  \
//                 \   \    |  |\/|  |   /  /_\  \     /  /_\  \
//              ----)   |   |  |  |  |  /  _____  \   /  _____  \
//             |_______/    |__|  |__| /__/     \__\ /__/     \__\
//
//                               E N H A N C E D
//       S U B P I X E L   M O R P H O L O G I C A L   A N T I A L I A S I N G
//
//                         http://www.iryoku.com/smaa/

in vec2 texcoord0;
uniform sampler2DRect tex0, tex1;

#if SMAA_SPLIT
    uniform sampler2DRect tex2, tex3;
#endif

layout(location = 0) out vec4 fragcolor;

// Neighborhood Blending Pixel Shader (Third Pass)

void main(void)
{
    // Fetch the blending weights for current pixel:
    vec4 a;
    a.xz = texture(tex1, texcoord0).rb;
    a.y = textureOffset(tex1, texcoord0, ivec2(0, 1)).g;
    a.w = textureOffset(tex1, texcoord0, ivec2(1, 0)).a;

    // Up to 4 lines can be crossing a pixel (one through each edge). We
    // favor blending by choosing the line with the maximum weight for each
    // direction:
    vec2 offset;
    offset.x = a.w > a.z ? a.w : -a.z; // left vs. right
    offset.y = a.y > a.x ? a.y : -a.x; // top vs. bottom

    // Then we go in the direction that has the maximum weight:
    if (abs(offset.x) > abs(offset.y)) // horizontal vs. vertical
        offset.y = 0.0;
    else
        offset.x = 0.0;

    // We exploit bilinear filtering to mix current pixel with the chosen
    // neighbor:
    fragcolor = texture(tex0, texcoord0 + offset);

#if SMAA_SPLIT
    a.xz = texture(tex3, texcoord0).rb;
    a.y = textureOffset(tex3, texcoord0, ivec2(0, 1)).g;
    a.w = textureOffset(tex3, texcoord0, ivec2(1, 0)).a;
    offset.x = a.w > a.z ? a.w : -a.z;
    offset.y = a.y > a.x ? a.y : -a.x;
    if (abs(offset.x) > abs(offset.y))
        offset.y = 0.0;
    else
        offset.x = 0.0;
    fragcolor = 0.5*(fragcolor + texture(tex2, texcoord0 + offset));
#endif
}
