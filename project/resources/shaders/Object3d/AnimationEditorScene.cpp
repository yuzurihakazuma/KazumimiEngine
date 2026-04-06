#include "AnimationEditorScene.h"
#include "Engine/Base/Input.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Base/WindowProc.h"
#include "Engine/Utils/ImGuiManager.h"
#include "Engine/3D/Model/ModelManager.h"
#include "Engine/3D/Obj/Obj3dCommon.h"
#include "Engine/Graphics/PipelineManager.h"
#include "TitleScene.h" // Tキーでタイトルに戻る用等

void AnimationEditorScene::Initialize() {
    auto windowProc = WindowProc::GetInstance();
    auto dxCommon = DirectXCommon::GetInstance();

    // モデル読み込み
    ModelManager::GetInstance()->LoadModel("human", "resources/human", "walk.gltf");

    camera_ = std::make_unique<Camera>(windowProc->GetClientWidth(), windowProc->GetClientHeight(), dxCommon);
    camera_->SetTranslation({ 0.0f, 2.0f, -15.0f });

    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize();

    skinnedObj_ = SkinnedObj3d::Create("human", "resources/human", "walk.gltf");
    skinnedObj_->SetCamera(camera_.get());
    skinnedObj_->SetScale({ 1.0f, 1.0f, 1.0f });
}

void AnimationEditorScene::Finalize() {}

void AnimationEditorScene::Update() {
    Input* input = Input::GetInstance();

    if (debugCamera_) debugCamera_->Update(camera_.get());
    camera_->Update();

    // Tキーを押したらタイトル画面に戻れるようにする
    if (input->Triggerkey(DIK_T)) {
        SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
        return;
    }

    if (skinnedObj_) {
        // 再生中なら時間を進めて計算
        if (isAnimPlaying_ && myAnimTrack_.duration > 0.0f) {
            animCurrentTime_ += 1.0f / 60.0f;
            if (animCurrentTime_ > myAnimTrack_.duration) {
                animCurrentTime_ = 0.0f;
            }
            Vector3 pos, rot, scale;
            myAnimTrack_.UpdateTransformAtTime(animCurrentTime_, pos, rot, scale);
            skinnedObj_->SetTranslation(pos);
            skinnedObj_->SetRotation(rot);
            skinnedObj_->SetScale(scale);
        }
        skinnedObj_->Update();
    }
}

void AnimationEditorScene::Draw() {
    auto dxCommon = DirectXCommon::GetInstance();
    auto commandList = dxCommon->GetCommandList();

    // 描画準備
    Obj3dCommon::GetInstance()->PreDraw(commandList);

    // 今回はモデルしか置かないシンプルな構成にします
    if (skinnedObj_) skinnedObj_->Draw();
}

void AnimationEditorScene::DrawDebugUI() {
#ifdef USE_IMGUI
    if (camera_) camera_->DrawDebugUI();
    if (debugCamera_) debugCamera_->DrawDebugUI();

    ImGui::Begin("Animation Editor");

    if (skinnedObj_) {
        Vector3 pos = skinnedObj_->GetTranslation();
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) skinnedObj_->SetTranslation(pos);
        
        Vector3 rot = skinnedObj_->GetRotation();
        if (ImGui::DragFloat3("Rotation", &rot.x, 0.05f)) skinnedObj_->SetRotation(rot);
        
        Vector3 scale = skinnedObj_->GetScale();
        if (ImGui::DragFloat3("Scale", &scale.x, 0.05f)) skinnedObj_->SetScale(scale);

        ImGui::Separator();
        
        // タイムラインと録画
        ImGui::SliderFloat("Time(sec)", &animCurrentTime_, 0.0f, 10.0f);
        if (ImGui::Button(isAnimPlaying_ ? "Stop" : "Play")) {
            isAnimPlaying_ = !isAnimPlaying_;
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Keyframe")) {
            myAnimTrack_.AddKeyframe(animCurrentTime_, pos, rot, scale);
        }
        ImGui::Text("Keyframes: %zu", myAnimTrack_.keyframes.size());

        ImGui::Separator();
        ImGui::Text("--- Save / Load (JSON) ---");

        if (ImGui::Button("Save")) {
            myAnimTrack_.SaveToJson("resources/custom_anim.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            myAnimTrack_.LoadFromJson("resources/custom_anim.json");
            animCurrentTime_ = 0.0f;
            isAnimPlaying_ = false;
        }
    }
    
    ImGui::End();
#endif
}