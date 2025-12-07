#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <print>
#include <numbers>
#include <typeinfo>
#include <stdexcept>

#include <cstdlib>
#include <cstdio>
#include <cmath>

#include "../support/error.hpp"
#include "../support/program.hpp"
#include "../support/checkpoint.hpp"
#include "../support/debug_output.hpp"
#include "../support/camera.hpp"
#include "../support/simple_obj.hpp"
#include "../support/texture.hpp"
#include "../support/space_vehicle.hpp"

#include "../vmlib/vec4.hpp"

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

#include "defaults.hpp"


namespace
{
	constexpr char const* kWindowTitle = "COMP3811 - CW2";
	
	// Camera modes
	enum class CameraMode
	{
		Free,           // Default free camera
		Follow,         // Fixed distance following camera
		Ground          // Ground-based tracking camera
	};
	
	// Application state
	struct State
	{
		Camera camera;
		CameraMode cameraMode = CameraMode::Free;
		Camera freeCamera;  // Store free camera state
		bool mouseActive = false;
		double lastMouseX = 0.0;
		double lastMouseY = 0.0;
		
		// Movement state
		bool moveForward = false;
		bool moveBackward = false;
		bool moveLeft = false;
		bool moveRight = false;
		bool moveUp = false;
		bool moveDown = false;
		bool speedBoost = false;
		bool speedSlow = false;
		
		// Animation state
		bool animationActive = false;
		bool animationPaused = false;
		double animationTime = 0.0;  // Time since animation started
		Vec3f vehicleOriginalPos;    // Original position of vehicle
	};
	
	// Helper function to set lighting uniforms
	void set_lighting_uniforms(GLuint programId, Vec3f const& cameraPos, 
		Vec3f const& dirLightDir, Vec3f const& dirLightColor,
		Vec3f const& pointLight1Pos, Vec3f const& pointLight1Color,
		Vec3f const& pointLight2Pos, Vec3f const& pointLight2Color,
		Vec3f const& pointLight3Pos, Vec3f const& pointLight3Color,
		float shininess)
	{
		// Camera position
		GLint locCameraPos = glGetUniformLocation( programId, "uCameraPosition" );
		if (locCameraPos >= 0)
			glUniform3f( locCameraPos, cameraPos.x, cameraPos.y, cameraPos.z );
		
		// Directional light
		GLint locDirLightDir = glGetUniformLocation( programId, "uDirLightDirection" );
		GLint locDirLightColor = glGetUniformLocation( programId, "uDirLightColor" );
		if (locDirLightDir >= 0)
			glUniform3f( locDirLightDir, dirLightDir.x, dirLightDir.y, dirLightDir.z );
		if (locDirLightColor >= 0)
			glUniform3f( locDirLightColor, dirLightColor.x, dirLightColor.y, dirLightColor.z );
		
		// Point lights - set each light individually
		char uniformName[64];
		for (int i = 0; i < 3; i++)
		{
			Vec3f pos, color;
			if (i == 0) { pos = pointLight1Pos; color = pointLight1Color; }
			else if (i == 1) { pos = pointLight2Pos; color = pointLight2Color; }
			else { pos = pointLight3Pos; color = pointLight3Color; }
			
			std::snprintf(uniformName, sizeof(uniformName), "uPointLights[%d].position", i);
			GLint locPos = glGetUniformLocation( programId, uniformName );
			if (locPos >= 0)
				glUniform3f( locPos, pos.x, pos.y, pos.z );
			
			std::snprintf(uniformName, sizeof(uniformName), "uPointLights[%d].color", i);
			GLint locColor = glGetUniformLocation( programId, uniformName );
			if (locColor >= 0)
				glUniform3f( locColor, color.x, color.y, color.z );
		}
		
		// Shininess
		GLint locShininess = glGetUniformLocation( programId, "uShininess" );
		if (locShininess >= 0)
			glUniform1f( locShininess, shininess );
	}
	
	void glfw_callback_error_( int, char const* );

	void glfw_callback_key_( GLFWwindow*, int, int, int, int );
	void glfw_callback_mouse_button_( GLFWwindow*, int, int, int );
	void glfw_callback_mouse_move_( GLFWwindow*, double, double );

	struct GLFWCleanupHelper
	{
		~GLFWCleanupHelper();
	};
	struct GLFWWindowDeleter
	{
		~GLFWWindowDeleter();
		GLFWwindow* window;
	};

}

