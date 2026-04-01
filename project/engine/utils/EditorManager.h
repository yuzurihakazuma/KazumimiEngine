#pragma once


class EditorManager{
public:
    // ImGuiのフレーム開始（Game::Update の先頭で呼ぶ）
    void Begin();

    // エディタUIの更新・描画
    // F1キーのトグル、ドッキングスペース、メニューバー、
    void Update();

    // ImGuiの描画コマンドを発行（Game::Draw の末尾で呼ぶ）
    void End();

    // パフォーマンス計測値をSceneManagerから受け取るためのセッター
    void SetCpuTimes(float updateMs, float drawMs){
        cpuUpdateTimeMs_ = updateMs;
        cpuDrawTimeMs_ = drawMs;
    }

    // エディタがアクティブかどうか
    bool IsActive() const{ return isEditorActive_; }

private:
    bool isEditorActive_ = false;

    // パフォーマンスモニター用（SceneManagerから渡してもらう）
    float cpuUpdateTimeMs_ = 0.0f;
    float cpuDrawTimeMs_ = 0.0f;
};
