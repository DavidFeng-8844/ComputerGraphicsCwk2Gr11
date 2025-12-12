// Windows-specific includes must come first
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "fontstash_gl.hpp"

#include <cstring>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <print>

#define FONTSTASH_IMPLEMENTATION
#include <fontstash.h>

// OpenGL Fontstash implementation
struct GLFONScontext
{
	GLuint texture;
	GLuint vao;
	GLuint vbo;
	GLuint program;
	int width, height;
	int viewportWidth, viewportHeight;  // Current viewport size for rendering
};

// Global pointer to current GL context (workaround for accessing userPtr)
static GLFONScontext* g_currentGLContext = nullptr;

// Vertex shader for text rendering
static const char* vertexShaderSource = R"(
#version 430 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

uniform vec2 uViewSize;

out vec2 vTexCoord;
out vec4 vColor;

void main()
{
	// Convert from pixel coordinates to NDC
	vec2 ndc = (aPosition / uViewSize) * 2.0 - 1.0;
	ndc.y = -ndc.y;  // Flip Y axis
	
	gl_Position = vec4(ndc, 0.0, 1.0);
	vTexCoord = aTexCoord;
	vColor = aColor;
}
)";

// Fragment shader for text rendering
static const char* fragmentShaderSource = R"(
#version 430 core
in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 fragColor;

void main()
{
	float alpha = texture(uTexture, vTexCoord).r;
	fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)";

// Compile shader
static GLuint compileShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char infoLog[512];
		glGetShaderInfoLog(shader, 512, nullptr, infoLog);
		std::print(stderr, "Shader compilation failed: {}\n", infoLog);
	}
	
	return shader;
}

// Create shader program
static GLuint createProgram()
{
	GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
	
	GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	
	GLint success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		char infoLog[512];
		glGetProgramInfoLog(program, 512, nullptr, infoLog);
		std::print(stderr, "Shader linking failed: {}\n", infoLog);
	}
	
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	
	return program;
}

// Fontstash callbacks
static int glfons_renderCreate(void* userPtr, int width, int height)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;
	
	// Create texture
	glGenTextures(1, &gl->texture);
	glBindTexture(GL_TEXTURE_2D, gl->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	gl->width = width;
	gl->height = height;
	
	return 1;
}

static int glfons_renderResize(void* userPtr, int width, int height)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;
	
	// Resize texture
	glBindTexture(GL_TEXTURE_2D, gl->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	gl->width = width;
	gl->height = height;
	
	return 1;
}

static void glfons_renderUpdate(void* userPtr, int* rect, const unsigned char* data)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;
	
	int w = rect[2] - rect[0];
	int h = rect[3] - rect[1];
	
	if (w <= 0 || h <= 0) return;
	
	glBindTexture(GL_TEXTURE_2D, gl->texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	// The data pointer points to the full atlas, so we need to set row length
	// and skip pixels to the correct starting position
	glPixelStorei(GL_UNPACK_ROW_LENGTH, gl->width);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, rect[0]);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, rect[1]);
	
	glTexSubImage2D(GL_TEXTURE_2D, 0, rect[0], rect[1], w, h, GL_RED, GL_UNSIGNED_BYTE, data);
	
	// Reset pixel store state
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	
	glBindTexture(GL_TEXTURE_2D, 0);
}

