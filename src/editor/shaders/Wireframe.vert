uniform mat4 ModelViewProjection;
uniform mat4 Model;

in vec3 Position;
in vec3 Normal;
in vec2 TexCoord0;

out vec3 vNormal;
out vec3 vWorldPos;

void main() {
    gl_Position = ModelViewProjection*vec4(Position, 1.0);
    vNormal = mat3x3(Model)*Normal;
    vWorldPos = (Model*vec4(Position, 1.0)).xyz;
}