#ifndef UI_SYSTEM_HPP_F1E2D3C4_B5A6_9788_CDEF_0123456789AB
#define UI_SYSTEM_HPP_F1E2D3C4_B5A6_9788_CDEF_0123456789AB

#include <glad/glad.h>
#include <vector>
#include <string>
#include <functional>
#include <memory>

#include "../vmlib/vec2.hpp"
#include "../vmlib/vec4.hpp"

// Forward declarations
struct FONScontext;

// UI anchor points for positioning
enum class UIAnchor
{
	TopLeft,
	TopCenter,
	TopRight,
	CenterLeft,
	Center,
	CenterRight,
	BottomLeft,
	BottomCenter,
	BottomRight
};

// Button states
enum class ButtonState
{
	Normal,
	Hovered,
	Pressed
};

// Base UI widget class
class UIWidget
{
public:
	virtual ~UIWidget() = default;
	
	virtual void update(float mouseX, float mouseY, bool mouseDown) = 0;
	virtual void render() = 0;
	
	void set_position(float x, float y) { position_ = Vec2f{x, y}; }
	void set_anchor(UIAnchor anchor) { anchor_ = anchor; }
	void set_offset(float x, float y) { offset_ = Vec2f{x, y}; }
	void set_visible(bool visible) { visible_ = visible; }
	
	bool is_visible() const { return visible_; }
	UIAnchor get_anchor() const { return anchor_; }
	Vec2f get_offset() const { return offset_; }
	
protected:
	Vec2f position_{0.f, 0.f};
	Vec2f offset_{0.f, 0.f};  // Offset from anchor point
	UIAnchor anchor_ = UIAnchor::TopLeft;
	bool visible_ = true;
};

// Text label widget
class UILabel : public UIWidget
{
public:
	UILabel(FONScontext* fontContext, const std::string& text, float fontSize);
	
	void update(float mouseX, float mouseY, bool mouseDown) override;
	void render() override;
	
	void set_text(const std::string& text) { text_ = text; }
	void set_color(Vec4f color) { color_ = color; }
	void set_font_size(float size) { fontSize_ = size; }
	
private:
	FONScontext* fontContext_;
	std::string text_;
	float fontSize_;
	Vec4f color_{1.f, 1.f, 1.f, 1.f};
};

// Button widget
class UIButton : public UIWidget
{
public:
	using ClickCallback = std::function<void()>;
	
	UIButton(FONScontext* fontContext, const std::string& label, 
	         float width, float height, ClickCallback onClick);
	
	void update(float mouseX, float mouseY, bool mouseDown) override;
	void render() override;
	
	void set_label(const std::string& label) { label_ = label; }
	void set_size(float width, float height) { width_ = width; height_ = height; }
	
	ButtonState get_state() const { return state_; }
	
	// Make these accessible to UISystem for rendering
	friend class UISystem;
	
private:
	bool is_point_inside(float x, float y) const;
	Vec4f get_fill_color() const;
	Vec4f get_outline_color() const;
	
	FONScontext* fontContext_;
	std::string label_;
	float width_;
	float height_;
	ClickCallback onClick_;
	
	ButtonState state_ = ButtonState::Normal;
	bool wasPressed_ = false;
};

// UI System manager
class UISystem
{
public:
	UISystem(int windowWidth, int windowHeight);
	~UISystem();
	
	// Initialize with font
	bool initialize(const std::string& fontPath);
	
	// Update all widgets
	void update(float mouseX, float mouseY, bool mouseDown);
	
	// Render all widgets
	void render();
	
	// Window resize callback
	void on_window_resize(int width, int height);
	
	// Add widgets
	UILabel* add_label(const std::string& text, float fontSize, UIAnchor anchor, float offsetX, float offsetY);
	UIButton* add_button(const std::string& label, float width, float height, 
	                     UIAnchor anchor, float offsetX, float offsetY,
	                     UIButton::ClickCallback onClick);
	
	// Get font context for custom rendering
	FONScontext* get_font_context() { return fontContext_; }
	
	// Calculate anchored position
	Vec2f calculate_anchored_position(UIAnchor anchor, float offsetX, float offsetY) const;
	
private:
	void setup_rendering();
	void begin_rendering();
	void end_rendering();
	
	FONScontext* fontContext_;
	int fontNormal_;
	
	int windowWidth_;
	int windowHeight_;
	
	std::vector<std::unique_ptr<UIWidget>> widgets_;
	
	// OpenGL resources for shape rendering
	GLuint shapeVAO_;
	GLuint shapeVBO_;
	GLuint shapeProgram_;
};

#endif // UI_SYSTEM_HPP_F1E2D3C4_B5A6_9788_CDEF_0123456789AB
