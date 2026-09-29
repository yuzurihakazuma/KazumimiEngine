#include "game/enemy/editor/EnemySpawnModel.h"
#include "engine/rail/SplineRail.h"
#include <algorithm>

void EnemySpawnModel::Replace(const std::vector<EnemySpawnData>& datas){
    spawnDatas_ = datas;
    selectedEntry_ = -1;
    changed_ = true; // シーン側がリスポーンするよう変更フラグを立てる
}

void EnemySpawnModel::Clear(){
    spawnDatas_.clear();
    selectedEntry_ = -1;
    changed_ = true;
}

void EnemySpawnModel::OnRestored(){
    DropStaleSelection();
    changed_ = true;
}

bool EnemySpawnModel::ConsumeFocusRequest(int& outIndex){
    if ( focusRequest_ < 0 ) return false;
    outIndex = focusRequest_;
    focusRequest_ = -1;
    return outIndex < Count();
}

// ================================================================
//  敵の追加・削除・複製
// ================================================================
int EnemySpawnModel::AddEntry(const EnemySpawnData& spawn){
    spawnDatas_.push_back(spawn);
    selectedEntry_ = Count() - 1;
    scrollToSelected_ = true;
    changed_ = true;
    return selectedEntry_;
}

void EnemySpawnModel::RemoveEntry(int index){
    if ( !IsValidIndex(index) ) return;
    spawnDatas_.erase(spawnDatas_.begin() + index);
    // 選択中の敵より前を消したら番号が1つ詰まる。消した本人を選んでいたら選択を外す
    if ( selectedEntry_ == index )     { selectedEntry_ = -1; }
    else if ( selectedEntry_ > index ) { --selectedEntry_; }
    changed_ = true;
}

int EnemySpawnModel::DuplicateEntry(int index, const std::vector<SplineRail>& splineRails){
    if ( !IsValidIndex(index) ) return -1;
    EnemySpawnData copied = spawnDatas_[index];
    int newIndex = AddEntry(copied);
    // 同じ場所に重ねると選べなくなるので、2m先（レールの端なら2m手前）へずらして置く
    if ( copied.railIndex >= 0 && copied.railIndex < ( int ) splineRails.size() ) {
        float railLength = splineRails[copied.railIndex].GetLength();
        float shifted = copied.distance + 2.0f;
        if ( shifted > railLength ) { shifted = copied.distance - 2.0f; }
        MoveEntry(newIndex, copied.railIndex, shifted, splineRails);
    }
    return newIndex;
}

// 敵を別の場所へ動かす。行動範囲つきの敵は範囲ごと平行移動する
void EnemySpawnModel::MoveEntry(int index, int railIndex, float distance, const std::vector<SplineRail>& splineRails){
    if ( !IsValidIndex(index) ) return;
    if ( railIndex < 0 || railIndex >= ( int ) splineRails.size() ) return;
    EnemySpawnData& spawn = spawnDatas_[index];
    float railLength = splineRails[railIndex].GetLength();
    distance = std::clamp(distance, 0.0f, railLength);

    if ( HasMoveRange(spawn) ) {
        // 範囲の幅と、その中での相対位置を保ったまま動かす（レールの端では幅を保って内側へ寄せる）
        float oldMin = ( spawn.patrolMin >= 0.0f ) ? spawn.patrolMin : 0.0f;
        float oldMax = ( spawn.patrolMax >= 0.0f ) ? spawn.patrolMax : oldMin;
        float width  = ( std::min )( ( std::max )( oldMax - oldMin, 0.0f ), railLength );
        float newMin = oldMin + ( distance - spawn.distance );
        newMin = std::clamp(newMin, 0.0f, ( std::max )( railLength - width, 0.0f ));
        spawn.patrolMin = newMin;
        spawn.patrolMax = newMin + width;
        distance = std::clamp(distance, spawn.patrolMin, spawn.patrolMax);
    }
    spawn.railIndex = railIndex;
    spawn.distance  = distance;
}

// ================================================================
//  レール番号の付け替え
// ================================================================
// 1つのリストを付け替える。範囲の外の番号（もともとレールが無い敵）はそのまま残す
void EnemySpawnModel::RemapRails(std::vector<EnemySpawnData>& list,
                                 const std::vector<int>& oldToNew, const std::vector<EnemySpawnData>& revived){
    std::erase_if(list, [&](const EnemySpawnData& spawn){
        return spawn.railIndex >= 0 && spawn.railIndex < ( int ) oldToNew.size() && oldToNew[spawn.railIndex] < 0;
    });
    for ( auto& spawn : list ) {
        if ( spawn.railIndex >= 0 && spawn.railIndex < ( int ) oldToNew.size() ) { spawn.railIndex = oldToNew[spawn.railIndex]; }
    }
    list.insert(list.end(), revived.begin(), revived.end());
}

void EnemySpawnModel::ApplyRailRemap(const std::vector<int>& oldToNew, const std::vector<EnemySpawnData>& revived){
    const std::vector<EnemySpawnData> before = spawnDatas_;
    const EnemySpawnData* selectedBefore = ( selectedEntry_ >= 0 && selectedEntry_ < ( int ) before.size() ) ? &before[selectedEntry_] : nullptr;
    RemapRails(spawnDatas_, oldToNew, revived);

    // 選んでいた敵がまだ残っていれば、その敵を選んだままにする（番号が詰まっても追いかける）
    selectedEntry_ = -1;
    if ( selectedBefore ) {
        EnemySpawnData expected = *selectedBefore;
        if ( expected.railIndex >= 0 && expected.railIndex < ( int ) oldToNew.size() ) { expected.railIndex = oldToNew[expected.railIndex]; }
        for ( int i = 0; i < Count(); ++i ) {
            if ( spawnDatas_[i] == expected ) { selectedEntry_ = i; break; }
        }
    }
    if ( !( before == spawnDatas_ ) ) { changed_ = true; }
}
