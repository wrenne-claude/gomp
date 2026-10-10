#version 440
// omp's "classic" working shimmer: a cosine band of light sweeping left to right across text.
// The source is the text rendered in white; its alpha is recoloured from dim through muted to the
// accent as the band passes, using the same thresholds omp's terminal palette steps through.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float sweep;      // band centre, in item pixels
    float halfWidth;  // band half-width, in item pixels
    float span;       // item width, in pixels
    vec4 low;
    vec4 mid;
    vec4 high;
};
layout(binding = 1) uniform sampler2D source;

void main() {
    vec4 tex = texture(source, qt_TexCoord0);
    float d = abs(qt_TexCoord0.x * span - sweep);
    float i = d < halfWidth ? 0.5 * (1.0 + cos(3.14159265 * d / halfWidth)) : 0.0;
    vec3 c = mix(low.rgb, mid.rgb, smoothstep(0.0, 0.3, i));
    c = mix(c, high.rgb, smoothstep(0.45, 0.85, i));
    fragColor = vec4(c, 1.0) * tex.a * qt_Opacity;
}
