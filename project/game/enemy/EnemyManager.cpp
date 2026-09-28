#include "game/enemy/EnemyManager.h"

#include "engine/rail/SplineRail.h"
#include <algorithm>

// 配置テンプレートから敵を再構築する。
//   同じ種類の実体が残っていれば使い回す（モデルの読み直しをしない）。
//   エディタで数値をドラッグしている間は毎フレーム呼ばれるので、ここが重いと敵が増えるほどカクつく
void EnemyManager::Spawn(const std::vector<EnemySpawnData>& spawns, const std::vector<SplineRail>& rails){
    std::vector<std::unique_ptr<Enemy>> oldEnemies = std::move(enemies_);
    enemies_.clear();

    // 使い回せる実体を探す：まず同じ並び順（＝同じ個体）を優先し、無ければ同じ種類の余りから取る
    auto takeReusable = [&](size_t preferredIndex, EnemyType type) -> std::unique_ptr<Enemy> {
        if ( preferredIndex < oldEnemies.size() && oldEnemies[preferredIndex]
            && oldEnemies[preferredIndex]->GetType() == type ) {
            return std::move(oldEnemies[preferredIndex]);
        }
        for ( auto& oldEnemy : oldEnemies ) {
            if ( oldEnemy && oldEnemy->GetType() == type ) { return std::move(oldEnemy); }
        }
        return nullptr;
    };

    for ( const auto& spawn : spawns ) {
        if ( spawn.railIndex < 0 || spawn.railIndex >= ( int ) rails.size() ) continue;
        if ( rails[spawn.railIndex].nodes.size() < 2 ) continue;

        std::unique_ptr<Enemy> enemy = takeReusable(enemies_.size(), spawn.type);
        if ( !enemy || !enemy->Reapply(spawn) ) {
            enemy = std::make_unique<Enemy>();
            enemy->Initialize(spawn);
        }
        // dt=0 で Update を呼び、レール上の初期位置を即座に確定させる（原点に巨大モデルが出るのを防ぐ）
        enemy->Update(rails, { 0.0f, 0.0f, 0.0f }, 0.0f);
        enemies_.push_back(std::move(enemy));
    }
}

// 移動＋吸い込みTick＋消化（完了で onConsumed を呼んで削除）
void EnemyManager::Update(const std::vector<SplineRail>& rails, const Vector3& playerPos, float dt,
                          const std::function<void(const Vector3&)>& onConsumed,
                          const BlockSystem* blocks){
    for ( auto& enemy : enemies_ ) {
        if ( enemy->IsSwallowing() ) { enemy->TickSwallow(playerPos, dt); } // 縮みながらプレイヤーへ吸い込まれる
        else { enemy->Update(rails, playerPos, dt, blocks); } // playerPos=追跡・噛みつき判定 / blocks=貫通防止
    }
    // 吸い込み完了 → 通知（お腹+1・演出はシーン側）してから消す
    for ( auto& enemy : enemies_ ) {
        if ( enemy->IsConsumed() && onConsumed ) { onConsumed(enemy->GetPosition()); }
    }
    enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(),
        [](const std::unique_ptr<Enemy>& enemy){ return enemy->IsConsumed(); }), enemies_.end());
}

void EnemyManager::Draw(){
    for ( auto& enemy : enemies_ ) { enemy->Draw(); }
}
