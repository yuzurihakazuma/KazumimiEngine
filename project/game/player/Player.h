#pragma once
// =====================================================================
//  Player：レール上を「距離」で動くプレイヤー。レールのタイプで操作キーが変わる。
//   - 横レール(Horizontal) … A/D で移動、W/S で縦レールへ乗り換え
//   - 縦レール(Vertical)   … W/S で移動、A/D で横レールへ乗り換え
//   - 同じタイプの連結レールは地続きで持ち越し。違うタイプの境目は自動で
//     進まず、プレイヤーが乗り換えキーを押した時だけ移る（行くか選べる）。
//   - 穴・Gapレールの端からはレールを離れて空中（自由落下）になり、下のレールへ着地できる。
//
//  中身は役割ごとのクラスに分けてあり、Player はそれらを順番に呼ぶ：
//    PlayerInput（キー）/ PlayerJump（縦の動き）/ PlayerBlockContact（ブロックとの当たり）/
//    PlayerFacing（向き）/ PlayerKnockback（被弾）/ PlayerRailQuery（次に乗るレール探し）
// =====================================================================
#include "engine/math/struct.h"
#include "game/player/PlayerInput.h"
#include "game/player/PlayerJump.h"
#include "game/player/PlayerBlockContact.h"
#include "game/player/PlayerFacing.h"
#include "game/player/PlayerKnockback.h"

#include <vector>

class SplineRail;
class BlockSystem;

class Player{
public:
    void Initialize();
    // 複数のレール情報を受け取って、自動で移動や乗り換えを行う
    void Update(const std::vector<SplineRail>& allRails);

    const Vector3& GetPosition() const{ return position_; } // 足元
    const Vector3& GetRotation() const{ return rotation_; }
    void SetRotation(const Vector3& rot){ rotation_ = rot; }

    // 接地状態（空中判定、および踏みつけ判定に使用）
    bool IsGrounded() const{ return jump_.grounded; }
    // ふんばり中（足バタバタアニメの切り替え用）
    bool IsFluttering() const{ return jump_.fluttering; }
    // 今乗っているレール番号（「乗ったら動き出すレール」の発動判定に使用）
    int  GetCurrentRail() const{ return currentRailIndex_; }

    // 向いている方向に、足元の道の坂を足した向き（長さ1）。坂道で前へ物を出す時に使う
    //   （水平の向きのまま出すと、上り坂では出した直後に道へぶつかってしまう）
    Vector3 GetFacingAlongGround() const{ return facing_.FacingAlongGround(rotation_.y, inAir_); }

    // 踏みつけ成功時にプレイヤーを上へ跳ね上がらせる
    void Bounce();
    // 敵にぶつかった時：awayDir（敵→自分の水平方向）へ小さく跳ねながら弾かれる
    void Knockback(const Vector3& awayDir);
    bool IsInvincible() const{ return knock_.IsInvincible(); }

    // 卵を構えている間など、移動・ジャンプを止める（その場で待機）
    void SetMovementLocked(bool locked){ movementLocked_ = locked; }
    // ベロ動作中などに呼ぶ：移動はできるが振り向かない（見た目の向きを固定する）
    void SetTurnLocked(bool locked){ facing_.SetTurnLocked(locked); }
    // 向きの強制指定（卵の構え中、狙い方向を向かせる）。active=false で通常の移動向きに戻る
    void SetFaceOverride(bool active, float yaw){ facing_.SetOverride(active, yaw); }

    // --- ノードエディタの「→ ゲーム値」用：調整したい変数のポインタを公開 ---
    float* MoveSpeedPtr(){ return &moveSpeed_; }
    float* JumpPowerPtr(){ return &jump_.power; }

    // スタート地点（リスポーン先）。マップ設定から Sync 後にシーンが渡す
    void SetSpawn(int rail, float dist){ spawnRail_ = rail; spawnDist_ = dist; }
    // カメラの向き(yaw)。シーンが毎フレーム渡す（キー割り当てを画面基準に回すため）
    void SetCameraYaw(float yawRad){ input_.SetCameraYaw(yawRad); }
    // 自動操縦（実キーの代わりに入力を与える。タイトルの登場演出用）。active=false で実キーに戻る
    void SetAutoPilot(bool active, const PlayerInput::AutoPilot& pilot = {}){ input_.SetAutoPilot(active, pilot); }
    // ブロック（乗れる/ぶつかる）の当たり判定窓口。シーンが渡す（所有しない）
    void SetBlocks(BlockSystem* blocks){ blockContact_.SetBlocks(blocks); }

