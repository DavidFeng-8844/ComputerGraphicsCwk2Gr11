#ifndef CAMERA_HPP_INCLUDED
#define CAMERA_HPP_INCLUDED

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

class Camera
{
public:
	Camera() noexcept;
	
	// Get view matrix
	Mat44f get_view_matrix() const noexcept;
	
	// Movement (relative to camera orientation)
	void move_forward(float distance) noexcept;
	void move_backward(float distance) noexcept;
	void move_left(float distance) noexcept;
	void move_right(float distance) noexcept;
	void move_up(float distance) noexcept;
	void move_down(float distance) noexcept;
	
	// Rotation (in radians)
	void rotate_yaw(float angle) noexcept;   // Left/Right
	void rotate_pitch(float angle) noexcept; // Up/Down
	
	// Getters
	Vec3f get_position() const noexcept { return position; }
	Vec3f get_forward() const noexcept { return forward; }
	Vec3f get_right() const noexcept { return right; }
	Vec3f get_up() const noexcept { return up; }
	
	// Setters for tracking cameras
	void set_position(Vec3f const& pos) noexcept { position = pos; }
	void look_at(Vec3f const& target, Vec3f const& worldUp = Vec3f{0.f, 1.f, 0.f}) noexcept;
	
private:
	void update_vectors() noexcept;
	
	Vec3f position;
	Vec3f forward;
	Vec3f right;
	Vec3f up;
	
	float yaw;   // Rotation around Y axis
	float pitch; // Rotation around X axis
};

#endif // CAMERA_HPP_INCLUDED
