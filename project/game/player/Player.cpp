#include "game/player/Player.h"
#include "game/player/PlayerRailQuery.h"
#include "engine/base/TimeManager.h"
#include "engine/math/VectorMath.h"
#include "engine/rail/SplineRail.h"

#include <algorithm>
#include <cmath>

using namespace VectorMath;

namespace {
    // 端に接続が無い時に「飛び出して落ちる」か。
    //   true  … 端を越えると空中へ（穴に落ちる／ジャンプで飛び越えられる）
    //   false … 端で停止する（崖なしの安全仕様）
    constexpr bool kFallOffEdges = true;

    // ここより下に落ちたらスタートへリスポーン
    constexpr float kKillY = -10.0f;

    // 水平(x,z)を単位ベクトル化（長さ0なら0ベクトル）
    Vector3 HorizDir(float x, float z){
        float len = std::sqrt(x * x + z * z);
        if ( len < 1e-4f ) return { 0.0f, 0.0f, 0.0f };
        return { x / len, 0.0f, z / len };
    }
}

void Player::Initialize(){
    position_ = { 0.0f, 0.0f, 0.0f };
    rotation_ = { 0.0f, 0.0f, 0.0f };

    // マップで指定されたスタート地点から開始（リスポーンもここへ戻る）
    currentDistance_  = spawnDist_;
    currentRailIndex_ = spawnRail_;
    dsSign_           = 0.0f;
    prevMoveInput_    = 0.0f;
    atJunction_       = false;
    switchCooldown_   = 0.0f;

    jump_.Reset();
    knock_.Reset();
    facing_.Reset();

    inAir_           = false;
    airVelocity_     = { 0.0f, 0.0f, 0.0f };
    airLandCooldown_ = 0.0f;
    airFromRail_     = -1;
    airDir_          = { 0.0f, 0.0f, 0.0f };
    posSmooth_       = { 0.0f, 0.0f, 0.0f };
}

// =====================================================================
//  メインのフレーム更新。段階ごとに専用の関数へ委譲する
// =====================================================================
void Player::Update(const std::vector<SplineRail>& allRails){
    if ( allRails.empty() ) return;
    if ( currentRailIndex_ < 0 || currentRailIndex_ >= ( int ) allRails.size() ) return;

    const float dt = Time::GetInstance()->GetDeltaTime(); // フレームレート非依存
    knock_.TickInvincible(dt);

    // 0. 空中状態（レール外）：自由落下しながら着地できるレールを探す
    if ( inAir_ ) {
        knock_.TickKnock(dt);
        UpdateAir(allRails, dt);
        return;
    }

    if ( switchCooldown_ > 0.0f ) { switchCooldown_ -= dt; }

    // 乗り移り前の見た目位置（このフレームで座標が飛んだら差分を平滑化に回す）
    const Vector3 worldBefore = position_;

    const SplineRail& currentRail = allRails[currentRailIndex_];
    if ( currentRail.nodes.size() < 2 ) return;

    // 穴の上に来たら、レールを離れて自由落下（弾道）。下の別レールへ着地できる。
    //   ただしブロックの上に立っている時は落ちない（穴を渡す橋・足場が作れるように）
    const bool onBlock = blockContact_.IsOnBlock(currentRailIndex_, currentDistance_, jump_.height);
    if ( jump_.grounded && !onBlock && PlayerRailQuery::IsOverHole(allRails, FootOnRail(allRails)) ) {
        EnterAir(currentRail.GetPositionByDistance(currentDistance_), currentRail.GetTangentByDistance(currentDistance_), 0.0f, 0.15f);
        return;
    }

    const bool horizontalRail = ( currentRail.type == SplineRail::RailType::Horizontal );

    // 1. 入力 → 2. 移動（構え中・弾かれている間は操作不可）
    PlayerInput::RailInput railInput = input_.ReadRail(horizontalRail);
    if ( movementLocked_ || knock_.IsKnocked() ) { railInput = {}; }
    MoveAlongRail(currentRail, horizontalRail, railInput.move, dt);
    const float knockSign = PushBackAlongRail(currentRail, dt);

    // 3. 終端処理（持ち越し/合流/落下/クランプ）。空中へ飛び出したら終了。
    //   弾かれている間は、端の処理に「弾かれている向き」を進む向きとして渡す
    //   （操作できない間は進む向きが0なので、そのままだと端から飛び出しても横へ進まず真下へ落ち、
    //     「前にあるレールにだけ合流する」判定も効かずに横の並走レールへ移ってしまう）
    bool transitioned = false;
    const float moveSignBeforeEnds = dsSign_;
    if ( knockSign != 0.0f ) { dsSign_ = knockSign; }
    if ( HandleRailEnds(allRails, currentRail, transitioned) ) return;
    if ( knockSign != 0.0f ) { dsSign_ = moveSignBeforeEnds; } // 向きは変えない（弾かれた方を向かない）

    // 4. 乗り換え
    if ( railInput.switchDir != 0 && switchCooldown_ <= 0.0f && !transitioned ) {
        transitioned = TrySwitchOrBranch(allRails, horizontalRail, railInput.switchDir);
    }

    // 5. 距離クランプ（ループはラップ）
    const SplineRail& rail = allRails[currentRailIndex_];
    if ( rail.isLoop && rail.GetLength() > 0.0f ) {
        while ( currentDistance_ > rail.GetLength() ) currentDistance_ -= rail.GetLength();
        while ( currentDistance_ < 0.0f )             currentDistance_ += rail.GetLength();
    } else {
        currentDistance_ = std::clamp(currentDistance_, 0.0f, rail.GetLength());
    }

    // 6. ジャンプ＋着地。穴の上で降りてきて空中になったら終了
    if ( UpdateJumpAndLand(rail, allRails, dt) ) return;

    // 7. 座標・向きを確定（乗り移りの平滑化つき）
    FinalizePosition(rail, worldBefore, transitioned, dt);
}

