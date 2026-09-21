$input v_texcoord0, v_color0, v_viewDist, v_worldPos

#include <bgfx_shader.sh>
#include "world_fog.sh"

#include "terrain_params.sh"

void main()
{
    // Match Benilla's detailed-world farclip wall: terrain fragments beyond
    // the planar eye-Z farclip must not survive rasterization.
    if (u_terrainFogParams.w > 0.0 && v_viewDist > u_terrainFogParams.w)
    {
        discard;
    }

    // Classic terrain has only the per-chunk MCSH bake. Do not sample the
    // optional view-following shadow map here: its light matrix is rebuilt
    // from the camera frustum and makes dark patches move with pitch.
    const float shadowVisibility = 1.0;
    vec3 shadowModulate = mix(u_terrainShadowMod.rgb, vec3_splat(1.0), shadowVisibility);
    // De 2.0 compenseerde de halve schaal van de MCCV hierboven; die
    // vermenigvuldiging is weg, dus de compensatie ook (referentie: MOD 1x).
    vec3 litColor = u_terrainColor.rgb * v_color0.rgb * shadowModulate;

    float fogFactor = openwowLinearFogVisibility(u_terrainFogParams, v_viewDist);
    gl_FragColor = vec4(mix(u_terrainFogColor.rgb, litColor, fogFactor), u_terrainColor.a);
}
