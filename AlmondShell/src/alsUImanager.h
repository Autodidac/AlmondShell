// UIManager.h
#pragma once
#include "alsUIbutton.h"
#include <vector>

class UIManager {
public:
    void AddButton(UIButton* button);
    void Update(EventSystem& eventSystem);
    //void Render(BasicRenderer& renderer);

private:
    std::vector<UIButton*> buttons;
};