Vector3 Player::FootOnRail(const std::vector<SplineRail>& rails) const{
    return rails[currentRailIndex_].GetPositionByDistance(currentDistance_);
}

// 今のレールを距離で進める（進行方向の記憶つき）。
//   キーを「押した瞬間」だけワールド方向(横=X/縦=Z)から進行符号を決め、押しっぱなしの
//   間は符号を保持する → 円状レールの頂点・急カーブで接線が反転しても止まらない/逆走しない。
void Player::MoveAlongRail(const SplineRail& rail, bool horizontalRail, float moveInput, float dt){
    if ( moveInput == 0.0f ) {
        dsSign_ = 0.0f; // 離したら次に押した時に向きを決め直す
        atJunction_ = false;
        prevMoveInput_ = moveInput;
        return;
    }
    const bool freshPress = ( prevMoveInput_ == 0.0f ) || ( moveInput * prevMoveInput_ < 0.0f );
    prevMoveInput_ = moveInput;
    // 合流直後のジャンクションで一旦停止中：押し直す(離して再入力 or 逆キー)まで動かない
    if ( atJunction_ && !freshPress ) return;
    atJunction_ = false;

    if ( freshPress || dsSign_ == 0.0f ) {
        Vector3 tangent = rail.GetTangentByDistance(currentDistance_);
        float axisComponent = horizontalRail ? tangent.x : tangent.z;
        if ( std::abs(axisComponent) > 0.05f ) {
            dsSign_ = moveInput * ( ( axisComponent >= 0.0f ) ? 1.0f : -1.0f ); // 接線の軸成分に合わせる
        } else {
            dsSign_ = moveInput; // 接線がほぼ直交（円の頂点など）→ 押した向きへ
        }
    }
    // 片方向レール：許可されていない向きへは進めない（ジェットコースター区間など）
    if ( rail.oneWay == 1 && dsSign_ < 0.0f ) dsSign_ = 0.0f; // 正方向(front→back)のみ
    if ( rail.oneWay == 2 && dsSign_ > 0.0f ) dsSign_ = 0.0f; // 逆方向(back→front)のみ

    // レールごとの速度倍率（加速/減速レール）を掛ける
    const float prevDistance = currentDistance_;
    const float oldFootY = rail.GetPositionByDistance(currentDistance_).y;
    currentDistance_ += dsSign_ * moveSpeed_ * rail.speedMul * dt;
    if ( !jump_.grounded ) {
        // 空中（ジャンプ中）はワールド空間の弾道を保つ：
        //   最終位置は「レール上の点 + 高さ」なので、何もしないと
        //   レールが下るとジャンプの弧ごと引きずり下ろされて低いジャンプになる。
        //   進んだ分のレール高低差を打ち消して、高い所から跳んだ高さを維持する
        //   （着地は高さ<=0 のまま＝弧がレールに届いた場所で着地する。上りでは早めに着地して自然）
        jump_.height += oldFootY - rail.GetPositionByDistance(currentDistance_).y;
    }
    // ブロックの横当たり（壁）
    currentDistance_ = blockContact_.ResolveWalk(currentRailIndex_, prevDistance, currentDistance_, jump_.height, jump_.grounded);
}

