#pragma once
#include "Engine/Scene/IScene.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Camera/DebugCamera.h"
#include "engine/3d/obj/SkinnedObj3d.h"
#include "engine/3d/animation/CustomAnimation.h"
#include <memory>

class AnimationEditorScene : public IScene {
public:
    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void DrawDebugUI() override;

private:
    std::unique_ptr<Camera> camera_ = nullptr;
    std::unique_ptr<DebugCamera> debugCamera_ = nullptr;
    std::unique_ptr<SkinnedObj3d> skinnedObj_ = nullptr;

    // アニメーション作成用
    CustomAnimationTrack myAnimTrack_;
    float animCurrentTime_ = 0.0f;
    bool isAnimPlaying_ = false;
};