#pragma once
// =====================================================================
//  StripContext：配置ビュー（レール展開図）が毎フレーム、シーンから受け取るもの
//   （RailStripPanel::Context の実体。どれも所有しない）
//
//   展開図の部品（操作部・キャンバス・各道具）が同じものを見るので、
//   RailStripPanel の中ではなく独立した構造体にしてある（部品側から前方宣言できないため）
// =====================================================================
#include "engine/math/struct.h"
#include <functional>
#include <vector>

class SplineRail;
class RailEditor;
class LevelEditor;
class EnemyEditor;
class BlockSystem;

struct StripContext {
    const std::vector<SplineRail>* rails = nullptr; // 実行時レール（RailField）
    RailEditor*  railEditor  = nullptr;             // ブロック・コインの編集先
    LevelEditor* levelEditor = nullptr;             // コイン編集の未保存マーク用
    EnemyEditor* enemyEditor = nullptr;             // 敵の編集先
    const BlockSystem* blockSystem = nullptr;       // Game View への枠表示・斜面の向き
    bool  editable = true;                          // false=表示だけ（プレイ中）
    int   startRail = -1;  float startDist = 0.0f;  // スタート地点
    int   goalRail  = -1;  float goalDist  = 0.0f;  // ゴール地点（-1=未設定）
    bool  hasPlayer = false;                        // プレイ中のプレイヤー位置を出すか
    int   playerRail = -1;
    Vector3 playerPos { 0.0f, 0.0f, 0.0f };
    std::function<void()> onCoinsChanged;           // コインを変えた後に呼ぶ（CoinSystem の作り直し）
};
