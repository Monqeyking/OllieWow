#ifndef OPENWOW_WORLD_FOG_SH
#define OPENWOW_WORLD_FOG_SH

// Moderne fog (cvar fogModel), een port van Benilla's fog_hook.wgsl (Everwood graphics).
// Per frame gezet door WorldScene; alles nul = de originele lineaire fog van 1.12.
//   u_fogModern.x   = fogeinde van de scene in yd (0 = classic); yzw = de scene-fogkleur
//   u_fogEndColor   = rgb: eindfogkleur, w: einddistance in yd
//   u_fogSunDir     = xyz: richting naar de zon (wereldruimte), w: cosinus waar de zonlob begint
//   u_fogSunColor   = rgb: zon-fogkleur, w: sterkte (al met dag/nacht en fogSunGlow vermenigvuldigd)
uniform vec4 u_fogModern;
uniform vec4 u_fogEndColor;
uniform vec4 u_fogSunDir;
uniform vec4 u_fogSunColor;

// Planaire oogdiepte: de classic fog en de farclip-muur van terrein blijven hierop rekenen.
float openwowWorldFogDepth(vec3 viewPosition)
{
    return max(viewPosition.z, 0.0);
}

#if BGFX_SHADER_TYPE_VERTEX
// De fogstraal van een vertex: xyz = eenheidsrichting oog -> punt in wereldruimte, w = radiale
// afstand tot het oog (yd). De fragmentstap interpoleert dit; de moderne fog rekent radiaal en
// gebruikt de richting voor de zonlob.
vec4 openwowFogRay(vec3 viewPosition)
{
    float radial = length(viewPosition);
    vec3 dirView = viewPosition / max(radial, 0.0001);
    vec3 dirWorld = mul(u_invView, vec4(dirView, 0.0)).xyz;
    return vec4(dirWorld, radial);
}
#endif

// 1 als de moderne fog voor dit paar (start, einde) geldt: alleen paren die op het fogeinde van
// de scene eindigen; een interieur-WMO-paar blijft classic.
float openwowFogIsModern(vec4 fogParams)
{
    return step(0.5, u_fogModern.x) * step(abs(fogParams.y - u_fogModern.x), 0.01);
}

// Overleving: 1 = geen fog, 0 = volledig in de fog. Bewust zonder vroege return (de DX11-compiler
// meldt die zonder optimalisatie als "potentially uninitialized"); met de modus uit is het
// resultaat precies de classic lineaire fog op de planaire diepte (mix met gewicht 0).
float openwowLinearFogVisibility(vec4 fogParams, float fogDepth, vec4 fogRay)
{
    float classicVisibility = clamp((fogParams.y - fogDepth) /
                                    max(fogParams.y - fogParams.x, 0.0001), 0.0, 1.0);

    // Moderne wet, op de radiale afstand: exponentieel vanaf het begin van de zone, zo dat 60 %
    // overleeft op het midden van het 1.12-bereik, en over het laatste stuk lineair naar nul zodat
    // de fog eindigt waar de zone hem bedoeld heeft.
    float radial = fogRay.w;
    float len = max(fogParams.y - fogParams.x, 0.0001);
    float k = 1.0216512 / len;
    float x = max(radial - fogParams.x, 0.0);
    float expo = exp(-k * x);
    float fadeFrom = max(fogParams.x, 0.3 * fogParams.y);
    float endFade = clamp((fogParams.y - radial) / max(fogParams.y - fadeFrom, 0.0001), 0.0, 1.0);
    float modernVisibility = min(expo, endFade);

    return mix(classicVisibility, modernVisibility, openwowFogIsModern(fogParams));
}

// De zonlob: 0 buiten de kegel rond de zon, 1 recht in de zon (kubisch).
float openwowFogSunLobe(vec3 dir)
{
    float a = u_fogSunDir.w;
    float s = clamp((dot(dir, u_fogSunDir.xyz) - a) / max(1.0 - a, 0.001), 0.0, 1.0);
    return s * s * s;
}

// De fogkleur langs de straal: verschuift naar de eindfogkleur met (d / einde)^3, zodat de verre
// wereld in dezelfde kleur eindigt als de horizon van de lucht, en leunt rond de zon naar de
// zon-fogkleur. Alleen de scene-fogkleur wordt herkleurd: de zwarte/witte/grijze fog van
// additieve en Mod-batches blijft zoals ze is.
vec3 openwowFogRgb(vec3 fogColor, vec4 fogParams, float fogDepth, vec4 fogRay)
{
    vec3 delta = abs(fogColor - u_fogModern.yzw);
    float sceneColour = 1.0 - step(0.004, max(delta.x, max(delta.y, delta.z)));
    float modern = openwowFogIsModern(fogParams) * sceneColour;

    float t = clamp(fogRay.w / max(u_fogEndColor.w, 0.0001), 0.0, 1.0);
    float endWeight = modern * step(0.0001, u_fogEndColor.w) * t * t * t;
    vec3 colour = mix(fogColor, u_fogEndColor.rgb, endWeight);

    vec3 dir = fogRay.xyz / max(length(fogRay.xyz), 0.0001);
    float sunWeight = modern * u_fogSunColor.w * openwowFogSunLobe(dir);
    return mix(colour, u_fogSunColor.rgb, sunWeight);
}

// De lage band van de luchtkoepel onder de moderne fog: de verre fogkleur langs `dir` (wereldruimte,
// z = omhoog) vervangt de horizon en gaat binnen 15 graden elevatie over in de eigen kleur van de
// koepel (een mix, nooit een optelling, zodat een al warme avondring niet voorbij zichzelf gaat).
vec3 openwowSkyHorizon(vec3 skyColour, vec3 dir)
{
    float modern = step(0.5, u_fogModern.x) * step(0.0001, u_fogEndColor.w);
    vec3 farColour = mix(u_fogEndColor.rgb, u_fogSunColor.rgb, u_fogSunColor.w * openwowFogSunLobe(dir));
    float elevation = asin(clamp(dir.z, -1.0, 1.0));
    float weight = modern * (1.0 - smoothstep(0.0, 0.26179939, elevation));
    return mix(skyColour, farColour, weight);
}
#endif
