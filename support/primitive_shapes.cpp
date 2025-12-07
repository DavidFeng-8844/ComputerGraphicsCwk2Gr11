#include "primitive_shapes.hpp"
#include "simple_obj.hpp"
#include <cmath>
#include <numbers>

SimpleObjMesh generate_box(Vec3f size)
{
	SimpleObjMesh mesh;
	
	// 8 vertices of a box
	Vec3f vertices[8] = {
		{ -size.x, -size.y, -size.z },  // 0: bottom-left-back
		{  size.x, -size.y, -size.z },  // 1: bottom-right-back
		{  size.x,  size.y, -size.z },  // 2: top-right-back
		{ -size.x,  size.y, -size.z },  // 3: top-left-back
		{ -size.x, -size.y,  size.z },  // 4: bottom-left-front
		{  size.x, -size.y,  size.z },  // 5: bottom-right-front
		{  size.x,  size.y,  size.z },  // 6: top-right-front
		{ -size.x,  size.y,  size.z }  // 7: top-left-front
	};
	
	// 6 faces, each with 2 triangles
	unsigned int faces[6][4] = {
		{ 0, 1, 2, 3 },  // back
		{ 5, 4, 7, 6 },  // front
		{ 4, 0, 3, 7 },  // left
		{ 1, 5, 6, 2 },  // right
		{ 4, 5, 1, 0 },  // bottom
		{ 3, 2, 6, 7 }   // top
	};
	
	Vec3f normals[6] = {
		{  0.f,  0.f, -1.f },  // back
		{  0.f,  0.f,  1.f },  // front
		{ -1.f,  0.f,  0.f },  // left
		{  1.f,  0.f,  0.f },  // right
		{  0.f, -1.f,  0.f },  // bottom
		{  0.f,  1.f,  0.f }   // top
	};
	
	unsigned int base_index = 0;
	
	// Generate vertices and normals for each face
	for (int face = 0; face < 6; ++face)
	{
		// Add 4 vertices for this face
		for (int i = 0; i < 4; ++i)
		{
			mesh.positions.push_back(vertices[faces[face][i]]);
			mesh.normals.push_back(normals[face]);
		}
		
		// Add 2 triangles (0,1,2 and 0,2,3)
		mesh.indices.push_back(base_index + 0);
		mesh.indices.push_back(base_index + 1);
		mesh.indices.push_back(base_index + 2);
		
		mesh.indices.push_back(base_index + 0);
		mesh.indices.push_back(base_index + 2);
		mesh.indices.push_back(base_index + 3);
		
		base_index += 4;
	}
	
	return mesh;
}

