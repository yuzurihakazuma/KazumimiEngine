#pragma once

#include "Engine/Scene/IScene.h"
#include "Engine/2D/Sprite.h"
#include <memory>

class GameOverScene : public IScene {
public:
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
    void DrawDebugUI() override {}

private:
    std::unique_ptr<Sprite> sprite_ = nullptr;
};