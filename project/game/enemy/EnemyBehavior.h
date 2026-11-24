#pragma once
#include "engine/math/struct.h"

#include <memory>

namespace KazumimiEngine { class SkinnedObj3d; }
enum class EnemyType;
class Enemy;
class EnemyHit;
struct EnemyHitContext;

// =====================================================================
//  EnemyBehavior：敵の種類ごとのふるまい（どのアニメーションを流すか・プレイヤーにどう反応するか）。
//   レールの上の動き（巡回・追跡・ブロックで引き返す）は全種類で同じなので Enemy が持ち、
//   種類で違う所だけをこのクラスの派生に分ける（Strategy）。Enemy は種類を見て分岐しない。
//   ・WalkerBehavior … 地上「ドングリン」：歩いている間は Walk、止まっている間は Idle
//   ・BiterBehavior  … 植物「カミバナ」：プレイヤーが近づくと噛みつき、近くでは体を向ける
//   ・FlyerBehavior  … 空中「フワリン」：ずっと羽ばたく
//   種類を足す時は、派生クラスを 1 つ足して Create に 1 行足す
// =====================================================================
class EnemyBehavior {
public:
    // 毎フレームの様子（Enemy が詰めて渡す）
    struct Frame {
        float   deltaTime = 0.0f;   // 0＝エディット中（時間が止まっている）
        bool    moving = false;     // このフレーム、レールの上を進んだ
        bool    canMove = false;    // 巡回か追跡の設定がある（エディット中に、動く敵を歩きの形で見せる）
        KazumimiEngine::Vector3 position {};        // 体の中心
        KazumimiEngine::Vector3 playerPos {};
        float   biteRange = 0.0f;   // 噛みつく距離（配置データ）
    };

    virtual ~EnemyBehavior() = default;

    // 種類に合うふるまいを作る（知らない種類は、いちばん基本の歩く敵）
    static std::unique_ptr<EnemyBehavior> Create(EnemyType type);

    // 置いた時に流すクリップ。patrols＝巡回する設定か
    virtual const char* StartClip(bool patrols) const = 0;
    // 配置データを入れ直した時：途中の状態（噛みつきの残り時間など）を捨てる
    virtual void Reset() {}
    // エディタで数値を動かして置き直した時：決まったクリップへ戻す
    virtual void OnReapplied(KazumimiEngine::SkinnedObj3d& /*model*/) {}
    // 毎フレーム：クリップを切り替える。体の向きを自分で決めたい時は outYaw に入れて true を返す
    virtual bool Update(KazumimiEngine::SkinnedObj3d& model, const Frame& frame, float& outYaw) = 0;
    // 何かが当たった：自分の種類に合う EnemyHit の関数を呼ぶ（ダブルディスパッチの 1 段目。結果は当たった物が決める）
    virtual void ReceiveHit(const EnemyHit& hit, Enemy& enemy, EnemyHitContext& context) const = 0;
};
