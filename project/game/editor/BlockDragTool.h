#pragma once
// =====================================================================
//  BlockDragTool：配置済みブロックのつかみ移動（エディット中・ブロック配置モードOFF時）。
//   ゲームビューのブロックを左ドラッグでつかんでレール上を移動できる。
//   マウスの上下で段、左右で横ずれも変わる。離した時に確定（塞がっていたら元へ戻る）。
//   Ctrl を押しながらだと複製（元を残して移動先へコピー）
// =====================================================================
#include "engine/utils/Level/LevelData.h" // BlockData

struct SceneEditContext;

class BlockDragTool {
public:
    // blockedByOthers=true の間は新しくつかまない（敵・コインをつかんでいる/指している最中など）
    void Update(const SceneEditContext& context, bool blockedByOthers);
    void Cancel() { dragIndex_ = -1; dragDuplicate_ = false; }

    bool IsDragging() const { return dragIndex_ >= 0; }

private:
    void UpdateDrag(const SceneEditContext& context);
    void Commit(const SceneEditContext& context);
    // セル（レール×距離×段×横）の見た目の中心位置
    static bool CellCenter(const SceneEditContext& context, int rail, float dist, int level, float side, Vector3& out);

    int       dragIndex_ = -1;       // つかんでいるブロック（-1=なし）
    BlockData dragOrig_ {};          // つかんだ時のブロック（キャンセル復帰用）
    BlockData dragCell_ {};          // ドラッグ中の移動先セル候補
    bool      dragDuplicate_ = false; // Ctrl押下＝複製モード
};