int main() try
{
	// Initialize GLFW
	if( GLFW_TRUE != glfwInit() )
	{
		char const* msg = nullptr;
		int ecode = glfwGetError( &msg );
		throw Error( "glfwInit() failed with '{}' ({})", msg, ecode );
	}

	// Ensure that we call glfwTerminate() at the end of the program.
	GLFWCleanupHelper cleanupHelper;

	// Configure GLFW and create window
	glfwSetErrorCallback( &glfw_callback_error_ );

	glfwWindowHint( GLFW_SRGB_CAPABLE, GLFW_TRUE );
	glfwWindowHint( GLFW_DOUBLEBUFFER, GLFW_TRUE );

	//glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
	glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE );
	glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );

	glfwWindowHint( GLFW_DEPTH_BITS, 24 );

#	if !defined(NDEBUG)
	// When building in debug mode, request an OpenGL debug context. This
	// enables additional debugging features. However, this can carry extra
	// overheads. We therefore do not do this for release builds.
	glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE );
#	endif // ~ !NDEBUG

	GLFWwindow* window = glfwCreateWindow(
		1280,
		720,
		kWindowTitle,
		nullptr, nullptr
	);

	if( !window )
	{
		char const* msg = nullptr;
		int ecode = glfwGetError( &msg );
		throw Error( "glfwCreateWindow() failed with '{}' ({})", msg, ecode );
	}

	GLFWWindowDeleter windowDeleter{ window };

	// Create application state
	State state;
	state.camera = Camera();
	// Position camera above terrain
	state.camera.move_up(100.f);
	state.camera.move_backward(200.f);
	
	// Store initial free camera state
	state.freeCamera = state.camera;
	
	// Store state pointer in window user pointer
	glfwSetWindowUserPointer( window, &state );

	// Set up event handling
	glfwSetKeyCallback( window, &glfw_callback_key_ );
	glfwSetMouseButtonCallback( window, &glfw_callback_mouse_button_ );
	glfwSetCursorPosCallback( window, &glfw_callback_mouse_move_ );

	// Set up drawing stuff
	glfwMakeContextCurrent( window );
	glfwSwapInterval( 1 ); // V-Sync is on.

	// Initialize GLAD
	// This will load the OpenGL API. We mustn't make any OpenGL calls before this!
	if( !gladLoadGLLoader( (GLADloadproc)&glfwGetProcAddress ) )
		throw Error( "gladLoadGLLoader() failed - cannot load GL API!" );

	std::print( "RENDERER {}\n", (char const*)glGetString( GL_RENDERER ) );
	std::print( "VENDOR {}\n", (char const*)glGetString( GL_VENDOR ) );
	std::print( "VERSION {}\n", (char const*)glGetString( GL_VERSION ) );
	std::print( "SHADING_LANGUAGE_VERSION {}\n", (char const*)glGetString( GL_SHADING_LANGUAGE_VERSION ) );

	// Ddebug output
#	if !defined(NDEBUG)
	setup_gl_debug_output();
