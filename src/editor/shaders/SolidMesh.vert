uniform mat4 ModelViewProjection;

in vec3 Position;

void main() {
    gl_Position = ModelViewProjection*vec4(Position, 1.0);
}