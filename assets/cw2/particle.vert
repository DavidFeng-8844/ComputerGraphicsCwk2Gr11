#version 430

// Vertex attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec4 aColor;
layout(location = 2) in float aSize;

// Uniforms
uniform mat4 uViewProjection;
uniform vec3 uCameraPosition;

// Outputs to fragment shader
out vec4 vColor;

void main()
{
	// Transform position to clip space
	gl_Position = uViewProjection * vec4(aPosition, 1.0);
	
	// Pass color to fragment shader
	vColor = aColor;
	
	// Set point size based on distance to camera
	float distance = length(aPosition - uCameraPosition);
	
	// Size attenuation: particles appear smaller when far away
	// Base size scaled by distance attenuation
	float attenuation = 1.0 / (1.0 + distance * 0.01);
	gl_PointSize = aSize * 50.0 * attenuation;  // Scale up for visibility
	
	// Clamp to reasonable range
	gl_PointSize = clamp(gl_PointSize, 1.0, 100.0);
}