// 敵にぶつかったノックバック：弾かれる方向に近いレールの向きへ押し戻す（壁は越えない）
float Player::PushBackAlongRail(const SplineRail& rail, float dt){
    if ( !knock_.IsKnocked() ) return 0.0f;
    knock_.TickKnock(dt);
    const Vector3 tangent = rail.GetTangentByDistance(currentDistance_);
    const Vector3& knockDir = knock_.Direction();
    const float knockSign = ( tangent.x * knockDir.x + tangent.z * knockDir.z >= 0.0f ) ? 1.0f : -1.0f;
    const float prevDistance = currentDistance_;
    currentDistance_ += knockSign * PlayerKnockback::kRailSpeed * dt;
    if ( blockContact_.IsBodyBlocked(currentRailIndex_, currentDistance_, jump_.height) ) {
        currentDistance_ = prevDistance;
    }
    return knockSign;
}

// レール終端の処理（ループ周回 / クールダウン中クランプ / 持ち越し・合流・落下・クランプ）。
//   空中へ飛び出したら true を返す（呼び出し側は return する）。
bool Player::HandleRailEnds(const std::vector<SplineRail>& rails, const SplineRail& rail, bool& transitioned){
    const float len = rail.GetLength();
    if ( rail.isLoop ) {
        // ループ：端が無い。距離を周回でラップ
        if ( len > 0.0f ) {
            while ( currentDistance_ > len )  currentDistance_ -= len;
            while ( currentDistance_ < 0.0f ) currentDistance_ += len;
        }
        return false;
    }
    if ( switchCooldown_ > 0.0f ) { // クールダウン中は端でクランプ（乗り換え直後のワープ防止）
        currentDistance_ = std::clamp(currentDistance_, 0.0f, len);
        return false;
    }

    const bool pastBack = ( currentDistance_ > len );
    const bool pastFront = ( currentDistance_ < 0.0f );
    if ( !pastBack && !pastFront ) return false;

    // 越えた側の端（終点 or 始点）の情報
    const float edge        = pastBack ? len : 0.0f;
    const int   connected   = pastBack ? rail.backConnIndex : rail.frontConnIndex;
    const bool  connToFront = pastBack ? rail.backConnToFront : rail.frontConnToFront;
    const float overshoot   = pastBack ? ( currentDistance_ - len ) : -currentDistance_;

    if ( TryContinueToConnected(rails, rail, connected, connToFront, overshoot)
        || TryJoinNearbyBody(rails, rail, edge) ) {
        transitioned = true;
        return false;
    }
    if ( rail.groundType == SplineRail::GroundType::Gap && kFallOffEdges ) {
        DetachToAir(rail, edge); // Gap レール：明示的に「落ちてよい端」（キーを離した瞬間でも飛び出せる）
        return true;
    }
    currentDistance_ = edge; // 未接続の端 → クランプ（落下は穴/Gap だけ）
    return false;
}

