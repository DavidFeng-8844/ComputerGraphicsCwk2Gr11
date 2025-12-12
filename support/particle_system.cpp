#include "particle_system.hpp"

#include <random>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
	// Random number generator
	std::random_device rd;
	std::mt19937 gen(rd());
	
	// Random float in range [min, max]
	float random_float(float min, float max)
	{
		std::uniform_real_distribution<float> dis(min, max);
		return dis(gen);
	}
	
	// Random direction in cone around base direction
	Vec3f random_cone_direction(Vec3f const& baseDir, float spreadAngle)
	{
		// Random angles
		float theta = random_float(0.f, spreadAngle);
		float phi = random_float(0.f, 2.f * std::numbers::pi_v<float>);
		
		// Create perpendicular vectors to baseDir
		Vec3f up = std::abs(baseDir.y) < 0.9f ? Vec3f{0.f, 1.f, 0.f} : Vec3f{1.f, 0.f, 0.f};
		Vec3f right = normalize(cross(up, baseDir));
		up = normalize(cross(baseDir, right));
		
		// Rotate around cone
		float sinTheta = std::sin(theta);
		float cosTheta = std::cos(theta);
		float sinPhi = std::sin(phi);
		float cosPhi = std::cos(phi);
		
		Vec3f direction = baseDir * cosTheta + 
		                  (right * cosPhi + up * sinPhi) * sinTheta;
		
		return normalize(direction);
	}
}

ParticleSystem::ParticleSystem(unsigned int maxParticles)
	: maxParticles_(maxParticles)
	, emissionRate_(100.f)
	, emissionAccumulator_(0.f)
	, minLifetime_(1.0f)
	, maxLifetime_(2.0f)
	, minSize_(0.5f)
	, maxSize_(1.5f)
	, minSpeed_(5.f)
	, maxSpeed_(15.f)
	, emissionDirection_{0.f, -1.f, 0.f}  // Downward by default (engine exhaust)
	, emissionSpread_(0.3f)  // ~17 degrees
	, vao_(0)
	, vbo_(0)
	, texture_(0)
{
	// Pre-allocate particle pool
	particles_.resize(maxParticles_);
	for (auto& p : particles_)
		p.active = false;
	
	// Pre-allocate vertex data
	vertexData_.reserve(maxParticles_);
	
	// Create OpenGL resources
	glGenVertexArrays(1, &vao_);
	glGenBuffers(1, &vbo_);
	
	glBindVertexArray(vao_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	
	// Allocate buffer (will be updated each frame)
	glBufferData(GL_ARRAY_BUFFER, maxParticles_ * sizeof(ParticleVertex), nullptr, GL_DYNAMIC_DRAW);
	
	// Position attribute (location 0)
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), 
	                      reinterpret_cast<void*>(offsetof(ParticleVertex, position)));
	
	// Color attribute (location 1)
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
	                      reinterpret_cast<void*>(offsetof(ParticleVertex, color)));
	
	// Size attribute (location 2)
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex),
	                      reinterpret_cast<void*>(offsetof(ParticleVertex, size)));
	
	glBindVertexArray(0);
	
	// Load particle texture (will be created/loaded later)
	// For now, create a simple procedural texture
	glGenTextures(1, &texture_);
	glBindTexture(GL_TEXTURE_2D, texture_);
	
	// Create a simple radial gradient texture with alpha
	const int texSize = 64;
	std::vector<unsigned char> texData(texSize * texSize * 4);
	
	for (int y = 0; y < texSize; ++y)
	{
		for (int x = 0; x < texSize; ++x)
		{
			float dx = (x - texSize / 2.f) / (texSize / 2.f);
			float dy = (y - texSize / 2.f) / (texSize / 2.f);
			float dist = std::sqrt(dx * dx + dy * dy);
			
			// Radial gradient
			float alpha = std::max(0.f, 1.f - dist);
			alpha = alpha * alpha;  // Smoother falloff
			
			int idx = (y * texSize + x) * 4;
			texData[idx + 0] = 255;  // R
			texData[idx + 1] = 200;  // G (slightly orange)
			texData[idx + 2] = 100;  // B
			texData[idx + 3] = static_cast<unsigned char>(alpha * 255);  // A
		}
	}
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	glBindTexture(GL_TEXTURE_2D, 0);
}

ParticleSystem::~ParticleSystem()
{
	cleanup();
}

