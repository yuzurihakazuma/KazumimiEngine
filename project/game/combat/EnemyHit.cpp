#include "game/combat/EnemyHit.h"

#include "engine/audio/AudioManager.h"
#include "game/combat/CombatSystem.h"
#include "game/combat/HitFeel.h"
#include "game/egg/EggSystem.h"
#include "game/enemy/Enemy.h"
#include "game/player/Player.h"

using namespace KazumimiEngine;

void StompHit::Resolve(Enemy& enemy, EnemyHitContext& context) const{
    const Vector3 enemyPos = enemy.GetPosition();
    enemy.Defeat();
    context.player.Bounce();
    context.hitFeel.Trigger(0.06f, 0.28f);      // 一瞬停止＋カメラ揺れ
    context.hitFeel.TriggerImpactFx(enemyPos);  // 踏んだ点中心のポストエフェクト起動
    context.combat.SpawnStompEffect(enemyPos, context.camera, StompEffectType::Stomp); // 敵は Defeat でその場で溶けて消える
}

void BodyHit::Resolve(Enemy& /*enemy*/, EnemyHitContext& context) const{
    // 敵と反対側へ弾かれる（すり抜けない）。直後は少しの間だけ無敵
    context.player.Knockback(away_);
    context.hitFeel.Trigger(0.05f, 0.22f);
    AudioManager::GetInstance()->PlayWave("resources/se/eggBreak.wav", false, 0.45f); // ぶつかった音（流用）
}

void SpitHit::Resolve(Enemy& enemy, EnemyHitContext& context) const{
    const Vector3 enemyPos = enemy.GetPosition();
    enemy.Defeat();                       // 敵はその場で溶けて消える
    context.hitFeel.Trigger(0.05f, 0.2f); // 命中の手応え
    context.combat.SpawnStompEffect(enemyPos, context.camera, StompEffectType::EggHit);
    AudioManager::GetInstance()->PlayWave("resources/se/eggHit.wav", false, 0.7f);
}

void EggHit::Resolve(Enemy& enemy, EnemyHitContext& context) const{
    const Vector3 enemyPos = enemy.GetPosition();
    enemy.Defeat();                       // 敵はその場で溶けて消える
    context.hitFeel.Trigger(0.05f, 0.2f); // 命中の手応え
    context.eggs.SpawnHitFx(enemyPos);    // 黄＆オレンジが鋭く飛び散る（踏みつけのリングとは別物）
    context.combat.SpawnStompEffect(enemyPos, context.camera, StompEffectType::EggHit); // 立体の着弾エフェクト
    AudioManager::GetInstance()->PlayWave("resources/se/eggHit.wav", false, 0.7f); // 命中音
}
