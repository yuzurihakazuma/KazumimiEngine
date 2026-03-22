#pragma once

#include "Engine/Scene/IScene.h"

class GameOverScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
    void DrawDebugUI() override {}
};