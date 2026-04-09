#define NOMINMAX
#include "AnimationEditor.h"
#include "engine/utils/ImGuiManager.h"
#include <algorithm>
#include <string>

void AnimationEditor::Initialize() {
    // 現時点では追加の初期化なし
    // AddTarget() で対象オブジェクトを外から登録してもらう
}

void AnimationEditor::AddTarget(IAnimatable* target) {
    targets_.push_back(target);
    // 登録と同時にトラックを初期化
    animTracks_[target->GetName()] = CustomAnimationTrack{};
}

void AnimationEditor::Update() {
    if (targets_.empty()) return;

    IAnimatable* target = targets_[selectedTargetIndex_];
    CustomAnimationTrack& track = animTracks_[target->GetName()];

    // 再生中かつキーフレームが存在する場合のみ時間を進めるボタン
    if (isAnimPlaying_ && track.duration > 0.0f) {
        animCurrentTime_ += (1.0f / 60.0f) * playbackSpeed_;

        // 末尾に達したらループ or 停止
        if (animCurrentTime_ > track.duration) {
            if (isLoop_) {
                animCurrentTime_ = 0.0f;
            }
            else {
                animCurrentTime_ = track.duration;
                isAnimPlaying_ = false;
            }
        }

        // フレーム番号を秒から逆算
        currentFrame_ = (int)(animCurrentTime_ * fps_);

        // キーフレーム補間でトランスフォームを計算してモデルに適用
        Vector3 pos, rot, scale;
        track.UpdateTransformAtTime(animCurrentTime_, pos, rot, scale);
        target->SetTranslation(pos);
        target->SetRotation(rot);
        target->SetScale(scale);
    }
}

