#pragma once
// =====================================================================
//  TitleLayout：タイトルのジオラマの寸法。
//   CraftKit は小さなジオラマ用の寸法（恐竜の背丈 0.8m 想定）で作られているので、
//   全体を kScale 倍して本編と同じ大きさ（恐竜 1.5m・道幅 2m）に合わせている。
//   ここの値は「キットの寸法 × kScale」。マップ（tools/craftkit/make_title_map.py の SCALE）と同じ倍率にすること
// =====================================================================
#include "engine/math/struct.h"

namespace TitleLayout {
    inline constexpr float kScale = 1.8f;
    inline constexpr float kGroundTop = 0.14f * kScale; // 地面タイルの上面の高さ

    // カメラ（CraftKit の CAM_Title と同じ構図：レンズ30mm＝縦の画角 約37度）
    inline constexpr Vector3 kCameraPosition { 0.0f, 2.3f * kScale, -7.8f * kScale };
    inline constexpr Vector3 kCameraTarget   { 0.0f, 1.0f * kScale, 1.6f * kScale };
    inline constexpr float   kCameraFovY = 0.6505f;

    // ロゴ看板が刺さる位置（杭の先はここから地面に刺さる）
    inline constexpr Vector3 kLogoLand { 0.0f, kGroundTop, 2.2f * kScale };

    // メニューの的（左から つづき／スタート／せってい）
    inline constexpr float kMenuSpacing = 1.9f * kScale;
    inline constexpr float kMenuZ       = -1.4f * kScale;
    inline constexpr float kMenuFaceCenterHeight = 0.786f * kScale; // 的の面の中心の高さ（地面から）

    // レールの四角（マップの railLines と同じ位置）。登場演出の自動操縦が目指す角
    inline constexpr float kRailX     = 3.0f * kScale;
    inline constexpr float kRailFront = -2.45f * kScale;
    inline constexpr float kRailBack  = 0.9f * kScale;
}
