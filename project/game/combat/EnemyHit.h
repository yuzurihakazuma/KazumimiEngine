#pragma once
#include "engine/math/struct.h"

class Enemy;
class Player;
class EggSystem;
class HitFeel;
class CombatSystem;
namespace KazumimiEngine { class Camera; }

// 当たった時の結果（倒す・跳ね返る・手ごたえ・エフェクト）を出すのに使う、シーンの部品
struct EnemyHitContext {
    Player&       player;
    EggSystem&    eggs;
    HitFeel&      hitFeel;
    CombatSystem& combat;
    KazumimiEngine::Camera* camera = nullptr;
};

// =====================================================================
//  EnemyHit：敵に当たった物 1 種類（踏みつけ・横からの体当たり・卵・吐き出した敵ボール）。
//   結果は「当たった物の種類 × 敵の種類」で決まる（ダブルディスパッチ）：
//     1 段目  Enemy::ReceiveHit → EnemyBehavior::ReceiveHit（敵の種類の仮想関数）
//     2 段目  EnemyHit::OnWalker / OnBiter / OnFlyer（当たった物の種類の仮想関数）
//   今はどの敵でも同じ結果（Resolve）。「カミバナは踏めない」のような組み合わせごとの違いは、
//   その当たり方のクラスで OnBiter だけを上書きすれば足せる（CombatSystem に if を足さない）
// =====================================================================
class EnemyHit {
public:
    virtual ~EnemyHit() = default;
    // 敵の種類ごとの結果（既定＝どの種類でも Resolve）
    virtual void OnWalker(Enemy& enemy, EnemyHitContext& context) const { Resolve(enemy, context); } // 地上「ドングリン」
    virtual void OnBiter(Enemy& enemy, EnemyHitContext& context) const { Resolve(enemy, context); }  // 植物「カミバナ」
    virtual void OnFlyer(Enemy& enemy, EnemyHitContext& context) const { Resolve(enemy, context); }  // 空中「フワリン」

protected:
    // この当たり方の、ふつうの結果
    virtual void Resolve(Enemy& enemy, EnemyHitContext& context) const = 0;
};

// 踏みつけ：敵を倒して跳ね返る。一瞬停止＋カメラ揺れ・踏んだ点のポストエフェクト・リング
class StompHit final : public EnemyHit {
protected:
    void Resolve(Enemy& enemy, EnemyHitContext& context) const override;
};

// 横や下からの体当たり：プレイヤーが敵と反対側へ弾かれる（敵は倒れない）
class BodyHit final : public EnemyHit {
public:
    // away＝敵からプレイヤーへの向き（水平）
    explicit BodyHit(const KazumimiEngine::Vector3& away) : away_(away) {}

protected:
    void Resolve(Enemy& enemy, EnemyHitContext& context) const override;

private:
    KazumimiEngine::Vector3 away_;
};

// 吐き出した敵ボールが当たった：敵を倒す（ボールも消える）
class SpitHit final : public EnemyHit {
protected:
    void Resolve(Enemy& enemy, EnemyHitContext& context) const override;
};

// 飛んでいる卵が当たった：敵を倒す。黄色とオレンジの粒が飛び散る
class EggHit final : public EnemyHit {
protected:
    void Resolve(Enemy& enemy, EnemyHitContext& context) const override;
};
