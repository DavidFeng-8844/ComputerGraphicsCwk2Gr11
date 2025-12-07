#include "texture.hpp"
#include "error.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <print>

GLuint load_texture_2d(std::string const& path)
{
	// Load image using stb_image
	int width, height, channels;
	stbi_set_flip_vertically_on_load(true); // OpenGL expects texture origin at bottom-left
	
	unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
	if (!data)
	{
		throw Error("Failed to load texture: {}", path);
	}
	
	std::print("Loaded texture: {} ({}x{}, {} channels)\n", path, width, height, channels);
	
	// Determine format
	GLenum format = GL_RGB;
	if (channels == 1)
		format = GL_RED;
	else if (channels == 3)
		format = GL_RGB;
	else if (channels == 4)
		format = GL_RGBA;
	
	// Create OpenGL texture
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	
	// Upload texture data
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	
	// Generate mipmaps
	glGenerateMipmap(GL_TEXTURE_2D);
	
	// Set texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	// Free image data
	stbi_image_free(data);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	return texture_id;
}
