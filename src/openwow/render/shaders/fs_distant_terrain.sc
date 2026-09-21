$input v_viewDepth

#include <bgfx_shader.sh>
#include "world_fog.sh"

#include "distant_terrain_params.sh"

void main()
{
    // Match Benilla's WDL far-band partition: the coarse hull reaches 33 yd
    // inside the detailed world's farclip wall, but never contributes nearer
    // than that overlap plane. The previous OpenWow shader omitted this
    // near-side discard and relied only on an ad-hoc vertex depth push; that
    // lets the pushed WDL depth win against detailed terrain at grazing angles.
    float farclip = u_distantTerrainFogParams.w;
    if (farclip > 0.0 && v_viewDepth < farclip - 33.0)
    {
        discard;
    }

    float fogVisibility = openwowLinearFogVisibility(u_distantTerrainFogParams, v_viewDepth);
    gl_FragColor = vec4(mix(u_distantTerrainFogColor.rgb, vec3_splat(1.0), fogVisibility), 1.0);
}
