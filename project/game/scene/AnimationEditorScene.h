#pragma once
#include "Engine/Scene/IScene.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Camera/DebugCamera.h"
#include "engine/3d/obj/SkinnedObj3d.h"
#include "engine/3d/animation/CustomAnimation.h"
#include "engine/3d/obj/IAnimatable.h"    
#include <memory>
#include <unordered_map>  
#include <vector> 

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

    // ↓ここから変更
    std::vector<IAnimatable*> targets_;                              // 編集対象リスト
    int selectedTargetIndex_ = 0;                                    // 選択中のオブジェクト
    std::unordered_map<std::string, CustomAnimationTrack> animTracks_; // オブジェクトごとのトラック

    float animCurrentTime_ = 0.0f;
    bool isAnimPlaying_ = false;
    int selectedKeyframeIndex_ = -1;
    int fps_ = 60;
    int currentFrame_ = 0;
    float timelineZoom_ = 1.0f;
    float timelineDuration_ = 5.0f;
};