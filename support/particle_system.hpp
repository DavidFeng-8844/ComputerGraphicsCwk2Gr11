#ifndef PARTICLE_SYSTEM_HPP_6D8A0F3B_3B44_4C7A_9D3E_8F6E5C2D1A0B
#define PARTICLE_SYSTEM_HPP_6D8A0F3B_3B44_4C7A_9D3E_8F6E5C2D1A0B

#include <vector>
#include <glad/glad.h>

#include "../vmlib/vec3.hpp"
#include "../vmlib/vec4.hpp"
#include "../vmlib/mat44.hpp"

// Single particle structure
struct Particle
{
	Vec3f position;      // World space position
	Vec3f velocity;      // Velocity vector
	Vec4f color;         // RGBA color (alpha for fade out)
	float lifetime;      // Remaining lifetime in seconds
	float size;          // Particle size
	bool active;         // Is particle active?
};

// Particle system class
class ParticleSystem
{
public:
	ParticleSystem(unsigned int maxParticles = 1000);
	~ParticleSystem();
	
	// Update particles (frame-rate independent)
	void update(float deltaTime, Vec3f const& emitterPosition, bool emitting);
	
	// Render particles
	void render(Mat44f const& viewProjection, Vec3f const& cameraPosition);
	
	// Set emission parameters
	void set_emission_rate(float particlesPerSecond);
	void set_particle_lifetime(float minLifetime, float maxLifetime);
	void set_particle_size(float minSize, float maxSize);
	void set_particle_velocity(float minSpeed, float maxSpeed);
	void set_emission_direction(Vec3f const& direction);
	void set_emission_spread(float spreadAngle);
	
	// Cleanup OpenGL resources
	void cleanup();
	
private:
	// Emit new particles
	void emit_particles(float deltaTime, Vec3f const& emitterPosition);
	
	// Find inactive particle slot
	int find_inactive_particle();
	
	// Upload particle data to GPU
	void update_gpu_data();
	
	// Particle pool
	std::vector<Particle> particles_;
	unsigned int maxParticles_;
	
	// Emission parameters
	float emissionRate_;           // Particles per second
	float emissionAccumulator_;    // Fractional particles accumulated
	float minLifetime_, maxLifetime_;
	float minSize_, maxSize_;
	float minSpeed_, maxSpeed_;
	Vec3f emissionDirection_;      // Base emission direction
	float emissionSpread_;         // Cone spread angle in radians
	
	// OpenGL resources
	GLuint vao_;
	GLuint vbo_;
	GLuint texture_;
	
	// Vertex data for GPU upload
	struct ParticleVertex
	{
		Vec3f position;
		Vec4f color;
		float size;
	};
	std::vector<ParticleVertex> vertexData_;
};

#endif // PARTICLE_SYSTEM_HPP_6D8A0F3B_3B44_4C7A_9D3E_8F6E5C2D1A0B
