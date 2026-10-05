$input a_position, a_color0, a_texcoord0
$output v_color0, v_texcoord0, v_viewDist, v_fogRay

#include <bgfx_shader.sh>
#include "world_fog.sh"

void main()
{
    vec4 viewPos = mul(u_modelView, vec4(a_position, 1.0));
    gl_Position = mul(u_viewProj, vec4(a_position, 1.0));
    v_color0    = a_color0;
    v_texcoord0 = a_texcoord0;
    v_viewDist = openwowWorldFogDepth(viewPos.xyz);
    v_fogRay = openwowFogRay(viewPos.xyz);
}
