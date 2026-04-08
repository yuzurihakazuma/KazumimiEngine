#define NOMINMAX
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
#include "engine/postEffect/PostEffect.h"
#include "engine/utils/EditorManager.h"
#include <algorithm>
#include <string>

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
    skinnedObj_->SetName("human");   

    // 編集対象に登録
    targets_.push_back(skinnedObj_.get());

    // 各オブジェクトのトラックを初期化
    for (auto* t : targets_) {
        animTracks_[t->GetName()] = CustomAnimationTrack{};
    }
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

    if (!targets_.empty()) {
        IAnimatable* target = targets_[selectedTargetIndex_];
        CustomAnimationTrack& track = animTracks_[target->GetName()];  

        if (isAnimPlaying_ && track.duration > 0.0f) {
            animCurrentTime_ += 1.0f / 60.0f;
            if (animCurrentTime_ > track.duration) {
                animCurrentTime_ = 0.0f;
            }
            Vector3 pos, rot, scale;
            track.UpdateTransformAtTime(animCurrentTime_, pos, rot, scale);
            target->SetTranslation(pos);
            target->SetRotation(rot);
            target->SetScale(scale);
        }
        target->Update();
    }
}

void AnimationEditorScene::Draw() {
    auto dxCommon = DirectXCommon::GetInstance();
    auto commandList = dxCommon->GetCommandList();

    // PostEffectのRenderTextureに描画
    PostEffect::GetInstance()->PreDrawScene(commandList);

    Obj3dCommon::GetInstance()->PreDraw(commandList);
    if (skinnedObj_) skinnedObj_->Draw();

    PostEffect::GetInstance()->PostDrawScene(commandList);
    PostEffect::GetInstance()->Draw(commandList);

    // Game Viewに表示するSRVを通知
    EditorManager::GetInstance()->SetGameViewSrvIndex(PostEffect::GetInstance()->GetSrvIndex());
}

