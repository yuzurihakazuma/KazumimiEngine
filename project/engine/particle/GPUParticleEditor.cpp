#include "GPUParticleEditor.h"
#include "engine/particle/GPUParticleEmitter.h"  
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
#include "engine/particle/GPUParticleManager.h"

// 編集対象のエミッターをセット（GamePlaySceneから渡してもらう）
void GPUParticleEditor::SetEmitter(GPUParticleEmitter* emitter){
	emitter_ = emitter;
}

// ImGuiの「Save」ボタンから呼ばれる
void GPUParticleEditor::Save(){ 
	if ( !emitter_ ){
		return;
	}
	// エミッターのデータをファイルに保存
	emitter_->SaveToFile(saveFileName_);

}



// ImGuiの「Load」ボタンから呼ばれる
void GPUParticleEditor::Load(){ 

	if ( !emitter_ ){
		return;
	}
	// ファイルからエミッターデータを読み込む
	emitter_->LoadFromFile(saveFileName_);

}


void GPUParticleEditor::DrawDebugUI(){
#ifdef USE_IMGUI

    ImGui::Begin("GPU Particle Editor");

    // エミッターが未設定なら何も表示しない
    if ( !emitter_ ) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "エミッターが未設定です");
        ImGui::End();
        return;
    }

    // エミッターのデータを直接参照して編集する
    GPUParticleEmitterData& d = emitter_->GetData();

    // =========================================================
    // ファイル操作
    // =========================================================
    ImGui::InputText("ファイル名", saveFileName_, sizeof(saveFileName_));
    if ( ImGui::Button("保存") ) { Save(); }
    ImGui::SameLine();
    if ( ImGui::Button("読み込み") ) { Load(); }

    ImGui::Separator();

    // =========================================================
    // 基本設定
    // =========================================================
    ImGui::Text("[ 基本設定 ]");

    // エミッター名の編集
    char nameBuf[64];
    strcpy_s(nameBuf, d.name.c_str());
    if ( ImGui::InputText("名前", nameBuf, sizeof(nameBuf)) ) {
        d.name = nameBuf;
    }

    // ON/OFF と ループ切り替え
    ImGui::Checkbox("アクティブ", &d.active);
    ImGui::SameLine();
    ImGui::Checkbox("ループ", &d.loop);

    // 発生レート（スライダーで0〜100個/秒）
    ImGui::SliderFloat("発生レート (個/秒)", &d.emitRate, 0.1f, 100.0f);

    // 発生位置
    ImGui::DragFloat3("発生位置", &d.position.x, 0.1f);

    ImGui::Separator();

    // =========================================================
    // 速度
    // =========================================================
    ImGui::Text("[ 速度 ]");
    ImGui::DragFloat3("初速 (velocity)", &d.velocity.x, 0.01f);

    // ばらつきのスライダー（0なら完全固定、大きいほどランダム）
    ImGui::SliderFloat("ばらつき (spread)", &d.velocitySpread, 0.0f, 2.0f);

    ImGui::Separator();

    // =========================================================
    // 寿命
    // =========================================================
    ImGui::Text("[ 寿命 ]");

    // min と max が逆転しないようにクランプする
    ImGui::DragFloat("寿命 最小 (秒)", &d.lifeTimeMin, 0.1f, 0.1f, d.lifeTimeMax);
    ImGui::DragFloat("寿命 最大 (秒)", &d.lifeTimeMax, 0.1f, d.lifeTimeMin, 30.0f);

    ImGui::Separator();

    // =========================================================
    // スケール
    // =========================================================
    ImGui::Text("[ サイズ ]");
    ImGui::DragFloat("サイズ 最小", &d.scaleMin, 0.01f, 0.01f, d.scaleMax);
    ImGui::DragFloat("サイズ 最大", &d.scaleMax, 0.01f, d.scaleMin, 10.0f);

    ImGui::Separator();

    // =========================================================
    // 色（ColorEdit4 はカラーピッカー付き）
    // =========================================================
    ImGui::Text("[ 色 ]");
    ImGui::ColorEdit4("開始色", &d.startColor.x);
    ImGui::ColorEdit4("終了色", &d.endColor.x);

    ImGui::Separator();

    // =========================================================
    // 物理
    // =========================================================
    ImGui::Text("[ 物理 ]");
    ImGui::DragFloat("重力 Y", &d.gravityY, 0.01f, -5.0f, 5.0f);

    ImGui::Separator();

    // =========================================================
// バースト
// =========================================================
    ImGui::Text("[ バースト ]");

    // 発生数（1〜1000個）
    ImGui::SliderInt("バースト数", &d.burstCount, 1, 1000);

    // ボタンを押した瞬間だけ一気に発生
    if ( ImGui::Button("バースト発生！") ) {
        emitter_->Burst();
    }

    ImGui::Separator();



    // =========================================================
    // パーティクル情報（読み取り専用）
    // =========================================================
    ImGui::Text("[ パーティクル情報 ]");

    uint32_t emitCount = GPUParticleManager::GetInstance()->GetLastFrameEmitCount();
    uint32_t maxCount = GPUParticleManager::GetInstance()->GetMaxParticles();
    uint32_t total = GPUParticleManager::GetInstance()->GetTotalEmitted();

    // ---- 今フレームの発生数 ----
    ImGui::Text("今フレーム発生数 : %u 個", emitCount);

    // ---- 生存数の推定（発生レート × 平均寿命） ----
    float avgLifeTime = ( d.lifeTimeMin + d.lifeTimeMax ) * 0.5f;
    uint32_t estimatedAlive = static_cast< uint32_t >( d.emitRate * avgLifeTime );
    if ( estimatedAlive > maxCount ) { estimatedAlive = maxCount; }

    float aliveRate = static_cast< float >( estimatedAlive ) / static_cast< float >( maxCount );
    if ( aliveRate > 0.8f ) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
            "推定生存数 (概算) : %u 個  ！バッファ残りわずか！", estimatedAlive);
    } else if ( aliveRate > 0.5f ) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
            "推定生存数 (概算) : %u 個  やや多め", estimatedAlive);
    } else {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
            "推定生存数 (概算) : %u 個  余裕あり", estimatedAlive);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%.1f個/秒 × 平均%.1f秒)", d.emitRate, avgLifeTime);

    // ---- バッファ使用状況 ----
    uint32_t filled = ( total < maxCount ) ? total : maxCount;
    float    fillRate = static_cast< float >(filled) / static_cast< float >(maxCount);

    ImGui::Spacing();
    ImGui::Text("バッファ使用状況 :");

    ImVec4 barColor;
    if ( fillRate > 0.8f ) { barColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); } else if ( fillRate > 0.5f ) { barColor = ImVec4(1.0f, 0.7f, 0.0f, 1.0f); } else                         { barColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); }

    char barLabel[64];
    snprintf(barLabel, sizeof(barLabel), "%u / %u スロット", filled, maxCount);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
    ImGui::ProgressBar(fillRate, ImVec2(-1.0f, 0.0f), barLabel);
    ImGui::PopStyleColor();

    ImGui::End();

#endif
}

