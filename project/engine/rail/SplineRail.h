#pragma once
#include <vector>
#include "engine/rail/RailSkin.h"
#include "engine/math/struct.h"

namespace KazumimiEngine {

// =====================================================================
//  SplineRail：レール 1 本。
//   ノード（通る点）をつないだ曲線で、「始めからの距離」を渡すと、その場所の位置・進む向き・横の向きを答える。
//   プレイヤー・敵・ブロック・コイン・道の見た目は、全部「どのレールの、何 m の所か」で場所を決める。
//   ほかのレールとつながった端は、つなぎ目の向きをそろえて、折れずに通れるようにする。
//
//   持ち物は役割ごとのまとまりに分け、外からは決まった入口だけで触る（メンバ変数は公開しない）：
//     Nodes()  … 通る点            Config() … エディタで決める設定（タイプ・表示・道の見た目・穴 など）
//     Links()  … ほかのレールとのつながり（RailConnections が作る）
//     Motion() … 動くレールの設定と今の動き    Train() … 列車    Board() … 渡し板
//   読むだけの所は Config() などを使う。書き換える所（レールを作る RailField・つなぐ RailConnections・
//   動かす RailMotion）だけが EditConfig() などを使う（書き換える場所が、名前で見分けられる）
// =====================================================================
class SplineRail{
public:
    // ============================================================
    // レールのタイプ：このレール上での移動操作キーを決める
    //   Horizontal(横) … A/D で移動、W/S で縦レールへ乗り換え
    //   Vertical(縦)   … W/S で移動、A/D で横レールへ乗り換え
    // タイプはレール固定（毎フレーム接線から判定しないので操作がブレない）
    // ============================================================
    enum class RailType { Horizontal, Vertical };

    // ============================================================
    // レールの地面タイプ
    //   Safe     = 端でクランプ（落ちない安全レール）
    //   Gap      = 端から飛び出せる（ギャップジャンプ・動くレール向け）
    //   NoGround = そのレール上に地面がない（入ったら即落下＝穴）
    // ============================================================
    enum class GroundType { Safe = 0, Gap = 1, NoGround = 2 };

    // 列車の状態（駅で待つ → 発車を待つ → 走る）
    enum class TrainState { Wait = 0, Depart = 1, Run = 2 };

    // エディタで決める設定（マップのデータから RailField が入れる）
    struct ConfigData {
        RailType type = RailType::Horizontal;

        // 表示フラグ：false なら「見えないレール（連結用）」。
        //   プレイヤーは普通にこの上を距離で進む（レール移動らしく滑らかに見える）が、
        //   ゲーム中は緑線(マーカー)を描かない。別々のレールを繋ぐ橋として使う。
        bool visible = true;

        // 線のつなぎ方（0=スプライン / 1=直線）。
        //   スプライン … 置いた点を全部通りながら、なめらかな曲線でつなぐ（従来どおり）
        //   直線       … 点をそのまま直線でつなぐ（角でカクッと折れる。階段や直角コース用）
        //   ノードの位置は同じで「つなぎ方」だけが違う。エディタからレール単位で切替できる。
        int lineMode = 0;

        // 道生成モード（仕様書_残タスク §4）：0=自動で道を敷く / 1=道なし
        //   「なし」のレールは道メッシュ・ジャンクション・ジョイントの対象外になる
        //   （カメラ用レール・敵専用レール・演出用の見えないレール向け）
        int roadMode = 0;
        int endPlaza = 0; // 端の丸広場（bit0=始点 / bit1=終点。RoadMesh が円形の広場を敷く）
        RailSkin skin;    // 道の見た目（土台のみ/布のみ/布＋土台/見せない。布の幅・色など）

        GroundType groundType = GroundType::Safe;

        // ノード単位の「穴」指定（nodes と同じ並び・1=穴）。
        //   エディタで「ここを穴にする」と指定したノード付近の区間が穴になる。
        //   プレイヤーはその区間に来ると落下する（ジャンプで飛び越え可）。
        std::vector<int> nodeHole;