void ParticleSystem::cleanup()
{
	if (vao_) glDeleteVertexArrays(1, &vao_);
	if (vbo_) glDeleteBuffers(1, &vbo_);
	if (texture_) glDeleteTextures(1, &texture_);
	
	vao_ = vbo_ = texture_ = 0;
}

void ParticleSystem::update(float deltaTime, Vec3f const& emitterPosition, bool emitting)
{
	// Update existing particles
	for (auto& p : particles_)
	{
		if (!p.active) continue;
		
		// Update lifetime
		p.lifetime -= deltaTime;
		if (p.lifetime <= 0.f)
		{
			p.active = false;
			continue;
		}
		
		// Update position
		p.position = p.position + p.velocity * deltaTime;
		
		// Fade out based on remaining lifetime
		float lifetimeRatio = p.lifetime / maxLifetime_;
		p.color.w = lifetimeRatio;  // Alpha fades with lifetime
	}
	
	// Emit new particles if emitting
	if (emitting)
	{
		emit_particles(deltaTime, emitterPosition);
	}
}

void ParticleSystem::emit_particles(float deltaTime, Vec3f const& emitterPosition)
{
	// Calculate number of particles to emit this frame
	emissionAccumulator_ += emissionRate_ * deltaTime;
	int particlesToEmit = static_cast<int>(emissionAccumulator_);
	emissionAccumulator_ -= particlesToEmit;
	
	// Emit particles
	for (int i = 0; i < particlesToEmit; ++i)
	{
		int idx = find_inactive_particle();
		if (idx < 0) break;  // No free slots
		
		Particle& p = particles_[idx];
		p.active = true;
		p.position = emitterPosition;
		p.lifetime = random_float(minLifetime_, maxLifetime_);
		p.size = random_float(minSize_, maxSize_);
		
		// Random velocity in cone
		Vec3f direction = random_cone_direction(emissionDirection_, emissionSpread_);
		float speed = random_float(minSpeed_, maxSpeed_);
		p.velocity = direction * speed;
		
		// Color (orange/yellow fire with full alpha initially)
		p.color = Vec4f{
			random_float(0.8f, 1.0f),   // R
			random_float(0.5f, 0.8f),   // G
			random_float(0.1f, 0.3f),   // B
			1.0f                         // A
		};
	}
}

int ParticleSystem::find_inactive_particle()
{
	// Linear search for inactive particle
	for (size_t i = 0; i < particles_.size(); ++i)
	{
		if (!particles_[i].active)
			return static_cast<int>(i);
	}
	return -1;  // No free slots
}

void ParticleSystem::update_gpu_data()
{
	vertexData_.clear();
	
	// Collect active particles
	for (const auto& p : particles_)
	{
		if (!p.active) continue;
		
		ParticleVertex v;
		v.position = p.position;
		v.color = p.color;
		v.size = p.size;
		vertexData_.push_back(v);
	}
	
	// Upload to GPU
	if (!vertexData_.empty())
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferSubData(GL_ARRAY_BUFFER, 0, vertexData_.size() * sizeof(ParticleVertex), vertexData_.data());
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

void ParticleSystem::render(Mat44f const& viewProjection, Vec3f const& cameraPosition)
{
	// Update GPU data
	update_gpu_data();
	
	if (vertexData_.empty()) return;
	
	// Enable blending for particles
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending
	glDepthMask(GL_FALSE);  // Don't write to depth buffer
	
	// Enable point sprites
	glEnable(GL_PROGRAM_POINT_SIZE);
	
	// Bind texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_);
	
	// Bind VAO and draw
	glBindVertexArray(vao_);
	glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertexData_.size()));
	glBindVertexArray(0);
	
	// Restore state
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glDisable(GL_PROGRAM_POINT_SIZE);
}

void ParticleSystem::set_emission_rate(float particlesPerSecond)
{
	emissionRate_ = particlesPerSecond;
}

void ParticleSystem::set_particle_lifetime(float minLifetime, float maxLifetime)
{
	minLifetime_ = minLifetime;
	maxLifetime_ = maxLifetime;
}

void ParticleSystem::set_particle_size(float minSize, float maxSize)
{
	minSize_ = minSize;
	maxSize_ = maxSize;
}

void ParticleSystem::set_particle_velocity(float minSpeed, float maxSpeed)
{
	minSpeed_ = minSpeed;
	maxSpeed_ = maxSpeed;
}

void ParticleSystem::set_emission_direction(Vec3f const& direction)
{
	emissionDirection_ = normalize(direction);
}

void ParticleSystem::set_emission_spread(float spreadAngle)
{
	emissionSpread_ = spreadAngle;
}
