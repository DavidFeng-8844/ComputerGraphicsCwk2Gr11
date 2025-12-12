#ifndef FONTSTASH_GL_HPP_A1B2C3D4_E5F6_7890_ABCD_EF1234567890
#define FONTSTASH_GL_HPP_A1B2C3D4_E5F6_7890_ABCD_EF1234567890

#include <glad/glad.h>

// Forward declaration
struct FONScontext;

// Create Fontstash context with OpenGL backend
FONScontext* fonsgl_create(int width, int height, int flags);

// Delete Fontstash context
void fonsgl_delete(FONScontext* ctx);

// Set viewport size for text rendering (call before fonsDrawText)
void fonsgl_set_viewport(FONScontext* ctx, int windowWidth, int windowHeight);

// Render accumulated text (call after fonsDrawText)
void fonsgl_render(FONScontext* ctx, int windowWidth, int windowHeight);

// Get texture ID (for debugging)
unsigned int fonsgl_get_texture(FONScontext* ctx);

#endif // FONTSTASH_GL_HPP_A1B2C3D4_E5F6_7890_ABCD_EF1234567890
