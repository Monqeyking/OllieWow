// Gedeelde schaduwontvangst voor de dynamische zon-schaduwkaart (stabiel, rond de speler).
// Slot 5 + u_shadowMtx/u_shadowParams (near) en slot 6 + u_shadowMtxFar/u_shadowParamsFar (far)
// worden door ShadowRenderData::BindShadowState gebonden.
#ifndef OPENWOW_SHADOW_RECEIVE_SH
#define OPENWOW_SHADOW_RECEIVE_SH

SAMPLER2DSHADOW(s_shadowMap, 5);
SAMPLER2DSHADOW(s_shadowMapFar, 6);
uniform mat4 u_shadowMtx;
uniform vec4 u_shadowParams;   // x = bias (genormaliseerde diepte), y = 1/resolutie, z = sterkte (0..1), w = straal
uniform mat4 u_shadowMtxFar;
uniform vec4 u_shadowParamsFar; // idem voor de verre kaart; z = 0 betekent: geen verre kaart

float sampleShadowMap(vec3 coord)
{
    return shadow2D(s_shadowMap, coord);
}

float sampleShadowMapFar(vec3 coord)
{
    return shadow2D(s_shadowMapFar, coord);
}

// 1 = belicht, 0 = in de schaduw. Twee cascades: de near-kaart (scherp, 9 taps PCF) en
// daarbuiten de far-kaart (grover, 5 taps); aan de rand van de near-kaart gaat de ene zacht over
// in de andere, en aan de rand van de far-kaart in "belicht". Zonder far-kaart (sterkte 0) is dit
// het gedrag van alleen de near-kaart. Geen early returns.
float dynamicShadowVisibility(vec3 worldPos)
{
    // Near.
    vec4 nearCoord = mul(u_shadowMtx, vec4(worldPos, 1.0));
    vec3 p = nearCoord.xyz / nearCoord.w;
    float nearInside = (p.x >= 0.0 && p.x <= 1.0 && p.y >= 0.0 && p.y <= 1.0 && p.z >= 0.0 && p.z <= 1.0) ? 1.0 : 0.0;
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
    float nearWeight = (1.0 - clamp((edge - 0.40) / 0.10, 0.0, 1.0)) * nearInside;
    float nearStrength = clamp(u_shadowParams.z, 0.0, 1.0);
    float nearVisibility = mix(1.0, lit, nearStrength);

    // Far.
    vec4 farCoord = mul(u_shadowMtxFar, vec4(worldPos, 1.0));
    vec3 q = farCoord.xyz / farCoord.w;
    float farInside = (q.x >= 0.0 && q.x <= 1.0 && q.y >= 0.0 && q.y <= 1.0 && q.z >= 0.0 && q.z <= 1.0) ? 1.0 : 0.0;
    float farTexel = u_shadowParamsFar.y;
    float farDepth = q.z - u_shadowParamsFar.x;
    float farLit = sampleShadowMapFar(vec3(q.x, q.y, farDepth))
                 + sampleShadowMapFar(vec3(q.x + farTexel, q.y, farDepth))
                 + sampleShadowMapFar(vec3(q.x - farTexel, q.y, farDepth))
                 + sampleShadowMapFar(vec3(q.x, q.y + farTexel, farDepth))
                 + sampleShadowMapFar(vec3(q.x, q.y - farTexel, farDepth));
    farLit *= 0.2;
    float farEdge = max(abs(q.x - 0.5), abs(q.y - 0.5));
    float farWeight = (1.0 - clamp((farEdge - 0.38) / 0.12, 0.0, 1.0)) * farInside;
    float farStrength = clamp(u_shadowParamsFar.z, 0.0, 1.0);
    float farVisibility = mix(1.0, farLit, farStrength * farWeight);

    // Debug (u_shadowParamsFar.w): 1 = alleen far, 2 = alleen near, 3 = dekking als grijstinten.
    float debugMode = u_shadowParamsFar.w;
    float combined = mix(farVisibility, nearVisibility, nearWeight);
    float onlyFar = farVisibility;
    float onlyNear = mix(1.0, nearVisibility, nearWeight);
    float coverage = mix(mix(1.0, 0.8, farWeight), 0.5, nearWeight);
    float result = combined;
    result = debugMode > 0.5 && debugMode < 1.5 ? onlyFar : result;
    result = debugMode > 1.5 && debugMode < 2.5 ? onlyNear : result;
    result = debugMode > 2.5 ? coverage : result;
    return result;
}

#endif
