#include "simple_obj.hpp"

#include "error.hpp"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cstdio>

SimpleObjMesh load_simple_obj(std::string const& path)
{
	std::ifstream file(path);
	if (!file.is_open())
		throw Error("Failed to open OBJ file: {}", path);
	
	SimpleObjMesh mesh;
	
	std::vector<Vec3f> temp_positions;
	std::vector<Vec3f> temp_normals;
	std::vector<float> temp_texcoords; // Store as pairs of floats (u,v)
	
	// Map to handle vertex deduplication
	std::unordered_map<std::string, unsigned int> vertex_map;
	
	// Extract directory from path for MTL file
	std::string directory;
	size_t last_slash = path.find_last_of("/\\");
	if (last_slash != std::string::npos)
		directory = path.substr(0, last_slash + 1);
	
	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#')
			continue;
		
		std::istringstream iss(line);
		std::string prefix;
		iss >> prefix;
		
		if (prefix == "v")
		{
			// Vertex position
			Vec3f pos;
			iss >> pos.x >> pos.y >> pos.z;
			temp_positions.push_back(pos);
		}
		else if (prefix == "vn")
		{
			// Vertex normal
			Vec3f normal;
			iss >> normal.x >> normal.y >> normal.z;
			temp_normals.push_back(normal);
		}
		else if (prefix == "vt")
		{
			// Texture coordinate
			float u, v;
			iss >> u >> v;
			temp_texcoords.push_back(u);
			temp_texcoords.push_back(v);
		}
		else if (prefix == "mtllib")
		{
			// Material library file
			std::string mtl_file;
			iss >> mtl_file;
			
			// Try to load texture path and material color from MTL file
			std::ifstream mtl(directory + mtl_file);
			if (mtl.is_open())
			{
				std::string mtl_line;
				while (std::getline(mtl, mtl_line))
				{
					std::istringstream mtl_iss(mtl_line);
					std::string mtl_prefix;
					mtl_iss >> mtl_prefix;
					
					if (mtl_prefix == "map_Kd")
					{
						// Diffuse texture map
						mtl_iss >> mesh.texture_path;
						mesh.texture_path = directory + mesh.texture_path;
					}
					else if (mtl_prefix == "Kd")
					{
						float r, g, b;
						if (mtl_iss >> r >> g >> b)
						{
							mesh.material_color = Vec3f{ r, g, b };
							mesh.has_material_color = true;
						}
					}
				}
			}
		}
		else if (prefix == "f")
		{
			// Face
			std::string vertex_str;
			std::vector<unsigned int> face_indices;
			
			while (iss >> vertex_str)
			{
				// Check if we've seen this vertex before
				auto it = vertex_map.find(vertex_str);
				if (it != vertex_map.end())
				{
					face_indices.push_back(it->second);
					continue;
				}
				
				// Parse vertex string (format: v/vt/vn or v//vn)
				int pos_idx = -1, tex_idx = -1, norm_idx = -1;
				
				if (std::sscanf(vertex_str.c_str(), "%d//%d", &pos_idx, &norm_idx) == 2)
				{
					// Format: v//vn
				}
				else if (std::sscanf(vertex_str.c_str(), "%d/%d/%d", &pos_idx, &tex_idx, &norm_idx) == 3)
				{
					// Format: v/vt/vn
				}
				else if (std::sscanf(vertex_str.c_str(), "%d", &pos_idx) == 1)
				{
					// Format: v
				}
				
				// OBJ indices are 1-based
				if (pos_idx > 0 && pos_idx <= static_cast<int>(temp_positions.size()))
				{
					unsigned int new_index = static_cast<unsigned int>(mesh.positions.size());
					
					mesh.positions.push_back(temp_positions[pos_idx - 1]);
					
					if (norm_idx > 0 && norm_idx <= static_cast<int>(temp_normals.size()))
					{
						mesh.normals.push_back(temp_normals[norm_idx - 1]);
					}
					else
					{
						// Default normal if not specified
						mesh.normals.push_back(Vec3f{ 0.f, 1.f, 0.f });
					}
					
					// Add texture coordinates
					if (tex_idx > 0 && static_cast<size_t>((tex_idx - 1) * 2 + 1) < temp_texcoords.size())
					{
						mesh.texcoords.push_back(temp_texcoords[(tex_idx - 1) * 2]);
						mesh.texcoords.push_back(temp_texcoords[(tex_idx - 1) * 2 + 1]);
					}
					else
					{
						// Default texture coordinates if not specified
						mesh.texcoords.push_back(0.f);
						mesh.texcoords.push_back(0.f);
					}
					
					vertex_map[vertex_str] = new_index;
					face_indices.push_back(new_index);
				}
			}
			
			// Triangulate face (assuming convex polygons)
			for (size_t i = 1; i + 1 < face_indices.size(); ++i)
			{
				mesh.indices.push_back(face_indices[0]);
				mesh.indices.push_back(face_indices[i]);
				mesh.indices.push_back(face_indices[i + 1]);
			}
		}
	}
	
	return mesh;
}

void SimpleObjMesh::upload_to_gpu()
{
	// Generate and bind VAO
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	
	// Upload positions (location 0)
	glGenBuffers(1, &vbo_positions);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_positions);
	glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(Vec3f), positions.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3f), nullptr);
	glEnableVertexAttribArray(0);
	
	// Upload normals (location 1)
	glGenBuffers(1, &vbo_normals);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_normals);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(Vec3f), normals.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3f), nullptr);
	glEnableVertexAttribArray(1);
	
	// Upload texture coordinates (location 2)
	if (!texcoords.empty())
	{
		glGenBuffers(1, &vbo_texcoords);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_texcoords);
		glBufferData(GL_ARRAY_BUFFER, texcoords.size() * sizeof(float), texcoords.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glEnableVertexAttribArray(2);
	}
	
	// Upload indices
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	
	glBindVertexArray(0);
}

void SimpleObjMesh::cleanup()
{
	if (vao) glDeleteVertexArrays(1, &vao);
	if (vbo_positions) glDeleteBuffers(1, &vbo_positions);
	if (vbo_normals) glDeleteBuffers(1, &vbo_normals);
	if (vbo_texcoords) glDeleteBuffers(1, &vbo_texcoords);
	if (ebo) glDeleteBuffers(1, &ebo);
	
	vao = vbo_positions = vbo_normals = vbo_texcoords = ebo = 0;
}
