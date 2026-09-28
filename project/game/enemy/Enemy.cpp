#include "Enemy.h"

#include "engine/3d/obj/Obj3d.h"
#include "engine/3d/obj/Obj3dCommon.h"
#include "engine/3d/obj/SkinnedObj3d.h"
#include "engine/3d/model/Model.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/rail/SplineRail.h"
#include "game/stage/BlockSystem.h"

#include <algorithm>
#include <cmath>

namespace {
    // 種類ごとのモデル
    struct TypeModel {
        const char* modelName;
        const char* modelFile;
    };
    const TypeModel& ModelOf(EnemyType type){
        static const TypeModel kZako   { "enemyGround", "enemy_ground.glb" }; // ドングリン
        static const TypeModel kStrong { "enemyPlant",  "enemy_plant.glb" };  // カミバナ
        static const TypeModel kAir    { "enemyAir",    "enemy_air.glb" };    // フワリン
        switch ( type ) {
        case EnemyType::Strong: return kStrong;
        case EnemyType::Air:    return kAir;
        default:                return kZako;
        }
    }

    // 点 p と縦線分（x,z 固定・y0〜y1）の距離の2乗
    float DistSqToVerticalSegment(const Vector3& p, float x, float z, float y0, float y1){
        float cy = std::clamp(p.y, y0, y1);
        float dx = p.x - x, dy = p.y - cy, dz = p.z - z;
        return dx * dx + dy * dy + dz * dz;
    }
}

Enemy::Enemy() = default;
Enemy::~Enemy() = default;

// 種類ごとの既定値。体の寸法はモデル（glb）の実寸から取った値＝見た目と判定が一致する
EnemyTypeSpec Enemy::TypeSpecOf(EnemyType type){
    //                     速度  浮遊  下端   上端   横半径
    switch ( type ) {
    case EnemyType::Strong: return { 1.0f, 0.0f, 0.00f, 1.15f, 0.32f }; // カミバナ
    case EnemyType::Air:    return { 1.6f, 1.4f, 0.15f, 0.77f, 0.40f }; // フワリン
    default:                return { 2.0f, 0.0f, 0.00f, 0.77f, 0.36f }; // ドングリン
    }
}

float Enemy::HoverOf(const EnemySpawnData& spawn){
    return ( spawn.hoverHeight >= 0.0f ) ? spawn.hoverHeight : TypeSpecOf(spawn.type).hover;
}

// 配置データをメンバへ写して初期状態に戻す（モデルには触らない）
void Enemy::ApplySpawn(const EnemySpawnData& spawn){
    const EnemyTypeSpec spec = TypeSpecOf(spawn.type);
    type_      = spawn.type;
    railIndex_ = spawn.railIndex;
    distance_  = spawn.distance;
    homeDistance_ = spawn.distance;
    dir_       = ( spawn.startDir < 0 ) ? -1.0f : 1.0f;
    patrol_    = spawn.patrol;
    patrolMin_ = spawn.patrolMin;
    patrolMax_ = spawn.patrolMax;
    speed_     = ( spawn.speed > 0.0f ) ? spawn.speed : spec.speed;
    turnWait_  = ( std::max )( spawn.turnWait, 0.0f );
    chaseRange_    = ( std::max )( spawn.chaseRange, 0.0f );
    chaseSpeedMul_ = ( spawn.chaseSpeedMul > 0.0f ) ? spawn.chaseSpeedMul : 1.0f;
    hover_     = HoverOf(spawn);
    bobAmp_    = ( std::max )( spawn.bobAmp, 0.0f );
    bobSpeed_  = spawn.bobSpeed;
    biteRange_ = ( spawn.biteRange >= 0.0f ) ? spawn.biteRange : 2.6f;
    scale_     = std::clamp(spawn.scale, 0.1f, 10.0f);

    // 体の寸法（大きさ倍率込み）
    bodyBottom_ = spec.bodyBottom * scale_;
    bodyTop_    = spec.bodyTop * scale_;
    bodyRadius_ = spec.bodyRadius * scale_;
    radius_     = ( std::max )( bodyRadius_, ( bodyTop_ - bodyBottom_ ) * 0.5f );

    // 状態のリセット（Play で倒された／食べられた個体もエディタへ戻れば復活する）
    alive_        = true;
    swallowing_   = false;
    consumed_     = false;
    swallowT_     = 0.0f;
    visualHidden_ = false;
    moving_       = false;
    waitTimer_    = 0.0f;
    turnCooldown_ = 0.0f;
    biteTimer_    = 0.0f;
    bobPhase_     = std::fmod(spawn.distance * 0.61f, 6.2831853f); // 個体ごとに揺れの位相をずらす

    if ( skinnedObj_ ) {
        skinnedObj_->SetScale({ scale_, scale_, scale_ });
        // 個体ごとにアニメの位相をずらす（並んだ敵が同じタイミングで動かないように）
        skinnedObj_->SetAnimationTime(std::fmod(distance_ * 0.37f, 1.0f));
    } else if ( obj_ ) {
        obj_->SetScale({ radius_, radius_, radius_ });
    }
    collider_.SetSphere(position_, radius_);
    collider_.SetOwner(this);
}