// 連結している端へ地続きで持ち越す。
bool Player::TryContinueToConnected(const std::vector<SplineRail>& rails, const SplineRail& rail,
                                    int connectedIndex, bool enterFront, float overshoot){
    if ( connectedIndex < 0 || connectedIndex >= ( int ) rails.size() ) return false;
    const SplineRail& next = rails[connectedIndex];
    // 動くレールへ/からは静的連結しない（rest 位置基準なので animOffset 分ワープする）。
    // 動くレールは TryJoinNearbyBody の「今の位置」での動的ドッキングに任せる。
    if ( next.HasMotion() || rail.HasMotion() ) return false;
    if ( next.IsRideBlocked() ) return false; // まだ出現していない道へは渡れない
    // 型が違うレールへ渡った場合は switchCooldown_ で即乗り換えを防ぐ
    if ( next.type != rail.type ) { switchCooldown_ = 0.25f; }
    const float nextLength = next.GetLength();
    currentRailIndex_ = connectedIndex;
    currentDistance_  = std::clamp(enterFront ? overshoot : ( nextLength - overshoot ), 0.0f, nextLength);
    dsSign_ = enterFront ? 1.0f : -1.0f; // 持ち越し後も同じ物理方向へ
    return true;
}

// 端のすぐ前にある別レール本体へ合流（動的ドッキング）。
//   地上なら一旦停止（ジャンクション）。ジャンプ中は高さを保ったまま乗り移る。
bool Player::TryJoinNearbyBody(const std::vector<SplineRail>& rails, const SplineRail& rail, float edgeDistance){
    const Vector3 edgePos = rail.GetPositionByDistance(edgeDistance);
    // 進行方向（端から出て行く向き）。「前方」にあるレールへだけ合流する
    const Vector3 tangent    = rail.GetTangentByDistance(edgeDistance);
    const Vector3 forwardDir = HorizDir(tangent.x * dsSign_, tangent.z * dsSign_);

    PlayerRailQuery::Spot spot;
    if ( !PlayerRailQuery::FindJoin(rails, currentRailIndex_, edgePos, forwardDir, spot) ) return false;

    // ジャンプ中：レール間の高低差を高さに反映し、見た目の高さを維持
    if ( !jump_.grounded ) {
        jump_.height += ( edgePos.y - spot.pos.y );
        if ( jump_.height < 0.0f ) {
            jump_.height = 0.0f; jump_.velocity = 0.0f; jump_.grounded = true; jump_.flutterTime = 0.0f;
        }
    }

    currentRailIndex_ = spot.rail;
    currentDistance_  = spot.dist;
    // 勢いの持ち越し：今の進行方向を相手レールの接線に射影して進行符号を決め、
    //   止まらずに走り抜けられるようにする（溶接持ち越しと同じ操作感に揃える）。
    //   ほぼ直角に突き当たるT字だけは、どちらへ進むか曖昧なので一旦停止して入力を待つ
    const float kCarryDot = 0.3f; // これ未満＝ほぼ直角の合流とみなす
    const Vector3 newTangent = rails[spot.rail].GetTangentByDistance(spot.dist);
    const Vector3 newDir     = HorizDir(newTangent.x, newTangent.z);
    const float carry = ( Length(forwardDir) > 1e-4f && Length(newDir) > 1e-4f )
        ? ( newDir.x * forwardDir.x + newDir.z * forwardDir.z ) : 0.0f;
    if ( std::abs(carry) >= kCarryDot ) {
        dsSign_     = ( carry >= 0.0f ) ? 1.0f : -1.0f;
        atJunction_ = false;
    } else {
        dsSign_     = 0.0f;
        atJunction_ = jump_.grounded; // 地上のみジャンクション停止。空中はそのまま着地を待つ
    }
    switchCooldown_ = 0.15f;
    return true;
}

// 乗り換えキー：T字路の途中分岐（同じタイプ同士でも分岐できる）を優先し、
//   無ければ押した方向に伸びている別タイプの近接レール（交差点近くだけ）へ乗り換える
bool Player::TrySwitchOrBranch(const std::vector<SplineRail>& rails, bool horizontalRail, int switchDir){
    PlayerRailQuery::Spot spot;
    const bool found =
        PlayerRailQuery::FindBranch(rails, currentRailIndex_, currentDistance_, switchDir, spot)
        || PlayerRailQuery::FindSwitch(rails, currentRailIndex_, FootOnRail(rails), horizontalRail, switchDir, spot);
    if ( !found ) return false;

    const float length = rails[spot.rail].GetLength();
    const float margin = ( std::min )( 0.15f, length * 0.25f ); // 端ちょうどに乗らないよう少し内側へ
    TransferTo(rails, spot.rail, std::clamp(spot.dist, margin, length - margin));
    switchCooldown_ = 0.25f;
    dsSign_         = 0.0f; // 新しいレールでは次の入力で進行方向を決め直す
    return true;
}

