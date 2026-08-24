// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // Seconds since the glow was lit. Everything that moves reads this.
    float time;
    // Overall strength, 0..1.
    float intensity;
    // Half-width of the lit band, as a fraction of the screen height.
    float thickness;
    // How far the light has travelled out from the island, 0..~1.6.
    float progress;
    // Screen width over height, so the band is as thick on the sides as on
    // the top rather than being stretched by the aspect ratio.
    float aspect;
    // Where the island sits, in 0..1 screen coordinates.
    vec2 origin;
    vec4 colorA;
    vec4 colorB;
    vec4 colorC;
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

    // The light does not appear everywhere at once: it runs outwards from the
    // island, which is what ties the effect to the thing that started it.
    vec2 delta = vec2((uv.x - origin.x) * aspect, uv.y - origin.y);
    float reach = length(delta);
    float wave = smoothstep(progress + 0.28, progress - 0.06, reach);

    // Hue travels along the border rather than sitting still.
    float phase = uv.x * 1.3 + uv.y * 0.9 + time * 0.11;
    float a = 0.5 + 0.5 * sin(phase * 6.2831853);
    float b = 0.5 + 0.5 * sin(phase * 6.2831853 + 2.0943951);
    float c = 0.5 + 0.5 * sin(phase * 6.2831853 + 4.1887902);
    float sum = max(a + b + c, 0.001);
    vec3 tint = (colorA.rgb * a + colorB.rgb * b + colorC.rgb * c) / sum;

    // A slow breath under a faster ripple: still enough to be ambient, alive
    // enough that the screen never looks merely tinted.
    float breath = 0.82 + 0.18 * sin(time * 1.7);
    float ripple = 0.90 + 0.10 * sin(phase * 18.0 - time * 3.0);

    float alpha = clamp(band * wave * intensity * breath * ripple, 0.0, 1.0);

    // Qt composites premultiplied.
    fragColor = vec4(tint * alpha, alpha) * qt_Opacity;
}
