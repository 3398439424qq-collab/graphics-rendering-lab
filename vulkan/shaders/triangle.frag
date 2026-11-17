#version 450
// 三角形片元着色器：把三角形覆盖的像素统一输出为橙红色，写入 0 号颜色附件。

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(0.9,0.2,0.1,1.0);
}