void Player::TransferTo(const std::vector<SplineRail>& rails, int rail, float dist){
    const float oldFootY = FootOnRail(rails).y;
    currentRailIndex_ = rail;
    currentDistance_  = dist;
    // 空中で乗り移った時は、見た目のワールドYが飛ばないよう高さを補正
    if ( !jump_.grounded ) { jump_.height += oldFootY - FootOnRail(rails).y; }
}

// ジャンプ＋ふんばり＋着地。穴の上で降りてきて空中状態になったら true（呼び出し側は return）。
bool Player::UpdateJumpAndLand(const SplineRail& rail, const std::vector<SplineRail>& rails, float dt){
    jump_.UpdateVelocity(input_.JumpPressed(), input_.JumpHeld(), movementLocked_, dt);

    float prevFootY = jump_.height; // 貫通防止：移動前の足の高さ（着地の掃引判定に使う）
    jump_.height += jump_.velocity * dt;

    // 頭上のブロック：上昇中に頭をぶつけたら上昇を止める（マリオ風）
    if ( jump_.velocity > 0.0f
        && blockContact_.ClampToCeiling(currentRailIndex_, currentDistance_, jump_.height) ) {
        jump_.velocity = 0.0f;
    }
    // ブロック内部に埋まった場合（空中からの着地・リスポーン・乗り換え等）は上面へ押し出して復帰する
    if ( blockContact_.PushOutOfBlocks(currentRailIndex_, currentDistance_, jump_.height) ) {
        prevFootY      = jump_.height;
        jump_.velocity = 0.0f;
    }

    // 足元の支持面：ブロックの上面 or レール面(0)。
    //   落下が速い時に1フレームで上面を飛び越さないよう、移動前の足の高さも見て掃引する
    const float groundHeight = blockContact_.GroundHeight(currentRailIndex_, currentDistance_,
                                                          ( std::max )( prevFootY, jump_.height ));

    // 上昇中でも、足が乗れる上面より下にある（段差の許容内で上面の真上に来た）なら上面まで持ち上げる。
    //   着地の判定は下降中だけなので、ふんばりでゆっくり上がっている間は、段差の許容（0.3m）ぶん
    //   ブロックにめり込んだまま中を進んでいた（階段を登る途中で長押しジャンプすると埋まる）
    if ( jump_.velocity > 0.0f && jump_.height < groundHeight ) { jump_.height = groundHeight; }

    // 接地中に足場が下がった時：
    //   小さな下り（斜面を歩いて降りる・小段差）は足を地面に吸い付けたまま歩く。
    //   大きく下がった（ブロックの端から歩き出た）時だけ落下を開始する
    if ( jump_.grounded && groundHeight < jump_.height - 0.01f ) {
        if ( jump_.height - groundHeight <= 0.35f ) {
            jump_.height = groundHeight; // 斜面・小段差はスナップして接地を維持
        } else {
            jump_.grounded = false;
            jump_.velocity = 0.0f;
        }
    }

    if ( jump_.height > groundHeight || jump_.velocity > 0.0f ) return false;

    if ( groundHeight <= 0.0f && PlayerRailQuery::IsOverHole(rails, FootOnRail(rails)) ) {
        // 穴の上で降りてきた（ブロックにも乗っていない）→ レールを離れて自由落下
        EnterAir(rail.GetPositionByDistance(currentDistance_), rail.GetTangentByDistance(currentDistance_), jump_.velocity, 0.15f);
        return true;
    }
    // 地面あり＆降りてきた → 着地（ブロックの上面 or レール面）
    const float landVelocity = jump_.velocity; // 着地の瞬間の落下速度（ジャンプ台の判定に使う）
    jump_.Land(groundHeight);

    // ジャンプ台ブロック：勢いよく飛び乗ったら大きく跳ね返る（歩いて乗っただけでは跳ねない）
    if ( landVelocity < -4.0f && blockContact_.IsOnSpring(currentRailIndex_, currentDistance_, jump_.height) ) {
        jump_.Launch(jump_.power * 1.35f); // 通常ジャンプより高く跳ぶ（跳ねた後もふんばり可能）
    }
    return false;
}

