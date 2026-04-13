#pragma once
#include <string>

class GPUParticleEmitter;

// GPUパーティクルエディタクラス
class GPUParticleEditor{
public:

    // 編集対象のエミッターをセット（GamePlaySceneから渡してもらう）
    void SetEmitter(GPUParticleEmitter* emitter);

   
    // メニューバーから呼ぶ保存/ロード
    void Save();
    void Load();

    // ImGui UIの描画（EditorManager::Update() から呼ぶ）
    void DrawDebugUI();


private:
    // 編集対象（所有はしない。シーン側が持つ）
    GPUParticleEmitter* emitter_ = nullptr;

    // 保存先ファイルパス
    char saveFileName_[256] = "resources/particles/emitter01.json";
};


