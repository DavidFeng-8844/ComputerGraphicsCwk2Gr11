#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <print>
#include <numbers>
#include <typeinfo>
#include <stdexcept>

#include <cstdlib>

#include "../support/error.hpp"
#include "../support/program.hpp"
#include "../support/checkpoint.hpp"
#include "../support/debug_output.hpp"
#include "../support/camera.hpp"
#include "../support/simple_obj.hpp"
#include "../support/texture.hpp"

#include "../vmlib/vec4.hpp"
#include "../vmlib/mat44.hpp"

#include "defaults.hpp"


namespace
{
	constexpr char const* kWindowTitle = "COMP3811 - CW2";
	
	// Application state
	struct State
	{
		Camera camera;
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
	};
	
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

		// Update camera based on input
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

		OGL_CHECKPOINT_DEBUG();

		// Display results
		glfwSwapBuffers( window );
	}

	// Cleanup.
	terrain.cleanup();
	launchpad.cleanup();
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
	}
	
	void glfw_callback_mouse_button_( GLFWwindow* aWindow, int aButton, int aAction, int )
	{
		auto* state = static_cast<State*>(glfwGetWindowUserPointer( aWindow ));
		if( !state ) return;
		
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
		if( !state || !state->mouseActive ) return;
		
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
