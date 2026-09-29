#pragma once
#include "game/enemy/Enemy.h" // EnemySpawnData
#include <vector>

class SplineRail;
class EnemySpawnModel;

// =====================================================================
//  EnemyEditorInspector : 敵エディタ下段の「選択中の1体の設定」欄
//   基本（名前・種類）／場所／動き／高さ・大きさ／種類ごとの設定／動きの一括反映／複製・削除。
//   何も選ばれていない時は案内と「敵を全削除」（2段階確認）を出す
// =====================================================================
class EnemyEditorInspector {
public:
    //   pickRail/pickDist: レールエディタで選択中ノードのレール番号と距離（hasPick=false なら未選択）
    void Draw(EnemySpawnModel& model, const std::vector<SplineRail>& splineRails,
              int pickRail, float pickDist, bool hasPick);

private:
    // 複製・削除のボタン行で押されたもの
    enum class EntryAction { None, Duplicate, Delete };

    void DrawNothingSelected(EnemySpawnModel& model);
    void DrawSelected(EnemySpawnModel& model, const std::vector<SplineRail>& splineRails,
                      int pickRail, float pickDist, bool hasPick);

    // --- 選択中の1体の各欄（どれも変更があれば model に変更通知を立てる）---
    void DrawBasicSection(EnemySpawnModel& model, EnemySpawnData& spawn);
    void DrawPlaceSection(EnemySpawnModel& model, int index, const std::vector<SplineRail>& splineRails,
                          float railLength, bool validRail, int pickRail, float pickDist, bool hasPick);
    void DrawMotionSection(EnemySpawnModel& model, EnemySpawnData& spawn, float railLength);
    void DrawBodySection(EnemySpawnModel& model, EnemySpawnData& spawn);
    void DrawTypeSection(EnemySpawnModel& model, EnemySpawnData& spawn);
    void DrawCopyMotionSection(EnemySpawnModel& model, int index);
    EntryAction DrawEntryButtons();

    // 全削除の2段階確認（1回目のクリック時刻）
    double clearArmedTime_ = -100.0;
};