void Enemy::Initialize(const EnemySpawnData& spawn){
    const TypeModel& spec = ModelOf(spawn.type);
    const char* startClip = "Idle";
    if ( spawn.type == EnemyType::Zako ) { startClip = spawn.patrol ? "Walk" : "Idle"; }
    if ( spawn.type == EnemyType::Air )  { startClip = "Fly"; }

    // 見た目：リグ付きモデルを生成（未登録などで失敗したら従来のsphereへフォールバック）
    obj_.reset();
    skinnedObj_ = SkinnedObj3d::Create(spec.modelName, "resources/enemy", spec.modelFile);
    if ( skinnedObj_ ) {
        // 環境マップは必ず束縛する（未設定だとslot0の2Dが刺さりGPU検証で落ちる）
        skinnedObj_->SetEnvironmentMap(Obj3dCommon::GetInstance()->GetEnvironmentTextureSrvIndex());
        skinnedObj_->LoadClips("resources/enemy", spec.modelFile);
        skinnedObj_->SetClip(startClip, true);
    } else {
        obj_ = Obj3d::Create("sphere");
        if ( obj_ && spawn.type != EnemyType::Zako ) {
            auto model = obj_->GetModel();
            if ( model && model->GetMaterial() ) {
                model->GetMaterial()->color = { 1.0f, 0.4f, 0.4f, 1.0f };
            }
        }
    }
    ApplySpawn(spawn);
}

bool Enemy::Reapply(const EnemySpawnData& spawn){
    if ( spawn.type != type_ ) return false;          // モデルが違う
    if ( !skinnedObj_ && !obj_ ) return false;        // まだ生成されていない
    ApplySpawn(spawn);
    if ( skinnedObj_ ) {
        if ( type_ == EnemyType::Air )         { skinnedObj_->SetClip("Fly", true); }
        else if ( type_ == EnemyType::Strong ) { skinnedObj_->SetClip("Idle", true); }
    }
    return true;
}

