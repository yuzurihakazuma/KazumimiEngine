#pragma once
#include "game/enemy/Enemy.h" // EnemySpawnData
#include <vector>

class SplineRail;
class EnemySpawnModel;

// =====================================================================
//  EnemyEditorAddPanel : 敵エディタ上段の「新しく置く」欄
//   ひな形（種類・レール・距離・巡回）を決めて、1体追加／選択ノードの位置に追加／
//   選んだレールに等間隔で並べる。置いた敵は選択状態になり、そのまま下の設定欄で動きを決められる
// =====================================================================
class EnemyEditorAddPanel {
public:
    // ひな形を初期状態に戻す（エディタの初期化時）
    void Reset();

    // 「新しく置く」の見出しと中身を描く。
    //   pickRail/pickDist: レールエディタで選択中ノードのレール番号と距離（hasPick=false なら未選択）
    void Draw(EnemySpawnModel& model, const std::vector<SplineRail>& splineRails,
              int pickRail, float pickDist, bool hasPick);

private:
    // 新しく置く敵のひな形（種類・動きの設定をそのまま引き継いで置ける）
    EnemySpawnData newTemplate_;
    int multiCount_ = 3; // 「等間隔で並べる」の体数
};
