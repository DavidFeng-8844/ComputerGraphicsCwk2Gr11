#include "space_vehicle.hpp"
#include "primitive_shapes.hpp"

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

#include <vector>
#include <numbers>
#include <cmath>

std::vector<VehiclePart> generate_space_vehicle()
{
	std::vector<VehiclePart> parts;
	
	// We'll create a rocket-like vehicle with:
	// 1. Main body (cylinder) - base
	// 2. Nose cone (cone) - on top of body
	// 3. Fins (4 boxes) - attached to body
	// 4. Engine nozzle (cylinder) - at bottom
	// 5. Window (sphere) - on side
	// 6. Antenna (cylinder) - on top
	// 7. Thruster pods (2 cylinders) - on sides
	
	// Main body (cylinder) - white/light gray
	// Reduced diameter to match launchpad center (radius ~0.8, diameter ~1.6)
	{
		VehiclePart part;
		part.mesh = generate_cylinder(0.8f, 8.0f, 16);  // Reduced radius from 2.0 to 0.8
		Mat44f body_transform = make_translation(Vec3f{ 0.f, 4.f, 0.f });
		transform_mesh(part.mesh, body_transform);
		part.color = Vec3f{ 0.9f, 0.9f, 0.95f };
		parts.push_back(part);
	}
	
	// Nose cone (cone) - red/orange
	// Scaled down to match smaller body
	{
		VehiclePart part;
		part.mesh = generate_cone(0.6f, 3.0f, 16);  // Reduced radius from 1.5 to 0.6
		Mat44f nose_transform = make_translation(Vec3f{ 0.f, 9.5f, 0.f });
		transform_mesh(part.mesh, nose_transform);
		part.color = Vec3f{ 0.9f, 0.3f, 0.2f };
		parts.push_back(part);
	}
	
	// Engine nozzle (cylinder, wider) - dark gray
	// Scaled down to match smaller body
	{
		VehiclePart part;
		part.mesh = generate_cylinder(1.0f, 1.5f, 16);  // Reduced radius from 2.5 to 1.0
		Mat44f nozzle_transform = make_translation(Vec3f{ 0.f, -0.75f, 0.f });
		transform_mesh(part.mesh, nozzle_transform);
		part.color = Vec3f{ 0.4f, 0.4f, 0.45f };
		parts.push_back(part);
	}
	
	// Fins (4 boxes) - blue
	// Scaled down to match smaller body
	Vec3f fin_size{ 0.15f, 2.5f, 1.0f };  // Reduced width and depth
	for (int i = 0; i < 4; ++i)
	{
		VehiclePart part;
		float angle = i * std::numbers::pi_v<float> / 2.f;
		Mat44f R = make_rotation_y(angle);
		Mat44f T = make_translation(Vec3f{ 0.9f, 2.0f, 0.f });  // Moved closer to body (from 2.2 to 0.9)
		Mat44f fin_transform = T * R;
		
		part.mesh = generate_box(fin_size);
		transform_mesh(part.mesh, fin_transform);
		part.color = Vec3f{ 0.2f, 0.5f, 0.8f };
		parts.push_back(part);
	}
	
	// Window (sphere, flattened) - blue/cyan
	// Scaled down to match smaller body
	{
		VehiclePart part;
		part.mesh = generate_sphere(0.5f, 16);  // Reduced radius from 1.2 to 0.5
		Mat44f window_scale = kIdentity44f;
		window_scale.v[0] = 1.0f;
		window_scale.v[5] = 0.6f;  // Flatten vertically
		window_scale.v[10] = 1.0f;
		Mat44f window_translate = make_translation(Vec3f{ 0.85f, 5.0f, 0.f });  // Moved closer (from 2.1 to 0.85)
		Mat44f window_transform = window_translate * window_scale;
		transform_mesh(part.mesh, window_transform);
		part.color = Vec3f{ 0.3f, 0.7f, 0.9f };
		parts.push_back(part);
	}
	
	// Antenna (thin cylinder) - yellow
	{
		VehiclePart part;
		part.mesh = generate_cylinder(0.1f, 1.5f, 8);
		Mat44f antenna_transform = make_translation(Vec3f{ 0.f, 11.25f, 0.f });
		transform_mesh(part.mesh, antenna_transform);
		part.color = Vec3f{ 0.9f, 0.9f, 0.3f };
		parts.push_back(part);
	}
	
	// Thruster pods (2 cylinders) - orange
	// Scaled down to match smaller body
	for (int i = 0; i < 2; ++i)
	{
		VehiclePart part;
		float side = (i == 0) ? 1.f : -1.f;
		part.mesh = generate_cylinder(0.3f, 1.5f, 12);  // Reduced radius from 0.8 to 0.3
		Mat44f pod_rotate = make_rotation_z(std::numbers::pi_v<float> / 2.f);
		Mat44f pod_translate = make_translation(Vec3f{ side * 1.1f, 1.5f, 0.f });  // Moved closer (from 2.5 to 1.1)
		Mat44f pod_transform = pod_translate * pod_rotate;
		transform_mesh(part.mesh, pod_transform);
		part.color = Vec3f{ 0.9f, 0.6f, 0.2f };
		parts.push_back(part);
	}
	
	return parts;
}

