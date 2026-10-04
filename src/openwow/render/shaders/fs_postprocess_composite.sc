$input v_texcoord0

#include <bgfx_shader.sh>
#include "postprocess_composite.sh"

SAMPLER2D(s_ppTexColor,  0);
SAMPLER2D(s_ppTexBloom,  1);

uniform vec4 u_compositeParams;

uniform vec4 u_colorGrade;

uniform vec4 u_sourceUvScale;

// x = verzadiging-1, y = contrast-1, z = dither (in 8-bit stappen). Alles nul = origineel beeld.
uniform vec4 u_gradeExtra;

void main()
{
    vec4 scene = texture2D(s_ppTexColor,
                           v_texcoord0 * u_sourceUvScale.xy);
    vec3 bloom = texture2D(s_ppTexBloom,
                           v_texcoord0 * u_sourceUvScale.zw).rgb;

    vec4 composite = openwowCompositePassGlow(scene, bloom, u_compositeParams);
    vec3 result = composite.rgb;

    float gradeStrength = u_compositeParams.w;
    if (gradeStrength > 0.001)
    {
        result = mix(result, result * u_colorGrade.rgb, gradeStrength);
    }

    if (abs(u_gradeExtra.x) > 0.001)
    {
        float luma = dot(result, vec3(0.299, 0.587, 0.114));
        result = mix(vec3(luma, luma, luma), result, 1.0 + u_gradeExtra.x);
    }
    if (abs(u_gradeExtra.y) > 0.001)
    {
        result = (result - 0.5) * (1.0 + u_gradeExtra.y) + 0.5;
    }
    if (u_gradeExtra.z > 0.0)
    {
        // Interleaved gradient noise: breekt banding in lucht en mist.
        float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
        result += (noise - 0.5) * (u_gradeExtra.z / 255.0);
    }

    gl_FragColor = vec4(clamp(result, 0.0, 1.0),
                        clamp(composite.a, 0.0, 1.0));
}