SimpleObjMesh generate_cylinder(float radius, float height, int segments)
{
	SimpleObjMesh mesh;
	
	float half_height = height * 0.5f;
	
	// Generate vertices for top and bottom circles
	std::vector<Vec3f> top_vertices;
	std::vector<Vec3f> bottom_vertices;
	
	for (int i = 0; i <= segments; ++i)
	{
		float angle = 2.f * std::numbers::pi_v<float> * i / segments;
		float x = std::cos(angle) * radius;
		float z = std::sin(angle) * radius;
		
		top_vertices.push_back({ x, half_height, z });
		bottom_vertices.push_back({ x, -half_height, z });
	}
	
	unsigned int base_index = 0;
	
	// Side faces
	for (int i = 0; i < segments; ++i)
	{
		int next = (i + 1) % (segments + 1);
		
		// Add 4 vertices for this quad
		mesh.positions.push_back(bottom_vertices[i]);
		mesh.positions.push_back(bottom_vertices[next]);
		mesh.positions.push_back(top_vertices[next]);
		mesh.positions.push_back(top_vertices[i]);
		
		// Calculate normal (perpendicular to the side)
		float angle = 2.f * std::numbers::pi_v<float> * (i + 0.5f) / segments;
		Vec3f normal = normalize(Vec3f{ std::cos(angle), 0.f, std::sin(angle) });
		
		for (int j = 0; j < 4; ++j)
		{
			mesh.normals.push_back(normal);
		}
		
		// Add 2 triangles
		mesh.indices.push_back(base_index + 0);
		mesh.indices.push_back(base_index + 1);
		mesh.indices.push_back(base_index + 2);
		
		mesh.indices.push_back(base_index + 0);
		mesh.indices.push_back(base_index + 2);
		mesh.indices.push_back(base_index + 3);
		
		base_index += 4;
	}
	
	// Top cap
	unsigned int top_center = base_index;
	mesh.positions.push_back({ 0.f, half_height, 0.f });
	mesh.normals.push_back({ 0.f, 1.f, 0.f });
	base_index++;
	
	for (int i = 0; i < segments; ++i)
	{
		int next = (i + 1) % (segments + 1);
		mesh.positions.push_back(top_vertices[i]);
		mesh.normals.push_back({ 0.f, 1.f, 0.f });
		mesh.positions.push_back(top_vertices[next]);
		mesh.normals.push_back({ 0.f, 1.f, 0.f });
		
		mesh.indices.push_back(top_center);
		mesh.indices.push_back(base_index);
		mesh.indices.push_back(base_index + 1);
		
		base_index += 2;
	}
	
	// Bottom cap
	unsigned int bottom_center = base_index;
	mesh.positions.push_back({ 0.f, -half_height, 0.f });
	mesh.normals.push_back({ 0.f, -1.f, 0.f });
	base_index++;
	
	for (int i = 0; i < segments; ++i)
	{
		int next = (i + 1) % (segments + 1);
		mesh.positions.push_back(bottom_vertices[next]);
		mesh.normals.push_back({ 0.f, -1.f, 0.f });
		mesh.positions.push_back(bottom_vertices[i]);
		mesh.normals.push_back({ 0.f, -1.f, 0.f });
		
		mesh.indices.push_back(bottom_center);
		mesh.indices.push_back(base_index);
		mesh.indices.push_back(base_index + 1);
		
		base_index += 2;
	}
	
	return mesh;
}

SimpleObjMesh generate_sphere(float radius, int segments)
{
	SimpleObjMesh mesh;
	
	// Generate sphere using latitude/longitude method
	for (int lat = 0; lat <= segments; ++lat)
	{
		float theta = std::numbers::pi_v<float> * lat / segments;  // 0 to pi
		float sin_theta = std::sin(theta);
		float cos_theta = std::cos(theta);
		
		for (int lon = 0; lon <= segments; ++lon)
		{
			float phi = 2.f * std::numbers::pi_v<float> * lon / segments;  // 0 to 2pi
			float sin_phi = std::sin(phi);
			float cos_phi = std::cos(phi);
			
			Vec3f pos{
				radius * sin_theta * cos_phi,
				radius * cos_theta,
				radius * sin_theta * sin_phi
			};
			
			// Normal is just the normalized position
			Vec3f normal = normalize(pos);
			
			mesh.positions.push_back(pos);
			mesh.normals.push_back(normal);
		}
	}
	
	// Generate indices for quads
	for (int lat = 0; lat < segments; ++lat)
	{
		for (int lon = 0; lon < segments; ++lon)
		{
			int current = lat * (segments + 1) + lon;
			int next = current + segments + 1;
			
			// Two triangles per quad
			mesh.indices.push_back(current);
			mesh.indices.push_back(next);
			mesh.indices.push_back(next + 1);
			
			mesh.indices.push_back(current);
			mesh.indices.push_back(next + 1);
			mesh.indices.push_back(current + 1);
		}
	}
	
	return mesh;
}