        // 片方向レール（0=両方向 / 1=正方向のみ(front→back) / 2=逆方向のみ(back→front)）
        int oneWay = 0;

        // このレール上での移動速度倍率（1=通常。2で加速レール、0.5で減速レール）
        float speedMul = 1.0f;
    };

    // 途中分岐 1 つ（RailConnections がロード時に 1 回だけ計算する）
    struct BranchPoint {
        float distance;      // このレール上の分岐距離
        int   targetRail;    // 乗り換え先レール番号
        float targetDist;    // 乗り換え先レール上の着地距離
        int   zSign;         // +1=W/Dキー(Z+方向), -1=S/Aキー(Z-方向)
    };

    // ほかのレールとのつながり（RailConnections が作る。実行時だけ・保存しない）
    struct LinkData {
        // ループ（円状レール）：front と back が同じ位置に溶接された閉じたレール。
        //   ループ中は距離が周回でラップし、端が無いので A/D(W/S) で回り続けられる。
        bool isLoop = false;

        // つなぎ目での向き。
        //   端どうしでつながった相手と共通の向き（長さ1。始点→終点へ進む向き）。端の区間の曲線を、端でこの向きになるように作る。
        //   無い時は自分の点を折り返して作るので、端はいつも最後の区間の向きのままになり、
        //   相手と向きが違うつなぎ目では、向きがその 1 点でカクッと折れていた（坂 45° → ゆるい坂 12° など）。
        //   入れると、2 本のレールが 1 本の曲線のようにつながる（つなぎ目で向きが同じになる）
        bool    hasFrontTangent = false;
        bool    hasBackTangent  = false;
        Vector3 frontTangent { 0.0f, 0.0f, 1.0f };
        Vector3 backTangent  { 0.0f, 0.0f, 1.0f };

        // 終端の接続情報（インデックスで管理・距離チェック不要）
        int  frontConnIndex = -1;      // front端の接続先レール番号 (-1=なし)
        bool frontConnToFront = true;  // true=接続先のfront, false=back
        int  backConnIndex = -1;       // back端の接続先レール番号
        bool backConnToFront = true;

        // 途中分岐情報
        std::vector<BranchPoint> branchPoints;
    };

    // 動くレール（ムービングプラットフォーム）の設定と、今の動き。
    //   motionAmp が非ゼロなら、時間に応じて sin 波でレール全体が平行移動する。
    //   形は変わらない剛体移動なので、距離テーブル・全長はそのまま使える。
    //   プレイヤーはレール上の距離から位置を毎フレーム計算するので自動で追従する。
    struct MotionData {
        Vector3 motionAmp { 0.0f, 0.0f, 0.0f };   // 振幅(m)：各軸 ±この量だけ往復する
        float   motionPeriod = 2.0f;              // 1往復にかかる秒数
        Vector3 animOffset { 0.0f, 0.0f, 0.0f };  // 現在のオフセット（RailMotion が毎フレーム更新）

        // 動きの波形と位相（エディタで選択）
        //   motionType: 0=サイン往復 / 1=端で一時停止する往復 / 2=円運動(cos,sin)
        //   motionPhase: 0〜1。複数レールの動きをずらす（0.5で半周期ずれ）
        //   motionType 3=ガイドレール追従：guideRail の経路に沿って一周し続ける（振幅は不使用）
        int   motionType  = 0;
        int   guideRail   = -1; // motionType=3 の追従先レール番号（-1=なし）
        float motionPhase = 0.0f;
        // ガイド追従の区間と動き方（ガイドの一部区間だけを滑らかに動ける）
        float guideStart = 0.0f;  // ガイド上の開始距離(m)
        float guideEnd   = -1.0f; // ガイド上の終了距離(m)。-1=ガイドの終点まで
        int   guideMode  = 0;     // 0=一周ループ / 1=往復（コサイン緩急で端が滑らか）/ 2=片道（到着で停車）
        float guideDwell = 0.0f;  // 停車時間(秒)：往復=両端で停まる / 片道=出発までの待ち

