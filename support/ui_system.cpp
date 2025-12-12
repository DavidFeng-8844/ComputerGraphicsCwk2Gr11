#include "ui_system.hpp"
#include "fontstash_gl.hpp"

#include <fontstash.h>
#include <cmath>
#include <print>
#include <format>

// Helper macro for creating RGBA color (if not defined by fontstash)
// Note: Fontstash uses ABGR byte order, but we parse as RGBA in the shader
// So we need to match: R at bit 0, G at bit 8, B at bit 16, A at bit 24
#ifndef fons_rgba
#define fons_rgba(r,g,b,a) (((unsigned int)(a) << 24) | ((unsigned int)(b) << 16) | ((unsigned int)(g) << 8) | (unsigned int)(r))
#endif

namespace
{
	// Vertex shader for shape rendering
	const char* shapeVertexShader = R"(
		#version 430 core
		layout(location = 0) in vec2 aPosition;
		layout(location = 1) in vec4 aColor;
		
		uniform vec2 uViewSize;
		
		out vec4 vColor;
		
		void main()
		{
			vec2 ndc = (aPosition / uViewSize) * 2.0 - 1.0;
			ndc.y = -ndc.y;
			gl_Position = vec4(ndc, 0.0, 1.0);
			vColor = aColor;
		}
	)";
	
	// Fragment shader for shape rendering
	const char* shapeFragmentShader = R"(
		#version 430 core
		in vec4 vColor;
		out vec4 fragColor;
		
		void main()
		{
			fragColor = vColor;
		}
	)";
	
	GLuint compileShader(GLenum type, const char* source)
	{
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);
		return shader;
	}
	
	GLuint createShapeProgram()
	{
		GLuint vs = compileShader(GL_VERTEX_SHADER, shapeVertexShader);
		GLuint fs = compileShader(GL_FRAGMENT_SHADER, shapeFragmentShader);
		
		GLuint program = glCreateProgram();
		glAttachShader(program, vs);
		glAttachShader(program, fs);
		glLinkProgram(program);
		
		glDeleteShader(vs);
		glDeleteShader(fs);
		
		return program;
	}
}

// UILabel implementation
UILabel::UILabel(FONScontext* fontContext, const std::string& text, float fontSize)
	: fontContext_(fontContext)
	, text_(text)
	, fontSize_(fontSize)
{
}

void UILabel::update(float mouseX, float mouseY, bool mouseDown)
{
	// Labels don't respond to mouse input
}

