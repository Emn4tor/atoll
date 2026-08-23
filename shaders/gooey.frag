// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Metaball threshold pass. Fed a blurred copy of the island's shapes, it cuts
// the blur back to a hard edge, so two shapes that drift close enough have
// their haloes overlap and fuse into one body before they ever touch.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float cutoff;
    float softness;
    vec4 tint;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    float mass = texture(source, qt_TexCoord0).a;
    float coverage = smoothstep(cutoff - softness, cutoff + softness, mass);
    fragColor = vec4(tint.rgb * tint.a, tint.a) * coverage * qt_Opacity;
}