    // 落下死でリスポーンした瞬間か（取得するとフラグは消える。アイリスワイプ演出用）
    bool ConsumeFellRespawn(){ bool fell = fellRespawned_; fellRespawned_ = false; return fell; }
    // ここより下に落ちたらスタートへリスポーン（箱庭の机の天板があれば、その高さ＝机に落ちたらやり直し）
    void SetKillY(float killY){ killY_ = killY; }

private:
    // --- レールの上 ---
    // 今のレールを距離で進める（進行方向の記憶つき。ブロックの壁で止まる）
    void MoveAlongRail(const SplineRail& rail, bool horizontalRail, float moveInput, float dt);
    // ノックバック中はレールに沿って押し戻す。押し戻した向き（±1。ノックバック中でなければ0）を返す
    float PushBackAlongRail(const SplineRail& rail, float dt);
    // レール終端の処理（持ち越し/合流/落下/クランプ）。空中へ飛び出したら true（呼び出し側は return）
    bool HandleRailEnds(const std::vector<SplineRail>& rails, const SplineRail& rail, bool& transitioned);
    // 連結している端へ地続きで持ち越す（成功:true）
    bool TryContinueToConnected(const std::vector<SplineRail>& rails, const SplineRail& rail,
                                int connectedIndex, bool enterFront, float overshoot);
    // 端のすぐ前にある別レール本体へ合流（動的ドッキング。成功:true）
    bool TryJoinNearbyBody(const std::vector<SplineRail>& rails, const SplineRail& rail, float edgeDistance);
    // 乗り換えキー：T字路の途中分岐を優先 → 無ければ別タイプの近接レールへ（成功:true）
    bool TrySwitchOrBranch(const std::vector<SplineRail>& rails, bool horizontalRail, int switchDir);
    // 別のレールへ乗り移る。空中なら見た目のワールドYが飛ばないよう高さを補正する
    void TransferTo(const std::vector<SplineRail>& rails, int rail, float dist);
    // ジャンプ＋ふんばり＋着地。穴の上で降りてきて空中状態になったら true（呼び出し側は return）
    bool UpdateJumpAndLand(const SplineRail& rail, const std::vector<SplineRail>& rails, float dt);
    // 最終的な座標・向きを確定（乗り移りの見た目平滑化つき。落下死ならリスポーン）
    void FinalizePosition(const SplineRail& rail, const Vector3& worldBefore, bool transitioned, float dt);
    // 今のレール上の足元（高さを足す前のレール面の点）
    Vector3 FootOnRail(const std::vector<SplineRail>& rails) const;

    // --- 空中（レールの外） ---
    // レールを離れて空中(弾道)状態へ移行する。landCooldown=元のレールへすぐ再着地しない猶予
    void EnterAir(const Vector3& startPos, const Vector3& tangent, float upVelocity, float landCooldown);
    // 端から空中へ飛び出す（Gap レール用）。飛び出す端は今の高さを保つ
    void DetachToAir(const SplineRail& rail, float edgeDistance);
    // 自由落下・着地判定・落下死
    void UpdateAir(const std::vector<SplineRail>& rails, float dt);
    // 空中でも真下のレールのブロックと当たる（側面・天井・上面）。ブロックの上に着地したら true
    bool CollideAirWithBlocks(const std::vector<SplineRail>& rails, const Vector3& prevPos);
    // 空中からレールの dist へ着地する（footHeight=レール面からの足の高さ）
    void LandFromAir(const std::vector<SplineRail>& rails, int rail, float dist, float footHeight);
    // 落下死 → スタートへリスポーン
    void RespawnAfterFall();

private:
    // --- 役割ごとの部品 ---
    PlayerInput        input_;
    PlayerJump         jump_;
    PlayerBlockContact blockContact_;
    PlayerFacing       facing_;
    PlayerKnockback    knock_;

    Vector3 position_ { 0.0f, 0.0f, 0.0f };
    Vector3 rotation_ { 0.0f, 0.0f, 0.0f };
    float   moveSpeed_ = 5.0f;        // 移動速度 (m/s)

    // スタート地点（Initialize / リスポーンで使う）
    int   spawnRail_ = 0;
    float spawnDist_ = 0.0f;

    bool movementLocked_ = false; // true の間は移動・ジャンプ入力を無視（構え中など）
    float killY_ = -10.0f;
    bool fellRespawned_ = false;  // 落下死→リスポーンが起きた瞬間のフラグ（シーンが Consume して演出に使う）

    // ---- レール移動の主状態（これだけで位置が決まる）----
    float currentDistance_ = 0.0f;    // 現在レール上を進んだ距離 (m)
    int   currentRailIndex_ = 0;      // 現在乗っているレール番号
    // キーを「押した瞬間」だけワールド方向(横=X/縦=Z)から進行符号を決め、押しっぱなしの間は保持する。
    //   円状レールの頂点や急カーブで接線の軸成分が反転しても止まらず・逆走しないための仕組み
    float dsSign_ = 0.0f;         // 現在の進行符号(±1)。0=停止中
    float prevMoveInput_ = 0.0f;  // 前フレームの移動入力（押した瞬間の検出用）
    bool  atJunction_ = false;    // 別レールへ合流した直後の停止中か（押し直すまで動かない）
    float switchCooldown_ = 0.0f; // 乗り換えの連打防止タイマー (秒)

    // ---- 空中状態（レール外）：穴の飛び越え・落下用 ----
    bool    inAir_ = false;                     // レールから離れて空中にいるか
    Vector3 airVelocity_ { 0.0f, 0.0f, 0.0f };  // 空中の速度 (m/s)
    float   airLandCooldown_ = 0.0f;            // 飛び出した直後に "元のレール" へ即着地しない猶予 (秒)
    int     airFromRail_ = -1;                  // 飛び出した元のレール番号
    Vector3 airDir_ { 0.0f, 0.0f, 0.0f };       // 空中に出た時の進行方向(水平・単位)。前後だけ操作できる

    // ---- 乗り移り時の見た目補間（ワープ隠し）----
    //   別レールへドッキング/乗り換え/着地でレール座標が飛ぶと見た目が瞬間移動する。
    //   飛んだ差分をこのオフセットに入れ、毎フレーム0へ減衰させて滑らかに繋ぐ
    Vector3 posSmooth_ { 0.0f, 0.0f, 0.0f };
};
