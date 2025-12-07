#version 430

in vec3 vNormal;
in vec3 vPosition;

uniform vec3 uMaterialColor;

out vec4 FragColor;

void main()
{
    vec3 lightDir = normalize(vec3(0.0, 1.0, -1.0));
    vec3 normal = normalize(vNormal);
    float ambient = 0.2;
    float diffuse = max(dot(normal, lightDir), 0.0);
    float lighting = ambient + diffuse;
    vec3 color = uMaterialColor * lighting;
    FragColor = vec4(color, 1.0);
}