#	endif // ~ !NDEBUG

	// Global GL state
	OGL_CHECKPOINT_ALWAYS();

	// Enable depth testing
	glEnable( GL_DEPTH_TEST );
	glDepthFunc( GL_LESS );
	
	// Disable backface culling by default (enable per-object if needed)
	glDisable( GL_CULL_FACE );
	
	// Set clear color
	glClearColor( 0.5f, 0.7f, 0.9f, 1.0f ); // Sky blue

	OGL_CHECKPOINT_ALWAYS();

	// Get actual framebuffer size.
	int iwidth, iheight;
	glfwGetFramebufferSize( window, &iwidth, &iheight );
	glViewport( 0, 0, iwidth, iheight );

	// Load shader programs
	OGL_CHECKPOINT_ALWAYS();
	
	// Shader for terrain (with texture)
	ShaderProgram prog({
		{ GL_VERTEX_SHADER, "assets/cw2/default.vert" },
		{ GL_FRAGMENT_SHADER, "assets/cw2/default.frag" }
	});
	
	ShaderProgram materialProg({
		{ GL_VERTEX_SHADER, "assets/cw2/material.vert" },
		{ GL_FRAGMENT_SHADER, "assets/cw2/material.frag" }
	});
	
	// Load terrain mesh
	std::print( "Loading terrain mesh (this may take a while...)\\n" );
	SimpleObjMesh terrain = load_simple_obj( "assets/cw2/parlahti.obj" );
	terrain.upload_to_gpu();
	std::print( "Terrain loaded: {} vertices, {} triangles\\n", 
		terrain.positions.size(), terrain.indices.size() / 3 );
	
	// Load texture if available
	GLuint terrain_texture = 0;
	if (!terrain.texture_path.empty())
	{
		std::print( "Loading texture: {}\n", terrain.texture_path );
		terrain_texture = load_texture_2d( terrain.texture_path );
	}

	std::print( "Loading launchpad mesh...\n" );
	SimpleObjMesh launchpad = load_simple_obj( "assets/cw2/landingpad.obj" );
	launchpad.upload_to_gpu();
	std::print( "Launchpad loaded: {} vertices, {} triangles\n", 
		launchpad.positions.size(), launchpad.indices.size() / 3 );
	std::print( "Launchpad material color: ({}, {}, {}), has_color: {}\n",
		launchpad.material_color.x, launchpad.material_color.y, launchpad.material_color.z,
		launchpad.has_material_color );

	// Task 1.4: Two launchpad instances in the sea
	// Position A
	Vec3f launchpadA_pos{ 75.f, -1.f, 20.f };
	// Position B
	Vec3f launchpadB_pos{ -70.f, -1.f, -55.f };
	
	std::print( "Launchpad Instance A position: ({}, {}, {})\n", 
		launchpadA_pos.x, launchpadA_pos.y, launchpadA_pos.z );
	std::print( "Launchpad Instance B position: ({}, {}, {})\n", 
		launchpadB_pos.x, launchpadB_pos.y, launchpadB_pos.z );

	// Generate space vehicle
	std::print( "Generating space vehicle...\n" );
	std::vector<VehiclePart> vehicle_parts = generate_space_vehicle();
	for (auto& part : vehicle_parts)
	{
		part.mesh.upload_to_gpu();
	}
	std::print( "Space vehicle generated with {} parts\n", vehicle_parts.size() );
	
	// Position vehicle on launchpad A (centered on pad, sitting on it)
	// Main body: center at y=4, height=8, so bottom at y=0 (in local space)
	// Launchpad is scaled 5x and positioned at launchpadA_pos.y = -1.f
	// The center platform surface is at approximately launchpadA_pos.y + (platform_height * 5)
	// Position rocket so body bottom (y=0 in local space) sits on pad center platform surface
	// Assuming center platform is slightly above base, adjust accordingly
	Vec3f vehicle_pos = launchpadA_pos;
	vehicle_pos.y += 0.2f;  // Adjust to sit on center platform surface (body bottom at y=0)
	
	// Store original position in state
	state.vehicleOriginalPos = vehicle_pos;

	// Point lights - positioned around the space vehicle (will be updated during animation)
	Vec3f pointLight1_baseOffset{ -3.f, 2.f, 0.f };
	Vec3f pointLight1_color{ 2.0f, 0.6f, 0.6f };  // Brighter red light
	Vec3f pointLight1_pos = state.vehicleOriginalPos + pointLight1_baseOffset;
	
	Vec3f pointLight2_baseOffset{ 3.f, 2.f, 0.f };
	Vec3f pointLight2_color{ 0.6f, 2.0f, 0.6f };  // Brighter green light
	Vec3f pointLight2_pos = state.vehicleOriginalPos + pointLight2_baseOffset;
	
	Vec3f pointLight3_baseOffset{ 0.f, 3.f, -3.f };
	Vec3f pointLight3_color{ 0.6f, 0.6f, 2.0f };  // Brighter blue light
	Vec3f pointLight3_pos = state.vehicleOriginalPos + pointLight3_baseOffset;
	
	// Directional light (from Section 1.2)
	Vec3f dirLightDirection = normalize(Vec3f{ 0.f, 1.f, -1.f });
	Vec3f dirLightColor{ 1.0f, 1.0f, 1.0f };  // Directional light (original from Section 1.2)
	
	// Material shininess for specular highlights
	float shininess = 32.0f;

	// Timing
	double lastTime = glfwGetTime();
	double lastPrintTime = glfwGetTime();
	
	// Main loop
	while( !glfwWindowShouldClose( window ) )
	{
		// Let GLFW process events
		glfwPollEvents();
		
		// Calculate delta time
		double currentTime = glfwGetTime();
		float deltaTime = static_cast<float>(currentTime - lastTime);
		lastTime = currentTime;
		
		// Print camera position every second for debugging
		if (currentTime - lastPrintTime > 1.0)
		{
			Vec3f camPos = state.camera.get_position();
			std::print( "Camera position: ({:.1f}, {:.1f}, {:.1f})\n", 
				camPos.x, camPos.y, camPos.z );
			lastPrintTime = currentTime;
		}
		
		// Check if window was resized.
		float fbwidth, fbheight;
		{
			int nwidth, nheight;
			glfwGetFramebufferSize( window, &nwidth, &nheight );

			fbwidth = float(nwidth);
			fbheight = float(nheight);

			if( 0 == nwidth || 0 == nheight )
			{
				// Window minimized? Pause until it is unminimized.
				do
				{
					glfwWaitEvents();
					glfwGetFramebufferSize( window, &nwidth, &nheight );
				} while( 0 == nwidth || 0 == nheight );
			}

			glViewport( 0, 0, nwidth, nheight );
		}

		// Update animation
		if (state.animationActive && !state.animationPaused)
		{
			state.animationTime += deltaTime;
		}
		
		// Calculate vehicle position and rotation based on animation
		Vec3f current_vehicle_pos = state.vehicleOriginalPos;
		Mat44f vehicle_rotation = kIdentity44f;
		
		if (state.animationActive)
		{
			// Realistic rocket trajectory: starts vertical, gradually tilts forward
			float t = static_cast<float>(state.animationTime);
			
			// Acceleration curve: starts slow, accelerates
			float accelerationPhase = 3.0f;  // First 3 seconds for acceleration
			float normalizedT;
			float normalizedTVelocity;  // For velocity calculation
			
			if (t < accelerationPhase)
			{
				// Acceleration phase: smooth quadratic acceleration
				float tNorm = t / accelerationPhase;
				normalizedT = tNorm * tNorm;  // Position: integral of velocity
				normalizedTVelocity = 2.0f * tNorm / accelerationPhase;  // Velocity derivative
			}
			else
			{
				// Constant speed phase
				normalizedT = 1.0f + (t - accelerationPhase) / accelerationPhase;
				normalizedTVelocity = 1.0f / accelerationPhase;  // Constant velocity
			}
			
			// Trajectory parameters
			float maxDistance = 300.0f;  // Maximum forward distance
			float maxHeight = 200.0f;    // Maximum height
			
			// Realistic rocket trajectory (curved path like real rocket):
			// - Starts nearly vertical (gravity turn)
			// - Gradually tilts forward with smooth curved path
			// - Height increases quickly, then levels off
			// - Forward movement accelerates from standstill
			
			// Time-based progress (0 to 1)
			float tProgress = std::min(normalizedT, 1.0f);
			
			// Vertical height: realistic ascent curve
			// Fast initial climb, then gradual leveling off
			// Using a smooth curve: h = t * H * (1 - t * 0.4)
			float vertical = tProgress * maxHeight * (1.0f - tProgress * 0.4f);
			
			// Forward distance (Z negative): curved acceleration from standstill
			// Starts at 0, slowly accelerates, creates curved path
			// Using cubic curve for smooth acceleration: d = t³ * D
			float forwardDist = tProgress * tProgress * tProgress * maxDistance;
			
			// Horizontal curve (X direction): gentle arc for realistic flight
			// Creates a smooth sideways curve in the flight path
			float horizontalCurve = std::sin(tProgress * std::numbers::pi_v<float> * 0.5f) * maxDistance * 0.15f;
			
			// Calculate position with curved path
			current_vehicle_pos = state.vehicleOriginalPos + Vec3f{
				horizontalCurve,  // Horizontal curve for realistic flight path
				vertical,         // Vertical height
				-forwardDist      // Forward movement (negative Z, curved acceleration)
			};
			
			// Calculate velocity (tangent to trajectory) by differentiating the position function
			// This gives us the exact tangent direction for the curved path
			// Note: tProgress is defined above at line 433, we reuse it here
			float dX_dT, dY_dT, dZ_dT;
			
			// Derivative of normalizedT with respect to time
			float dtProgress_dT = normalizedTVelocity;
			
			// Derivative of horizontal curve: d(sin(π*t/2) * maxDistance * 0.15) / dt
			dX_dT = std::cos(tProgress * std::numbers::pi_v<float> * 0.5f) * 
			        std::numbers::pi_v<float> * 0.5f * maxDistance * 0.15f * dtProgress_dT;
			
			// Derivative of height: d(t * maxHeight * (1 - t * 0.4)) / dt
			dY_dT = dtProgress_dT * maxHeight * (1.0f - 2.0f * tProgress * 0.4f);
			
			// Derivative of forward distance: d(t³ * maxDistance) / dt = 3t² * maxDistance
			dZ_dT = -3.0f * tProgress * tProgress * maxDistance * dtProgress_dT;  // Negative for -Z direction
			
			// Velocity vector (tangent to curved trajectory)
			Vec3f velocity{ dX_dT, dY_dT, dZ_dT };
			
			// Normalize velocity to get direction
			if (length(velocity) > 0.001f)
			{
				Vec3f tangentDir = normalize(velocity);
				
				// Calculate rotation to align rocket forward (+Y, since vehicle is built vertically) with tangent direction
				Vec3f rocketForward{ 0.f, 1.f, 0.f };  // Rocket's default forward direction (upward, +Y)
				Vec3f targetForward = tangentDir;
				
				// Calculate rotation axis and angle
				Vec3f axis = cross(rocketForward, targetForward);
				float axisLen = length(axis);
				
				if (axisLen > 0.001f)
				{
					axis = normalize(axis);
					float cosAngle = dot(rocketForward, targetForward);
					cosAngle = std::max(-1.0f, std::min(1.0f, cosAngle));
					float angle = std::acos(cosAngle);
					
					// Create rotation matrix using Rodrigues' rotation formula
					float c = std::cos(angle);
					float s = std::sin(angle);
					float t_rot = 1.0f - c;
					
					vehicle_rotation = Mat44f{ {
						t_rot * axis.x * axis.x + c,
						t_rot * axis.x * axis.y - s * axis.z,
						t_rot * axis.x * axis.z + s * axis.y,
						0.f,
						t_rot * axis.x * axis.y + s * axis.z,
						t_rot * axis.y * axis.y + c,
						t_rot * axis.y * axis.z - s * axis.x,
						0.f,
						t_rot * axis.x * axis.z - s * axis.y,
						t_rot * axis.y * axis.z + s * axis.x,
						t_rot * axis.z * axis.z + c,
						0.f,
						0.f, 0.f, 0.f, 1.f
					} };
				}
				else
				{
					// Vectors are parallel
					if (dot(rocketForward, targetForward) < -0.99f)
					{
						// Opposite direction, rotate 180 degrees around Y axis
						vehicle_rotation = make_rotation_y(std::numbers::pi_v<float>);
					}
					// Otherwise, no rotation needed (identity)
				}
			}
		}
		else
		{
			// Not animating, use original position
			current_vehicle_pos = state.vehicleOriginalPos;
		}
		
		// Update point light positions to follow vehicle
		pointLight1_pos = current_vehicle_pos + pointLight1_baseOffset;
		pointLight2_pos = current_vehicle_pos + pointLight2_baseOffset;
		pointLight3_pos = current_vehicle_pos + pointLight3_baseOffset;

		// Update camera based on mode
		if (state.cameraMode == CameraMode::Free)
		{
			// Free camera: update based on input
			float baseSpeed = 20.f; // units per second (reduced from 50)
			if (state.speedBoost) baseSpeed *= 3.f; // 3x boost (reduced from 5x)
			if (state.speedSlow) baseSpeed *= 0.2f;
			
			float moveDistance = baseSpeed * deltaTime;
			
			if (state.moveForward) state.camera.move_forward(moveDistance);
			if (state.moveBackward) state.camera.move_backward(moveDistance);
			if (state.moveLeft) state.camera.move_left(moveDistance);
			if (state.moveRight) state.camera.move_right(moveDistance);
			if (state.moveUp) state.camera.move_up(moveDistance);
			if (state.moveDown) state.camera.move_down(moveDistance);
			
			// Update free camera state
			state.freeCamera = state.camera;
		}
		else if (state.cameraMode == CameraMode::Follow)
		{
			// Follow camera: fixed distance behind and above the vehicle
			float followDistance = 30.0f;  // Distance behind vehicle
			float followHeight = 15.0f;    // Height above vehicle
			
			// Calculate camera position: behind and above the vehicle
			// Get vehicle's forward direction from rotation matrix
			Vec3f vehicleForward{ -vehicle_rotation.v[8], -vehicle_rotation.v[9], -vehicle_rotation.v[10] };
			Vec3f vehicleUp{ vehicle_rotation.v[4], vehicle_rotation.v[5], vehicle_rotation.v[6] };
			
			// Position camera behind vehicle
			Vec3f cameraOffset = -vehicleForward * followDistance + vehicleUp * followHeight;
			Vec3f cameraPos = current_vehicle_pos + cameraOffset;
			
			state.camera.set_position(cameraPos);
			state.camera.look_at(current_vehicle_pos);
		}
		else if (state.cameraMode == CameraMode::Ground)
		{
			// Ground camera: fixed on ground, always looks at vehicle
			Vec3f groundCameraPos = state.vehicleOriginalPos;
			groundCameraPos.y = 5.0f;  // Fixed height above ground
			groundCameraPos.x += 20.0f;  // Slight offset for better view
			groundCameraPos.z += 20.0f;
			
			state.camera.set_position(groundCameraPos);
			state.camera.look_at(current_vehicle_pos);
		}

		// Draw scene
		OGL_CHECKPOINT_DEBUG();

		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		
		// Use shader program
		glUseProgram( prog.programId() );
		
		// Calculate matrices
		Mat44f model = kIdentity44f;
		Mat44f view = state.camera.get_view_matrix();
		Mat44f projection = make_perspective_projection(
			60.f * std::numbers::pi_v<float> / 180.f,  // 60 degree FOV
			fbwidth / fbheight,                         // aspect ratio
			0.1f,                                       // near plane
			10000.f                                     // far plane
		);
		
		Mat44f mvp = projection * view * model;
		
		// Set uniforms
		GLint locMVP = glGetUniformLocation( prog.programId(), "uModelViewProjection" );
		GLint locModel = glGetUniformLocation( prog.programId(), "uModel" );
		GLint locNormal = glGetUniformLocation( prog.programId(), "uNormalMatrix" );
		GLint locTexture = glGetUniformLocation( prog.programId(), "uTexture" );
		GLint locUseTexture = glGetUniformLocation( prog.programId(), "uUseTexture" );
		
		glUniformMatrix4fv( locMVP, 1, GL_TRUE, mvp.v );
		glUniformMatrix4fv( locModel, 1, GL_TRUE, model.v );
		
		// Normal matrix (upper 3x3 of model matrix, for now just identity)
		float normalMat[9] = { 1,0,0, 0,1,0, 0,0,1 };
		glUniformMatrix3fv( locNormal, 1, GL_TRUE, normalMat );
		
		// Set lighting uniforms
		Vec3f cameraPos = state.camera.get_position();
		set_lighting_uniforms( prog.programId(), cameraPos,
			dirLightDirection, dirLightColor,
			pointLight1_pos, pointLight1_color,
			pointLight2_pos, pointLight2_color,
			pointLight3_pos, pointLight3_color,
			shininess );
		
		// Bind texture if available
		if (terrain_texture != 0)
		{
			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, terrain_texture );
			glUniform1i( locTexture, 0 );
			glUniform1i( locUseTexture, 1 );
		}
		else
		{
			glUniform1i( locUseTexture, 0 );
		}
		
		// Draw terrain
		glBindVertexArray( terrain.vao );
		glDrawElements( GL_TRIANGLES, static_cast<GLsizei>(terrain.indices.size()), GL_UNSIGNED_INT, nullptr );
		glBindVertexArray( 0 );

		// Draw two launchpad instances with material color
		{
			glUseProgram( materialProg.programId() );
			GLint locMVP_mat = glGetUniformLocation( materialProg.programId(), "uModelViewProjection" );
			GLint locModel_mat = glGetUniformLocation( materialProg.programId(), "uModel" );
			GLint locNormal_mat = glGetUniformLocation( materialProg.programId(), "uNormalMatrix" );
			GLint locMatColor   = glGetUniformLocation( materialProg.programId(), "uMaterialColor" );

			// Set lighting uniforms for material shader
			Vec3f cameraPos = state.camera.get_position();
			set_lighting_uniforms( materialProg.programId(), cameraPos,
				dirLightDirection, dirLightColor,
				pointLight1_pos, pointLight1_color,
				pointLight2_pos, pointLight2_color,
				pointLight3_pos, pointLight3_color,
				shininess );

			float normalMat3[9] = { 1,0,0, 0,1,0, 0,0,1 };
			Vec3f matCol = launchpad.has_material_color ? launchpad.material_color : Vec3f{0.8f,0.8f,0.8f};

			// Instance A: scaled 5x
			{
				Mat44f S = kIdentity44f;
				S.v[0] = 5.0f; S.v[5] = 5.0f; S.v[10] = 5.0f;
				Mat44f T = make_translation( launchpadA_pos );
				Mat44f M = T * S;
				Mat44f MVP = projection * view * M;
				glUniformMatrix4fv( locMVP_mat, 1, GL_TRUE, MVP.v );
				glUniformMatrix4fv( locModel_mat, 1, GL_TRUE, M.v );
				glUniformMatrix3fv( locNormal_mat, 1, GL_TRUE, normalMat3 );
				glUniform3f( locMatColor, matCol.x, matCol.y, matCol.z );
				glBindVertexArray( launchpad.vao );
				glDrawElements( GL_TRIANGLES, static_cast<GLsizei>(launchpad.indices.size()), GL_UNSIGNED_INT, nullptr );
				glBindVertexArray( 0 );
			}

			// Instance B: scaled 5x
			{
				Mat44f S = kIdentity44f;
				S.v[0] = 5.0f; S.v[5] = 5.0f; S.v[10] = 5.0f;
				Mat44f T = make_translation( launchpadB_pos );
				Mat44f M = T * S;
				Mat44f MVP = projection * view * M;
				glUniformMatrix4fv( locMVP_mat, 1, GL_TRUE, MVP.v );
				glUniformMatrix4fv( locModel_mat, 1, GL_TRUE, M.v );
				glUniformMatrix3fv( locNormal_mat, 1, GL_TRUE, normalMat3 );
				glUniform3f( locMatColor, matCol.x, matCol.y, matCol.z );
				glBindVertexArray( launchpad.vao );
				glDrawElements( GL_TRIANGLES, static_cast<GLsizei>(launchpad.indices.size()), GL_UNSIGNED_INT, nullptr );
				glBindVertexArray( 0 );
			}
		}

		// Draw space vehicle on launchpad A
		{
			glUseProgram( materialProg.programId() );
			GLint locMVP_mat = glGetUniformLocation( materialProg.programId(), "uModelViewProjection" );
			GLint locModel_mat = glGetUniformLocation( materialProg.programId(), "uModel" );
			GLint locNormal_mat = glGetUniformLocation( materialProg.programId(), "uNormalMatrix" );
			GLint locMatColor   = glGetUniformLocation( materialProg.programId(), "uMaterialColor" );

			// Set lighting uniforms for material shader
			Vec3f cameraPos = state.camera.get_position();
			set_lighting_uniforms( materialProg.programId(), cameraPos,
				dirLightDirection, dirLightColor,
				pointLight1_pos, pointLight1_color,
				pointLight2_pos, pointLight2_color,
				pointLight3_pos, pointLight3_color,
				shininess );

			// Transform to place vehicle (with animation)
			Mat44f vehicle_translate = make_translation(current_vehicle_pos);
			Mat44f vehicle_model = vehicle_translate * vehicle_rotation;
			
			// Calculate normal matrix (for now identity since we're only translating)
			float normalMat3[9] = { 1,0,0, 0,1,0, 0,0,1 };
			
			// Render each part with its own color
			for (const auto& part : vehicle_parts)
			{
				Mat44f MVP = projection * view * vehicle_model;
				glUniformMatrix4fv( locMVP_mat, 1, GL_TRUE, MVP.v );
				glUniformMatrix4fv( locModel_mat, 1, GL_TRUE, vehicle_model.v );
				glUniformMatrix3fv( locNormal_mat, 1, GL_TRUE, normalMat3 );
				glUniform3f( locMatColor, part.color.x, part.color.y, part.color.z );
				
				glBindVertexArray( part.mesh.vao );
				glDrawElements( GL_TRIANGLES, static_cast<GLsizei>(part.mesh.indices.size()), GL_UNSIGNED_INT, nullptr );
				glBindVertexArray( 0 );
			}
		}

		OGL_CHECKPOINT_DEBUG();

		// Display results
		glfwSwapBuffers( window );
	}

	// Cleanup.
	terrain.cleanup();
	launchpad.cleanup();
	for (auto& part : vehicle_parts)
	{
		part.mesh.cleanup();
	}
	if (terrain_texture != 0)
		glDeleteTextures(1, &terrain_texture);
	
	return 0;
}
catch( std::exception const& eErr )
{
	std::print( stderr, "Top-level Exception ({}):\n", typeid(eErr).name() );
	std::print( stderr, "{}\n", eErr.what() );
	std::print( stderr, "Bye.\n" );
	return 1;
}


