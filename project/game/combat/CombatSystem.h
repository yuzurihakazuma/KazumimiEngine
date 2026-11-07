#pragma once
#include "engine/math/struct.h"
#include "StompEffect.h"
#include <list>
#include <vector>
#include <memory>
#include <cstdint>
#include <d3d12.h>

class Player;
class EnemyManager;
class EggSystem;
class HitFeel;
class Camera;

// =====================================================================
//  CombatSystem：プレイヤーと敵の戦闘判定＋ヒット演出（StompEffect）の管理。
//   ・踏みつけ：空中から敵の上に接触 → 敵を倒して跳ね返る＋演出一式
//   ・卵の命中：飛行中の卵 × 敵 → 敵を倒して卵を割る＋演出
//   ・倒した敵は自分のモデルが溶けて消える（Enemy::Defeat。ここでは Defeat を呼ぶだけ）
//  シーンから分離した理由：判定と演出生成が対になった独立の関心事で、
//  他シーンでも同じ組み合わせで再利用できる。
// =====================================================================
class CombatSystem {
public:
    CombatSystem();
    ~CombatSystem();

    // エフェクト生成に使うテクスチャ（circle / 環境マップ）の SRV インデックスを受け取る
    void Initialize(uint32_t circleTexSrv, uint32_t envTexSrv);

    // エフェクトの事前生成（シーンロード時に呼ぶ）。StompEffect 1体=Obj3d×23個
    //   （GPUリソース確保46回）なので、初撃破のフレームで作るとゲームが止まる。
    //   ここで count 体ぶん作ってプールに積んでおき、実戦では使い回すだけにする。
    //   ※ Initialize（テクスチャ受け取り）の後に呼ぶこと
    void Prewarm(Camera* camera, int count);

    // 踏みつけ＋卵vs敵の当たり判定（Play中に毎フレーム呼ぶ）
    void Update(Player& player, EnemyManager& enemies, EggSystem& eggs, HitFeel& hitFeel, Camera* camera);

    void UpdateEffects(float dt); // StompEffect の進行（モード問わず毎フレーム）
    void Draw();                  // StompEffect の描画
    void ClearEffects();          // モード切替時など

    // 敵に横や下からぶつかった時に弾かれるか（false=従来どおり素通り）
    void SetContactKnockback(bool enabled){ contactKnockback_ = enabled; }
    bool IsContactKnockback() const{ return contactKnockback_; }

    // プレイヤーの当たり判定の形をワイヤーで描く（デバッグ表示用）
    void DrawPlayerHitShape(const Player& player, const Vector4& color) const;

private:
    void SpawnStompEffect(const Vector3& pos, Camera* camera, StompEffectType type);

    std::list<std::unique_ptr<StompEffect>> stompEffects_;
    // 使い終わったエフェクトの置き場（Obj3d×23個/体の作り直しを避ける。敵を倒した瞬間のカクつき対策）
    std::vector<std::unique_ptr<StompEffect>> stompPool_;
    uint32_t circleTex_ = 0;
    uint32_t envTex_    = 0;

    bool  contactKnockback_ = true;  // 敵に横からぶつかると弾かれる
    Vector3 prevPlayerFoot_ { 0.0f, 0.0f, 0.0f }; // 前フレームの足元（落ちてきたか・体の中にいたかの判定用）
    bool    hasPrevPlayerFoot_ = false;           // 前フレームの足元を持っているか（モード切替で捨てる）
};
