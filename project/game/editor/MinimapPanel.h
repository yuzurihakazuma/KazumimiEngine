#pragma once
// =====================================================================
//  MinimapPanel：ミニマップ（俯瞰ビュー）パネル。コース全体と配置物を上から一望する。
//   ・線に触れる＝そのレールを選択 / 選択レールの点をドラッグ＝XZ移動（高さは保持）
//   ・空クリック＝そこが画面の中央に来るようにカメラを移動 / ホイール＝ズーム / 中ボタン＝パン
//  レール（選択=黄 / ガイド骨組み=橙 / 動く=水色 / 通常=白）、ブロック・コイン・敵・
//  スタート・ゴール・プレイヤー・カメラを描く
// =====================================================================
#include "externals/imgui/imgui.h"

struct SceneEditContext;
class RailEditor;

class MinimapPanel {
public:
    // パネルが表示中の時だけ呼ぶ
    void Draw(const SceneEditContext& context);

private:
    // world XZ ⇔ キャンバス座標の対応（中心基準＋ズーム＋パン。奥(+Z)=上、手前=下）
    struct MapView {
        ImVec2 canvasMin {}, canvasSize {};
        float  scale = 1.0f;
        float  fitScale = 1.0f;
        float  worldCx = 0.0f, worldCz = 0.0f;
        float  centerPxX = 0.0f, centerPxY = 0.0f;
        ImVec2 ToCanvas(float wx, float wz) const {
            return { centerPxX + ( wx - worldCx ) * scale, centerPxY + ( worldCz - wz ) * scale };
        }
        float ToWorldX(float px) const { return worldCx + ( px - centerPxX ) / scale; }
        float ToWorldZ(float py) const { return worldCz - ( py - centerPxY ) / scale; }
    };

    void UpdateBounds(const SceneEditContext& context);
    void DrawCanvas(const SceneEditContext& context);
    // レール線を描き、マウスが触れているレール番号を返す（-1=なし）
    int  DrawRails(const SceneEditContext& context, const MapView& map, ImDrawList* draw,
                   bool canPick, int currentRail);
    void DrawObjects(const SceneEditContext& context, const MapView& map, ImDrawList* draw);
    // 選択レールのノードを描き、ドラッグで動かす。マウスが触れているノード番号を返す
    int  UpdateNodeHandles(const SceneEditContext& context, const MapView& map, ImDrawList* draw,
                           bool hovered, RailEditor* railEditor, int currentRail);
    void UpdateZoomPan(const MapView& map, bool hovered);

    float zoom_ = 1.0f;
    float panX_ = 0.0f, panY_ = 0.0f;
    int   dragRail_ = -1, dragNode_ = -1;
    bool  hasBounds_ = false;
    float minX_ = 0.0f, maxX_ = 0.0f, minZ_ = 0.0f, maxZ_ = 0.0f;
};
