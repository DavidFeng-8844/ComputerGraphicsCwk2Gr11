#version 430

in vec3 vNormal;
in vec3 vPosition;
in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform bool uUseTexture;

out vec4 FragColor;

void main()
{
	// Simplified directional light model (Exercise G.5)
	// Light direction: (0, 1, -1) normalized
	vec3 lightDir = normalize(vec3(0.0, 1.0, -1.0));
	
	// Normalize the normal
	vec3 normal = normalize(vNormal);
	
	// Ambient component
	float ambient = 0.2;
	
	// Diffuse component (n dot l)
	float diffuse = max(dot(normal, lightDir), 0.0);
	
	// Combine lighting
	float lighting = ambient + diffuse;
	
	// Get base color from texture or use default
	vec3 baseColor;
	if (uUseTexture)
	{
		baseColor = texture(uTexture, vTexCoord).rgb;
	}
	else
	{
		baseColor = vec3(0.8, 0.8, 0.8);
	}
	
	// Apply lighting to texture/color
	vec3 color = baseColor * lighting;
	
	FragColor = vec4(color, 1.0);
}

