#ifndef SIMPLE_OBJ_HPP_INCLUDED
#define SIMPLE_OBJ_HPP_INCLUDED

#include <glad/glad.h>

#include <vector>
#include <string>

#include "../vmlib/vec3.hpp"

struct SimpleObjMesh
{
	std::vector<Vec3f> positions;
	std::vector<Vec3f> normals;
	std::vector<float> texcoords; // 2D texture coordinates (u,v pairs)
	std::vector<unsigned int> indices;
	
	// OpenGL buffers
	GLuint vao = 0;
	GLuint vbo_positions = 0;
	GLuint vbo_normals = 0;
	GLuint vbo_texcoords = 0;
	GLuint ebo = 0;
	
	// Texture
	std::string texture_path;
	
	Vec3f material_color = Vec3f{ 0.8f, 0.8f, 0.8f };
	bool has_material_color = false;
	
	void upload_to_gpu();
	void cleanup();
};

// Load a simple OBJ file (vertices and normals only)
SimpleObjMesh load_simple_obj(std::string const& path);

#endif // SIMPLE_OBJ_HPP_INCLUDED
