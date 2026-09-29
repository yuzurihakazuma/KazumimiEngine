#pragma once
// =====================================================================
//  RailEditOverlay：エディット中に Game View へ重ねて描く、レールの動きの見える化（描くだけ）。
//   ・動くレールの移動範囲（往復＝両端のゴースト線＋スイープ線 / 円運動＝軌道リング /
//     ガイド追従＝実際に通る経路）。「どこまで動くか」が配置の時点で見える
//   ・見えないガイドレール（骨組み）の場所（細いオレンジ線）
//   ・後から出現する道（紫の線）と、発動レールからのリンク線
//  つかんで編集できるハンドルは GuideHandleTool が描く
// =====================================================================
#include <vector>

class SplineRail;

namespace RailEditOverlay {
    void DrawMotionRanges(const std::vector<SplineRail>& rails);
    // skipGuide：このガイドは描かない（選択中の足場のガイドは GuideHandleTool がハンドル付きで描く）
    void DrawHiddenGuides(const std::vector<SplineRail>& rails, int skipGuide);
    void DrawAppearRoads(const std::vector<SplineRail>& rails);
}
