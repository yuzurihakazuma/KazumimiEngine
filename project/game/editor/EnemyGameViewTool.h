#pragma once
// =====================================================================
//  EnemyGameViewTool：Game View 上での敵の見える化と直接ドラッグ（エディット中）。
//   ・種類別の色分けピン＋名札「#番号 名前」
//   ・追いかける距離の輪（薄い赤）と行動範囲（橙線）
//   ・敵をつかんでレール上を移動（別レールへの乗せ替えも可）→ 離して確定
//   ・敵エディタの一覧でホバー/選択中の敵のハイライト
//  敵パネルの表示に関係なく有効（アイコンモードでパネルを閉じていても、
//  ゲームビューの敵をそのままつかんで配置変更できる）
// =====================================================================
struct SceneEditContext;

class EnemyGameViewTool {
public:
    // エディット中に毎フレーム呼ぶ（Edit以外では Cancel を呼ぶ）
    void Update(const SceneEditContext& context);
    void Cancel();

    bool IsDragging() const { return dragIndex_ >= 0; }
    bool IsHovering() const { return hoverIndex_ >= 0; }

private:
    void DrawPinsAndLabels(const SceneEditContext& context);
    void UpdateDrag(const SceneEditContext& context);

    int  dragIndex_ = -1;   // ドラッグ中の敵（-1=なし）
    bool dragMoved_ = false; // つかんでから実際に動かしたか（クリックだけなら確定しない）
    int  hoverIndex_ = -1;  // マウス直下の敵（つかめる候補）
};