static void glfons_renderDraw(void* userPtr, const float* verts, const float* tcoords, 
                              const unsigned int* colors, int nverts)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;
	
	if (nverts == 0) return;
	
	// Interleave vertex data: position (2), texcoord (2), color (4)
	struct Vertex
	{
		float x, y;
		float u, v;
		float r, g, b, a;
	};
	
	std::vector<Vertex> vertices(nverts);
	for (int i = 0; i < nverts; ++i)
	{
		vertices[i].x = verts[i * 2 + 0];
		vertices[i].y = verts[i * 2 + 1];
		vertices[i].u = tcoords[i * 2 + 0];
		vertices[i].v = tcoords[i * 2 + 1];
		
		unsigned int c = colors[i];
		vertices[i].r = ((c >> 0) & 0xFF) / 255.0f;
		vertices[i].g = ((c >> 8) & 0xFF) / 255.0f;
		vertices[i].b = ((c >> 16) & 0xFF) / 255.0f;
		vertices[i].a = ((c >> 24) & 0xFF) / 255.0f;
	}
	
	// Upload to GPU and render immediately
	glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
	
	// Use shader program
	glUseProgram(gl->program);
	
	// Set viewport size uniform
	GLint locViewSize = glGetUniformLocation(gl->program, "uViewSize");
	if (locViewSize >= 0)
	{
		glUniform2f(locViewSize, static_cast<float>(gl->viewportWidth), static_cast<float>(gl->viewportHeight));
	}
	else
	{
		std::print("WARNING: uViewSize uniform not found!\n");
	}
	
	// Set texture uniform to texture unit 0
	GLint locTexture = glGetUniformLocation(gl->program, "uTexture");
	if (locTexture >= 0)
	{
		glUniform1i(locTexture, 0);
	}
	
	// Bind VAO and draw
	glBindVertexArray(gl->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl->texture);
	glDrawArrays(GL_TRIANGLES, 0, nverts);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}

static void glfons_renderDelete(void* userPtr)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;
	
	if (gl->texture)
		glDeleteTextures(1, &gl->texture);
	if (gl->vao)
		glDeleteVertexArrays(1, &gl->vao);
	if (gl->vbo)
		glDeleteBuffers(1, &gl->vbo);
	if (gl->program)
		glDeleteProgram(gl->program);
	
	delete gl;
}

FONScontext* fonsgl_create(int width, int height, int flags)
{
	GLFONScontext* gl = new GLFONScontext();
	std::memset(gl, 0, sizeof(GLFONScontext));
	
	// Create shader program
	gl->program = createProgram();
	
	// Create VAO and VBO
	glGenVertexArrays(1, &gl->vao);
	glGenBuffers(1, &gl->vbo);
	
	glBindVertexArray(gl->vao);
	glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
	
	// Position attribute
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	
	// TexCoord attribute
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));
	
	// Color attribute
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
	
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	// Create Fontstash context
	FONSparams params;
	std::memset(&params, 0, sizeof(params));
	params.width = width;
	params.height = height;
	params.flags = (unsigned char)flags;
	params.renderCreate = glfons_renderCreate;
	params.renderResize = glfons_renderResize;
	params.renderUpdate = glfons_renderUpdate;
	params.renderDraw = glfons_renderDraw;
	params.renderDelete = glfons_renderDelete;
	params.userPtr = gl;
	
	FONScontext* ctx = fonsCreateInternal(&params);
	
	// Store global pointer for viewport setting
	g_currentGLContext = gl;
	
	// Initialize viewport to texture size
	gl->viewportWidth = width;
	gl->viewportHeight = height;
	
	return ctx;
}

void fonsgl_delete(FONScontext* ctx)
{
	fonsDeleteInternal(ctx);
	
	// Clear global pointer
	if (g_currentGLContext)
	{
		g_currentGLContext = nullptr;
	}
}

void fonsgl_set_viewport(FONScontext* ctx, int windowWidth, int windowHeight)
{
	if (!ctx) return;
	
	// Use global context pointer
	if (g_currentGLContext)
	{
		g_currentGLContext->viewportWidth = windowWidth;
		g_currentGLContext->viewportHeight = windowHeight;
	}
}

void fonsgl_render(FONScontext* ctx, int windowWidth, int windowHeight)
{
	// Rendering is now done immediately in glfons_renderDraw
	// This function is kept for API compatibility but does nothing
	(void)ctx;
	(void)windowWidth;
	(void)windowHeight;
}

unsigned int fonsgl_get_texture(FONScontext* ctx)
{
	if (!ctx) return 0;
	
	// Access internal structure - this is implementation-specific
	// In a real implementation, we'd need proper accessor functions
	// For now, return 0 as this is just for debugging
	return 0;
}
