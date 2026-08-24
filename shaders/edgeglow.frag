// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // Overall strength, 0..1.
    float intensity;
    // Half-width of the lit band, as a fraction of the screen height.
    float thickness;
    // How far the light has travelled out from the island, 0..~1.6. This is
    // the only thing here that moves, and it moves once, on the way in.
    float progress;
    // Screen width over height, so the band is as thick on the sides as on
    // the top rather than being stretched by the aspect ratio.
    float aspect;
    // Where the island sits, in 0..1 screen coordinates.
    vec2 origin;
    // The colour at the island, and the colour the far corners fade to.
    vec4 colorNear;
    vec4 colorFar;
};

void main()
{
    vec2 uv = qt_TexCoord0;

    // Distance to the nearest edge, measured in units of screen height.
    float dx = min(uv.x, 1.0 - uv.x) * aspect;
    float dy = min(uv.y, 1.0 - uv.y);
    float edge = min(dx, dy);

    float band = 1.0 - smoothstep(0.0, thickness, edge);
    // A steep curve keeps the light against the bezel. A gentler one washes
    // a long way inwards and starts to read as a coloured frame around the
    // desktop rather than as a glow coming off its edge.
    band = pow(band, 2.6);

    // The corners are where two bands meet, and leaving them at the sum of the
    // two reads as a bright square; a gentler bloom is what makes the outline
    // look continuous.
    float corner = (1.0 - smoothstep(0.0, thickness * 1.7, dx))
                 * (1.0 - smoothstep(0.0, thickness * 1.7, dy));
    band = clamp(band + corner * 0.3, 0.0, 1.0);

    // How far this pixel is from the island, which is the only thing the
    // colour depends on. Nothing here reads a clock: the light that appears
    // when the assistant is opened is the same light every time, and it stays
    // that colour for as long as it is up. A glow that cycles through hues, or
    // that follows whatever is playing, turns a panel somebody is reading into
    // a moving thing at the edge of their eye.
    vec2 delta = vec2((uv.x - origin.x) * aspect, uv.y - origin.y);
    float reach = length(delta);

    // The light does not appear everywhere at once: it runs outwards from the
    // island, which is what ties the effect to the thing that started it. Once
    // progress has passed the far corner this is 1 everywhere and stays there.
    float wave = smoothstep(progress + 0.28, progress - 0.06, reach);

    // One fade, laid out in space rather than in time: the assistant's colour
    // where the light comes from, easing into the second colour by the far
    // corners, and a little dimmer out there as well.
    float spread = smoothstep(0.0, 1.5, reach);
    vec3 tint = mix(colorNear.rgb, colorFar.rgb, spread);
    float falloff = mix(1.0, 0.74, spread);

    float alpha = clamp(band * wave * intensity * falloff, 0.0, 1.0);

    // Qt composites premultiplied.
    fragColor = vec4(tint * alpha, alpha) * qt_Opacity;
}
