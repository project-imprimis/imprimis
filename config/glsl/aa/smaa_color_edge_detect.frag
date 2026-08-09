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

uniform sampler2DRect tex0;
in vec2 texcoord0;
layout(location = 0) out vec4 fragcolor;

void main(void)
{
    // Calculate color deltas:
    vec3 C = texture(tex0, texcoord0).rgb;
    vec3 Cleft = abs(C - textureOffset(tex0, texcoord0, ivec2(-1, 0)).rgb);
    vec3 Ctop = abs(C - textureOffset(tex0, texcoord0, ivec2(0, -1)).rgb);
    vec2 delta;
    delta.x = max(max(Cleft.r, Cleft.g), Cleft.b);
    delta.y = max(max(Ctop.r, Ctop.g), Ctop.b);

    // We do the usual threshold:
    vec2 edges = step(SMAA_THRESHOLD, delta);

    // Then discard if there is no edge:
#if SMAA_DISCARD_ENABLED
    if (edges.x + edges.y == 0.0) discard;
    else
#else
    if (edges.x + edges.y > 0.0)
#endif
    {
        // Calculate right and bottom deltas:
        vec3 Cright = abs(C - textureOffset(tex0, texcoord0, ivec2(1, 0)).rgb);
        vec3 Cbottom = abs(C - textureOffset(tex0, texcoord0, ivec2(0, 1)).rgb);
        // Calculate left-left and top-top deltas:
        vec3 Cleftleft = abs(C - textureOffset(tex0, texcoord0, ivec2(-2, 0)).rgb);
        vec3 Ctoptop = abs(C - textureOffset(tex0, texcoord0, ivec2(0, -2)).rgb);
        // Calculate the maximum delta in the direct neighborhood:
        vec3 t = max(max(Cright, Cbottom), max(Cleftleft, Ctoptop));
        // Calculate the final maximum delta:
        float maxDelta = max(max(delta.x, delta.y), max(max(t.r, t.g), t.b));

        // Local contrast adaptation in action:
        edges *= step(maxDelta, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta);
    }

    fragcolor = vec4(edges, 0.0, 0.0);
}
