#pragma once
// =====================================================================
//  Enemy : レール上を動く敵（プレイヤーが上から踏んで倒せる）。
//   ・railIndex のレール上を distance(進んだ距離) ベースで動く
//   ・位置はレール上に乗せる（モデルは足元原点。判定は体の実寸のカプセル）
//   ・動き方は個体ごとの設定（EnemySpawnData）で決まる：巡回/追跡/速度/浮遊など
// =====================================================================
#include "engine/math/struct.h"
#include "engine/collision/Collider.h"

#include <memory>
#include <string>
#include <vector>

class Obj3d;
class SkinnedObj3d;
class SplineRail;
class BlockSystem;

// 敵の種類（エディタから選択可能）。保存データは int なので既存の並びは変えないこと
enum class EnemyType{
    Zako,   // 地上「ドングリン」：よちよち歩き（Idle/Walk）
    Strong, // 植物「カミバナ」：その場でゆらゆら、近づくと噛みつく（Idle/Bite）
    Air,    // 空中「フワリン」：レールの上を浮遊しながら羽ばたく（Fly）
};

// 敵1体ぶんの配置＋動きの設定（エディタで個体ごとに編集する）。
//   「0 や負の値＝種類ごとの既定値を使う」項目は、旧マップ（項目なし）でも従来どおり動く
struct EnemySpawnData {
    EnemyType type = EnemyType::Zako; // 敵の種類
    int   railIndex = 0;       // 配置先のレール番号
    float distance  = 0.0f;    // レール上の初期位置(メートル)
    bool  patrol    = false;   // true=レールを往復パトロール / false=置いた場所に留まる（既定）
    float patrolMin = -1.0f;   // 行動範囲の始点(m)。-1=レール全体
    float patrolMax = -1.0f;   // 行動範囲の終点(m)。-1=レール全体

    // --- 個体ごとの動き ---
    std::string name;            // 一覧に出す名前（空=種類名）
    float speed       = 0.0f;    // 移動速度(m/s)。0以下=種類の既定値
    int   startDir    = 1;       // 最初の進行方向（+1=距離が増える向き / -1=逆）
    float turnWait    = 0.0f;    // 折り返す時に立ち止まる秒数
    float chaseRange  = 0.0f;    // プレイヤーがこの距離(m)以内に来たら追いかける。0=追わない
    float chaseSpeedMul = 1.5f;  // 追跡中の速度倍率
    float hoverHeight = -1.0f;   // レールから浮く高さ(m)。負=種類の既定（フワリン1.4 / 他0）
    float bobAmp      = 0.0f;    // 上下にふわふわ揺れる幅(m)。0=揺れない
    float bobSpeed    = 2.0f;    // ふわふわの速さ
    float biteRange   = -1.0f;   // カミバナ：噛みつきが出る距離(m)。負=既定2.6
    float scale       = 1.0f;    // 大きさの倍率（見た目と当たり判定の両方に掛かる）

    bool operator==(const EnemySpawnData&) const = default; // Undo履歴の変化検知用
};

// 種類ごとの既定値（体の寸法はモデルの実寸）。エディタの表示と実体の両方がここを見る
struct EnemyTypeSpec {
    float speed;       // 既定の移動速度(m/s)
    float hover;       // 既定の浮遊高さ(m)
    float bodyBottom;  // 体の下端（足元から）
    float bodyTop;     // 体の上端（足元から）
    float bodyRadius;  // 体の横半径
};

class Enemy{
public:
    Enemy();
    ~Enemy();

    // 種類ごとの既定値を返す
    static EnemyTypeSpec TypeSpecOf(EnemyType type);
    // 配置データでの実際の浮遊高さ（個体の指定が無ければ種類の既定）
    static float HoverOf(const EnemySpawnData& spawn);
    // エディタ上で敵をつかむ/印を付ける高さ（レール線から）。見えている体の中心に合わせる
    //   （フワリンは浮いているので、足元の高さで探すと見えているモデルをつかめない）
    static float PickHeightOf(const EnemySpawnData& spawn);

    // 配置データから生成する（モデルの読み込みを伴う）
    void Initialize(const EnemySpawnData& spawn);

    // モデルは作り直さず、配置データだけ入れ直して初期状態へ戻す。
    //   種類が違う（＝モデルが違う）時は false を返す（呼び出し側が Initialize し直す）。
    //   エディタで数値を動かすたびにモデルを読み直さないための入口
    bool Reapply(const EnemySpawnData& spawn);

    // レール上を移動。位置・向き・アニメを更新する。
    //   playerPos は追跡・カミバナの噛みつき発動・向きの制御に使う。
    //   blocks を渡すと、進行方向にブロックがある時に引き返す（貫通防止。nullptr=判定なし）
    void Update(const std::vector<SplineRail>& rails, const Vector3& playerPos, float dt,
                const BlockSystem* blocks = nullptr);

    // 描画（Obj3dCommon::PreDraw 済みの状態で呼ぶ）
    void Draw();

    // --- 状態 ---
    bool IsAlive() const{ return alive_ && !swallowing_; } // 吸い込み中は踏み・ロック対象外
    void Defeat(){ alive_ = false; }   // 踏まれた時など

