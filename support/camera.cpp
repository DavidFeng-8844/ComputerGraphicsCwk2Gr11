#include "camera.hpp"

#include <cmath>

Camera::Camera() noexcept
	: position{ 0.f, 0.f, 0.f }
	, forward{ 0.f, 0.f, -1.f }
	, right{ 1.f, 0.f, 0.f }
	, up{ 0.f, 1.f, 0.f }
	, yaw{ 0.f }
	, pitch{ 0.f }
{
}

Mat44f Camera::get_view_matrix() const noexcept
{
	// Create view matrix using look-at approach
	// View matrix transforms world space to camera space
	
	Vec3f const f = normalize(forward);
	Vec3f const r = normalize(right);
	Vec3f const u = normalize(up);
	
	// View matrix is inverse of camera transform
	Mat44f view = kIdentity44f;
	
	// Rotation part (transpose of camera rotation)
	view.v[0] = r.x;  view.v[1] = r.y;  view.v[2] = r.z;
	view.v[4] = u.x;  view.v[5] = u.y;  view.v[6] = u.z;
	view.v[8] = -f.x; view.v[9] = -f.y; view.v[10] = -f.z;
	
	// Translation part
	view.v[3] = -dot(r, position);
	view.v[7] = -dot(u, position);
	view.v[11] = dot(f, position);
	
	return view;
}

void Camera::move_forward(float distance) noexcept
{
	position = position + forward * distance;
}

void Camera::move_backward(float distance) noexcept
{
	position = position - forward * distance;
}

void Camera::move_left(float distance) noexcept
{
	position = position - right * distance;
}

void Camera::move_right(float distance) noexcept
{
	position = position + right * distance;
}

void Camera::move_up(float distance) noexcept
{
	position = position + up * distance;
}

void Camera::move_down(float distance) noexcept
{
	position = position - up * distance;
}

void Camera::rotate_yaw(float angle) noexcept
{
	yaw += angle;
	update_vectors();
}

void Camera::rotate_pitch(float angle) noexcept
{
	pitch += angle;
	
	// Clamp pitch to avoid gimbal lock
	constexpr float maxPitch = 1.55f; // ~89 degrees
	if (pitch > maxPitch) pitch = maxPitch;
	if (pitch < -maxPitch) pitch = -maxPitch;
	
	update_vectors();
}

void Camera::update_vectors() noexcept
{
	// Calculate forward vector from yaw and pitch
	forward.x = std::cos(yaw) * std::cos(pitch);
	forward.y = std::sin(pitch);
	forward.z = std::sin(yaw) * std::cos(pitch);
	forward = normalize(forward);
	
	// Calculate right vector (perpendicular to forward and world up)
	Vec3f const worldUp{ 0.f, 1.f, 0.f };
	right = normalize(cross(forward, worldUp));
	
	// Calculate up vector
	up = normalize(cross(right, forward));
}

void Camera::look_at(Vec3f const& target, Vec3f const& worldUp) noexcept
{
	// Calculate forward direction (from camera to target)
	forward = normalize(target - position);
	
	// Calculate right vector
	right = normalize(cross(forward, worldUp));
	
	// Calculate up vector
	up = normalize(cross(right, forward));
	
	// Update yaw and pitch from forward vector
	pitch = std::asin(forward.y);
	yaw = std::atan2(forward.z, forward.x);
}
