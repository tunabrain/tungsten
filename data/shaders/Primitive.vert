uniform mat4 ModelViewProjection;

in vec3 Position;
in vec4 Color;

out vec4 vColor;

void main() {
    gl_Position = ModelViewProjection*vec4(Position, 1.0);
    vColor = Color;
}