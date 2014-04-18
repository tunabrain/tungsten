layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

uniform bool Smooth;
uniform vec2 Resolution;

in vec3 vNormal[3];
in vec3 vWorldPos[3];

out vec3 gNormal;
out vec3 gWorldPos;
noperspective out vec4 gDist0;
noperspective out vec4 gDist1;

vec2 project(vec4 p) {
    return Resolution*(p.xy/p.w*0.5f + 0.5f);
}

void cornerCase(vec4 A, vec4 B, vec4 Aprime, vec4 Bprime) {
    vec3 n = cross(vWorldPos[1] - vWorldPos[0], vWorldPos[2] - vWorldPos[0]);
    
    vec2 Av = project(A);
    vec2 Bv = project(B);
    for (int i = 0; i < 3; ++i) {
        gDist0 = vec4(Av, normalize(Av - project(Aprime)));
        gDist1 = vec4(Bv, normalize(Bv - project(Bprime)));
        if (Smooth)
            gNormal = vNormal[i];
        else
            gNormal = n;
        gNormal = normalize(gNormal);
        gWorldPos = vWorldPos[i];
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
}

void commonCase() {
    vec3 n = cross(vWorldPos[1] - vWorldPos[0], vWorldPos[2] - vWorldPos[0]);
    vec2 p0 = project(gl_in[0].gl_Position);
    vec2 p1 = project(gl_in[1].gl_Position);
    vec2 p2 = project(gl_in[2].gl_Position);
    vec2 v0 = p2 - p1;
    vec2 v1 = p2 - p0;
    vec2 v2 = p1 - p0;
    float area = abs(v1.x*v2.y - v1.y*v2.x);
    vec4 dists = vec4(area/vec3(length(v0), length(v1), length(v2)), 0.0);
    
    for (int i = 0; i < 3; ++i) {
        switch (i) {
        case 0: gDist0 = dists.xwww; break;
        case 1: gDist0 = dists.wyww; break;
        case 2: gDist0 = dists.wwzw; break;
        default: gDist0 = vec4(0.0);
        }
        gDist1 = vec4(0.0);
        if (Smooth)
            gNormal = vNormal[i];
        else
            gNormal = n;
        gNormal = normalize(gNormal);
        gWorldPos = vWorldPos[i];
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
}

void main(void)
{
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;
    vec4 p2 = gl_in[2].gl_Position;
    
    int mask = (p0.z <= 0.0 ? 4 : 0) +
               (p1.z <= 0.0 ? 2 : 0) +
               (p2.z <= 0.0 ? 1 : 0);
    
    switch (mask) {
    case 0: commonCase(); break;
    case 1: cornerCase(p0, p1, p2, p2); break;
    case 2: cornerCase(p0, p2, p1, p1); break;
    case 3: cornerCase(p0, p0, p1, p2); break;
    case 4: cornerCase(p1, p2, p0, p0); break;
    case 5: cornerCase(p1, p1, p0, p2); break;
    case 6: cornerCase(p2, p2, p0, p1); break;
    default: break;
    }
}