void Enemy::Update(const std::vector<SplineRail>& rails, const Vector3& playerPos, float dt,
                   const BlockSystem* blocks){
    if ( !alive_ ) return;
    if ( swallowing_ ) return; // 吸い込み中は TickSwallow が動かすのでレール移動はしない
    if ( railIndex_ < 0 || railIndex_ >= ( int ) rails.size() ) return;
    const SplineRail& rail = rails[railIndex_];
    if ( rail.nodes.size() < 2 ) return;

    const float railLength = rail.GetLength();

    // 行動範囲：指定があればその中だけを動く（-1=レール全体）。
    //   レール編集で全長が縮んだ時も範囲内に収める
    //   動かない敵（巡回も追跡もOFF）には範囲を効かせない＝置いた場所から動かさない
    const bool ranged = ( patrol_ || chaseRange_ > 0.0f )
                     && ( patrolMin_ >= 0.0f || patrolMax_ >= 0.0f );
    float lo = ( ranged && patrolMin_ >= 0.0f ) ? ( std::min )( patrolMin_, railLength ) : 0.0f;
    float hi = ( ranged && patrolMax_ >= 0.0f ) ? std::clamp(patrolMax_, lo, railLength) : railLength;

    moving_ = false;
    if ( dt > 0.0f ) {
        if ( turnCooldown_ > 0.0f ) { turnCooldown_ -= dt; }
        bobPhase_ += bobSpeed_ * dt;

        // --- どう動くかを決める：追跡 ＞ 巡回 ＞ 持ち場へ戻る ＞ 待機 ---
        bool  wantMove  = false;
        bool  chasing   = false;
        float moveSpeed = speed_;
        if ( chaseRange_ > 0.0f ) {
            Vector3 toPlayer = { playerPos.x - footPos_.x, playerPos.y - footPos_.y, playerPos.z - footPos_.z };
            float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
            // 上下の別レールにいるプレイヤーは追わない（レール面からの高さの差で ±2m まで）。
            //   足元（footPos_）は浮いている分だけ上にあるので、レール面の高さに直して比べる
            const float railY = rail.GetPositionByDistance(distance_).y;
            const float heightFromRail = playerPos.y - railY;
            if ( distXZ < chaseRange_ && heightFromRail > -2.0f && heightFromRail < 2.0f ) {
                chasing = true;
                Vector3 tangent = rail.GetTangentByDistance(distance_);
                float along = tangent.x * toPlayer.x + tangent.z * toPlayer.z; // レールに沿った前後
                if ( std::abs(along) > 0.25f ) { // 真横まで来たら足踏みしない
                    dir_ = ( along > 0.0f ) ? 1.0f : -1.0f;
                    wantMove  = true;
                    moveSpeed = speed_ * chaseSpeedMul_;
                }
            }
        }
        if ( !chasing ) {
            if ( patrol_ ) {
                if ( waitTimer_ > 0.0f ) { waitTimer_ -= dt; }
                else { wantMove = true; }
            } else if ( chaseRange_ > 0.0f && std::abs(homeDistance_ - distance_) > 0.05f ) {
                // 追跡をやめたら置かれた場所へ歩いて戻る
                dir_ = ( homeDistance_ > distance_ ) ? 1.0f : -1.0f;
                wantMove = true;
            }
        }

        if ( wantMove ) {
            float step = moveSpeed * dt;
            if ( !chasing && !patrol_ ) {
                step = ( std::min )( step, std::abs(homeDistance_ - distance_) ); // 持ち場を行き過ぎない
            }
            float nextDist = std::clamp(distance_ + dir_ * step, lo, hi);
            // 進行方向にブロックがあれば進まない（プレイヤーと同じ BlockedAt 判定＝貫通しない）。
            //   高さはレール面からの相対値で渡す。体の高さ帯で見るので、
            //   フワリン（浮遊）は低いブロックの上をそのまま飛び越えられる
            bool hitBlock = false;
            if ( blocks ) {
                float bandBottom = hover_ + bodyBottom_ + 0.05f; // 足元すれすれは接地扱いにしない
                float bandTop    = hover_ + bodyTop_;
                float blockMin = 0.0f, blockMax = 0.0f;
                hitBlock = blocks->BlockedAt(railIndex_, nextDist, bandBottom, bandTop,
                                             &blockMin, &blockMax, nullptr, bodyRadius_);
                // すでにブロックの当たり範囲の中にいる（置いた場所がブロックに近すぎた等）時は、
                // ブロックから離れる向きの一歩だけ通す。通さないと、どちらへも動けず その場で回り続ける
                if ( hitBlock && distance_ >= blockMin && distance_ <= blockMax ) {
                    const float blockCenter = ( blockMin + blockMax ) * 0.5f;
                    if ( std::abs(nextDist - blockCenter) > std::abs(distance_ - blockCenter) ) { hitBlock = false; }
                }
            }
            if ( hitBlock ) {
                if ( patrol_ && !chasing && turnCooldown_ <= 0.0f ) {
                    dir_ = -dir_;          // 壁に当たった：向きを変えて引き返す
                    turnCooldown_ = 0.3f;  // 両側が塞がっている時にクルクル震えないための猶予
                    waitTimer_ = turnWait_;
                }
                // その場に留まる（めり込まない）
            } else {
                moving_   = ( std::abs(nextDist - distance_) > 1e-5f );
                distance_ = nextDist;
                // 巡回の端に着いたら折り返す（設定があれば立ち止まってから）
                if ( patrol_ && !chasing ) {
                    if ( distance_ >= hi && dir_ > 0.0f ) { dir_ = -1.0f; waitTimer_ = turnWait_; }
                    if ( distance_ <= lo && dir_ < 0.0f ) { dir_ =  1.0f; waitTimer_ = turnWait_; }
                }
            }
        }
    }
    // 範囲の外に置かれていた／レールが縮んだ時の保険
    if ( distance_ > hi ) { distance_ = hi; }
    if ( distance_ < lo ) { distance_ = lo; }

    // 位置：モデルの原点は足元。浮遊＋ふわふわの分だけレールから浮かせる
    Vector3 railPos = rail.GetPositionByDistance(distance_);
    float bob = ( bobAmp_ > 0.0f ) ? std::sin(bobPhase_) * bobAmp_ : 0.0f;
    footPos_  = { railPos.x, railPos.y + hover_ + bob, railPos.z };
    position_ = { footPos_.x, footPos_.y + ( bodyBottom_ + bodyTop_ ) * 0.5f, footPos_.z };

    // 進行方向を向く
    Vector3 tangent = rail.GetTangentByDistance(distance_);
    if ( std::abs(tangent.x) > 1e-4f || std::abs(tangent.z) > 1e-4f ) {
        rotation_.y = std::atan2(tangent.x * dir_, tangent.z * dir_);
    }

    // --- 種類ごとのアニメ制御 ---
    if ( skinnedObj_ ) {
        if ( type_ == EnemyType::Zako ) {
            // 歩いている時だけ Walk、それ以外は Idle（SetClipは同名なら何もしないので毎フレーム呼んでよい）。
            //   エディット中(dt=0)は動く設定の敵を Walk で見せる
            bool walk = ( dt > 0.0f ) ? moving_ : ( patrol_ || chaseRange_ > 0.0f );
            skinnedObj_->SetClip(walk ? "Walk" : "Idle", true);
        } else if ( type_ == EnemyType::Strong && dt > 0.0f ) {
            // カミバナ：プレイヤーが近づくと噛みつく（終わったらIdleへ戻る）。近くでは体も向ける
            float dx = playerPos.x - position_.x;
            float dz = playerPos.z - position_.z;
            float distXZ = std::sqrt(dx * dx + dz * dz);
            float dy = std::abs(playerPos.y - position_.y);
            if ( biteTimer_ > 0.0f ) {
                biteTimer_ -= dt;
                if ( biteTimer_ <= 0.0f ) { skinnedObj_->SetClip("Idle", true); }
            } else if ( biteRange_ > 0.0f && distXZ < biteRange_ && dy < 2.0f ) {
                skinnedObj_->SetClip("Bite", false);
                skinnedObj_->SetAnimationTime(0.0f);
                biteTimer_ = 1.5f; // Bite(24f)＋ひと呼吸
            }
            if ( distXZ < biteRange_ + 1.4f && ( std::abs(dx) > 1e-4f || std::abs(dz) > 1e-4f ) ) {
                rotation_.y = std::atan2(dx, dz); // プレイヤーの方を向く（鉢は不動でも上体が向く）
            }
        }
    }

    // 当たり判定と見た目を追従（モデルは足元原点なので footPos に置く）
    collider_.SetCenter(position_);
    if ( skinnedObj_ ) {
        skinnedObj_->SetTranslation(footPos_);
        skinnedObj_->SetRotation(rotation_);
        skinnedObj_->SetScale({ scale_, scale_, scale_ });
        skinnedObj_->Update();
    } else if ( obj_ ) {
        obj_->SetTranslation(position_);
        obj_->SetRotation(rotation_);
        obj_->SetScale({ radius_, radius_, radius_ });
        obj_->Update();
    }
}

