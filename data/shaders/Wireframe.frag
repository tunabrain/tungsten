uniform mat4 View;
uniform bool NoShading;

in vec3 gNormal;
in vec3 gWorldPos;
noperspective in vec4 gDist0;
noperspective in vec4 gDist1;

out vec4 FragColor0;

float dist(vec2 A, vec2 Adir) {
    vec2 AF = A - gl_FragCoord.xy;
    return abs(dot(AF, vec2(-Adir.y, Adir.x)));
}

void main() {
    vec3 camPos = (View*vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 eye = normalize(camPos - gWorldPos);
    vec3 normal = normalize(gNormal);
    
    float L = abs(dot(normal, eye));
    L = pow(L, 10.0)*0.4 + L*L*0.4 + L*0.2;
    
    float ratio;
    
    if (gDist1.z == 0.0 && gDist1.w == 0.0)
        ratio = min(gDist0.x, min(gDist0.y, gDist0.z));
    else {
        ratio = min(dist(gDist0.xy, gDist0.zw), dist(gDist1.xy, gDist1.zw));
        if (gDist0.x != gDist1.x || gDist0.y != gDist1.y)
            ratio = min(ratio, dist(gDist0.xy, normalize(gDist0.xy - gDist1.xy)));
    }
    
    FragColor0 = vec4(mix(vec3(NoShading ? 1.0 : L), vec3(1.0, 0.53, 0.05), 1.0 - smoothstep(0.0, 1.5, ratio)), 1.0);
}