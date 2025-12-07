#version 430

in vec3 vNormal;
in vec3 vPosition;
in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform bool uUseTexture;

// Camera position for view vector
uniform vec3 uCameraPosition;

// Directional light
uniform vec3 uDirLightDirection;
uniform vec3 uDirLightColor;

// Point lights (3 lights)
struct PointLight {
	vec3 position;
	vec3 color;
};
uniform PointLight uPointLights[3];

// Material properties
uniform float uShininess;  // For specular highlight

out vec4 FragColor;

// Blinn-Phong shading calculation (without ambient, ambient is handled separately)
vec3 blinnPhong(vec3 normal, vec3 viewDir, vec3 lightDir, vec3 lightColor, float attenuation)
{
	// Diffuse
	float NdotL = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = NdotL * lightColor;
	
	// Specular (Blinn-Phong)
	vec3 halfDir = normalize(lightDir + viewDir);
	float NdotH = max(dot(normal, halfDir), 0.0);
	vec3 specular = pow(NdotH, uShininess) * lightColor;
	
	return (diffuse + specular) * attenuation;
}

void main()
{
	// Normalize the normal
	vec3 normal = normalize(vNormal);
	
	// Calculate view direction
	vec3 viewDir = normalize(uCameraPosition - vPosition);
	
	// Get base color from texture or use default
	vec3 baseColor;
	if (uUseTexture)
	{
		baseColor = texture(uTexture, vTexCoord).rgb;
			// Keep original texture colors - don't modify them
	}
	else
	{
		baseColor = vec3(0.8, 0.8, 0.8);
	}
	
	// Initialize lighting with ambient (like original Section 1.2)
	vec3 lighting = vec3(0.2);  // Base ambient light (original value from Section 1.2)
	
	// Directional light (no attenuation) - main light source from Section 1.2
	// Section 1.2 requires simplified model: ambient + diffuse only (no specular)
	vec3 dirLightDir = normalize(-uDirLightDirection);
	float NdotL = max(dot(normal, dirLightDir), 0.0);
	vec3 dirDiffuse = NdotL * uDirLightColor;
	lighting += dirDiffuse;  // Only diffuse, no specular for directional light
	
	// Point lights with full Blinn-Phong (Section 1.6)
	// Full Blinn-Phong includes: ambient + diffuse + specular
	
	// Point lights with full Blinn-Phong and 1/r² distance attenuation (Section 1.6)
	for (int i = 0; i < 3; i++)
	{
		vec3 lightVec = uPointLights[i].position - vPosition;
		float distance = length(lightVec);
		vec3 lightDir = normalize(lightVec);
		
		// Distance attenuation: 1/r² (standard formula)
		float attenuation = 1.0 / (1.0 + distance * distance);
		
		// Full Blinn-Phong: diffuse + specular (ambient already handled globally)
		lighting += blinnPhong(normal, viewDir, lightDir, uPointLights[i].color, attenuation);
	}
	
	// Apply lighting to base color
	vec3 color = baseColor * lighting;
	
	// Don't clamp color - let texture colors show through naturally
	// The original version didn't have any clamping
	
	FragColor = vec4(color, 1.0);
}

