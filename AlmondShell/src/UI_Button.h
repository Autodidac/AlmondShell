// UIButton.h
#pragma once
//#include "BasicRenderer.h"
#include "EventSystem.h"
#include <string>

class UIButton {
public:
    UIButton(float x, float y, float width, float height, const std::string& label);

    void SetOnClick(void (*callback)());
    void Update(const Event& event);
    //void Render(BasicRenderer& renderer);

private:
    float x, y, width, height;
    std::string label;
    void (*onClickCallback)();
    bool isHovered;
    bool isPressed;
};