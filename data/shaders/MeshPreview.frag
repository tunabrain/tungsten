uniform mat4 View;
uniform bool NoShading;

in vec3 gNormal;
in vec3 gWorldPos;

out vec4 FragColor0;

void main() {
    vec3 camPos = (View*vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 eye = normalize(camPos - gWorldPos);
    vec3 normal = normalize(gNormal);
    
    float L = abs(dot(normal, eye));
    L = pow(L, 10.0)*0.4 + L*L*0.4 + L*0.2;
    
    FragColor0 = vec4(vec3(NoShading ? 1.0 : L), 1.0);
}