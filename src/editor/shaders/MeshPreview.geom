layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

uniform bool Smooth;

in vec3 vNormal[3];
in vec3 vWorldPos[3];

out vec3 gNormal;
out vec3 gWorldPos;

void main(void)
{
    vec3 n = cross(vWorldPos[1] - vWorldPos[0], vWorldPos[2] - vWorldPos[0]);
    
    for (int i = 0; i < 3; ++i) {
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