// 最終的な座標・向きを確定する。乗り移りの瞬間移動は posSmooth_ で滑らかに繋ぐ。
void Player::FinalizePosition(const SplineRail& rail, const Vector3& worldBefore, bool transitioned, float dt){
    Vector3 basePos = rail.GetPositionByDistance(currentDistance_);
    basePos.y += jump_.height;

    // 乗り移りでレール座標が飛んだら、その差分を平滑化に回す（動くレールでもワープしない）
    if ( transitioned ) {
        Vector3 jump = { worldBefore.x - basePos.x, worldBefore.y - basePos.y, worldBefore.z - basePos.z };
        if ( Length(jump) < 3.0f ) posSmooth_ = jump; // 大ワープ(リスポーン等)は補間しない
    }
    const float smoothingFactor = ( std::min )( 14.0f * dt, 1.0f ); // 約0.15秒で 0 へ減衰
    posSmooth_.x -= posSmooth_.x * smoothingFactor;
    posSmooth_.y -= posSmooth_.y * smoothingFactor;
    posSmooth_.z -= posSmooth_.z * smoothingFactor;
    position_ = { basePos.x + posSmooth_.x, basePos.y + posSmooth_.y, basePos.z + posSmooth_.z };

    if ( position_.y < kKillY ) { RespawnAfterFall(); return; }

    facing_.UpdateOnRail(rail.GetTangentByDistance(currentDistance_), dsSign_, dt, rotation_.y);
    rotation_.x = 0.0f;
    rotation_.z = 0.0f;
}

// =====================================================================
//  空中状態：レールから離れて自由落下。
//   ・下降中に下へレールがあれば着地して復帰
//   ・kKillY より下に落ちたらスタートへリスポーン
// =====================================================================
void Player::EnterAir(const Vector3& startPos, const Vector3& tangent, float upVelocity, float landCooldown){
    inAir_       = true;
    position_    = startPos;
    airVelocity_ = { tangent.x * dsSign_ * moveSpeed_, upVelocity, tangent.z * dsSign_ * moveSpeed_ };
    airDir_      = HorizDir(tangent.x * dsSign_, tangent.z * dsSign_);
    if ( Length(airDir_) < 1e-4f ) {
        // 合流直後などで進行符号が0のまま落ちると airDir_ がゼロになり、
        // 空中の前後操作が全て効かなくなる（操作不能で落ちる）。向こうとしている方向で代用する
        airDir_ = { std::sin(facing_.TargetYaw()), 0.0f, std::cos(facing_.TargetYaw()) };
    }
    jump_.height   = 0.0f;
    jump_.velocity = 0.0f;
    jump_.grounded = false;
    if ( jump_.flutterTime <= 0.0f ) { jump_.flutterTime = PlayerJump::kFlutterTime; } // 落下中もふんばり可能
    airLandCooldown_ = landCooldown;
    airFromRail_     = currentRailIndex_; // この間は元レールへの即再着地を抑止
}

void Player::DetachToAir(const SplineRail& rail, float edgeDistance){
    Vector3 edgePos = rail.GetPositionByDistance(edgeDistance);
    Vector3 tangent = rail.GetTangentByDistance(edgeDistance);
    EnterAir({ edgePos.x, edgePos.y + jump_.height, edgePos.z }, tangent, jump_.velocity, 0.25f);
    currentDistance_ = edgeDistance;
}

