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
#include "../support/particle_system.hpp"
#include "../support/ui_system.hpp"
#include "../support/performance_timer.hpp"

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
		
		// Split screen state (Task 1.9)
		bool splitScreenMode = false;
		Camera leftCamera;           // Camera for left view
		CameraMode leftCameraMode = CameraMode::Free;
		Camera leftFreeCamera;       // Store left free camera state
		Camera rightCamera;          // Camera for right view
		CameraMode rightCameraMode = CameraMode::Follow;
		Camera rightFreeCamera;      // Store right free camera state
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
	
	// Helper function to render the scene (Task 1.9 - for split screen)
	// This function encapsulates all rendering logic to avoid code duplication
	// Task 1.12: Added performance measurement parameter
	void render_scene(
		Camera const& camera,
		float viewportWidth, float viewportHeight,
		ShaderProgram const& prog,
		ShaderProgram const& materialProg,
		ShaderProgram const& particleProg,
		SimpleObjMesh const& terrain,
		GLuint terrain_texture,
		SimpleObjMesh const& launchpad,
		Vec3f const& launchpadA_pos,
		Vec3f const& launchpadB_pos,
		std::vector<VehiclePart> const& vehicle_parts,
		Vec3f const& current_vehicle_pos,
		Mat44f const& vehicle_rotation,
		Vec3f const& dirLightDirection,
		Vec3f const& dirLightColor,
		Vec3f const& pointLight1_pos,
		Vec3f const& pointLight1_color,
		Vec3f const& pointLight2_pos,
		Vec3f const& pointLight2_color,
		Vec3f const& pointLight3_pos,
		Vec3f const& pointLight3_color,
		float shininess,
		ParticleSystem& particleSystem,
		PerformanceMeasurement* perfMeasure = nullptr)
	{
		// Calculate matrices
		Mat44f model = kIdentity44f;
		Mat44f view = camera.get_view_matrix();
		Mat44f projection = make_perspective_projection(
			60.f * std::numbers::pi_v<float> / 180.f,  // 60 degree FOV
			viewportWidth / viewportHeight,             // aspect ratio
			0.1f,                                       // near plane
			10000.f                                     // far plane
		);
		
		// Render terrain with texture (Section 1.2)
		if (perfMeasure) perfMeasure->begin_gpu_section("Terrain");
		{
			glUseProgram( prog.programId() );
			
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
			Vec3f cameraPos = camera.get_position();
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
		}
		if (perfMeasure) perfMeasure->end_gpu_section("Terrain");
		
		// Draw two launchpad instances with material color (Section 1.4)
		if (perfMeasure) perfMeasure->begin_gpu_section("Launchpad");
		{
			glUseProgram( materialProg.programId() );
			GLint locMVP_mat = glGetUniformLocation( materialProg.programId(), "uModelViewProjection" );
			GLint locModel_mat = glGetUniformLocation( materialProg.programId(), "uModel" );
			GLint locNormal_mat = glGetUniformLocation( materialProg.programId(), "uNormalMatrix" );
			GLint locMatColor   = glGetUniformLocation( materialProg.programId(), "uMaterialColor" );

			// Set lighting uniforms for material shader
			Vec3f cameraPos = camera.get_position();
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
		if (perfMeasure) perfMeasure->end_gpu_section("Launchpad");

		// Draw space vehicle on launchpad A (Section 1.5)
		if (perfMeasure) perfMeasure->begin_gpu_section("Vehicle");
		{
			glUseProgram( materialProg.programId() );
			GLint locMVP_mat = glGetUniformLocation( materialProg.programId(), "uModelViewProjection" );
			GLint locModel_mat = glGetUniformLocation( materialProg.programId(), "uModel" );
			GLint locNormal_mat = glGetUniformLocation( materialProg.programId(), "uNormalMatrix" );
			GLint locMatColor   = glGetUniformLocation( materialProg.programId(), "uMaterialColor" );

			// Set lighting uniforms for material shader
			Vec3f cameraPos = camera.get_position();
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
		if (perfMeasure) perfMeasure->end_gpu_section("Vehicle");
		
		// Render particles (Task 1.10)
		{
			glUseProgram( particleProg.programId() );
			
			// Set uniforms
			Mat44f viewProjection = projection * view;
			GLint locVP = glGetUniformLocation( particleProg.programId(), "uViewProjection" );
			GLint locCamPos = glGetUniformLocation( particleProg.programId(), "uCameraPosition" );
			GLint locTexture = glGetUniformLocation( particleProg.programId(), "uParticleTexture" );
			
			glUniformMatrix4fv( locVP, 1, GL_TRUE, viewProjection.v );
			Vec3f camPos = camera.get_position();
			glUniform3f( locCamPos, camPos.x, camPos.y, camPos.z );
			glUniform1i( locTexture, 0 );  // Texture unit 0
			
			// Render particles
			particleSystem.render(viewProjection, camPos);
		}
	}
	
	// Helper function to update a camera based on its mode (Task 1.9)
	void update_camera_by_mode(
		Camera& camera,
		Camera& freeCamera,
		CameraMode mode,
		Vec3f const& current_vehicle_pos,
		Mat44f const& vehicle_rotation,
		Vec3f const& vehicleOriginalPos,
		float deltaTime,
		bool moveForward, bool moveBackward,
		bool moveLeft, bool moveRight,
		bool moveUp, bool moveDown,
		bool speedBoost, bool speedSlow)
	{
		if (mode == CameraMode::Free)
		{
			// Free camera: update based on input
			float baseSpeed = 20.f;
			if (speedBoost) baseSpeed *= 3.f;
			if (speedSlow) baseSpeed *= 0.2f;
			
			float moveDistance = baseSpeed * deltaTime;
			
			if (moveForward) camera.move_forward(moveDistance);
			if (moveBackward) camera.move_backward(moveDistance);
			if (moveLeft) camera.move_left(moveDistance);
			if (moveRight) camera.move_right(moveDistance);
			if (moveUp) camera.move_up(moveDistance);
			if (moveDown) camera.move_down(moveDistance);
			
			// Update free camera state
			freeCamera = camera;
		}
		else if (mode == CameraMode::Follow)
		{
			// Follow camera: fixed distance behind and above the vehicle
			float followDistance = 30.0f;
			float followHeight = 15.0f;
			
			// Get vehicle's forward direction from rotation matrix
			Vec3f vehicleForward{ -vehicle_rotation.v[8], -vehicle_rotation.v[9], -vehicle_rotation.v[10] };
			Vec3f vehicleUp{ vehicle_rotation.v[4], vehicle_rotation.v[5], vehicle_rotation.v[6] };
			
			// Position camera behind vehicle
			Vec3f cameraOffset = -vehicleForward * followDistance + vehicleUp * followHeight;
			Vec3f cameraPos = current_vehicle_pos + cameraOffset;
			
			camera.set_position(cameraPos);
			camera.look_at(current_vehicle_pos);
		}
		else if (mode == CameraMode::Ground)
		{
			// Ground camera: fixed on ground, always looks at vehicle
			Vec3f groundCameraPos = vehicleOriginalPos;
			groundCameraPos.y = 5.0f;
			groundCameraPos.x += 20.0f;
			groundCameraPos.z += 20.0f;
			
			camera.set_position(groundCameraPos);
			camera.look_at(current_vehicle_pos);
		}
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
	
	// Initialize split screen cameras (Task 1.9)
	state.leftCamera = state.camera;
	state.leftFreeCamera = state.camera;
	state.rightCamera = state.camera;
	state.rightFreeCamera = state.camera;
	
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
	
	// Shader for particles (Task 1.10)
	ShaderProgram particleProg({
		{ GL_VERTEX_SHADER, "assets/cw2/particle.vert" },
		{ GL_FRAGMENT_SHADER, "assets/cw2/particle.frag" }
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
	
	// Create particle system for rocket exhaust (Task 1.10)
	std::print( "Creating particle system...\n" );
	ParticleSystem particleSystem(2000);  // Max 2000 particles
	particleSystem.set_emission_rate(200.f);  // 200 particles per second
	particleSystem.set_particle_lifetime(0.5f, 1.5f);  // 0.5-1.5 seconds
	particleSystem.set_particle_size(0.8f, 2.0f);  // Size range
	particleSystem.set_particle_velocity(10.f, 25.f);  // Speed range
	particleSystem.set_emission_direction(Vec3f{0.f, -1.f, 0.f});  // Downward (exhaust)
	particleSystem.set_emission_spread(0.4f);  // ~23 degree cone
	std::print( "Particle system created\n" );
	
	// Create UI system (Task 1.11)
	std::print( "Creating UI system...\n" );
	UISystem uiSystem(iwidth, iheight);
	
	// Try to use system fonts (Windows)
	const char* fontPaths[] = {
		"C:/Windows/Fonts/Arial.ttf",           // Arial (capital A)
		"C:/Windows/Fonts/arial.ttf",           // Arial (lowercase)
		"C:/Windows/Fonts/calibri.ttf",         // Calibri
		"C:/Windows/Fonts/segoeui.ttf",         // Segoe UI
		"C:/Windows/Fonts/consola.ttf",         // Consolas
		"C:/Windows/Fonts/verdana.ttf",         // Verdana
		"assets/cw2/DroidSansMonoDotted.ttf"    // Fallback
	};
	
	bool fontLoaded = false;
	for (const char* fontPath : fontPaths)
	{
		std::print("Trying font: {}\n", fontPath);
		if (uiSystem.initialize(fontPath))
		{
			std::print("✓ Successfully loaded font: {}\n", fontPath);
			fontLoaded = true;
			break;
		}
		else
		{
			std::print("✗ Failed to load: {}\n", fontPath);
		}
	}
	
	if (!fontLoaded)
	{
		std::print(stderr, "Failed to initialize UI system with any font\n");
		return 1;
	}
	
	// Add altitude label (top left) - larger font for better visibility
	UILabel* altitudeLabel = uiSystem.add_label("Altitude: 0.0 m", 28.0f, UIAnchor::TopLeft, 10.0f, 10.0f);
	altitudeLabel->set_color(Vec4f{1.0f, 1.0f, 0.0f, 1.0f});  // Yellow text
	
	// Add launch button (bottom center)
	UIButton* launchButton = uiSystem.add_button("Launch", 120.0f, 40.0f, UIAnchor::BottomCenter, -70.0f, 60.0f,
		[&state]() {
			// Launch callback
			state.animationActive = true;
			state.animationPaused = false;
			state.animationTime = 0.0;
		});
	
	// Add reset button (bottom center, to the right of launch button)
	UIButton* resetButton = uiSystem.add_button("Reset", 120.0f, 40.0f, UIAnchor::BottomCenter, 70.0f, 60.0f,
		[&state]() {
			// Reset callback
			state.animationActive = false;
			state.animationPaused = false;
			state.animationTime = 0.0;
		});
	
	std::print( "UI system created\n" );
	
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
	
	// Task 1.12: Performance measurement
	// To enable: define ENABLE_PERFORMANCE_MEASUREMENT in performance_timer.hpp
	// or add -DENABLE_PERFORMANCE_MEASUREMENT to compiler flags
	PerformanceMeasurement perfMeasure;
	perfMeasure.initialize();
	int perfFrameCount = 0;
	constexpr int PERF_REPORT_INTERVAL = 300;  // Report every 300 frames (~5 seconds at 60fps)
	
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
		// Commented out to reduce console spam
		//if (currentTime - lastPrintTime > 1.0)
		//{
		//	Vec3f camPos = state.camera.get_position();
		//	std::print( "Camera position: ({:.1f}, {:.1f}, {:.1f})\n", 
		//		camPos.x, camPos.y, camPos.z );
		//	lastPrintTime = currentTime;
		//}
		
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
			
			// Update UI system on window resize (Task 1.11)
			uiSystem.on_window_resize(nwidth, nheight);
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
		
		// Update particle system (Task 1.10)
		// Emit from engine position (bottom of rocket)
		Vec3f enginePos = current_vehicle_pos;
		enginePos.y -= 0.5f;  // Slightly below vehicle center (engine nozzle)
		bool emitting = state.animationActive && !state.animationPaused;
		particleSystem.update(deltaTime, enginePos, emitting);
		
		// Update UI system (Task 1.11)
		// Update altitude label
		float altitude = current_vehicle_pos.y - state.vehicleOriginalPos.y;
		char altitudeText[64];
		std::snprintf(altitudeText, sizeof(altitudeText), "Altitude: %.1f m", altitude);
		altitudeLabel->set_text(altitudeText);
		
		// Get mouse position for UI
		double mouseX, mouseY;
		glfwGetCursorPos(window, &mouseX, &mouseY);
		bool mouseDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
		uiSystem.update(static_cast<float>(mouseX), static_cast<float>(mouseY), mouseDown);

		// Update cameras based on mode (Task 1.9 - support split screen)
		if (!state.splitScreenMode)
		{
			// Single screen mode: update main camera
			update_camera_by_mode(
				state.camera, state.freeCamera, state.cameraMode,
				current_vehicle_pos, vehicle_rotation, state.vehicleOriginalPos,
				deltaTime,
				state.moveForward, state.moveBackward,
				state.moveLeft, state.moveRight,
				state.moveUp, state.moveDown,
				state.speedBoost, state.speedSlow
			);
		}
		else
		{
			// Split screen mode: update both cameras
			// Left camera
			update_camera_by_mode(
				state.leftCamera, state.leftFreeCamera, state.leftCameraMode,
				current_vehicle_pos, vehicle_rotation, state.vehicleOriginalPos,
				deltaTime,
				state.moveForward, state.moveBackward,
				state.moveLeft, state.moveRight,
				state.moveUp, state.moveDown,
				state.speedBoost, state.speedSlow
			);
			
			// Right camera
			update_camera_by_mode(
				state.rightCamera, state.rightFreeCamera, state.rightCameraMode,
				current_vehicle_pos, vehicle_rotation, state.vehicleOriginalPos,
				deltaTime,
				false, false, false, false, false, false,  // Right camera doesn't respond to keyboard
				false, false
			);
		}

		// Draw scene (Task 1.9 - support split screen)
		OGL_CHECKPOINT_DEBUG();

		// Task 1.12: Begin frame timing
		perfMeasure.begin_frame();
		perfMeasure.begin_cpu_timing();  // Start CPU timing for command submission

		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		
		if (!state.splitScreenMode)
		{
			// Single screen mode: render once with full viewport
			glViewport( 0, 0, static_cast<GLsizei>(fbwidth), static_cast<GLsizei>(fbheight) );
			
			render_scene(
				state.camera, fbwidth, fbheight,
				prog, materialProg, particleProg,
				terrain, terrain_texture,
				launchpad, launchpadA_pos, launchpadB_pos,
				vehicle_parts, current_vehicle_pos, vehicle_rotation,
				dirLightDirection, dirLightColor,
				pointLight1_pos, pointLight1_color,
				pointLight2_pos, pointLight2_color,
				pointLight3_pos, pointLight3_color,
				shininess,
				particleSystem,
				&perfMeasure  // Pass performance measurement
			);
		}
		else
		{
			// Split screen mode: render twice with half viewports
			// Note: In split screen, we only measure the left view for simplicity
			
			// Left view
			glViewport( 0, 0, static_cast<GLsizei>(fbwidth / 2), static_cast<GLsizei>(fbheight) );
			glEnable( GL_SCISSOR_TEST );
			glScissor( 0, 0, static_cast<GLsizei>(fbwidth / 2), static_cast<GLsizei>(fbheight) );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			glDisable( GL_SCISSOR_TEST );
			
			render_scene(
				state.leftCamera, fbwidth / 2, fbheight,
				prog, materialProg, particleProg,
				terrain, terrain_texture,
				launchpad, launchpadA_pos, launchpadB_pos,
				vehicle_parts, current_vehicle_pos, vehicle_rotation,
				dirLightDirection, dirLightColor,
				pointLight1_pos, pointLight1_color,
				pointLight2_pos, pointLight2_color,
				pointLight3_pos, pointLight3_color,
				shininess,
				particleSystem,
				&perfMeasure  // Measure left view
			);
			
			// Right view
			glViewport( static_cast<GLsizei>(fbwidth / 2), 0, static_cast<GLsizei>(fbwidth / 2), static_cast<GLsizei>(fbheight) );
			glEnable( GL_SCISSOR_TEST );
			glScissor( static_cast<GLsizei>(fbwidth / 2), 0, static_cast<GLsizei>(fbwidth / 2), static_cast<GLsizei>(fbheight) );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			glDisable( GL_SCISSOR_TEST );
			
			render_scene(
				state.rightCamera, fbwidth / 2, fbheight,
				prog, materialProg, particleProg,
				terrain, terrain_texture,
				launchpad, launchpadA_pos, launchpadB_pos,
				vehicle_parts, current_vehicle_pos, vehicle_rotation,
				dirLightDirection, dirLightColor,
				pointLight1_pos, pointLight1_color,
				pointLight2_pos, pointLight2_color,
				pointLight3_pos, pointLight3_color,
				shininess,
				particleSystem
				// Don't measure right view to avoid double-counting
			);
			
			// Restore full viewport
			glViewport( 0, 0, static_cast<GLsizei>(fbwidth), static_cast<GLsizei>(fbheight) );
		}
		
		// Render UI (Task 1.11) - render last so it appears on top
		uiSystem.render();

		// Task 1.12: End CPU timing (command submission complete)
		perfMeasure.end_cpu_timing_ms();

		OGL_CHECKPOINT_DEBUG();

		// Task 1.12: End frame timing (before swap buffers)
		perfMeasure.end_frame();
		
		// Task 1.12: Periodic performance report
		++perfFrameCount;
		if (perfFrameCount >= PERF_REPORT_INTERVAL && perfMeasure.has_results())
		{
			perfMeasure.print_summary();
			perfFrameCount = 0;
		}

		// Display results
		glfwSwapBuffers( window );
	}

	// Cleanup.
	// Task 1.12: Print final performance summary and cleanup
	perfMeasure.print_summary();
	perfMeasure.cleanup();
	
	terrain.cleanup();
	launchpad.cleanup();
	for (auto& part : vehicle_parts)
	{
		part.mesh.cleanup();
	}
	particleSystem.cleanup();  // Task 1.10
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
		
		// Split screen toggle (Task 1.9)
		if( aKey == GLFW_KEY_V && isPress )
		{
			state->splitScreenMode = !state->splitScreenMode;
			
			// When entering split screen, sync cameras
			if( state->splitScreenMode )
			{
				state->leftCamera = state->camera;
				state->leftCameraMode = state->cameraMode;
				state->leftFreeCamera = state->freeCamera;
				state->rightCamera = state->camera;
				state->rightCameraMode = CameraMode::Follow;  // Default right to Follow mode
				state->rightFreeCamera = state->camera;
			}
			else
			{
				// When exiting split screen, restore main camera from left
				state->camera = state->leftCamera;
				state->cameraMode = state->leftCameraMode;
				state->freeCamera = state->leftFreeCamera;
			}
		}
		
		// Camera mode switching (Task 1.9 - support split screen)
		if( aKey == GLFW_KEY_C && isPress )
		{
			// Check if Shift is pressed
			bool shiftPressed = (glfwGetKey(aWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
			                     glfwGetKey(aWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
			
			if( !state->splitScreenMode )
			{
				// Single screen mode: cycle main camera
				if( state->cameraMode == CameraMode::Free )
				{
					state->cameraMode = CameraMode::Follow;
					state->freeCamera = state->camera;
				}
				else if( state->cameraMode == CameraMode::Follow )
				{
					state->cameraMode = CameraMode::Ground;
				}
				else // Ground
				{
					state->cameraMode = CameraMode::Free;
					state->camera = state->freeCamera;
				}
			}
			else
			{
				// Split screen mode: C cycles left camera, Shift+C cycles right camera
				if( !shiftPressed )
				{
					// Cycle left camera mode
					if( state->leftCameraMode == CameraMode::Free )
					{
						state->leftCameraMode = CameraMode::Follow;
						state->leftFreeCamera = state->leftCamera;
					}
					else if( state->leftCameraMode == CameraMode::Follow )
					{
						state->leftCameraMode = CameraMode::Ground;
					}
					else // Ground
					{
						state->leftCameraMode = CameraMode::Free;
						state->leftCamera = state->leftFreeCamera;
					}
				}
				else
				{
					// Cycle right camera mode (Shift+C)
					if( state->rightCameraMode == CameraMode::Free )
					{
						state->rightCameraMode = CameraMode::Follow;
						state->rightFreeCamera = state->rightCamera;
					}
					else if( state->rightCameraMode == CameraMode::Follow )
					{
						state->rightCameraMode = CameraMode::Ground;
					}
					else // Ground
					{
						state->rightCameraMode = CameraMode::Free;
						state->rightCamera = state->rightFreeCamera;
					}
				}
			}
		}
	}
	
	void glfw_callback_mouse_button_( GLFWwindow* aWindow, int aButton, int aAction, int )
	{
		auto* state = static_cast<State*>(glfwGetWindowUserPointer( aWindow ));
		if( !state ) return;
		
		// Only allow mouse control in Free camera mode (Task 1.9 - check appropriate camera)
		CameraMode currentMode = state->splitScreenMode ? state->leftCameraMode : state->cameraMode;
		if( currentMode != CameraMode::Free )
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
		
		// Only allow mouse control in Free camera mode (Task 1.9 - check appropriate camera)
		CameraMode currentMode = state->splitScreenMode ? state->leftCameraMode : state->cameraMode;
		if( !state || !state->mouseActive || currentMode != CameraMode::Free ) return;
		
		// Calculate mouse delta
		double deltaX = aX - state->lastMouseX;
		double deltaY = aY - state->lastMouseY;
		
		state->lastMouseX = aX;
		state->lastMouseY = aY;
		
		// Mouse sensitivity
		float const sensitivity = 0.002f;
		
		// Update camera rotation (Task 1.9 - update appropriate camera)
		if( !state->splitScreenMode )
		{
			state->camera.rotate_yaw( static_cast<float>(deltaX) * sensitivity );
			state->camera.rotate_pitch( static_cast<float>(-deltaY) * sensitivity );
		}
		else
		{
			// In split screen mode, control left camera
			state->leftCamera.rotate_yaw( static_cast<float>(deltaX) * sensitivity );
			state->leftCamera.rotate_pitch( static_cast<float>(-deltaY) * sensitivity );
		}
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
