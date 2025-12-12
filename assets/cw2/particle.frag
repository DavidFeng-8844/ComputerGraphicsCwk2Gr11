#version 430

// Input from vertex shader
in vec4 vColor;

// Uniforms
uniform sampler2D uParticleTexture;

// Output
out vec4 fragColor;

void main()
{
	// Sample texture using point sprite coordinates
	// gl_PointCoord is automatically provided for point sprites (0,0 to 1,1)
	vec4 texColor = texture(uParticleTexture, gl_PointCoord);
	
	// Combine texture color with vertex color
	// Texture provides shape (alpha channel), vertex color provides tint and fade
	fragColor = vec4(vColor.rgb * texColor.rgb, vColor.a * texColor.a);
	
	// Discard fully transparent fragments (optimization)
	if (fragColor.a < 0.01)
		discard;
}
