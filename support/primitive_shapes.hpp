#ifndef PRIMITIVE_SHAPES_HPP_INCLUDED
#define PRIMITIVE_SHAPES_HPP_INCLUDED

#include "../vmlib/vec3.hpp"

// Suppress warning about multi-parameter operator[] (C++23 feature)
// mat44.hpp defines operator[] with multiple parameters, which is valid C++23
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 5246)  // 'operator[]': too many parameters for this operator function
#endif

#include "../vmlib/mat44.hpp"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "simple_obj.hpp"
#include <vector>

// Generate a box (cuboid) mesh
// size: half-extents in x, y, z directions
SimpleObjMesh generate_box(Vec3f size);

// Generate a cylinder mesh
// radius: radius of the cylinder
// height: height of the cylinder
// segments: number of segments around the circumference (more = smoother)
SimpleObjMesh generate_cylinder(float radius, float height, int segments = 16);

// Generate a sphere mesh
// radius: radius of the sphere
// segments: number of segments (more = smoother)
SimpleObjMesh generate_sphere(float radius, int segments = 16);

// Generate a cone mesh
// radius: base radius of the cone
// height: height of the cone
// segments: number of segments around the base (more = smoother)
SimpleObjMesh generate_cone(float radius, float height, int segments = 16);

// Transform a mesh by a matrix
// This applies the transformation to all vertices and normals
void transform_mesh(SimpleObjMesh& mesh, Mat44f const& transform);

// Combine multiple meshes into one
// All meshes are combined into the first mesh
void combine_meshes(SimpleObjMesh& target, SimpleObjMesh const& source);

#endif // PRIMITIVE_SHAPES_HPP_INCLUDED

