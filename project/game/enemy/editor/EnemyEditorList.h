#pragma once
#include "game/enemy/Enemy.h" // EnemySpawnData
#include <vector>

class SplineRail;
class EnemySpawnModel;

// =====================================================================
//  EnemyEditorList : 敵エディタ中段の「配置済みの敵」一覧
//   絞り込み（種類・レール・名前）とレール別まとめ表示を持つ、高さ固定のスクロール一覧。
//   敵が何十体に増えても一覧の高さは変わらず、下の設定欄が流れていかない。
//   クリック=選択 / ダブルクリック=カメラ要求 / ホバー=Game View のハイライト
// =====================================================================
class EnemyEditorList {
public:
    void Draw(EnemySpawnModel& model, const std::vector<SplineRail>& splineRails);

    // Game View ハイライト用：一覧でホバー中のエントリ番号（-1=なし）
    int HoveredEntry() const { return hoveredEntry_; }

    // レール番号が付け替わった時に、絞り込み中のレール番号も追いかける
    void RemapRailFilter(const std::vector<int>& oldToNew);

private:
    void DrawFilter(const std::vector<EnemySpawnData>& spawns, const std::vector<SplineRail>& splineRails);
    void DrawRow(EnemySpawnModel& model, int index); // 一覧の1行
    bool PassesFilter(const EnemySpawnData& spawn) const;

    // 一覧でホバー中のエントリ（Game View ハイライト用。毎フレーム取り直す）
    int hoveredEntry_ = -1;

    // 一覧の絞り込み
    int  filterType_ = -1;      // -1=全種類
    int  filterRail_ = -1;      // -1=全レール
    char filterText_[64] = {};  // 名前の部分一致
    bool groupByRail_ = true;   // レール別にまとめて表示
};
