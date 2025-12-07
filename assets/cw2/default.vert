#version 430

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModelViewProjection;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;

out vec3 vNormal;
out vec3 vPosition;
out vec2 vTexCoord;

void main()
{
	// Transform position
	gl_Position = uModelViewProjection * vec4(aPosition, 1.0);
	
	// Pass world space position and normal to fragment shader
	vPosition = vec3(uModel * vec4(aPosition, 1.0));
	vNormal = normalize(uNormalMatrix * aNormal);
	vTexCoord = aTexCoord;
}
