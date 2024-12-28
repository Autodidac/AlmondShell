// UIButton.cpp
#include "UI_Button.h"

UIButton::UIButton(float x, float y, float width, float height, const std::string& label)
    : x(x), y(y), width(width), height(height), label(label), onClickCallback(nullptr), isHovered(false), isPressed(false) {
}

void UIButton::SetOnClick(void (*callback)()) {
    onClickCallback = callback;
}

void UIButton::Update(const Event& event) {
    // Check if mouse is hovering
    if (event.type == EventType::MouseMove) {
        isHovered = (event.x >= x && event.x <= x + width && event.y >= y && event.y <= y + height);
    }

    // Check for click
    if (event.type == EventType::MouseButtonClick && isHovered) {
        isPressed = true;
        if (onClickCallback) {
            onClickCallback(); // Trigger the callback
        }
    }
}
/*
void UIButton::Render(BasicRenderer& renderer) {
    unsigned int color = isHovered ? 0x00FF00 : 0xFFFFFF;  // Green when hovered, white when normal
    renderer.DrawRect(x, y, width, height, color);
    renderer.DrawText(x + 10, y + 10, label, 0x000000);  // Text color black
}
*/