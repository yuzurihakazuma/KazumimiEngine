#pragma once
// =====================================================================
//  StageDiorama：ステージの「箱庭」。本物のステージのデータ（resources/map/<stage>.json）を読み、
//   四角いタイルに切り抜いて、縮めて台座の上に載せる（ステージ選択・飛び込み演出 企画書 A案：実データ縮小描画）。
//   ・切り抜く所：ブロック・敵・道・スタートがいちばん多く入る四角（worldmap.json の window で指定もできる）
//   ・道は四角のふちで切り、地面もふちで切る。四角いっぱいに草の台を敷いて、タイルの形にする
//   ・目印（feature）："train"＝四角のふちに沿って線路を敷き、おもちゃの汽車がぐるぐる走る（地面は平らにならす）
//   ・選ばれている間は、台座ごとゆっくり回る（離れると正面へ戻る）
//   ・道（布ロード）・地面・木・草花・ブロック … ステージと同じ物を等倍の縮小で（RoadMesh / StageBackdrop の SetRoot）
//   ・空・丘・雲・穴の底は載せない（台座の外へはみ出すので）
//   ・敵とゴール … 縮めると見えないので、ボードゲームの駒くらいの大きさで立てる（何がいるか・どこがゴールか）
//   ・プレビュー中だけ敵のアニメを動かし、布の道をスタートから順に敷き直して道筋をなぞる
//   ステージを直せば、箱庭も自動で最新になる（追加の素材は要らない）
// =====================================================================
#include "engine/math/struct.h"
#include "engine/rail/SplineRail.h"
#include "game/rail/RoadMesh.h"
#include "game/stage/StageBackdrop.h"
#include "game/worldmap/DioramaTrain.h"

#include <memory>
#include <string>
#include <vector>

class Camera;
class Obj3d;
class SkinnedObj3d;

class StageDiorama {
public:
    StageDiorama();
    ~StageDiorama();

    // 切り抜く四角（ステージの中の m）。横は台座の縦横の比から決まる
    struct Window {
        float depth = 30.0f;     // 奥行き
        bool  hasCenter = false; // false＝自動で選ぶ
        float centerX = 0.0f, centerZ = 0.0f;
        std::string feature;     // 目印（"train"＝汽車が走る。空＝なし）
    };
    // stage＝resources/map/<stage>.json。top＝台座の上面の中心。width/depth＝箱庭の大きさ(m)
    bool Build(const std::string& stage, const Vector3& top, float width, float depth, const Window& window,
               Camera* camera, uint32_t noiseSrv, uint32_t envSrv);
    void Finalize();

    // 毎フレーム。previewing＝プレビュー中（敵が動く・布の道をなぞる）
    void Update(float deltaTime, bool previewing, const Camera* camera);
    void DrawInstanced(const Camera* camera); // 地面・木・ブロック（一括描画）
    void DrawOpaque();                        // 道・敵・ゴールの駒

    // 選んだ時に、少し跳ねて持ち上がる（1.0 → 1.1 → 1.05）。離れたら 1.0 へ戻る
    void SetSelected(bool selected);
    // 飛び込まれた時に、水面のように弾む
    void Splash();

    Vector3 Center() const{ return center_; }   // 箱庭の真ん中（上面の少し上）
    float Yaw() const{ return yaw_; }           // 回っている角度（台座も同じだけ回す）
    float Scale() const{ return scale_ * bounce_; } // 箱庭の縮み（ステージの 1m が何 m に見えるか）
    Vector3 StartPoint() const;                 // 箱庭の中のスタート地点（世界）
    bool  TrainChimney(Vector3& out) const{ return train_.ChimneyTop(out); } // 汽車の煙突の口（けむり用）
    // 飛び込み中：回るのをやめて、正面へすばやく戻して止める
    void  SetSpinHold(bool hold){ spinHold_ = hold; }
    void  SetSpinSeconds(float seconds){ spinSeconds_ = seconds; } // 選ばれている間、1周する秒数
    int CoinCount() const{ return coinCount_; }
    // 台座の上で箱庭が占める範囲（跳ねていない時。台座の大きさを合わせる・カメラが追う）
    Vector3 BoundsMin() const{ return boundsMin_; }
    Vector3 BoundsMax() const{ return boundsMax_; }
    // 布の道をなぞっている所（0＝スタート 1＝全部敷いた）
    float TraceProgress() const{ return traceProgress_; }

private:
    struct Token {
        std::unique_ptr<SkinnedObj3d> skinned;
        std::unique_ptr<Obj3d> obj;
        Vector3 local {};   // ステージの中の位置
        float   yaw = 0.0f;
        float   scale = 0.1f;
        float   bobPhase = 0.0f;
    };

    Vector3 ToWorld(const Vector3& local) const;
    DioramaRoot Root() const;
    void ApplyRoot();
    void BuildTrain(float cx, float cz, float halfWidth, float halfDepth, float height, Camera* camera);

    std::vector<SplineRail> rails_;
    RoadMesh road_;
    StageBackdrop backdrop_;
    std::vector<Token> tokens_;
    // 目印の汽車と線路
    DioramaTrain train_;
    std::vector<SplineRail> trackRails_; // 線路の2本のレール（布で描く）
    RoadMesh trackMesh_;

    Vector3 anchor_ {};     // 台座の上面の中心（跳ねる時の支点）
    Vector3 pivot_ {};      // 箱庭の中で、台座の上面の中心に来る所
    float   yaw_ = 0.0f;    // 回っている角度
    float   spinSeconds_ = 10.0f;
    bool    spinHold_ = false;
    float   scale_ = 0.01f;
    float   bounce_ = 1.0f; // 跳ねの大きさ（1=そのまま）
    float   bounceTime_ = -1.0f;
    bool    selected_ = false;
    float   splashTime_ = -1.0f;
    Vector3 center_ {};
    Vector3 boundsMin_ {}, boundsMax_ {};
    float   traceProgress_ = 0.0f;
    int     coinCount_ = 0;
    // 布の道をなぞる（スタートから順に敷く）
    int     startRail_ = 0;
    float   startDist_ = 0.0f;
    float   traceTime_ = -1.0f;
    float   pathLength_ = 100.0f; // 布の道の長さの合計（なぞり終わりの目安）
    uint32_t envSrv_ = 0;
    bool    built_ = false;
};