        // 動き出しのタイミング（0=最初から動く / 1=プレイヤーが乗ったら動き出す）と実行時状態
        int   motionTrigger = 0;
        float motionTime    = 0.0f;  // このレールの動きの経過時間（発動してから進む）
        bool  motionStarted = false; // 乗ったら動き出すレールが発動済みか（一度動いたら動き続ける）

        // 列車式の回転（ガイド追従で、足場の向きを経路のカーブに合わせて回す）
        //   回転も剛体変換なので距離テーブル・全長はそのまま使える（接線は位置差分から自動で回る）
        int     guideAlign = 0;              // 0=向きそのまま（平行移動のみ）/ 1=経路の向きに合わせて回る
        float   animYaw    = 0.0f;           // 現在のヨー回転（ラジアン。animPivot 中心にレール全体を回す）
        Vector3 animPivot { 0.0f, 0.0f, 0.0f }; // 回転の中心（基準位置でのレール中点。Syncで計測）
        float   restYaw    = 0.0f;           // 基準位置でのレールの向き（atan2(tan.x, tan.z)。Syncで計測）

        // 後から出現する道（指定レールに乗るとこの道が現れる）
        int   appearTrigger = -1;    // -1=最初からある / >=0 = そのレール番号に乗ったら出現
        bool  appeared      = true;  // 出現済みか（実行時状態。トリガー無しなら常にtrue）
        float appearAnim    = 1.0f;  // 出現アニメ（0=隠れている → 1=完全に出現。下からせり上がる）
    };

    // 列車（RailMotion の列車の動き。ガイド追従の足場だけ）の設定と、今の状態。
    //   駅＝ガイドの区間の両はし。駅で待つ → プレイヤーが乗ると trainDepart 秒で発車 → 次の駅で止まる。
    //   走っている間（Run）はプレイヤーが降りられない・だれも乗れない。
    //   ガイドのレールは isTrack（線路）になる：歩けない・つながない・道を敷かない
    struct TrainData {
        int   train       = 0;     // 1＝列車
        float trainDepart = 0.8f;  // 乗ってから発車までの秒
        float trainReturn = 4.0f;  // 降りてから元の駅へ戻るまでの秒（負＝戻らない）
        int   trainEngine = 1;     // 1＝先頭に機関車（見た目と駅の位置）
        float trainSpeed  = 5.0f;  // 走る速さ(m/秒)
        std::vector<float> trainSpeedMuls; // ガイドの点ごとの速さの倍率（足りない点は 1）
        // 実行時（RailField の Sync で決める物）
        int   trainHome   = 0;     // 元の駅（0＝区間のはじめ側 / 1＝おわり側）。足場を置いた所に近い方
        float trainFlip   = 0.0f;  // 足場の向きとガイドの向きが逆なら π（足場の前後をそのままにして走らせる）
        // 実行時（動き）
        TrainState trainState = TrainState::Wait;
        int   trainAt       = 0;     // 今いる駅（Run 中は行き先）
        int   trainFrom     = 0;     // Run の出発駅
        float trainTimer    = 0.0f;  // Depart の待ち・駅で空のまま待った秒
        float trainProgress = 0.0f;  // Run の進み（0〜1。確かめ用）
        float trainSpeedNow = 0.0f;  // 今の速さ(m/秒)（なめらかに加速・減速する）
        float trainDist     = 0.0f;  // 今の、足場の真ん中のガイド上の距離(m)
        bool  trainNeedsLeave = false; // 乗ったまま着いた：一度降りるまで発車しない
        bool  isTrack = false;       // 列車のガイド（線路）
    };

