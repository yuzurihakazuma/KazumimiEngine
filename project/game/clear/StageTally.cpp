#include "game/clear/StageTally.h"

#include <algorithm>

void StageTally::Reset(int enemyTotal){
    result_ = {};
    result_.enemyTotal = ( std::max )( enemyTotal, 0 );
}

void StageTally::NoteCombo(int combo){
    result_.maxCombo = ( std::max )( result_.maxCombo, combo );
}

void StageTally::NoteDamage(){
    ++result_.damageCount;
}

void StageTally::NoteEnemyDefeated(){
    // 置いてあった数より多くは数えない（やりなおしで置き直した直後の知らせなどで、ずれないように）
    result_.enemiesDefeated = ( std::min )( result_.enemiesDefeated + 1, result_.enemyTotal );
}