SimpleObjMesh generate_cone(float radius, float height, int segments)
{
	SimpleObjMesh mesh;
	
	float half_height = height * 0.5f;
	
	// Generate vertices for base circle
	std::vector<Vec3f> base_vertices;
	for (int i = 0; i <= segments; ++i)
	{
		float angle = 2.f * std::numbers::pi_v<float> * i / segments;
		float x = std::cos(angle) * radius;
		float z = std::sin(angle) * radius;
		base_vertices.push_back({ x, -half_height, z });
	}
	
	// Apex
	unsigned int apex_index = 0;
	mesh.positions.push_back({ 0.f, half_height, 0.f });
	mesh.normals.push_back({ 0.f, 1.f, 0.f });  // Will be recalculated
	apex_index++;
	
	unsigned int base_index = apex_index;
	
	// Side faces
	for (int i = 0; i < segments; ++i)
	{
		int next = (i + 1) % (segments + 1);
		
		mesh.positions.push_back(base_vertices[i]);
		mesh.positions.push_back(base_vertices[next]);
		
		// Calculate normal for side face
		Vec3f v1 = base_vertices[i];
		Vec3f v2 = base_vertices[next];
		Vec3f apex{ 0.f, half_height, 0.f };
		Vec3f edge1 = v2 - v1;
		Vec3f edge2 = apex - v1;
		Vec3f normal = normalize(cross(edge1, edge2));
		
		mesh.normals.push_back(normal);
		mesh.normals.push_back(normal);
		
		// Triangle: apex, base[i], base[next]
		mesh.indices.push_back(0);
		mesh.indices.push_back(base_index);
		mesh.indices.push_back(base_index + 1);
		
		base_index += 2;
	}
	
	// Base cap
	unsigned int base_center = base_index;
	mesh.positions.push_back({ 0.f, -half_height, 0.f });
	mesh.normals.push_back({ 0.f, -1.f, 0.f });
	base_index++;
	
	for (int i = 0; i < segments; ++i)
	{
		int next = (i + 1) % (segments + 1);
		mesh.positions.push_back(base_vertices[next]);
		mesh.normals.push_back({ 0.f, -1.f, 0.f });
		mesh.positions.push_back(base_vertices[i]);
		mesh.normals.push_back({ 0.f, -1.f, 0.f });
		
		mesh.indices.push_back(base_center);
		mesh.indices.push_back(base_index);
		mesh.indices.push_back(base_index + 1);
		
		base_index += 2;
	}
	
	return mesh;
}

void transform_mesh(SimpleObjMesh& mesh, Mat44f const& transform)
{
	// Transform positions
	for (auto& pos : mesh.positions)
	{
		Vec4f pos4{ pos.x, pos.y, pos.z, 1.f };
		Vec4f transformed = transform * pos4;
		pos = { transformed.x, transformed.y, transformed.z };
	}
	
	// Transform normals (use upper 3x3 of transform, then normalize)
	// For proper normal transformation, we need the inverse transpose of the upper 3x3
	Mat44f inv_transform = invert(transform);
	Mat44f normal_transform = transpose(inv_transform);
	
	for (auto& normal : mesh.normals)
	{
		Vec4f norm4{ normal.x, normal.y, normal.z, 0.f };
		Vec4f transformed = normal_transform * norm4;
		normal = normalize(Vec3f{ transformed.x, transformed.y, transformed.z });
	}
}

void combine_meshes(SimpleObjMesh& target, SimpleObjMesh const& source)
{
	unsigned int offset = static_cast<unsigned int>(target.positions.size());
	
	// Add positions
	target.positions.insert(target.positions.end(), source.positions.begin(), source.positions.end());
	
	// Add normals
	target.normals.insert(target.normals.end(), source.normals.begin(), source.normals.end());
	
	// Add indices with offset
	for (auto idx : source.indices)
	{
		target.indices.push_back(idx + offset);
	}
	
	// Add texture coordinates if present
	if (!source.texcoords.empty())
	{
		target.texcoords.insert(target.texcoords.end(), source.texcoords.begin(), source.texcoords.end());
	}
}

