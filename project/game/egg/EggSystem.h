#pragma once
#include "game/egg/Egg.h"
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <unordered_map>

namespace KazumimiEngine { class Obj3d; }            // トレイル煙パフ用（実体で確実に表示する）
namespace KazumimiEngine { class SDFVolumeObject; }  // 産卵エロージョン演出用（SDFの卵が芯から育つ）
struct ID3D12GraphicsCommandList;

// =====================================================================
//  EggSystem：ヨッシーの卵を管理する（飲み込みで生成 → 後ろに整列・追従 → 投擲 → 割れる）。
//   ・各卵は自分の状態(Held/Flying/Broken)を持つ → ここは「並び順」と「全体の管理」を担当。
//   ・卵はヨッシー追従かつ敵に当たるので、両方を知るシーンが所有して使う。
// =====================================================================
class EggSystem {
public:
    static const int kMaxEggs = 6; // 同時に持てる卵の最大数（お腹の容量）
    // 後ろに並ぶ卵の中心の高さ（足元から）。卵の半分の高さ（モデル 1.3m×0.7 の半分＝0.455m）より少し上にして、
    //   地面にめり込まないようにする（前は 0.3 で、卵の下が 15cm ほど地面に沈んでいた）
    static constexpr float kHeldEggHeight = 0.47f;

    EggSystem() = default;
    ~EggSystem(); // unique_ptr<Obj3d> を含むため cpp 側で定義

    void Initialize();   // 全卵をクリア（プレイ開始/リセット時）

    // 毎フレーム更新。保持中の卵に「後ろのスロット」を割り当てて整列・追従させ、各卵の状態を進める。
    //   playerPos : プレイヤー位置 / facing : プレイヤーの向き(水平単位ベクトル)
    void Update(const KazumimiEngine::Vector3& playerPos, const KazumimiEngine::Vector3& facing, float dt);
    void Draw() const;

    // 敵を飲み込んだ → お腹(stomach)に1匹ためる（まだ卵にはしない）。
    //   お腹が一杯(kMaxEggs)なら古い分は増えない。
    void AddToBelly();
    int  BellyCount() const{ return stomach_; }

    // しゃがみ等で「産む」：お腹が1以上なら1匹を卵(Held)にして birthPos に生む。産んだら true。
    //   持っている卵が満杯なら一番古い卵を割って捨ててから生む（常に産める）。
    bool LayEgg(const KazumimiEngine::Vector3& birthPos);
    // 産卵演出中か（プレイヤーの EggLay アニメ切り替えに使う）
    bool IsBirthActive() const{ return birthTimer_ >= 0.0f; }

    // 構え中フラグ：後ろに並ぶ保持卵の先頭1個を描かない（手に持っている扱い。キャンセルで自動復帰）
    void SetAimHolding(bool holding){ aimHolding_ = holding; }
    // 後ろの卵を地面に置くための情報（Update の前に呼ぶ）：恐竜が着地しているか・真下に地面があるか・その高さ
    void SetFollowGround(bool playerGrounded, bool hasGround, float groundY){
        followGrounded_ = playerGrounded; followHasGround_ = hasGround; followGroundY_ = groundY;
    }
    // カメラから見て奥の向き（水平・長さ1）。重ねた卵を少しずつ奥へずらす・後ろに場所が無い時に奥へよける向き
    void SetFollowAway(const KazumimiEngine::Vector3& away){ followAway_ = away; }
    // index 番目（0＝一番近い）の卵が並ぶ所。卵を産む場所にも使う。
    //   動いている時は足あとを一列に付いてくる・止まったら恐竜の後ろにまとまる（並び方が変わると近い卵から 1 個ずつ動く）
    KazumimiEngine::Vector3 FollowSlot(int index) const;