void AnimationEditorScene::DrawDebugUI() {
#ifdef USE_IMGUI
    if (camera_) camera_->DrawDebugUI();
    if (debugCamera_) debugCamera_->DrawDebugUI();

    ImGui::Begin("Animation Editor");

    if (!targets_.empty()) {

        // -----------------------------------------------
        // [0] オブジェクト選択
        // -----------------------------------------------
        std::vector<const char*> names;
        for (auto* t : targets_) names.push_back(t->GetName().c_str());
        ImGui::Combo("Target", &selectedTargetIndex_, names.data(), (int)names.size());

        IAnimatable* target = targets_[selectedTargetIndex_];
        CustomAnimationTrack& track = animTracks_[target->GetName()];

        // -----------------------------------------------
        // [1] モデルのトランスフォーム操作
        // -----------------------------------------------
        Vector3 pos = target->GetTranslation();
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) target->SetTranslation(pos);

        Vector3 rot = target->GetRotation();
        if (ImGui::DragFloat3("Rotation", &rot.x, 0.05f)) target->SetRotation(rot);

        Vector3 scale = target->GetScale();
        if (ImGui::DragFloat3("Scale", &scale.x, 0.05f)) target->SetScale(scale);

        ImGui::Separator();

        // -----------------------------------------------
        // [2] 再生コントロール（FPS・フレーム・秒）
        // -----------------------------------------------
        ImGui::InputInt("FPS", &fps_);
        if (fps_ < 1) fps_ = 1;

        int maxFrame = (int)(timelineDuration_ * fps_);
        if (ImGui::SliderInt("Frame", &currentFrame_, 0, maxFrame)) {
            animCurrentTime_ = currentFrame_ / (float)fps_;
        }
        if (ImGui::SliderFloat("Time(sec)", &animCurrentTime_, 0.0f, timelineDuration_)) {
            currentFrame_ = (int)(animCurrentTime_ * fps_);
        }

        if (ImGui::Button(isAnimPlaying_ ? "Stop" : "Play")) {
            isAnimPlaying_ = !isAnimPlaying_;
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Keyframe")) {
            track.AddKeyframe(animCurrentTime_, pos, rot, scale);
            if (track.duration < animCurrentTime_) track.duration = animCurrentTime_;
            if (!track.keyframes.empty())
                selectedKeyframeIndex_ = (int)track.keyframes.size() - 1;
            timelineDuration_ = std::max(timelineDuration_, track.duration + 1.0f);
        }

        ImGui::Separator();

        // -----------------------------------------------
        // [3] タイムラインバー
        // -----------------------------------------------
        ImVec2 barPos = ImGui::GetCursorScreenPos();
        float barWidth = ImGui::GetContentRegionAvail().x;
        float barHeight = 40.0f;
        ImDrawList* draw = ImGui::GetWindowDrawList();

        draw->AddRectFilled(
            barPos,
            ImVec2(barPos.x + barWidth, barPos.y + barHeight),
            IM_COL32(50, 50, 50, 255));

        for (int i = 0; i < (int)track.keyframes.size(); i++) {
            float t = track.keyframes[i].time / timelineDuration_;
            float x = barPos.x + t * barWidth;
            float y = barPos.y + barHeight * 0.5f;
            ImU32 color = (i == selectedKeyframeIndex_)
                ? IM_COL32(255, 200, 0, 255)
                : IM_COL32(100, 200, 255, 255);
            draw->AddQuadFilled(
                ImVec2(x, y - 8),
                ImVec2(x + 6, y),
                ImVec2(x, y + 8),
                ImVec2(x - 6, y),
                color);
        }

        float cx = barPos.x + (animCurrentTime_ / timelineDuration_) * barWidth;
        draw->AddLine(
            ImVec2(cx, barPos.y),
            ImVec2(cx, barPos.y + barHeight),
            IM_COL32(255, 50, 50, 255), 2.0f);

        ImGui::InvisibleButton("timeline", ImVec2(barWidth, barHeight));
        if (ImGui::IsItemActive()) {
            float ratio = (ImGui::GetIO().MousePos.x - barPos.x) / barWidth;
            ratio = std::clamp(ratio, 0.0f, 1.0f);
            animCurrentTime_ = ratio * timelineDuration_;
            currentFrame_ = (int)(animCurrentTime_ * fps_);
        }

        ImGui::Separator();

        // -----------------------------------------------
        // [4] キーフレーム一覧テーブル
        // -----------------------------------------------
        ImGui::Text("Keyframes: %zu", track.keyframes.size());

        if (ImGui::BeginTable("kf_table", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0, 120))) {

            ImGui::TableSetupColumn("Frame");
            ImGui::TableSetupColumn("Time(s)");
            ImGui::TableSetupColumn("Pos");
            ImGui::TableSetupColumn("Del");
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)track.keyframes.size(); i++) {
                auto& kf = track.keyframes[i];
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                char label[32];
                snprintf(label, sizeof(label), "%d##row%d", (int)(kf.time * fps_), i);
                if (ImGui::Selectable(label, selectedKeyframeIndex_ == i,
                    ImGuiSelectableFlags_SpanAllColumns)) {
                    selectedKeyframeIndex_ = i;
                    animCurrentTime_ = kf.time;
                    currentFrame_ = (int)(kf.time * fps_);
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f", kf.time);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.1f / %.1f / %.1f",
                    kf.position.x, kf.position.y, kf.position.z);

                ImGui::TableSetColumnIndex(3);
                std::string delLabel = "X##" + std::to_string(i);
                if (ImGui::Button(delLabel.c_str())) {
                    track.keyframes.erase(track.keyframes.begin() + i);
                    track.duration = track.keyframes.empty()
                        ? 0.0f : track.keyframes.back().time;
                    if (selectedKeyframeIndex_ >= (int)track.keyframes.size())
                        selectedKeyframeIndex_ = -1;
                    break;
                }
            }
            ImGui::EndTable();
        }

        ImGui::Separator();

        // -----------------------------------------------
        // [5] 選択中キーフレームの直接編集
        // -----------------------------------------------
        if (selectedKeyframeIndex_ >= 0 &&
            selectedKeyframeIndex_ < (int)track.keyframes.size()) {

            auto& kf = track.keyframes[selectedKeyframeIndex_];
            ImGui::Text("Edit Keyframe [%d]", selectedKeyframeIndex_);

            int kfFrame = (int)(kf.time * fps_);
            if (ImGui::InputInt("KF Frame", &kfFrame)) {
                kf.time = kfFrame / (float)fps_;
                if (!track.keyframes.empty())
                    track.duration = track.keyframes.back().time;
            }
            ImGui::DragFloat3("KF Position", &kf.position.x, 0.1f);
            ImGui::DragFloat3("KF Rotation", &kf.rotation.x, 0.05f);
            ImGui::DragFloat3("KF Scale", &kf.scale.x, 0.05f);
        }

        ImGui::Separator();

        // -----------------------------------------------
        // [6] Save / Load
        // -----------------------------------------------
        if (ImGui::Button("Save")) {
            track.SaveToJson("resources/" + target->GetName() + "_anim.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            track.LoadFromJson("resources/" + target->GetName() + "_anim.json");
            animCurrentTime_ = 0.0f;
            currentFrame_ = 0;
            isAnimPlaying_ = false;
            selectedKeyframeIndex_ = -1;
            timelineDuration_ = std::max(5.0f, track.duration + 1.0f);
        }
    }

    ImGui::End();
#endif
}