void UILabel::render()
{
	if (!visible_ || !fontContext_) return;
	
	fonsSetSize(fontContext_, fontSize_);
	fonsSetColor(fontContext_, fons_rgba(
		static_cast<unsigned char>(color_.x * 255),
		static_cast<unsigned char>(color_.y * 255),
		static_cast<unsigned char>(color_.z * 255),
		static_cast<unsigned char>(color_.w * 255)
	));
	fonsSetAlign(fontContext_, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
	
	fonsDrawText(fontContext_, position_.x, position_.y, text_.c_str(), nullptr);
}

// UIButton implementation
UIButton::UIButton(FONScontext* fontContext, const std::string& label,
                   float width, float height, ClickCallback onClick)
	: fontContext_(fontContext)
	, label_(label)
	, width_(width)
	, height_(height)
	, onClick_(onClick)
{
}

void UIButton::update(float mouseX, float mouseY, bool mouseDown)
{
	if (!visible_) return;
	
	bool inside = is_point_inside(mouseX, mouseY);
	
	if (inside && mouseDown)
	{
		state_ = ButtonState::Pressed;
		wasPressed_ = true;
	}
	else if (inside)
	{
		state_ = ButtonState::Hovered;
		
		// Trigger click on release
		if (wasPressed_ && !mouseDown && onClick_)
		{
			onClick_();
		}
		wasPressed_ = false;
	}
	else
	{
		state_ = ButtonState::Normal;
		wasPressed_ = false;
	}
}

void UIButton::render()
{
	if (!visible_) return;
	
	// Button rendering is handled by UISystem
	// This is just a placeholder - actual rendering happens in UISystem::render()
}

bool UIButton::is_point_inside(float x, float y) const
{
	return x >= position_.x && x <= position_.x + width_ &&
	       y >= position_.y && y <= position_.y + height_;
}

Vec4f UIButton::get_fill_color() const
{
	switch (state_)
	{
		case ButtonState::Normal:
			return Vec4f{0.2f, 0.2f, 0.2f, 0.7f};  // Semi-transparent dark gray
		case ButtonState::Hovered:
			return Vec4f{0.3f, 0.3f, 0.4f, 0.8f};  // Lighter on hover
		case ButtonState::Pressed:
			return Vec4f{0.4f, 0.4f, 0.5f, 0.9f};  // Even lighter when pressed
	}
	return Vec4f{0.2f, 0.2f, 0.2f, 0.7f};
}

Vec4f UIButton::get_outline_color() const
{
	switch (state_)
	{
		case ButtonState::Normal:
			return Vec4f{0.6f, 0.6f, 0.6f, 1.0f};  // Gray outline
		case ButtonState::Hovered:
			return Vec4f{0.8f, 0.8f, 0.9f, 1.0f};  // Brighter on hover
		case ButtonState::Pressed:
			return Vec4f{1.0f, 1.0f, 1.0f, 1.0f};  // White when pressed
	}
	return Vec4f{0.6f, 0.6f, 0.6f, 1.0f};
}

// UISystem implementation
UISystem::UISystem(int windowWidth, int windowHeight)
	: fontContext_(nullptr)
	, fontNormal_(-1)
	, windowWidth_(windowWidth)
	, windowHeight_(windowHeight)
	, shapeVAO_(0)
	, shapeVBO_(0)
	, shapeProgram_(0)
{
}

UISystem::~UISystem()
{
	if (fontContext_)
		fonsgl_delete(fontContext_);
	
	if (shapeVAO_)
		glDeleteVertexArrays(1, &shapeVAO_);
	if (shapeVBO_)
		glDeleteBuffers(1, &shapeVBO_);
	if (shapeProgram_)
		glDeleteProgram(shapeProgram_);
}

bool UISystem::initialize(const std::string& fontPath)
{
	// Create Fontstash context
	fontContext_ = fonsgl_create(512, 512, FONS_ZERO_TOPLEFT);
	if (!fontContext_)
	{
		std::print(stderr, "Failed to create Fontstash context\n");
		return false;
	}
	
	// Load font
	fontNormal_ = fonsAddFont(fontContext_, "sans", fontPath.c_str());
	if (fontNormal_ == FONS_INVALID)
	{
		std::print(stderr, "Failed to load font: {}\n", fontPath);
		return false;
	}
	
	// Setup shape rendering
	setup_rendering();
	
	// Set initial viewport size for fontstash
	fonsgl_set_viewport(fontContext_, windowWidth_, windowHeight_);
	
	std::print("UI System initialized successfully\n");
	return true;
}

void UISystem::setup_rendering()
{
	// Create shader program for shapes
	shapeProgram_ = createShapeProgram();
	
	// Create VAO and VBO
	glGenVertexArrays(1, &shapeVAO_);
	glGenBuffers(1, &shapeVBO_);
	
	glBindVertexArray(shapeVAO_);
	glBindBuffer(GL_ARRAY_BUFFER, shapeVBO_);
	
	// Position attribute
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	
	// Color attribute
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
	
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void UISystem::update(float mouseX, float mouseY, bool mouseDown)
{
	for (auto& widget : widgets_)
	{
		widget->update(mouseX, mouseY, mouseDown);
	}
}

void UISystem::render()
{
	begin_rendering();
	
	// Set viewport for fontstash at the beginning
	if (fontContext_)
	{
		fonsgl_set_viewport(fontContext_, windowWidth_, windowHeight_);
	}
	
	// Render buttons first (shapes)
	for (auto& widget : widgets_)
	{
		UIButton* button = dynamic_cast<UIButton*>(widget.get());
		if (button && button->is_visible())
		{
			Vec2f pos = button->position_;
			float w = button->width_;
			float h = button->height_;
			
			Vec4f fillColor = button->get_fill_color();
			Vec4f outlineColor = button->get_outline_color();
			
			// Draw filled rectangle
			struct Vertex { float x, y, r, g, b, a; };
			Vertex fillVerts[6] = {
				{pos.x, pos.y, fillColor.x, fillColor.y, fillColor.z, fillColor.w},
				{pos.x + w, pos.y, fillColor.x, fillColor.y, fillColor.z, fillColor.w},
				{pos.x, pos.y + h, fillColor.x, fillColor.y, fillColor.z, fillColor.w},
				{pos.x + w, pos.y, fillColor.x, fillColor.y, fillColor.z, fillColor.w},
				{pos.x + w, pos.y + h, fillColor.x, fillColor.y, fillColor.z, fillColor.w},
				{pos.x, pos.y + h, fillColor.x, fillColor.y, fillColor.z, fillColor.w},
			};
			
			glUseProgram(shapeProgram_);
			GLint locViewSize = glGetUniformLocation(shapeProgram_, "uViewSize");
			glUniform2f(locViewSize, static_cast<float>(windowWidth_), static_cast<float>(windowHeight_));
			
			glBindVertexArray(shapeVAO_);
			glBindBuffer(GL_ARRAY_BUFFER, shapeVBO_);
			glBufferData(GL_ARRAY_BUFFER, sizeof(fillVerts), fillVerts, GL_DYNAMIC_DRAW);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			
			// Draw outline (4 lines)
			Vertex outlineVerts[8] = {
				{pos.x, pos.y, outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.w},
				{pos.x + w, pos.y, outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.w},
				{pos.x + w, pos.y, outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.w},
				{pos.x + w, pos.y + h, outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.w},
				{pos.x + w, pos.y + h, outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.w},
				{pos.x, pos.y + h, outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.w},
				{pos.x, pos.y + h, outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.w},
				{pos.x, pos.y, outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.w},
			};
			
			glBufferData(GL_ARRAY_BUFFER, sizeof(outlineVerts), outlineVerts, GL_DYNAMIC_DRAW);
			glDrawArrays(GL_LINES, 0, 8);
			
			glBindVertexArray(0);
			glUseProgram(0);  // Unbind shape shader before rendering text
			
			// Draw button label (centered)
			if (fontContext_)
			{
				fonsSetSize(fontContext_, 22.0f);  // Larger font for better visibility
				fonsSetColor(fontContext_, fons_rgba(255, 255, 255, 255));
				fonsSetAlign(fontContext_, FONS_ALIGN_CENTER | FONS_ALIGN_MIDDLE);
				
				float textX = pos.x + w / 2.0f;
				float textY = pos.y + h / 2.0f;
				fonsDrawText(fontContext_, textX, textY, button->label_.c_str(), nullptr);
			}
		}
	}
	
	// Render labels (text)
	for (auto& widget : widgets_)
	{
		UILabel* label = dynamic_cast<UILabel*>(widget.get());
		if (label && label->is_visible())
		{
			label->render();
		}
	}
	
	end_rendering();
}

void UISystem::begin_rendering()
{
	// Enable blending for UI
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
}

void UISystem::end_rendering()
{
	// Restore state
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}

void UISystem::on_window_resize(int width, int height)
{
	windowWidth_ = width;
	windowHeight_ = height;
	
	// Update fontstash viewport
	if (fontContext_)
	{
		fonsgl_set_viewport(fontContext_, width, height);
	}
	
	// Update widget positions based on anchors
	for (auto& widget : widgets_)
	{
		UIAnchor anchor = widget->get_anchor();
		Vec2f offset = widget->get_offset();
		
		Vec2f pos = calculate_anchored_position(anchor, offset.x, offset.y);
		
		// Adjust for button size if it's a button
		UIButton* button = dynamic_cast<UIButton*>(widget.get());
		if (button)
		{
			float w = button->width_;
			float h = button->height_;
			
			if (anchor == UIAnchor::BottomCenter || anchor == UIAnchor::Center || anchor == UIAnchor::TopCenter)
				pos.x -= w / 2.0f;
			else if (anchor == UIAnchor::BottomRight || anchor == UIAnchor::CenterRight || anchor == UIAnchor::TopRight)
				pos.x -= w;
			
			if (anchor == UIAnchor::CenterLeft || anchor == UIAnchor::Center || anchor == UIAnchor::CenterRight)
				pos.y -= h / 2.0f;
			else if (anchor == UIAnchor::BottomLeft || anchor == UIAnchor::BottomCenter || anchor == UIAnchor::BottomRight)
				pos.y -= h;
		}
		
		widget->set_position(pos.x, pos.y);
	}
}

UILabel* UISystem::add_label(const std::string& text, float fontSize, 
                             UIAnchor anchor, float offsetX, float offsetY)
{
	auto label = std::make_unique<UILabel>(fontContext_, text, fontSize);
	label->set_anchor(anchor);
	label->set_offset(offsetX, offsetY);
	
	Vec2f pos = calculate_anchored_position(anchor, offsetX, offsetY);
	label->set_position(pos.x, pos.y);
	
	UILabel* ptr = label.get();
	widgets_.push_back(std::move(label));
	return ptr;
}

UIButton* UISystem::add_button(const std::string& label, float width, float height,
                               UIAnchor anchor, float offsetX, float offsetY,
                               UIButton::ClickCallback onClick)
{
	auto button = std::make_unique<UIButton>(fontContext_, label, width, height, onClick);
	button->set_anchor(anchor);
	button->set_offset(offsetX, offsetY);
	
	Vec2f pos = calculate_anchored_position(anchor, offsetX, offsetY);
	
	// Adjust for button size based on anchor
	if (anchor == UIAnchor::BottomCenter || anchor == UIAnchor::Center || anchor == UIAnchor::TopCenter)
		pos.x -= width / 2.0f;
	else if (anchor == UIAnchor::BottomRight || anchor == UIAnchor::CenterRight || anchor == UIAnchor::TopRight)
		pos.x -= width;
	
	if (anchor == UIAnchor::CenterLeft || anchor == UIAnchor::Center || anchor == UIAnchor::CenterRight)
		pos.y -= height / 2.0f;
	else if (anchor == UIAnchor::BottomLeft || anchor == UIAnchor::BottomCenter || anchor == UIAnchor::BottomRight)
		pos.y -= height;
	
	button->set_position(pos.x, pos.y);
	
	UIButton* ptr = button.get();
	widgets_.push_back(std::move(button));
	return ptr;
}

Vec2f UISystem::calculate_anchored_position(UIAnchor anchor, float offsetX, float offsetY) const
{
	Vec2f pos{0.f, 0.f};
	
	switch (anchor)
	{
		case UIAnchor::TopLeft:
			pos = Vec2f{offsetX, offsetY};
			break;
		case UIAnchor::TopCenter:
			pos = Vec2f{windowWidth_ / 2.0f + offsetX, offsetY};
			break;
		case UIAnchor::TopRight:
			pos = Vec2f{windowWidth_ - offsetX, offsetY};
			break;
		case UIAnchor::CenterLeft:
			pos = Vec2f{offsetX, windowHeight_ / 2.0f + offsetY};
			break;
		case UIAnchor::Center:
			pos = Vec2f{windowWidth_ / 2.0f + offsetX, windowHeight_ / 2.0f + offsetY};
			break;
		case UIAnchor::CenterRight:
			pos = Vec2f{windowWidth_ - offsetX, windowHeight_ / 2.0f + offsetY};
			break;
		case UIAnchor::BottomLeft:
			pos = Vec2f{offsetX, windowHeight_ - offsetY};
			break;
		case UIAnchor::BottomCenter:
			pos = Vec2f{windowWidth_ / 2.0f + offsetX, windowHeight_ - offsetY};
			break;
		case UIAnchor::BottomRight:
			pos = Vec2f{windowWidth_ - offsetX, windowHeight_ - offsetY};
			break;
	}
	
	return pos;
}
