// UIManager.cpp
#include "alsUImanager.h"

void UIManager::AddButton(UIButton* button) {
    buttons.push_back(button);
}

void UIManager::Update(EventSystem& eventSystem) {
    // Poll events and update buttons
    eventSystem.PollEvents();
    for (auto& button : buttons) {
        // Pass events to each button
        Event event;
        button->Update(event);  // Handle event logic in the button
    }
}
/*
void UIManager::Render(BasicRenderer& renderer) {
    // Render each button
    for (auto& button : buttons) {
        button->Render(renderer);
    }
}
*/