    // --- 飲み込み（ヨッシー）：その場から縮みながらプレイヤーへ吸い込まれる ---
    void StartSwallow();                                  // 吸い込み開始
    void TickSwallow(const Vector3& playerPos, float dt); // 吸い込み中の更新（縮小＋移動）
    bool IsSwallowing() const{ return swallowing_; }
    bool IsConsumed()  const{ return consumed_; }         // 吸い込み完了（消してお腹+1）

    // 見た目だけ非表示にする（舌で捕まえた瞬間、SDF溶解演出に差し替えるため使う。
    //   position_/collider_ 等の内部状態はそのまま動き続ける＝実体は生きている）
    void SetVisualHidden(bool hidden){ visualHidden_ = hidden; }

    // 体の中心（ロックオン・演出・舌の狙い先）
    const Vector3& GetPosition() const{ return position_; }
    // 体を包む球の半径（演出の大きさ・ロックオンリング用。当たり判定は下のカプセルを使う）
    float     GetRadius() const{ return radius_; }
    Collider* GetCollider(){ return &collider_; }
    EnemyType GetType() const{ return type_; }
    int       GetRailIndex() const{ return railIndex_; }
    float     GetDistance() const{ return distance_; }

    // --- 当たり判定（見た目の実寸に合わせた縦カプセル）---
    const Vector3& GetFootPosition() const{ return footPos_; } // モデル原点（足元）
    float GetBodyBottomY() const{ return footPos_.y + bodyBottom_; } // 体の下端（ワールドY）
    float GetBodyTopY()    const{ return footPos_.y + bodyTop_; }    // 体の上端（ワールドY）
    float GetBodyRadius()  const{ return bodyRadius_; }              // 体の横半径
    // 球（卵など）が体に触れているか
    bool HitsSphere(const Vector3& center, float radius) const;
    // 球が from→to へ動いた間に体へ触れたか（速い弾のすり抜け防止）
    bool HitsSweptSphere(const Vector3& from, const Vector3& to, float radius) const;
    // 当たり判定の形をワイヤーで描く（デバッグ表示用）
    void DrawHitShape(const Vector4& color) const;

private:
    // 配置データをメンバへ写して初期状態に戻す（Initialize / Reapply 共通）
    void ApplySpawn(const EnemySpawnData& spawn);

    EnemyType type_     = EnemyType::Zako;
    int       railIndex_ = 0;       // 乗っているレール番号
    float     distance_  = 0.0f;    // レール上を進んだ距離 (m)
    float     homeDistance_ = 0.0f; // 配置された距離（追跡をやめた時に戻る場所）
    float     dir_       = 1.0f;    // 進行方向(+1/-1)
    float     speed_     = 2.0f;    // 移動速度 (m/s)
    bool      patrol_    = false;   // true=レールを往復 / false=その場に留まる（既定）
    float     patrolMin_ = -1.0f;   // 行動範囲の始点(m)。-1=レール全体
    float     patrolMax_ = -1.0f;   // 行動範囲の終点(m)。-1=レール全体
    float     turnWait_  = 0.0f;    // 折り返しで立ち止まる秒数
    float     waitTimer_ = 0.0f;    // 立ち止まりの残り時間
    float     chaseRange_ = 0.0f;   // 追跡を始める距離（0=追わない）
    float     chaseSpeedMul_ = 1.5f;
    float     hover_     = 0.0f;    // レールから浮く高さ
    float     bobAmp_    = 0.0f;    // ふわふわの幅
    float     bobSpeed_  = 2.0f;    // ふわふわの速さ
    float     bobPhase_  = 0.0f;    // ふわふわの位相
    float     biteRange_ = 2.6f;    // カミバナの噛みつき距離
    float     scale_     = 1.0f;    // 大きさの倍率
    bool      moving_    = false;   // このフレーム動いたか（歩きアニメの切り替え用）
    bool      alive_     = true;

    // 飲み込みアニメ用
    bool      swallowing_ = false;        // 吸い込み中
    bool      consumed_   = false;        // 吸い込み完了（シーンが消してお腹を増やす）
    float     swallowT_   = 0.0f;         // 吸い込み経過(秒)
    Vector3   swallowStart_ { 0.0f, 0.0f, 0.0f }; // 吸い込み開始位置
    bool      visualHidden_ = false;      // true の間は Draw をスキップ（SDF演出に差し替え中）

    Vector3 footPos_  { 0.0f, 0.0f, 0.0f }; // 足元（モデル原点）
    Vector3 position_ { 0.0f, 0.0f, 0.0f }; // 体の中心
    Vector3 rotation_ { 0.0f, 0.0f, 0.0f };
    float   radius_     = 0.4f;     // 体を包む球の半径（演出用）
    float   bodyBottom_ = 0.0f;     // 体の下端（足元からの高さ）
    float   bodyTop_    = 0.77f;    // 体の上端（足元からの高さ）
    float   bodyRadius_ = 0.36f;    // 体の横半径

    std::unique_ptr<SkinnedObj3d> skinnedObj_; // 見た目（リグ+クリップ入りの敵モデル）
    std::unique_ptr<Obj3d> obj_;    // フォールバックの見た目（モデル未登録時のsphere）
    float biteTimer_ = 0.0f;        // カミバナ：噛みつきモーションの残り時間（0=Idle中）
    float turnCooldown_ = 0.0f;     // ブロックにぶつかって折り返した直後の再判定待ち（振動防止）
    Collider collider_;             // 当たり判定（球）
};
