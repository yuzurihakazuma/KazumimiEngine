#include "GPUParticleEditor.h"
#include "GPUParticleEmitter.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif


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
    ImGui::SliderFloat("ばらつき (spread)", &d.velocitySpeed, 0.0f, 2.0f);

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

    ImGui::End();

#endif
}