namespace
{
	void glfw_callback_error_( int aErrNum, char const* aErrDesc )
	{
		std::print( stderr, "GLFW error: {} ({})\n", aErrDesc, aErrNum );
	}

	void glfw_callback_key_( GLFWwindow* aWindow, int aKey, int, int aAction, int )
	{
		auto* state = static_cast<State*>(glfwGetWindowUserPointer( aWindow ));
		if( !state ) return;
		
		// Only handle press and release events, ignore repeat
		if( aAction != GLFW_PRESS && aAction != GLFW_RELEASE )
			return;
		
		bool const isPress = (aAction == GLFW_PRESS);
		
		// ESC to close
		if( GLFW_KEY_ESCAPE == aKey && isPress )
		{
			glfwSetWindowShouldClose( aWindow, GLFW_TRUE );
			return;
		}
		
		// Movement keys (WSADEQ)
		if( aKey == GLFW_KEY_W ) state->moveForward = isPress;
		if( aKey == GLFW_KEY_S ) state->moveBackward = isPress;
		if( aKey == GLFW_KEY_A ) state->moveLeft = isPress;
		if( aKey == GLFW_KEY_D ) state->moveRight = isPress;
		if( aKey == GLFW_KEY_E ) state->moveUp = isPress;
		if( aKey == GLFW_KEY_Q ) state->moveDown = isPress;
		
		// Speed modifiers
		if( aKey == GLFW_KEY_LEFT_SHIFT || aKey == GLFW_KEY_RIGHT_SHIFT )
			state->speedBoost = isPress;
		if( aKey == GLFW_KEY_LEFT_CONTROL || aKey == GLFW_KEY_RIGHT_CONTROL )
			state->speedSlow = isPress;
		
		// Animation controls
		if( aKey == GLFW_KEY_F && isPress )
		{
			if( !state->animationActive )
			{
				// Start animation
				state->animationActive = true;
				state->animationPaused = false;
				state->animationTime = 0.0;
			}
			else
			{
				// Toggle pause
				state->animationPaused = !state->animationPaused;
			}
		}
		
		if( aKey == GLFW_KEY_R && isPress )
		{
			// Reset animation
			state->animationActive = false;
			state->animationPaused = false;
			state->animationTime = 0.0;
		}
		
		// Camera mode switching
		if( aKey == GLFW_KEY_C && isPress )
		{
			// Cycle through camera modes: Free -> Follow -> Ground -> Free
			if( state->cameraMode == CameraMode::Free )
			{
				state->cameraMode = CameraMode::Follow;
				// Save current free camera state
				state->freeCamera = state->camera;
			}
			else if( state->cameraMode == CameraMode::Follow )
			{
				state->cameraMode = CameraMode::Ground;
			}
			else // Ground
			{
				state->cameraMode = CameraMode::Free;
				// Restore free camera state
				state->camera = state->freeCamera;
			}
		}
	}
	
