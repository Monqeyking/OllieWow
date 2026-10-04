// Gedeelde schaduwontvangst voor de dynamische zon-schaduwkaart (stabiel, rond de camera).
// Slot 5 + u_shadowMtx/u_shadowParams worden door ShadowRenderData::BindShadowState gebonden.
#ifndef OPENWOW_SHADOW_RECEIVE_SH
#define OPENWOW_SHADOW_RECEIVE_SH

SAMPLER2DSHADOW(s_shadowMap, 5);
uniform mat4 u_shadowMtx;
uniform vec4 u_shadowParams;   // x = bias (genormaliseerde diepte), y = 1/resolutie, z = aan, w = straal

float sampleShadowMap(vec3 coord)
{
    return shadow2D(s_shadowMap, coord);
}

// 1 = belicht, 0 = in de schaduw (9 taps PCF, zachte uitfade aan de kaartrand).
float dynamicShadowVisibility(vec3 worldPos)
{
    if (u_shadowParams.z < 0.5) {
        return 1.0;
    }
    vec4 shadowCoord = mul(u_shadowMtx, vec4(worldPos, 1.0));
    vec3 p = shadowCoord.xyz / shadowCoord.w;
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z < 0.0 || p.z > 1.0) {
        return 1.0;
    }
    float texel = u_shadowParams.y;
    float depth = p.z - u_shadowParams.x;
    float lit = 0.0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            lit += sampleShadowMap(
                vec3(p.x + float(dx) * texel, p.y + float(dy) * texel, depth));
        }
    }
    lit /= 9.0;
    float edge = max(abs(p.x - 0.5), abs(p.y - 0.5));
    float fade = clamp((edge - 0.40) / 0.10, 0.0, 1.0);
    return mix(lit, 1.0, fade);
}

#endif
