#version 450
// 三角形顶点着色器：用内置 gl_VertexIndex 从常量数组取出 3 个顶点，不需要顶点缓冲。

const vec2 triangle[3] = vec2[3](
    vec2(-0.6, -0.4), vec2( 0.6, -0.4),
    vec2( 0.0, 0.6)
);

void main() {
    vec2 p = triangle[gl_VertexIndex];
    gl_Position = vec4(p, 0.5, 1.0);
}