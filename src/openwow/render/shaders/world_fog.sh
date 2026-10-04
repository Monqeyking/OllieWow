#ifndef OPENWOW_WORLD_FOG_SH
#define OPENWOW_WORLD_FOG_SH

// Moderne fog (cvar fogModel), een port van Benilla's fog_hook.wgsl (Everwood graphics).
// Per frame gezet door WorldScene; alles nul = de originele lineaire fog van 1.12.
//   u_fogModern.x   = fogeinde van de scene in yd (0 = classic)
//   u_fogEndColor   = rgb: eindfogkleur, w: einddistance in yd
uniform vec4 u_fogModern;
uniform vec4 u_fogEndColor;

float openwowWorldFogDepth(vec3 viewPosition)
{
    return max(viewPosition.z, 0.0);
}

// 1 als de moderne fog voor dit paar (start, einde) geldt: alleen paren die op het fogeinde van
// de scene eindigen; een interieur-WMO-paar blijft classic.
float openwowFogIsModern(vec4 fogParams)
{
    return step(0.5, u_fogModern.x) * step(abs(fogParams.y - u_fogModern.x), 0.01);
}

// Overleving: 1 = geen fog, 0 = volledig in de fog. Bewust zonder vroege return (de DX11-compiler
// meldt die zonder optimalisatie als "potentially uninitialized"); met de modus uit is het
// resultaat precies de classic lineaire fog (mix met gewicht 0).
float openwowLinearFogVisibility(vec4 fogParams, float fogDepth)
{
    float classicVisibility = clamp((fogParams.y - fogDepth) /
                                    max(fogParams.y - fogParams.x, 0.0001), 0.0, 1.0);

    // Moderne wet: exponentieel vanaf het begin van de zone, zo dat 60 % overleeft op het midden
    // van het 1.12-bereik, en over het laatste stuk lineair naar nul zodat de fog eindigt waar de
    // zone hem bedoeld heeft.
    float len = max(fogParams.y - fogParams.x, 0.0001);
    float k = 1.0216512 / len;
    float x = max(fogDepth - fogParams.x, 0.0);
    float expo = exp(-k * x);
    float fadeFrom = max(fogParams.x, 0.3 * fogParams.y);
    float endFade = clamp((fogParams.y - fogDepth) / max(fogParams.y - fadeFrom, 0.0001), 0.0, 1.0);
    float modernVisibility = min(expo, endFade);

    return mix(classicVisibility, modernVisibility, openwowFogIsModern(fogParams));
}

// De fogkleur op afstand fogDepth: verschuift naar de eindfogkleur met (d / einde)^3, zodat de
// verre wereld in dezelfde kleur eindigt als de horizon van de lucht.
vec3 openwowFogRgb(vec3 fogColor, vec4 fogParams, float fogDepth)
{
    float t = clamp(fogDepth / max(u_fogEndColor.w, 0.0001), 0.0, 1.0);
    float weight = openwowFogIsModern(fogParams) * step(0.0001, u_fogEndColor.w) * t * t * t;
    return mix(fogColor, u_fogEndColor.rgb, weight);
}
#endif