	void glfw_callback_mouse_button_( GLFWwindow* aWindow, int aButton, int aAction, int )
	{
		auto* state = static_cast<State*>(glfwGetWindowUserPointer( aWindow ));
		if( !state ) return;
		
		// Only allow mouse control in Free camera mode
		if( state->cameraMode != CameraMode::Free )
			return;
		
		if( aButton == GLFW_MOUSE_BUTTON_RIGHT && aAction == GLFW_PRESS )
		{
			state->mouseActive = !state->mouseActive;
			
			if( state->mouseActive )
			{
				// Get initial mouse position
				glfwGetCursorPos( aWindow, &state->lastMouseX, &state->lastMouseY );
				glfwSetInputMode( aWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
			}
			else
			{
				glfwSetInputMode( aWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
			}
		}
	}
	
	void glfw_callback_mouse_move_( GLFWwindow* aWindow, double aX, double aY )
	{
		auto* state = static_cast<State*>(glfwGetWindowUserPointer( aWindow ));
		// Only allow mouse control in Free camera mode
		if( !state || !state->mouseActive || state->cameraMode != CameraMode::Free ) return;
		
		// Calculate mouse delta
		double deltaX = aX - state->lastMouseX;
		double deltaY = aY - state->lastMouseY;
		
		state->lastMouseX = aX;
		state->lastMouseY = aY;
		
		// Mouse sensitivity
		float const sensitivity = 0.002f;
		
		// Update camera rotation
		state->camera.rotate_yaw( static_cast<float>(deltaX) * sensitivity );
		state->camera.rotate_pitch( static_cast<float>(-deltaY) * sensitivity );
	}

}

namespace
{
	GLFWCleanupHelper::~GLFWCleanupHelper()
	{
		glfwTerminate();
	}

	GLFWWindowDeleter::~GLFWWindowDeleter()
	{
		if( window )
			glfwDestroyWindow( window );
	}
}
