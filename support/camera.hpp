#ifndef CAMERA_HPP_INCLUDED
#define CAMERA_HPP_INCLUDED

#include "../vmlib/vec3.hpp"
#include "../vmlib/mat44.hpp"

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
