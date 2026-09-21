$input a_position
$output v_viewDepth

#include <bgfx_shader.sh>
#include "world_fog.sh"

void main()
{
    vec4 viewPosition = mul(u_modelView, vec4(a_position, 1.0));
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));

    // De far band is een BACKDROP, geen deel van de dieptepartitie met de
    // gedetailleerde wereld. De referentie tekent hem met een eigen projectie
    // (near = farclip - 33, far = horizonfarclip) in een gecomprimeerd
    // dieptebereik [0.955, 0.96] ([0x80febc] = 0.96, [0x80fec4] = 0.005), zodat
    // hij nooit iets van de gedetailleerde wereld kan occluderen. Zonder die
    // compressie vecht hij op de grens van het geladen gebied om dezelfde diepte:
    // de dunne donkere lijnen die met de camera meebewegen.
    // Zie benilla-assets/src/shaders/wdl.wgsl (de far-band backdrop-wet).
    float normalizedDepth = clamp(gl_Position.z / gl_Position.w, 0.0, 1.0);
    // Piepklein bereik vlak onder 1.0, niet [0.955, 0.96]: bij ONZE projectie
    // reikt het verre terrein dieper dan 0.96, dus daar zou de backdrop het
    // occluderen (het hele beeld werd paars). Alles wat wij tekenen zit onder
    // 0.99999, en de lege dieptebuffer is 1.0 -- dus hier ligt hij gegarandeerd
    // achter de wereld maar voor de leegte. De referentie gebruikt [0.955, 0.96]
    // omdat hun gedetailleerde terrein nooit zo diep komt.
    gl_Position.z = (0.99999 + 0.000009 * normalizedDepth) * gl_Position.w;

    v_viewDepth = openwowWorldFogDepth(viewPosition.xyz);
}
