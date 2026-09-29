#pragma once
// =====================================================================
//  StripCommon：配置ビュー（レール展開図）の部品で共通に使う定数と小さな関数
//   寸法・段数の上限・ブロックの種類名・色など、描画と操作の両方が同じ値を見る必要があるもの。
//   名前が他のファイルの関数とぶつからないよう namespace railstrip に入れてある
// =====================================================================
#include "engine/rail/SplineRail.h"
#include "externals/imgui/imgui.h"
#include "game/enemy/Enemy.h"

#include <algorithm>
#include <cmath>

namespace railstrip {
    inline constexpr int   kMaxLevels      = 8;     // 置ける段数（0〜7段）
    inline constexpr int   kBlockTypeCount = 9;
    inline constexpr float kCoinMaxHeight  = 5.0f;  // コインの高さの上限(m)
    inline constexpr float kHoverMaxHeight = 8.0f;  // 敵の浮く高さの上限(m)
    inline constexpr float kPadLeft        = 40.0f;
    inline constexpr float kPadRight       = 40.0f;
    inline constexpr float kRulerHeight    = 20.0f;
    inline constexpr float kGroundHeight   = 12.0f;
    inline constexpr float kInfoHeight     = 46.0f;
    inline constexpr float kMinCellPx      = 12.0f; // 拡大率の範囲（1m のピクセル数）
    inline constexpr float kMaxCellPx      = 56.0f;

    inline constexpr const char* kBlockTypeNames[kBlockTypeCount] = {
        "スポンジ", "段ボール", "斜面45°", "斜面26°", "バネ", "？", "すり抜け床", "横長2m", "台座2×2" };
    // 敵の道具で選べる種類（ボタンの並び・数字キー 1〜3 の順）
    inline constexpr EnemyType kEnemyTypes[3] = { EnemyType::Zako, EnemyType::Strong, EnemyType::Air };

    // 見えないレールに新しく置こうとした時の説明（ブロック・敵・コインで同じ文）
    inline constexpr const char* kInvisibleRailMessage =
        "見えないレールには新しく置けません（今ある物の移動・削除はできます）";

    inline bool RailUsable(const SplineRail& rail){
        return rail.nodes.size() >= 2 && rail.GetLength() > 0.01f;
    }
    // 表示と当たりに使う距離。レールが短くなって範囲の外に残った物は、端に寄せて見せる
    inline float ShownDist(float dist, float railLength){ return std::clamp(dist, 0.0f, railLength); }

#ifdef USE_IMGUI
    inline float SnapTo(float value, float step){ return std::round(value / step) * step; }

    // 敵の体の中心の高さ（レール面から。浮いている敵は浮いた分も含む）
    inline float EnemyCenterHeight(const EnemySpawnData& spawn){ return Enemy::PickHeightOf(spawn); }
    // 足元から体の中心までの高さ
    inline float EnemyBodyMid(const EnemySpawnData& spawn){ return Enemy::PickHeightOf(spawn) - Enemy::HoverOf(spawn); }
    // 動ける範囲を決めてある敵か
    inline bool EnemyHasRange(const EnemySpawnData& spawn){
        return ( spawn.patrol || spawn.chaseRange > 0.0f )
            && ( spawn.patrolMin >= 0.0f || spawn.patrolMax >= 0.0f );
    }
    // 展開図に描く敵の体の半径（ピクセル）。描画と当たりで同じ大きさを使う
    inline float EnemyRadiusPx(const EnemySpawnData& spawn, float cellPx){
        return ( std::max )( 7.0f, Enemy::TypeSpecOf(spawn.type).bodyRadius * spawn.scale * cellPx * 1.15f );
    }
    // 展開図に描くコインの半径（ピクセル）
    inline float CoinRadiusPx(float cellPx){ return ( std::max )( 4.0f, 0.2f * cellPx ); }

    // ブロックの色（モデルの見た目に近い色）
    inline ImU32 BlockColor(int type, int alpha){
        switch ( type ) {
        case 0:  return IM_COL32(238, 200, 72, alpha);  // スポンジ（黄）
        case 1:  return IM_COL32(186, 140, 92, alpha);  // 段ボール層
        case 2:  return IM_COL32(200, 156, 104, alpha); // 斜面45°
        case 3:  return IM_COL32(200, 156, 104, alpha); // ゆるい斜面
        case 4:  return IM_COL32(96, 200, 110, alpha);  // ジャンプ台（緑）
        case 5:  return IM_COL32(250, 186, 40, alpha);  // ？ブロック（金）
        case 6:  return IM_COL32(236, 240, 246, alpha); // すり抜け床（白）
        case 7:  return IM_COL32(238, 200, 72, alpha);  // 横長
        default: return IM_COL32(170, 124, 80, alpha);  // 台座
        }
    }
    // 敵の色（Game View のピン・敵エディタの一覧と同じ色）
    inline ImU32 EnemyColor(EnemyType type, int alpha){
        switch ( type ) {
        case EnemyType::Zako:   return IM_COL32(255, 90, 64, alpha);   // 赤
        case EnemyType::Strong: return IM_COL32(190, 102, 255, alpha); // 紫
        default:                return IM_COL32(90, 204, 255, alpha);  // 水色
        }
    }

    // 文字を中央ぞろえで描く
    inline void AddTextCentered(ImDrawList* draw, const ImVec2& center, ImU32 color, const char* text){
        ImVec2 size = ImGui::CalcTextSize(text);
        draw->AddText({ center.x - size.x * 0.5f, center.y - size.y * 0.5f }, color, text);
    }
#endif
}
