$input a_position, a_color0
$output v_color0

#include <bgfx_shader.sh>
#include "world_fog.sh"

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));

    // De koepel is op de camera gecentreerd met de wereldassen (z = omhoog): a_position is dus de
    // richting. Met fogModel 1 gaat de lage band over in de verre fogkleur; anders is dit a_color0.
    vec3 dir = a_position / max(length(a_position), 0.0001);
    v_color0 = vec4(openwowSkyHorizon(a_color0.rgb, dir), a_color0.a);
}