void AnimationEditor::DrawDebugUI() {
#ifdef USE_IMGUI
    ImGui::Begin("アニメーションエディター");

    if (!targets_.empty()) {
            ImGui::PushItemWidth(-150.0f);
        // -----------------------------------------------
        // [0] 編集対象オブジェクトの選択
        // -----------------------------------------------
        std::vector<const char*> names;
        for (auto* t : targets_) names.push_back(t->GetName().c_str());
        ImGui::Combo("対象オブジェクト", &selectedTargetIndex_, names.data(), (int)names.size());

        IAnimatable* target = targets_[selectedTargetIndex_];
        CustomAnimationTrack& track = animTracks_[target->GetName()];

        // -----------------------------------------------
        // [1] トランスフォームの直接操作
        // -----------------------------------------------
        Vector3 pos = target->GetTranslation();
        if (ImGui::DragFloat3("位置", &pos.x, 0.1f))  target->SetTranslation(pos);

        Vector3 rot = target->GetRotation();
        if (ImGui::DragFloat3("回転", &rot.x, 0.05f)) target->SetRotation(rot);

        Vector3 scale = target->GetScale();
        if (ImGui::DragFloat3("スケール", &scale.x, 0.05f)) target->SetScale(scale);

        if (ImGui::Button("位置リセット")) { target->SetTranslation({ 0,0,0 }); }
        ImGui::SameLine();
        if (ImGui::Button("回転リセット")) { target->SetRotation({ 0,0,0 }); }
        ImGui::SameLine();
        if (ImGui::Button("スケールリセット")) { target->SetScale({ 1,1,1 }); }

        ImGui::Separator();

		// 先頭に戻るボタン
        if (ImGui::Button("|◀")) {
			animCurrentTime_ = 0.0f;
			currentFrame_ = 0;
			isAnimPlaying_ = false;
			// トランスフォームを先頭のキーフレームに合わせる
            if (!track.keyframes.empty()){
                target->SetTranslation(track.keyframes.front().position);
                target->SetRotation(track.keyframes.front().rotation);
				target->SetScale(track.keyframes.front().scale);

            }

        }
        

		ImGui::SameLine();

        if (ImGui::Button("◀")) {
			float best = -1.0f;
			int bestIndex = -1;
            for (int i = 0; i < (int)track.keyframes.size();i++) {
                float time = track.keyframes[i].time;
                if (time< animCurrentTime_ -0.001f && time > best){
					best = time; bestIndex = i;
                }
            }
            if (bestIndex>=0) {
				selectedKeyframeIndex_ = bestIndex;
				animCurrentTime_ = track.keyframes[bestIndex].time;
				currentFrame_ = (int)(animCurrentTime_ * fps_);
				target->SetTranslation(track.keyframes[bestIndex].position);
				target->SetRotation(track.keyframes[bestIndex].rotation);
				target->SetScale(track.keyframes[bestIndex].scale);
                
            }
        }
		ImGui::SameLine();
        if (ImGui::Button("▶|")) {
            float best = -1.0f;
            int bestIndex = -1;
            for (int i = 0; i < (int)track.keyframes.size(); i++) {
                float time = track.keyframes[i].time;
                if (time > animCurrentTime_ + 0.001f && (best < 0.0f || time < best)) {
                    best = time; bestIndex = i;
                }
            }
            if (bestIndex >= 0) {
                selectedKeyframeIndex_ = bestIndex;
                animCurrentTime_ = track.keyframes[bestIndex].time;
                currentFrame_ = (int)(animCurrentTime_ * fps_);
                target->SetTranslation(track.keyframes[bestIndex].position);
                target->SetRotation(track.keyframes[bestIndex].rotation);
                target->SetScale(track.keyframes[bestIndex].scale);
            }
        }

        if (selectedKeyframeIndex_>=0 && selectedKeyframeIndex_<(int)track.keyframes.size()){
            if (ImGui::Button("キーフレーム更新")) {
				auto& kf = track.keyframes[selectedKeyframeIndex_];
				kf.position = target->GetTranslation();
				kf.rotation = target->GetRotation();
				kf.scale = target->GetScale();

            }

        }
        ImGui::SameLine();
        if (selectedKeyframeIndex_ >= 0 && selectedKeyframeIndex_ < (int)track.keyframes.size()) {
            if (ImGui::Button("コピー")) {
                copiedKeyframeIndex_ = selectedKeyframeIndex_;
            }
        }
        ImGui::SameLine();
        if (copiedKeyframeIndex_ >= 0 && copiedKeyframeIndex_ < (int)track.keyframes.size()) {
            if (ImGui::Button("ペースト")) {
                auto kf = track.keyframes[copiedKeyframeIndex_]; // コピー
                kf.time = animCurrentTime_; // 現在時刻に配置
                track.AddKeyframe(kf.time, kf.position, kf.rotation, kf.scale);
                std::sort(track.keyframes.begin(), track.keyframes.end(),
                    [](const TransformKeyframe& a, const TransformKeyframe& b) {
                        return a.time < b.time;
                    });
                if (!track.keyframes.empty()) track.duration = track.keyframes.back().time;
            }
        }


        // -----------------------------------------------
        // [2] 再生コントロール
        // -----------------------------------------------
        ImGui::InputInt("FPS", &fps_);
        if (fps_ < 1) fps_ = 1;

        // フレーム番号スライダー（動かすと秒に反映）
        int maxFrame = (int)(timelineDuration_ * fps_);
        if (ImGui::SliderInt("フレーム", &currentFrame_, 0, maxFrame)) {
            animCurrentTime_ = currentFrame_ / (float)fps_;
        }
        // 秒スライダー（動かすとフレームに反映）
        if (ImGui::SliderFloat("時間 (秒)", &animCurrentTime_, 0.0f, timelineDuration_)) {
            currentFrame_ = (int)(animCurrentTime_ * fps_);
        }

        ImGui::SliderFloat("再生速度", &playbackSpeed_, 0.1f, 3.0f);
        ImGui::SliderFloat("タイムライン長 (秒)", &timelineDuration_, 1.0f, 30.0f);
       
        // 再生/停止・ループ・キーフレーム追加
        if (ImGui::Button(isAnimPlaying_ ? "停止" : "再生")) {
            isAnimPlaying_ = !isAnimPlaying_;
        }
        ImGui::SameLine();
        ImGui::Checkbox("ループ", &isLoop_);
        ImGui::SameLine();
        ImGui::SameLine();
        ImGui::Checkbox("スナップ", &snapToFrame_);
        if (ImGui::Button("キーフレーム追加")) {
            track.AddKeyframe(animCurrentTime_, pos, rot, scale);
            if (track.duration < animCurrentTime_) track.duration = animCurrentTime_;
            if (!track.keyframes.empty())
                selectedKeyframeIndex_ = (int)track.keyframes.size() - 1;
            // タイムラインが短い場合は自動延長
            float minDuration = track.duration + 1.0f;
            if (timelineDuration_ < minDuration) timelineDuration_ = minDuration;
        }

        ImGui::Separator();

        // -----------------------------------------------
        // [3] タイムラインバー
        //     - クリックで赤い線をシーク
        //     - ◆をドラッグでキーフレームの時間を移動
        // -----------------------------------------------
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 barPos = ImGui::GetCursorScreenPos();
        float barWidth = ImGui::GetContentRegionAvail().x;
        float barHeight = 40.0f;

        // 背景
        draw->AddRectFilled(barPos,
            ImVec2(barPos.x + barWidth, barPos.y + barHeight),
            IM_COL32(50, 50, 50, 255));

		// タイムラインの目盛り（1秒ごと）
        float tickStop = 1.0f;
        if (timelineDuration_ > 20.0f){
    
            tickStop = 5.0f;
        // タイムラインが長い場合は目盛りを粗くする
		}else if(timelineDuration_>10.0f) {
			tickStop = 2.0f;
        }
		// 目盛りとラベル
        for (float t = 0.0f; t <= timelineDuration_ + 0.001f; t+= tickStop){
			float x = barPos.x + (t / timelineDuration_) * barWidth;
			// 目盛り線
            draw->AddLine(
                ImVec2(x, barPos.y),
                ImVec2(x, barPos.y + 6.0f),
                IM_COL32(150, 150, 150, 255));
			// ラベル
			char tickLabel[16];
			// 小数点以下は表示しない
			snprintf(tickLabel, sizeof(tickLabel), "%.0f", t);
            draw->AddText(ImVec2(x + 2.0f, barPos.y + 1.0f),
                IM_COL32(180, 180, 180, 255), tickLabel);

        }

        // バー全体を1つのInvisibleButtonで管理（複数ボタン競合を防ぐため）
        ImGui::SetCursorScreenPos(barPos);
        ImGui::InvisibleButton("timeline", ImVec2(barWidth, barHeight));

        // クリック開始時: キーフレームの近くかどうかを判定
        if (ImGui::IsItemActivated()) {
            float mx = ImGui::GetIO().MousePos.x;
            draggingKeyframe_ = -1;
            for (int i = 0; i < (int)track.keyframes.size(); i++) {
                float kx = barPos.x + (track.keyframes[i].time / timelineDuration_) * barWidth;
                if (mx >= kx - 10.0f && mx <= kx + 10.0f) {
                    // キーフレームの近くをクリック → そのキーフレームをドラッグ対象に
                    draggingKeyframe_ = i;
                    selectedKeyframeIndex_ = i;
                    animCurrentTime_ = track.keyframes[i].time;
                    currentFrame_ = (int)(animCurrentTime_ * fps_);
                    target->SetTranslation(track.keyframes[i].position);
                    target->SetRotation(track.keyframes[i].rotation);
                    target->SetScale(track.keyframes[i].scale);
                    break;
                }
            }
        }

        // ドラッグ中: キーフレーム移動 or 赤い線シーク
        if (ImGui::IsItemActive()) {
            float mx = ImGui::GetIO().MousePos.x;
            float ratio = (mx - barPos.x) / barWidth;
            ratio = (ratio < 0.0f) ? 0.0f : (ratio > 1.0f) ? 1.0f : ratio;

            if (draggingKeyframe_ >= 0 && draggingKeyframe_ < (int)track.keyframes.size()) {
                // キーフレームの時間を移動
                track.keyframes[draggingKeyframe_].time = ratio * timelineDuration_;
				// スナップ有効ならフレーム単位に丸める
                if (snapToFrame_) {
                    float frameTime = 1.0f / fps_;
                    track.keyframes[draggingKeyframe_].time =
                        roundf(track.keyframes[draggingKeyframe_].time / frameTime) * frameTime;
                }
                // 移動後は時間順にソート
                std::sort(track.keyframes.begin(), track.keyframes.end(),
                    [](const TransformKeyframe& a, const TransformKeyframe& b) {
                        return a.time < b.time;
                    });
                if (!track.keyframes.empty())
                    track.duration = track.keyframes.back().time;
            }
            else {
                // 赤い線（現在時刻）をシーク
                animCurrentTime_ = ratio * timelineDuration_;
                currentFrame_ = (int)(animCurrentTime_ * fps_);
            }
        }

        // ボタンを離したらドラッグ状態をリセット
        if (!ImGui::IsItemActive()) {
            draggingKeyframe_ = -1;
        }

        // キーフレームの◆を描画
        for (int i = 0; i < (int)track.keyframes.size(); i++) {
            float t = track.keyframes[i].time / timelineDuration_;
            float x = barPos.x + t * barWidth;
            float y = barPos.y + barHeight * 0.5f;
            // 選択中: 黄色 / 通常: 水色
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

        // 現在時刻の縦線（赤）
        float cx = barPos.x + (animCurrentTime_ / timelineDuration_) * barWidth;
        draw->AddLine(
            ImVec2(cx, barPos.y),
            ImVec2(cx, barPos.y + barHeight),
            IM_COL32(255, 50, 50, 255), 2.0f);


        if (!track.keyframes.empty()) {
            int prevKf = -1, nextKf = -1;
            for (int i = 0; i < (int)track.keyframes.size(); i++) {
                if (track.keyframes[i].time <= animCurrentTime_) prevKf = i;
                if (track.keyframes[i].time > animCurrentTime_ && nextKf < 0) nextKf = i;
            }
            if (prevKf >= 0 && nextKf >= 0)
                ImGui::Text("補間中: KF[%d] → KF[%d]", prevKf, nextKf);
            else if (prevKf >= 0)
                ImGui::Text("最終キーフレーム: KF[%d]", prevKf);
            else
                ImGui::Text("キーフレーム前");
        }

        ImGui::Separator();

        // -----------------------------------------------
        // [4] キーフレーム一覧テーブル
        //     - 行をクリックで選択・モデルに反映
        //     - Xボタンで削除
        // -----------------------------------------------
        ImGui::Text("キーフレーム数: %zu", track.keyframes.size());
        int deleteIndex = -1; // EndTable後に削除するため記録

        if (ImGui::BeginTable("kf_table", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0, 120))) {

            ImGui::TableSetupColumn("フレーム");
            ImGui::TableSetupColumn("時間(秒)");
            ImGui::TableSetupColumn("位置");
            ImGui::TableSetupColumn("削除");
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)track.keyframes.size(); i++) {
                auto& kf = track.keyframes[i];
                ImGui::TableNextRow();

                // フレーム列（SpanAllColumnsなし → Delボタンが正常に反応する）
                ImGui::TableSetColumnIndex(0);
                char label[32];
                snprintf(label, sizeof(label), "%d##row%d", (int)(kf.time * fps_), i);
                if (ImGui::Selectable(label, selectedKeyframeIndex_ == i)) {
                    selectedKeyframeIndex_ = i;
                    animCurrentTime_ = kf.time;
                    currentFrame_ = (int)(kf.time * fps_);
                    // 選択したキーフレームのトランスフォームをモデルに即反映
                    target->SetTranslation(kf.position);
                    target->SetRotation(kf.rotation);
                    target->SetScale(kf.scale);
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f", kf.time);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.1f/%.1f/%.1f",
                    kf.position.x, kf.position.y, kf.position.z);

                ImGui::TableSetColumnIndex(3);
                char delLabel[16];
                snprintf(delLabel, sizeof(delLabel), "X##del%d", i);
                if (ImGui::Button(delLabel)) {
                    deleteIndex = i; // ここでは記録だけ（ループ中に削除しない）
                }
            }
            ImGui::EndTable();
        }

        // テーブル終了後に安全に削除
        if (deleteIndex >= 0) {
            track.keyframes.erase(track.keyframes.begin() + deleteIndex);
            track.duration = track.keyframes.empty()
                ? 0.0f : track.keyframes.back().time;
            if (selectedKeyframeIndex_ >= (int)track.keyframes.size())
                selectedKeyframeIndex_ = -1;
        }

        ImGui::Separator();

        // -----------------------------------------------
        // [5] 選択中キーフレームの数値直接編集
        //     - 編集した値はモデルに即反映される
        // -----------------------------------------------
        if (selectedKeyframeIndex_ >= 0 &&
            selectedKeyframeIndex_ < (int)track.keyframes.size()) {

            auto& kf = track.keyframes[selectedKeyframeIndex_];
            ImGui::Text("キーフレーム編集 [%d]", selectedKeyframeIndex_);

            // フレーム番号で時間を指定
            int kfFrame = (int)(kf.time * fps_);
            if (ImGui::InputInt("フレーム番号", &kfFrame)) {
                if (kfFrame<0){
                    kfFrame = 0;
                }
				// 入力されたフレーム番号から時間を逆算して更新
				kf.time = kfFrame / (float)fps_;

                std::sort(track.keyframes.begin(), track.keyframes.end(),
                    [](const TransformKeyframe& a, const TransformKeyframe& b) {
                        return a.time < b.time;
					});
                if (!track.keyframes.empty()){
                    track.duration = track.keyframes.back().time;
					selectedKeyframeIndex_ = -1; // フレーム番号変更で順序が変わる可能性があるため選択解除
                }

            }
            if (selectedKeyframeIndex_ >= 0) {
                if (ImGui::DragFloat3("位置##kf", &kf.position.x, 0.1f))  target->SetTranslation(kf.position);
                if (ImGui::DragFloat3("回転##kf", &kf.rotation.x, 0.05f)) target->SetRotation(kf.rotation);
                if (ImGui::DragFloat3("スケール##kf", &kf.scale.x, 0.05f)) target->SetScale(kf.scale);
            }
        }

        ImGui::Separator();

        // -----------------------------------------------
        // [6] マウスドラッグによる位置操作パッド
        //     左側: XZパッド（左右=X, 上下=Z）
        //     右側: Yスライダー（上下=Y）
        // -----------------------------------------------
        ImGui::Text("位置操作パッド");
        ImVec2 origin = ImGui::GetCursorScreenPos();
        float xzSize = 120.0f;
        float yWidth = 30.0f;
        float padHeight = 120.0f;

        // --- XZパッド ---
        draw->AddRectFilled(origin,
            ImVec2(origin.x + xzSize, origin.y + padHeight),
            IM_COL32(30, 30, 70, 255));
        draw->AddRect(origin,
            ImVec2(origin.x + xzSize, origin.y + padHeight),
            IM_COL32(80, 80, 200, 255));
        // 十字ガイド線
        draw->AddLine(
            ImVec2(origin.x + xzSize * 0.5f, origin.y),
            ImVec2(origin.x + xzSize * 0.5f, origin.y + padHeight),
            IM_COL32(60, 60, 100, 255));
        draw->AddLine(
            ImVec2(origin.x, origin.y + padHeight * 0.5f),
            ImVec2(origin.x + xzSize, origin.y + padHeight * 0.5f),
            IM_COL32(60, 60, 100, 255));

        // 現在位置を黄色ドットで表示
        pos = target->GetTranslation();
        float dotX = origin.x + xzSize * 0.5f + pos.x * 5.0f;
        float dotZ = origin.y + padHeight * 0.5f - pos.z * 5.0f;
        dotX = (dotX < origin.x) ? origin.x : (dotX > origin.x + xzSize) ? origin.x + xzSize : dotX;
        dotZ = (dotZ < origin.y) ? origin.y : (dotZ > origin.y + padHeight) ? origin.y + padHeight : dotZ;
        draw->AddCircleFilled(ImVec2(dotX, dotZ), 5.0f, IM_COL32(255, 200, 0, 255));

        ImGui::SetCursorScreenPos(origin);
        ImGui::InvisibleButton("xzpad", ImVec2(xzSize, padHeight));
        if (ImGui::IsItemActive()) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            pos.x += d.x * 0.03f;
            pos.z -= d.y * 0.03f;
            target->SetTranslation(pos);
        }

        // --- Yスライダー（XZパッドの右隣）---
        ImVec2 yPos = ImVec2(origin.x + xzSize + 4.0f, origin.y);
        draw->AddRectFilled(yPos,
            ImVec2(yPos.x + yWidth, yPos.y + padHeight),
            IM_COL32(30, 70, 30, 255));
        draw->AddRect(yPos,
            ImVec2(yPos.x + yWidth, yPos.y + padHeight),
            IM_COL32(80, 200, 80, 255));

        // 現在のY位置を黄色ドットで表示
        pos = target->GetTranslation();
        float dotY = yPos.y + padHeight * 0.5f - pos.y * 5.0f;
        dotY = (dotY < yPos.y) ? yPos.y : (dotY > yPos.y + padHeight) ? yPos.y + padHeight : dotY;
        draw->AddCircleFilled(ImVec2(yPos.x + yWidth * 0.5f, dotY), 5.0f, IM_COL32(255, 200, 0, 255));

        ImGui::SetCursorScreenPos(yPos);
        ImGui::InvisibleButton("ypad", ImVec2(yWidth, padHeight));
        if (ImGui::IsItemActive()) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            pos.y -= d.y * 0.03f;
            target->SetTranslation(pos);
        }

        // パッド分の高さを確保してカーソルを下に移動
        ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + padHeight + 4.0f));

        ImGui::Separator();

        // -----------------------------------------------
        // [7] JSONファイルへの保存 / 読み込み
        //     ファイル名: resources/{オブジェクト名}_anim.json
        // -----------------------------------------------

		// 最初の表示時にファイルパスをセット（以降は変更しない）
        if (saveFilePath_.empty()){
			saveFilePath_ = "resources/" + target->GetName() + "_anim.json";

        }
		// ファイルパス表示（編集不可）
		char pathBuffer[256];
		// 安全にコピー（_TRUNCATEでバッファサイズを超える場合は切り捨て）
        strncpy_s(pathBuffer, sizeof(pathBuffer), saveFilePath_.c_str(), _TRUNCATE);
        if (ImGui::InputText("ファイルパス", pathBuffer, sizeof(pathBuffer))) {
			saveFilePath_ = pathBuffer; // ユーザーがファイルパスを編集した場合は更新
        }


        if (ImGui::Button("保存")) {
            track.SaveToJson(saveFilePath_);
        }
        ImGui::SameLine();
        if (ImGui::Button("全オブジェクト保存")) {
            for (auto* t : targets_) {
                animTracks_[t->GetName()].SaveToJson(
                    "resources/" + t->GetName() + "_anim.json");
            }
        }
        if (ImGui::Button("読み込み")) {
            track.LoadFromJson(saveFilePath_);
            animCurrentTime_ = 0.0f;
            currentFrame_ = 0;
            isAnimPlaying_ = false;
            selectedKeyframeIndex_ = -1;
            float minDuration = track.duration + 1.0f;
            timelineDuration_ = (5.0f > minDuration) ? 5.0f : minDuration;
        }
		ImGui::SameLine();
        static bool confirmClear = false;
        if (ImGui::Button("クリア")) { confirmClear = true; }
        if (confirmClear) {
            ImGui::OpenPopup("クリア確認");
            confirmClear = false;
        }
        if (ImGui::BeginPopupModal("クリア確認", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("全キーフレームを削除しますか？");
            if (ImGui::Button("はい")) {
                track.keyframes.clear();
                track.duration = 0.0f;
                animCurrentTime_ = 0.0f;
                currentFrame_ = 0;
                isAnimPlaying_ = false;
                selectedKeyframeIndex_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("キャンセル")) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        if (ImGui::Button("x2速")) {
            for (auto& kf : track.keyframes) kf.time *= 0.5f;
            track.duration *= 0.5f;
            animCurrentTime_ = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("x0.5速")) {
            for (auto& kf : track.keyframes) kf.time *= 2.0f;
            track.duration *= 2.0f;
            float minDuration = track.duration + 1.0f;
            if (timelineDuration_ < minDuration) timelineDuration_ = minDuration;
            animCurrentTime_ = 0.0f;
        }
        ImGui::PopItemWidth();
    }

    ImGui::End();
#endif
}