    // 渡し板（列車が駅に止まっている時だけ開く板・卵で広がる板）の設定と、今の状態。RailMotion が開け閉めする
    struct BoardData {
        int   boardKind    = 0;      // 0＝渡し板ではない / 1＝ぱたん（列車が止まったら開く）/ 2＝ころころ（卵で広がる）
        bool  boardTriggered = false; // ころころ：卵が当たった（広がり始めた。一度当たったらそのまま）
        int   boardTrain   = -1;     // ぱたん：つなぐ列車のレール番号
        int   boardStation = 0;      // 列車のどちらの駅の板か（0＝区間のはじめ側）
        bool  boardHingeFront = true; // ちょうつがい（駅の側）が始点側か
        float boardOpen    = 1.0f;   // 開き具合（0＝起き上がって閉じている → 1＝倒れて開いている）
    };

    // ============================================================
    // 持ち物への入口。読むだけの所は const の方（Config() など）を使う。
    //   書き換える所は Edit〜 を使う（レールを作る・つなぐ・動かす所だけ）。
    //   ノードや線のつなぎ方を書き換えた後は、BuildDistanceTable を呼ぶこと
    // ============================================================
    const std::vector<Vector3>& Nodes() const { return nodes_; }
    std::vector<Vector3>&       EditNodes() { return nodes_; }
    const ConfigData& Config() const { return config_; }
    ConfigData&       EditConfig() { return config_; }
    const LinkData&   Links() const { return links_; }
    LinkData&         EditLinks() { return links_; }
    const MotionData& Motion() const { return motion_; }
    MotionData&       EditMotion() { return motion_; }
    const TrainData&  Train() const { return train_; }
    TrainData&        EditTrain() { return train_; }
    const BoardData&  Board() const { return board_; }
    BoardData&        EditBoard() { return board_; }

    // front→back ベクトルの主軸からタイプを自動判定（|X|>=|Z|→横／それ以外→縦）
    void AutoDetectType();

    // --- ここから等速移動用の追加コード ---

    // 座標を計算する関数（既存のものと仮定）
    Vector3 EvaluatePosition(float t) const;

    // ① レールの長さを事前に計算する関数（ノードを追加/削除した時に1回だけ呼ぶ）
    void BuildDistanceTable();

    // ② 進みたい「距離(Distance)」から、スプラインの「t」を逆算する関数
    float GetTFromDistance(float targetDistance) const;

    // ③ 現在のtにおける「進行方向（接線）」を求める関数（カメラ用）
    Vector3 EvaluateTangent(float t) const;

    // ④ 逆に、tから距離を求める関数もあると便利（必要に応じて）
    float GetDistanceFromT(float t) const;

    // ============================================================
    // 【推奨API】距離(s)ベースの公開関数。利用側は t を意識せず、これだけ使えばよい。
    //   - 内部の曲線パラメータ t は隠蔽し、すべて「レール上を進んだ距離(m)」で扱う
    //   - これにより等速移動・端点の安定・乗り換えがシンプルになる
    // ============================================================

    // 距離 s における座標を返す（s は 0〜GetLength() にクランプ）
    Vector3 GetPositionByDistance(float distance) const;

    // 距離 s における進行方向（単位ベクトル）を返す。距離空間で前後サンプルするため端でも安定
    Vector3 GetTangentByDistance(float distance) const;

    // レール全長(m)
    float GetLength() const { return totalLength_; }

    // 指定ワールド座標に最も近いレール上の距離 s を返す（スナップ／分岐検出用）
    float GetClosestDistance(const Vector3& worldPos) const;

    // ============================================================
    // 【道システム】RailFrame / FrameCache（設計書 §3）
    //   道メッシュ・プレイヤー・敵・カメラが「同じ位置と向き」を共有するための
    //   フレーム（位置＋right/up/tangent）。0.25m刻みで事前計算してテーブル化し、
    //   実行時は線形補間で引くだけ（毎回のスプライン計算より約3.5倍速い）。
    //   向きは RMF（平行移動フレーム）＋ロール回復で、急カーブでもねじれない。
    //   ※位置は animOffset を含まない基準位置（動くレールは利用側で offset を足す）
    // ============================================================
    struct RailFrame {
        Vector3 position { 0.0f, 0.0f, 0.0f };
        Vector3 right    { 1.0f, 0.0f, 0.0f };
        Vector3 up       { 0.0f, 1.0f, 0.0f };
        Vector3 tangent  { 0.0f, 0.0f, 1.0f };
    };