    // 保持中の卵を1個、指定方向へ投げる（Held → Flying）。speed=初速。投げられたら true。
    bool TryThrow(const KazumimiEngine::Vector3& playerPos, const KazumimiEngine::Vector3& dir, float speed = 12.0f);
    // TryThrow と同じ。投げた卵を返す（無ければ nullptr）。奥投げコンボ（EggCombo）が卵を追うのに使う
    class Egg* ThrowEgg(const KazumimiEngine::Vector3& playerPos, const KazumimiEngine::Vector3& dir, float speed);
    // その卵がまだあるか（割れて消えた卵を指したままにしない）
    bool Contains(const class Egg* egg) const;

    // お腹の敵を1匹、指定方向へ吐き出す（ヨッシーの吐き出し攻撃）。吐けたら true。
    //   from=口元の位置 / dir=プレイヤーの向いている水平方向
    bool SpitOut(const KazumimiEngine::Vector3& from, const KazumimiEngine::Vector3& dir, float speed = 13.0f);

    // 当たり判定の問い合わせ：球が from→to へ動いた間に何かへ触れたか。
    //   1フレームに動いた区間まるごとで判定するので、速い弾でも薄い物をすり抜けない
    using SweepHit = std::function<bool(const KazumimiEngine::Vector3& from, const KazumimiEngine::Vector3& to, float radius)>;
    // 壁・ブロック・道への問い合わせ。当たったら outHitPos に当たる直前の位置を返す
    using ObstacleQuery = std::function<bool(const KazumimiEngine::Vector3& from, const KazumimiEngine::Vector3& to, float radius, KazumimiEngine::Vector3& outHitPos)>;

    // 卵と吐き出し弾が壁・ブロック・道に当たるようにする（シーンが一度渡す。
    //   ブロックやレールを知っているのはシーンなので、判定の中身は外から貰う）。
    //   未設定の間は従来どおり「高さ0の床」だけで止まる
    void SetObstacleQuery(ObstacleQuery query){ obstacleQuery_ = std::move(query); }

    // 飛行中の吐き出し弾を onHit(動く前の位置, 今の位置, 半径) で判定し、true が返ったら弾を消す。
    //   （卵の ResolveHits と同じ流儀。敵側の処理は CombatSystem が行う）
    void ResolveSpitHits(const SweepHit& onHit);

    // 飛行中の卵それぞれを onHit(動く前の位置, 今の位置, 半径) で判定し、true が返ったら卵を割る。
    //   敵を知るのはシーンなので、当たり判定の中身はシーンから渡す（割れ演出は Update が拾う）。
    void ResolveHits(const SweepHit& onHit);

    // 飛んでいる卵・吐き出し弾の当たり判定の形をワイヤーで描く（デバッグ表示用）
    void DrawHitShapes(const KazumimiEngine::Vector4& color) const;

    // --- イベント別の演出（実体パフ。踏みつけのリングとは別物にして使い回しに見せない）---
    void SpawnSwallowFx(const KazumimiEngine::Vector3& pos); // 飲み込み：緑がふわっと上へ吸い込まれる
    void SpawnLayFx(const KazumimiEngine::Vector3& pos);     // 産卵：白＆緑がぽわっと広がる（生まれた感）
    void SpawnHitFx(const KazumimiEngine::Vector3& pos);     // 命中：黄＆オレンジが鋭く飛び散る（衝撃）
    // 卵が割れた：殻のかけら（白と緑＝卵の模様の色）が、飛んで来た方へはね返って落ちる。flightDir＝飛んでいた向き（0 なら真上へ）
    void SpawnBreakFx(const KazumimiEngine::Vector3& pos, const KazumimiEngine::Vector3& flightDir);

    int HeldCount() const;    // 保持中の卵の数
    int TotalCount() const { return ( int ) eggs_.size(); }

