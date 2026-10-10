#version 440
// The Corona: gomp's activity visualisation. Every term is driven by real adapter state
// (see qml/Shell/Corona.qml); nothing animates without a reason.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;        // seconds
    float intensity;   // overall brightness 0..1
    float thinking;    // 0..1
    float streaming;   // 0..1
    float tools;       // 0..1
    float approval;    // 0..1
    float errorFlash;  // 0..1
    float rate;        // outward pulse speed, from tokens/s
    float segments;    // running tool count (arc segments)
    float satellites;  // running subagents
    float pixelSize;   // 1 / item size in pixels, for antialiasing
    vec4 color;        // state colour
    vec4 dimColor;     // ring at rest
};

const float TAU = 6.28318530718;

float band(float d, float halfWidth, float aa) {
    return 1.0 - smoothstep(halfWidth - aa, halfWidth + aa, abs(d));
}

void main() {
    vec2 p = qt_TexCoord0 * 2.0 - 1.0;
    // Error: a short radial shudder.
    p *= 1.0 + errorFlash * 0.025 * sin(time * 47.0);
    float r = length(p);
    float a = atan(p.y, p.x);
    float aa = pixelSize * 2.0;

    const float R = 0.62;
    float breathe = 0.82 + 0.18 * sin(time * TAU / 6.0);

    // Base ring, broken at the upper right like the gomp mark.
    float gapCenter = TAU * 0.125;
    float gap = smoothstep(0.30, 0.20, abs(mod(a - gapCenter + TAU * 0.5, TAU) - TAU * 0.5));
    float restGap = mix(gap, 0.0, max(max(streaming, tools), thinking));
    float ring = band(r - R, 0.045, aa) * (1.0 - restGap);

    // Tools: the ring splits into rotating arcs, one per running tool.
    float n = max(segments, 1.0);
    float arc = fract(a / TAU * n - time * 0.18);
    float seg = smoothstep(0.0, 0.04, arc) * smoothstep(0.78, 0.70, arc);
    ring *= mix(1.0, seg, tools);

    // Thinking: interference bands drifting inward inside the ring.
    float inner = smoothstep(R - 0.02, R - 0.22, r) * smoothstep(0.05, 0.2, r);
    float bands = 0.5 + 0.5 * sin(r * 34.0 + time * 3.2 + sin(a * 3.0 - time * 0.9) * 1.4);
    float think = inner * pow(bands, 6.0) * thinking * 0.75;

    // Streaming: pulses travelling outward; frequency follows the token rate.
    float outer = smoothstep(R, R + 0.06, r) * smoothstep(1.0, 0.78, r);
    float wave = pow(0.5 + 0.5 * sin((r - time * (0.12 + rate)) * 26.0), 10.0);
    float stream = outer * wave * streaming * 0.9;

    // Subagents: satellites orbiting just outside the ring.
    float sats = 0.0;
    for (int i = 0; i < 8; i++) {
        if (float(i) >= satellites) break;
        float ang = time * 0.55 + float(i) * TAU / max(satellites, 1.0);
        vec2 c = vec2(cos(ang), sin(ang)) * (R + 0.2);
        sats += 1.0 - smoothstep(0.035, 0.035 + aa * 1.5, length(p - c));
    }

    // Approval: the ring holds still and a beacon pulses at the gap.
    vec2 beaconPos = vec2(cos(gapCenter), sin(gapCenter)) * R;
    float pulse = 0.55 + 0.45 * sin(time * TAU);
    float beacon = (1.0 - smoothstep(0.06, 0.06 + aa * 2.0, length(p - beaconPos))) * approval;
    float halo = exp(-pow(length(p - beaconPos) * 7.0, 2.0)) * approval * pulse * 0.6;

    // Soft glow hugging the ring.
    float glow = exp(-pow((r - R) * 9.0, 2.0)) * 0.22 * intensity;

    float lit = (ring * mix(0.55, 1.0, intensity) * mix(breathe, 1.0, max(streaming, tools)))
                + think + stream + glow + halo;
    vec3 rgb = mix(dimColor.rgb, color.rgb, clamp(intensity + approval, 0.0, 1.0)) * lit;
    rgb += color.rgb * (sats + beacon);
    float alpha = clamp(lit + sats + beacon, 0.0, 1.0);
    fragColor = vec4(rgb, alpha) * qt_Opacity;
}
