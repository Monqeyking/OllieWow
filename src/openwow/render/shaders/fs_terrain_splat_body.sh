
#ifndef OPENWOW_TERRAIN_LAYER_ARRAY
#define OPENWOW_TERRAIN_LAYER_ARRAY 0
#endif

#include <bgfx_shader.sh>
#include "world_fog.sh"

#include "terrain_params.sh"

#if OPENWOW_TERRAIN_LAYER_ARRAY

SAMPLER2DARRAY(s_terrainLayers, 0);
#else

SAMPLER2D(s_terrainTex0, 0);
SAMPLER2D(s_terrainTex1, 1);
SAMPLER2D(s_terrainTex2, 2);
SAMPLER2D(s_terrainTex3, 3);
#endif

SAMPLER2DARRAY(s_terrainAlpha, 4);

void main()
{
    // Match Benilla's detailed-world farclip wall: terrain fragments beyond
    // the planar eye-Z farclip must not survive rasterization.
    if (u_terrainFogParams.w > 0.0 && v_viewDist > u_terrainFogParams.w)
    {
        discard;
    }

    // Benilla keeps one 64x64 alpha map per chunk in a texture array. Clamp
    // the local bilinear footprint to half a texel so linear filtering cannot
    // sample outside this chunk's map. The vertex carries the array slice.
    const float kAlphaMapSize = 64.0;
    const float kAlphaHalfTexel = 0.5;
    vec2 alphaUV = clamp(v_alphaUV,
                         vec2_splat(kAlphaHalfTexel / kAlphaMapSize),
                         vec2_splat(1.0 - kAlphaHalfTexel / kAlphaMapSize));
    vec4 alphaBytes = texture2DArray(
        s_terrainAlpha, vec3(alphaUV, floor(v_alphaSlice + 0.5)));

#if OPENWOW_TERRAIN_LAYER_ARRAY

    vec4 layerSlice = floor(v_layerSlice + vec4_splat(0.5));
    vec4 c0 = texture2DArray(s_terrainLayers, vec3(v_texcoord0, layerSlice.x));
    vec4 c1 = texture2DArray(s_terrainLayers, vec3(v_texcoord0, layerSlice.y));
    vec4 c2 = texture2DArray(s_terrainLayers, vec3(v_texcoord0, layerSlice.z));
    vec4 c3 = texture2DArray(s_terrainLayers, vec3(v_texcoord0, layerSlice.w));
#else
    vec4 c0 = texture2D(s_terrainTex0, v_texcoord0);
    vec4 c1 = texture2D(s_terrainTex1, v_texcoord0);
    vec4 c2 = texture2D(s_terrainTex2, v_texcoord0);
    vec4 c3 = texture2D(s_terrainTex3, v_texcoord0);
#endif

    vec4 blended = mix(c0, c1, alphaBytes.r);
    blended = mix(blended, c2, alphaBytes.g);
    blended = mix(blended, c3, alphaBytes.b);

    // Classic terrain uses only the static per-chunk MCSH bake. The
    // view-following shadow map is intentionally not sampled: its projection
    // changes with camera pitch and produces moving dark patches.
    float shadowVisibility = alphaBytes.a;
    vec3 bakedShadowModulate =
        mix(u_terrainShadowMod.rgb, vec3_splat(1.0), shadowVisibility);
    // The uniform alpha is the render-flag switch; keep the MCSH bake available
    // for normal rendering while retaining the Vanilla MOD 1x terrain path.
    vec3 shadowModulate =
        mix(vec3_splat(1.0), bakedShadowModulate, u_terrainShadowMod.a);
    vec3 litColor = blended.rgb * v_color0.rgb * u_terrainColor.rgb
                  * shadowModulate;

    float fogFactor = openwowLinearFogVisibility(u_terrainFogParams, v_viewDist);
    gl_FragColor = vec4(mix(u_terrainFogColor.rgb, litColor, fogFactor), 1.0);
}
