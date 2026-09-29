#pragma once
// =====================================================================
//  CoinDragTool：配置済みコインのつかみ移動（エディット中・ブロック配置モードOFF時）。
//   コインを左ドラッグでレール上を移動。Shiftを押しながら上下ドラッグで高さ調整。
//   離した時に確定して CoinSystem を作り直す（ドラッグ中はゴースト表示のみ＝軽い）
// =====================================================================
#include "engine/utils/Level/LevelData.h" // CoinData

struct SceneEditContext;

class CoinDragTool {
public:
    // blockedByOthers=true の間は新しくつかまない（敵をつかんでいる最中など）
    void Update(const SceneEditContext& context, bool blockedByOthers);
    void Cancel() { dragIndex_ = -1; hoverIndex_ = -1; }

    bool IsDragging() const { return dragIndex_ >= 0; }
    bool IsHovering() const { return hoverIndex_ >= 0; }

private:
    void UpdateDrag(const SceneEditContext& context);
    void Commit(const SceneEditContext& context);

    int      hoverIndex_ = -1; // 今フレームにマウス直下だったコイン（ブロックつかみとの優先制御用）
    int      dragIndex_ = -1;  // ドラッグ中のコイン（-1=なし）
    CoinData dragData_ {};     // ドラッグ中の移動先候補（rail/dist/height）
    CoinData dragOrig_ {};     // つかんだ瞬間のコイン（クリックだけ/Undo割込み時に書き込まない照合用）
};