void Enemy::Draw(){
    if ( !alive_ || visualHidden_ ) return; // 吸い込み中も alive_ は true のまま → 縮む様子を描画する
    if ( skinnedObj_ )   { skinnedObj_->Draw(); }
    else if ( obj_ )     { obj_->Draw(); }
}

// 球が体（縦カプセル）に触れているか
bool Enemy::HitsSphere(const Vector3& center, float radius) const{
    // カプセルの芯：体の下端〜上端から半径ぶん内側へ寄せた縦線分
    float capRadius = ( std::min )( bodyRadius_, ( bodyTop_ - bodyBottom_ ) * 0.5f );
    float y0 = footPos_.y + bodyBottom_ + capRadius;
    float y1 = footPos_.y + bodyTop_ - capRadius;
    float reach = capRadius + radius;
    return DistSqToVerticalSegment(center, footPos_.x, footPos_.z, y0, y1) <= reach * reach;
}

// 球が from→to へ動いた間に体へ触れたか。移動区間を半径の半分刻みで調べる
bool Enemy::HitsSweptSphere(const Vector3& from, const Vector3& to, float radius) const{
    Vector3 delta = { to.x - from.x, to.y - from.y, to.z - from.z };
    float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    int steps = std::clamp(( int ) std::ceil(length / ( std::max )( radius * 0.5f, 0.05f )), 1, 64);
    for ( int i = 1; i <= steps; ++i ) {
        float t = ( float ) i / ( float ) steps;
        if ( HitsSphere({ from.x + delta.x * t, from.y + delta.y * t, from.z + delta.z * t }, radius) ) {
            return true;
        }
    }
    return false;
}

