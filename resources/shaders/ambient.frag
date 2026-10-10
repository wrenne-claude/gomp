#version 440
// Ambient light behind the conversation: a very low-alpha wash in the mode's hue whose strength
// follows agent activity. Dithered so the gradient never bands on 8-bit panels.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float strength;   // 0..1
    float aspect;     // width / height
    vec4 color;
    vec2 origin;      // light source in item coordinates (0..1)
};

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = qt_TexCoord0;
    vec2 d = (uv - origin) * vec2(aspect, 1.0);
    float drift = 0.04 * sin(time * 0.21);
    float falloff = exp(-dot(d, d) * (2.2 + drift));
    float a = falloff * strength * 0.16;
    a += (hash(uv * 1024.0 + time) - 0.5) / 255.0;
    a = clamp(a, 0.0, 1.0);
    fragColor = vec4(color.rgb * a, a) * qt_Opacity;
}