    // --- 産卵エロージョン演出（SDFの卵が芯から育ち、育ちきったら実体メッシュへ交代）---
    //   セットアップ：egg.sdf3d を読み込む（シーンの LoadResources から呼ぶ。
    //   読み込み失敗時は演出なし＝従来のポン出しに自動フォールバック）
    void InitializeBirthFx(ID3D12GraphicsCommandList* commandList);
    //   描画：専用PSOへ切り替えるため、シーンMRTパスの最後（他のObj3dの後）で呼ぶこと
    void DrawBirthFx(ID3D12GraphicsCommandList* commandList);
    // 初回描画の前倒し：数フレームだけ、画面の外で SDF の卵を描く（最初に産んだ瞬間に止まらないように）
    void WarmupBirthFx();

private:
    std::vector<std::unique_ptr<Egg>> eggs_;
    int stomach_ = 0; // お腹にためた敵の数（産むと卵になる）
    ObstacleQuery obstacleQuery_; // 壁・ブロック・道への問い合わせ（シーンが渡す）

    // --- 産卵エロージョン演出の状態 ---
    std::unique_ptr<KazumimiEngine::SDFVolumeObject> birthFx_; // SDFの卵（1個を使い回す）
    bool birthLookReady_ = false;              // SDF の卵に、実体の卵と同じ見た目（テクスチャ・光）を設定したか
    Egg*  birthEgg_   = nullptr;               // 演出対象（eggs_ 内に存在するか毎フレーム検証）
    float birthTimer_ = -1.0f;                 // 経過秒。負=演出していない
    int   birthWarmupDraws_ = 0;               // WarmupBirthFx：画面の外で描く残りフレーム
    bool  aimHolding_ = false;                 // 構え中（列の先頭の卵を手に持っている扱いで非表示）
    // 後ろの卵は、恐竜が通った所（足あと）を少し遅れて付いてくる。
    //   前は「今の向きの真後ろ・恐竜と同じ高さ」に並べていたので、壁を背にした時・階段を降りる時に卵がブロックの中に入っていた
    struct TrailPoint { KazumimiEngine::Vector3 pos; float groundY; bool hasGround; };
    std::vector<TrailPoint> trail_;            // 新しい順（[0] が一番新しい）
    KazumimiEngine::Vector3 trailHead_ { 0.0f, 0.0f, 0.0f };   // 今の恐竜の足元
    KazumimiEngine::Vector3 trailFacing_ { 0.0f, 0.0f, 1.0f };
    bool  trailReady_ = false;
    bool  followGrounded_ = false, followHasGround_ = false;
    float followGroundY_ = 0.0f;
    void UpdateTrail(const KazumimiEngine::Vector3& playerPos, const KazumimiEngine::Vector3& facing, float dt);
    void TrailPointAt(float along, KazumimiEngine::Vector3& outPos, KazumimiEngine::Vector3& outDir, TrailPoint* outGround) const;
    KazumimiEngine::Vector3 followAway_ { 0.0f, 0.0f, 1.0f };  // カメラから見て奥の向き
    float FlatLengthBehind() const;
    bool  StandingPointBehind(float fromAlong, float maxAlong, KazumimiEngine::Vector3& outPos) const;
    bool  followStopped_ = true;               // 恐竜が止まっている（卵は後ろにまとまる）
    float stillTime_ = 0.0f;                   // 横に動いていない秒
    float modeTime_ = 0.0f;                    // 並び方（一列／まとまる）が変わってからの秒
    float lastGroundFootY_ = 0.0f;             // 恐竜が最後に立っていた足の高さ
    struct FootSample { float time; float y; };
    std::vector<FootSample> footHistory_;      // 恐竜の足の高さ（古い順。kHistoryTime 秒ぶん）
    float clock_ = 0.0f;
    float walkedTotal_ = 0.0f;                 // 恐竜が歩いた道のり（卵がぴょこぴょこ跳ねる拍子）
    float FootYAgo(float delay) const;

