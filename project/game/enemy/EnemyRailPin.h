#pragma once
// =====================================================================
//  EnemyRailPin：レールを作り直す時に、敵をワールド上の同じ場所へ留める。
//   敵は「レール番号＋距離(m)」で置かれているため、レールを編集すると全長が伸縮し、
//   触っていない敵まで距離ぶんズルズル滑ってしまう（＝レール編集に追従する問題）。
//   → 作り直す前に各敵のワールド位置を Capture し、作り直した後に Apply で
//     「その位置へ一番近い点」に距離を張り直して固定する。
//   路線まるごと移動なら一緒に付いていき、形の部分編集なら他の敵は動かない。
// =====================================================================
#include "engine/math/struct.h"

#include <vector>

class EnemyEditor;
class SplineRail;

class EnemyRailPin {
public:
    // 作り直す前のレールで、各敵のワールド位置を覚える
    void Capture(const EnemyEditor& enemyEditor, const std::vector<SplineRail>& oldRails);

    // 作り直した後のレールで、覚えた位置の最寄り点へ距離を張り直す。
    //   張り直しは利用者の操作ではないので、敵の履歴の1手には数えない。
    //   張り直した距離が1つでもあれば true
    bool Apply(EnemyEditor& enemyEditor, const std::vector<SplineRail>& newRails);

private:
    std::vector<Vector3> positions_;
    std::vector<char>    valid_;
};