void Player::UpdateAir(const std::vector<SplineRail>& rails, float dt){
    // ---- 空中の軌道修正（進行方向の前後だけ）----
    //   空中に出た時の進行方向(airDir_)に沿った前後だけ速度を増減できる。
    //   横方向の自由移動は不可（落下中に WASD で自由に動き回れないようにする）。
    //   airDir_ が0（真下に落下）の時は水平入力を受け付けない＝その場でまっすぐ落ちる
    if ( Length(airDir_) > 1e-4f ) {
        const float forwardInput = input_.ReadAirForward(airDir_); // 進行方向成分(+前/-後)
        const float kAccelFwd = 12.0f;            // 前後の加速
        const float kMaxFwd   = moveSpeed_ * 0.9f; // 前後の最高速度
        airVelocity_.x += airDir_.x * forwardInput * kAccelFwd * dt;
        airVelocity_.z += airDir_.z * forwardInput * kAccelFwd * dt;
        // 速度を進行方向(±)に分解してクランプ。横ブレ分は捨てる＝軌道は直線のまま
        float forwardSpeed = std::clamp(airVelocity_.x * airDir_.x + airVelocity_.z * airDir_.z, -kMaxFwd, kMaxFwd);
        airVelocity_.x = airDir_.x * forwardSpeed;
        airVelocity_.z = airDir_.z * forwardSpeed;
    }

    // ふんばり（空中でも SPACE 長押しで滞空できる。回数での減衰は無し）
    PlayerJump::StepFlutterOrGravity(airVelocity_.y, jump_.flutterTime, PlayerJump::kFlutterTarget,
                                     jump_.gravity, input_.JumpHeld(), dt);

    // 移動前の位置を覚えておく：着地はレール面を上→下に通過した瞬間に判定
    const Vector3 prevPos = position_;
    const float prevY = position_.y;
    position_.x += airVelocity_.x * dt;
    position_.y += airVelocity_.y * dt;
    position_.z += airVelocity_.z * dt;
    facing_.FaceVelocity(airVelocity_, rotation_.y); // 進行方向を向く

    // 飛び出した直後の "元レール" 再着地を抑止する猶予を減らす
    if ( airLandCooldown_ > 0.0f ) { airLandCooldown_ -= dt; }

    // レールの外でも、真下のレールに置かれたブロックとは当たる（側面・天井・上面への着地）
    if ( CollideAirWithBlocks(rails, prevPos) ) return;

    // 下降中だけ着地判定（上昇中にレールへ吸い付かないように）。
    //   飛び出した直後だけ元のレールへの再着地を抑止（端で跳ね返らない）。
    //   別のレールへは猶予中でも着地できる（同じ高さの渡りを取りこぼさない）
    PlayerRailQuery::Spot landing;
    const int ignoreRail = ( airLandCooldown_ > 0.0f ) ? airFromRail_ : -1;
    if ( airVelocity_.y <= 0.0f && PlayerRailQuery::FindLanding(rails, position_, prevY, ignoreRail, landing) ) {
        LandFromAir(rails, landing.rail, landing.dist, 0.0f);
        return;
    }

    if ( position_.y < kKillY ) { RespawnAfterFall(); }
}