// 当たり判定の形（縦カプセル）をワイヤーで描く
void Enemy::DrawHitShape(const Vector4& color) const{
    if ( !IsAlive() ) return;
    float capRadius = ( std::min )( bodyRadius_, ( bodyTop_ - bodyBottom_ ) * 0.5f );
    float y0 = footPos_.y + bodyBottom_ + capRadius;
    float y1 = footPos_.y + bodyTop_ - capRadius;
    DebugDraw* debugDraw = DebugDraw::GetInstance();
    debugDraw->Sphere({ footPos_.x, y0, footPos_.z }, capRadius, color, 12);
    if ( y1 - y0 > 0.01f ) {
        debugDraw->Sphere({ footPos_.x, y1, footPos_.z }, capRadius, color, 12);
        const float offsets[4][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f } };
        for ( const auto& offset : offsets ) {
            float x = footPos_.x + offset[0] * capRadius;
            float z = footPos_.z + offset[1] * capRadius;
            debugDraw->Line({ x, y0, z }, { x, y1, z }, color);
        }
    }
}

// 飲み込み開始：今いる場所を起点に、縮みながらプレイヤーへ吸い込まれる。
void Enemy::StartSwallow(){
    if ( swallowing_ || !alive_ ) return;
    swallowing_   = true;
    swallowT_     = 0.0f;
    swallowStart_ = position_;
}

// 吸い込み中の更新：プレイヤーの口元へ寄りながらスケールを 1→0 へ縮める。
void Enemy::TickSwallow(const Vector3& playerPos, float dt){
    if ( !swallowing_ ) return;
    const float duration = 0.06f; // 食ったら即回収（掴んだ瞬間ほぼその場で消えてお腹に入る）
    swallowT_ += dt;
    float t = swallowT_ / duration;
    if ( t > 1.0f ) t = 1.0f;

    // 口元へ近づく（少し上）
    Vector3 mouth = { playerPos.x, playerPos.y + 0.5f, playerPos.z };
    position_ = {
        swallowStart_.x + ( mouth.x - swallowStart_.x ) * t,
        swallowStart_.y + ( mouth.y - swallowStart_.y ) * t,
        swallowStart_.z + ( mouth.z - swallowStart_.z ) * t
    };

    collider_.SetCenter(position_);
    float shrink = 1.0f - t;
    float centerHeight = ( bodyBottom_ + bodyTop_ ) * 0.5f; // 足元→体の中心
    footPos_ = { position_.x, position_.y - centerHeight * shrink, position_.z };
    if ( skinnedObj_ ) {
        skinnedObj_->SetTranslation(footPos_); // 足元原点ぶん下げる
        skinnedObj_->SetScale({ scale_ * shrink, scale_ * shrink, scale_ * shrink });
        skinnedObj_->SetRotation(rotation_);
        skinnedObj_->Update();
    } else if ( obj_ ) {
        float shrinkScale = radius_ * shrink; // どんどん小さく
        obj_->SetTranslation(position_);
        obj_->SetScale({ shrinkScale, shrinkScale, shrinkScale });
        obj_->SetRotation(rotation_);
        obj_->Update();
    }

    if ( t >= 1.0f ) { // 吸い込み完了
        swallowing_ = false;
        consumed_   = true;
        alive_      = false;
    }
}