    // --- 飛行中の卵が残す「煙パフ」---
    //   加算パーティクルは明るい背景で見えないので、実体(Obj3d)の小球で表示する（縮んで消える）。
    struct TrailPuff {
        std::unique_ptr<KazumimiEngine::Obj3d> obj;
        std::string model;                     // プール返却先のキー（モデル名）
        KazumimiEngine::Vector3 pos { 0.0f, 0.0f, 0.0f };
        KazumimiEngine::Vector3 vel { 0.0f, 0.0f, 0.0f };
        KazumimiEngine::Vector3 rot { 0.0f, 0.0f, 0.0f };      // 姿勢（欠片が舞うように回す）
        KazumimiEngine::Vector3 rotSpeed { 0.0f, 0.0f, 0.0f };
        float   life    = 0.0f;
        float   maxLife = 0.4f;
        float   baseScale = 0.3f;
        float   gravity = 3.0f;                // 落ちる速さ（煙はふわっと・殻はすとんと）
        bool    lateShrink = false;            // true＝終わりの方でだけ縮む（殻。最初から縮むと、かけらに見えない）
    };
    std::vector<TrailPuff> puffs_;

    // 見た目（Obj3d）のプール（モデル名ごと。オブジェクトプール）。けむりの粒・殻の欠片・卵・吐き出す敵ボールで使う。
    //   1 個ごとの生成/破棄は GPU リソースの確保を伴い、敵を倒した瞬間などの一斉発生でカクつく原因になるので、
    //   使い終わった物は捨てずにここへ返して、次に出す時に使い回す
    std::unordered_map<std::string, std::vector<std::unique_ptr<KazumimiEngine::Obj3d>>> visualPool_;
    static constexpr size_t kPoolLimit = 32; // モデルごとに取っておく数の上限
    // プールから 1 個出す（空なら作る。モデルが未登録なら nullptr）
    std::unique_ptr<KazumimiEngine::Obj3d> AcquireVisual(const std::string& modelName);
    // 使い終わった 1 個をプールへ返す（上限を超える分は捨てる）
    void ReleaseVisual(const std::string& modelName, std::unique_ptr<KazumimiEngine::Obj3d> visual);
    float trailTimer_ = 0.0f; // 一定間隔でトレイルを出すためのタイマー

    // --- 吐き出し弾（飲んだ敵をそのまま前方へ発射。モンスターボール柄の球）---
    struct SpitBall {
        std::unique_ptr<KazumimiEngine::Obj3d> obj;
        KazumimiEngine::Vector3 pos { 0.0f, 0.0f, 0.0f };
        KazumimiEngine::Vector3 prevPos { 0.0f, 0.0f, 0.0f }; // 直前の更新で動く前の位置（区間での当たり判定用）
        KazumimiEngine::Vector3 vel { 0.0f, 0.0f, 0.0f };
        float   life   = 2.0f;   // 保険の寿命(秒)
        float   radius = 0.45f;  // 当たり判定＆見た目の半径
        float   spin   = 0.0f;   // 転がり回転
        bool    dead   = false;
        bool    hitObstacle = false; // 壁/地面に当たって止まった（敵の判定を1回通してから消える）
    };
    std::vector<SpitBall> spits_;

public:
    // 見た目（Obj3d）の事前生成（シーンロード時に呼ぶ）。初回バーストで一斉に Create すると
    //   その瞬間だけ固まるので、ここでプールに count 個積んでおく（プールはモード切替でも生き続ける）
    void PrewarmVisualPool(const std::string& modelName, int count);

    // パフを1個出す（pos=位置, vel=初速, color=色, scale=大きさ, life=寿命秒, modelName=見た目）。
    //   modelName="fxSphere"（白い専用の粒。敵の"sphere"とは別物）の時だけ color でタイントする。
    //   "eggShell" 等の専用モデルは自前の色をそのまま見せる。
    //   ？ブロックのコイン演出など、外部のちょっとした粒エフェクトにも使える
    // 殻のかけらを 1 個（green＝緑のかけら）。重く落ちて、終わりの方でだけ縮む
    void SpawnShell(const KazumimiEngine::Vector3& pos, const KazumimiEngine::Vector3& vel, float scale, float life, bool green);
    void SpawnPuff(const KazumimiEngine::Vector3& pos, const KazumimiEngine::Vector3& vel, const KazumimiEngine::Vector4& color, float scale, float life,
                   const std::string& modelName = "fxSphere");
};
