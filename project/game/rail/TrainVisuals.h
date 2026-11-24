#pragma once
#include "engine/3d/obj/Obj3d.h"
#include "engine/math/struct.h"
#include "game/effect/PaperFx.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace KazumimiEngine {
class Camera;
class InstancedGroup;
class SplineRail;
}

// =====================================================================
//  TrainVisuals：列車（客車・機関車）と線路（枕木・レール）を描く。見た目はワールドマップの箱庭の汽車と同じ
//   （形は ToyTrain：フェルトの色の箱・筒・球）。
//   ・客車：列車の足場（SplineRail::IsTrain）の長さに、客車を並べる（長さに合わせて前後に伸び縮み）。
//           足場の高さ＝客車の屋根の上の面（プレイヤーは屋根の上に乗る）。位置と向きは動くレールと同じ
//           （真ん中＝animPivot＋animOffset、向き＝restYaw＋animYaw）
//   ・機関車：足場から向こうの駅の側へ、ガイドの上に置く（カーブでもガイドに沿って曲がる）。煙突から綿のけむり
//   ・渡し板（SplineRail::IsBoard）：草の板（tools/train/make_board.py の board_grass を 1m ずつ並べる）。
//           ぱたん＝駅の側をちょうつがいに倒れて開く（着いた時に「ぱたん」と土ぼこり、開いている間は黄色い矢印が弾む）
//           ころころ＝巻いた草のマット（board_roll）に卵が当たると「ぽん」、転がりながら板が伸び、広がりきると「ぱたん」
//   ・線路：列車のガイドのレール（SplineRail::isTrack）に沿って、段ボールの枕木と灰色のフェルトのレール 2 本
//           （ガイドは動かないので、作り直す時に1回だけ置く）。線路の上の面＝ガイドの高さ − 屋根の高さ
//   同じモデルはまとめて1回で描く（InstancedGroup）
// =====================================================================
class TrainVisuals {
public:
    TrainVisuals();
    ~TrainVisuals();

    // レールを作り直した後に呼ぶ（線路を置き直し、列車の数を数える）。noiseSrv＝一括描画が束縛するノイズ画像
    void Build(const std::vector<KazumimiEngine::SplineRail>& rails, uint32_t noiseSrv);
    // 毎フレーム（動くレールを進めた後・カメラが決まった後）。dt＝けむりの時間。
    //   playerRail＝プレイヤーのレール（その列車・渡し板に乗っている間は矢印を出さない）
    void Update(const std::vector<KazumimiEngine::SplineRail>& rails, const KazumimiEngine::Camera* camera, float deltaTime, int playerRail = -1);
    void Draw(const KazumimiEngine::Camera* camera); // 一括描画（BaseScene::OnDrawInstanced から）

private:
    // 同じモデルの一括描画 1 組（動かない線路の行列と、毎フレーム詰め直す列車の行列）
    struct Batch {
        std::unique_ptr<KazumimiEngine::InstancedGroup> group;
        uint32_t capacity = 0;
        std::vector<KazumimiEngine::Obj3d::TransformationMatrix> statics; // 動かない物（線路）の世界行列（WVP は毎フレーム）
        std::vector<KazumimiEngine::Matrix4x4> dynamics;                  // 毎フレーム詰め直す物（列車）
        std::vector<KazumimiEngine::Obj3d::TransformationMatrix> matrices;
    };
    // 列車 1 本ぶんの見た目の状態（レールの番号・けむりの間隔・煙突の口の位置）
    struct TrainState {
        int     rail = -1;
        float   smokeTimer = 0.0f;
        bool    hasChimney = false;
        KazumimiEngine::Vector3 chimney {};   // 煙突の口（世界）
        bool    wasRunning = false; // 前のフレームに走っていたか（発車・停車の瞬間を見つける）
    };

    // 渡し板 1 枚ぶんの見た目の状態（レールの番号・前のフレームの開き具合・矢印の弾み）
    struct BoardState {
        int   rail = -1;
        float lastOpen = 0.0f; // 前のフレームの開き具合（倒れた瞬間を見つける）
        float time = 0.0f;     // 矢印の弾み
    };

    void AddTrack(const KazumimiEngine::SplineRail& guide);
    // countAll＝入れ物の大きさを数える時（開いた形・矢印も出した形で置く。音は出さない）
    void AddBoard(const std::vector<KazumimiEngine::SplineRail>& rails, BoardState& state, const KazumimiEngine::Camera* camera, float deltaTime, int playerRail, bool countAll = false);
    void AddTrain(const std::vector<KazumimiEngine::SplineRail>& rails, TrainState& state);
    void AddBoardPlanks(const KazumimiEngine::Matrix4x4& frame, float length, float visible); // 草の板を 1m ずつ並べる（visible で切る）
    void Push(const std::string& model, const KazumimiEngine::Matrix4x4& world, bool isStatic);

    std::map<std::string, Batch> batches_;
    std::vector<TrainState> trains_;
    std::vector<BoardState> boards_;
    uint32_t noiseSrv_ = 0;
    PaperFx  smoke_;              // 煙突の綿のけむり
    bool     smokeReady_ = false;
};