    // 距離 d のフレームを返す（テーブルを線形補間。未構築なら都度計算にフォールバック）
    RailFrame GetFrameAtDistance(float distance) const;

    // FrameCache の構築（BuildDistanceTable の最後で自動的に呼ばれる）
    void BuildFrameCache();

    // フレームの表（kFrameStep 刻み）。読むだけ（作るのは BuildFrameCache）
    const std::vector<RailFrame>& GetFrames() const { return frameCache_; }
    static constexpr float kFrameStep = 0.25f; // テーブルの刻み(m)

    // ============================================================
    // 穴の「距離区間」API（仕様書_穴区間の見た目生成 §1-1）。
    //   nodeHole の連続フラグ列を [d0, d1] のレール距離区間へ変換して返す。
    //   区間の境界は隣接ノードとの中間点（従来の「最も近いノードが穴」判定と
    //   完全に同じ範囲になる）。端のノードが穴なら 0 / GetLength() まで広がる。
    //   落下判定・道メッシュ・マーカーの全てがこの区間を共通参照する（ズレ防止）。
    //   ソート済み・重なりなし。
    // ============================================================
    struct HoleInterval { float d0, d1; };
    std::vector<HoleInterval> GetHoleIntervals() const;

    // 指定距離(s)が「穴」区間か？（GetHoleIntervals と同じ区間を参照）
    bool IsHoleAtDistance(float distance) const;

    // ============================================================
    // 状態の問い合わせ（まとまりをまたぐ条件は、ここに 1 回だけ書く）
    // ============================================================
    // まだ出現していない道か（＝プレイ中は見えない・乗れない・着地できない）
    //   渡し板は、開ききるまで乗れない
    bool IsRideBlocked() const { return ( motion_.appearTrigger >= 0 && !motion_.appeared ) || ( board_.boardKind != 0 && board_.boardOpen < 0.95f ); }
    bool IsBoard() const { return board_.boardKind != 0; }
    bool IsRollBoard() const { return board_.boardKind == 2; }
    bool IsTrain() const { return train_.train != 0 && motion_.motionType == 3 && motion_.guideRail >= 0; }
    // 走っている最中か（降りられない・乗れない）
    bool IsTrainRunning() const { return IsTrain() && train_.trainState == TrainState::Run; }
    // 端どうしの連結・分岐・端の寄せから外すレール（動くレールと線路）
    bool IsLinkExcluded() const { return HasMotion() || train_.isTrack; }
    // 歩いて乗り移れないレール（線路・走っている列車）
    bool IsUnwalkable() const { return train_.isTrack || IsTrainRunning(); }
    // 動きが設定されているか
    bool HasMotion() const {
        return motion_.motionAmp.x != 0.0f || motion_.motionAmp.y != 0.0f || motion_.motionAmp.z != 0.0f
            || ( motion_.motionType == 3 && motion_.guideRail >= 0 ); // ガイド追従は振幅なしでも動く
    }

private:
    std::vector<Vector3> nodes_; // 通る点
    ConfigData config_;          // エディタで決める設定
    LinkData   links_;           // ほかのレールとのつながり
    MotionData motion_;          // 動くレール
    TrainData  train_;           // 列車
    BoardData  board_;           // 渡し板

    // --- 中で作る表（nodes から BuildDistanceTable / BuildFrameCache が作る。外からは書き換えない）---
    std::vector<float> distanceTable_;  // 曲線を細かく刻んだ点ごとの、始めからの距離
    std::vector<float> tTable_;         // その点の、曲線のパラメーター t
    float totalLength_ = 0.0f;          // レールの長さ(m)。読むのは GetLength
    std::vector<RailFrame> frameCache_; // 位置と向きの表。読むのは GetFrames / GetFrameAtDistance
};

} // namespace KazumimiEngine
