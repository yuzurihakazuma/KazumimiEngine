#pragma once
// =====================================================================
//  GuideHandleTool：選択中の足場（ガイド追従で動くリフト）のガイドレールを、
//   Game View の上で直接つかんで編集するハンドル（エディット中）。
//   ・点をドラッグ＝カメラに平行な面で移動（Shift=縦だけ / Ctrl=横だけ）
//   ・Ctrl+クリック＝線の上に点を追加 / 右クリック＝点を削除（最低2点は残す）
//   ハンドル圏内のクリックはレール選択に化けないので、モード切替や固定は不要。
//   他の足場のガイドは RailEditOverlay が細線で場所だけ示す
// =====================================================================
#include "engine/math/struct.h"

struct SceneEditContext;
class RailEditor;

class GuideHandleTool {
public:
    // エディット中に毎フレーム呼ぶ
    void Update(const SceneEditContext& context);

    bool IsHovering() const { return hover_; }
    bool IsDragging() const { return dragNode_ >= 0; }

private:
    // 選択中の足場のガイド番号（足場を選んでいない・ガイドが無い時は -1）
    static int ActiveGuideOf(const SceneEditContext& context);
    // ドラッグ中のノードを、マウスの動きに合わせてカメラに平行な面で動かす
    void DragNode(const SceneEditContext& context, int guide);

    int  dragRail_ = -1;  // ドラッグ中のガイドレール番号（-1=なし）
    int  dragNode_ = -1;  // ドラッグ中のノード番号
    bool hover_ = false;  // 今フレームにハンドル圏内だったか（右クリックメニュー抑制用）
};
