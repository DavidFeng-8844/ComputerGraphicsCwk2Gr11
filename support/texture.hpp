#ifndef TEXTURE_HPP_INCLUDED
#define TEXTURE_HPP_INCLUDED

#include <glad/glad.h>
#include <string>

// Load a texture from file and return OpenGL texture ID
GLuint load_texture_2d(std::string const& path);

#endif // TEXTURE_HPP_INCLUDED