// 空中（レールの外）でのブロックとの当たり。真下のレールを基準に、レール空間の判定をそのまま使う。
//   以前は空中の間はブロックを一切見ていなかったため、穴に落ちる途中で縁のブロックの側面を
//   通り抜けたり、上から落ちてきてブロックを素通りしてレール面から押し上げられたりしていた。
//   ブロックの上面に降りたら true（そのレールに乗って着地済み）
bool Player::CollideAirWithBlocks(const std::vector<SplineRail>& rails, const Vector3& prevPos){
    PlayerRailQuery::Spot below;
    if ( !PlayerRailQuery::FindRailBelow(rails, position_, below) ) return false;
    const SplineRail& rail = rails[below.rail];
    // 最寄り点探しはレールの距離テーブルの刻み（数cm）でしか求まらないので、接線方向へ1回寄せて正確にする
    //   （ずれたままだと壁の手前で止めても数cm食い込んで見える）
    auto preciseDist = [&](const Vector3& pos, float roughDist){
        Vector3 railPos = rail.GetPositionByDistance(roughDist);
        Vector3 tangent = rail.GetTangentByDistance(roughDist);
        float along = ( pos.x - railPos.x ) * tangent.x + ( pos.y - railPos.y ) * tangent.y + ( pos.z - railPos.z ) * tangent.z;
        return std::clamp(roughDist + along, 0.0f, rail.GetLength());
    };
    const float prevDist = preciseDist(prevPos, rail.GetClosestDistance(prevPos));

    // 横：ブロックの側面は通り抜けない（当たったら面の手前で止め、前後の勢いを消す）
    float dist = preciseDist(position_, below.dist);
    float footY = position_.y - rail.GetPositionByDistance(dist).y;
    const float resolved = blockContact_.ResolveWalk(below.rail, prevDist, dist, footY, false);
    if ( resolved != dist ) {
        Vector3 wallPos = rail.GetPositionByDistance(resolved);
        position_.x = wallPos.x;
        position_.z = wallPos.z;
        airVelocity_.x = 0.0f;
        airVelocity_.z = 0.0f;
        dist  = resolved;
        footY = position_.y - wallPos.y;
    }
    const float railY = position_.y - footY; // このレール上の dist の高さ

    // 上：上昇中に頭をぶつけたら止める
    if ( airVelocity_.y > 0.0f && blockContact_.ClampToCeiling(below.rail, dist, footY) ) {
        position_.y = railY + footY;
        airVelocity_.y = 0.0f;
    }

    // 下：降りてきてブロックの上面に届いたら、そのレールのブロックの上に着地する。
    //   飛び出した直後の元のレールには乗らない（端で跳ね返らない。レール面への着地と同じ猶予）
    if ( airVelocity_.y > 0.0f ) return false;
    if ( airLandCooldown_ > 0.0f && below.rail == airFromRail_ ) return false;
    const float prevFootY = prevPos.y - railY;
    const float ground = blockContact_.GroundHeight(below.rail, dist, ( std::max )( prevFootY, footY ));
    if ( ground <= 0.0f || footY > ground ) return false;
    LandFromAir(rails, below.rail, dist, ground);
    return true;
}

// 空中からレールの dist へ着地する（footHeight=レール面からの足の高さ。ブロックの上なら上面）
void Player::LandFromAir(const std::vector<SplineRail>& rails, int rail, float dist, float footHeight){
    const Vector3 railPos = rails[rail].GetPositionByDistance(dist);
    const Vector3 landPos = { railPos.x, railPos.y + footHeight, railPos.z };
    // 水平のズレは平滑化に回して見た目を滑らかに
    Vector3 jump = { position_.x - landPos.x, 0.0f, position_.z - landPos.z };
    if ( Length(jump) < 3.0f ) posSmooth_ = jump;
    inAir_ = false;
    currentRailIndex_ = rail;
    currentDistance_  = dist;
    position_ = landPos;
    facing_.SetGroundTangent(rails[rail].GetTangentByDistance(dist)); // 着地したレールの坂（このフレームから使われる）
    // ふんばりの回数は持ち越す（次のジャンプかレール上の着地でリセット。以前からの挙動）
    jump_.height      = footHeight;
    jump_.velocity    = 0.0f;
    jump_.grounded    = true;
    jump_.flutterTime = 0.0f;
    airVelocity_    = { 0.0f, 0.0f, 0.0f };
    dsSign_         = 0.0f; // 次の入力で進行方向を決め直す
    switchCooldown_ = 0.1f;
}

void Player::RespawnAfterFall(){
    Initialize();
    fellRespawned_ = true; // アイリスワイプ演出用（シーンが Consume する）
}

// 敵にぶつかった：敵と反対側へ小さく跳ねながら弾かれる
void Player::Knockback(const Vector3& awayDir){
    knock_.Start(awayDir, rotation_.y);
    dsSign_ = 0.0f; // 次の入力で進行方向を決め直す
    const Vector3& knockDir = knock_.Direction();
    if ( inAir_ ) {
        airVelocity_ = { knockDir.x * PlayerKnockback::kAirSpeed, PlayerKnockback::kHopVelocity,
                         knockDir.z * PlayerKnockback::kAirSpeed };
        airDir_ = knockDir;
    } else {
        jump_.Launch(PlayerKnockback::kHopVelocity);
    }
}

// 敵を踏みつけた：上へ跳ね返る（ふんばり滞空のタイマーも補充）
void Player::Bounce(){
    if ( inAir_ ) {
        airVelocity_.y = jump_.power; // 空中自由落下状態：空中用のY速度を直接上向きに
    } else {
        jump_.Launch(jump_.power);
    }
    jump_.flutterTime = PlayerJump::kFlutterTime;
}
