#pragma once
#include "engine/math/struct.h"
#include "engine/3d/model/Model.h"
#include <cstdint>
#include <memory>
#include <vector>

class Obj3d;
class Camera;
class SplineRail;

// =====================================================================
//  RoadRenderSlots：道の GPU 側（動的メッシュスロット＋ジョイントの Obj3d プール）の管理クラス。
//   ・GPUバッファはスロット毎の固定容量を使い回す（編集中の CreateBuffers ゼロ）。
//     Build のたびに BeginBuild → EmitMesh/PlaceMovingJoint → EndBuild で詰め直す
//   ・動くレールへは「基準位置 + animOffset」で毎フレーム追従（再生成不要）。
//     列車式（animYaw）は pivot 中心の回転で表す
//   ・後から出現する道（appearTrigger）はプレイ中だけ隠す/せり上げる
// =====================================================================
class RoadRenderSlots {
public:
    RoadRenderSlots();
    ~RoadRenderSlots(); // unique_ptr<Model>/<Obj3d> のため cpp 側で定義

    // Build 開始：使用数と統計をリセットし、この Build で使うカメラとアトラスを覚える
    void BeginBuild(Camera* camera, uint32_t atlasSrv);
    // 生成済みメッシュを空きスロットへ書き込む（スロットが足りなければ1個だけ確保）。
    //   isJoint: ジョイントのベイクメッシュ（表示モードで描画を切替）
    void EmitMesh(const Model::ModelData& data, int followRail, bool isJoint = false);
    // 動くレールのジョイント（road_joint）を Obj3d プールで1個置く（animOffset 追従が必要なため個別）
    void PlaceMovingJoint(Model* model, const Vector3& pos, float yaw, int railIdx, const Vector3& animOffset);
    // Build 終了：余ったスロットは空メッシュにして描かない（バッファは保持＝次の編集で使い回す）
    void EndBuild();
    // 保持しているメッシュスロット・ジョイントスロットの全GPUリソースを解放する
    void Clear();

    // 毎フレーム：動くレールへの追従＋カメラ行列の焼き直し
    void Update(const std::vector<SplineRail>& rails, bool playMode);
    void Draw(bool showJoints) const;

    // 背面カリングの切替（既存スロットにも即時反映）
    void SetCullNone(bool cullNone);
    bool IsCullNone() const{ return cullNone_; }

    size_t TileCount() const{ return slotsUsed_ + jointsUsed_; }
    size_t VertexCount() const{ return lastVertexCount_; }
    size_t TriangleCount() const{ return lastTriangleCount_; }

private:
    // --- 動的メッシュスロット（VB/IB を使い回す。編集中の CreateBuffers ゼロ）---
    struct MeshSlot {
        std::unique_ptr<Model> model;
        std::unique_ptr<Obj3d> obj;
        int rail = -1;
        bool isJoint = false;   // ジョイントのベイクメッシュ（表示モードで描画を切替）
        bool hiddenNow = false; // 出現前の道（Update で毎フレーム判定 → Draw でスキップ）
    };
    // --- 動くレールのジョイント（road_joint）スロット。Obj3d を使い回す ---
    struct JointSlot {
        std::unique_ptr<Obj3d> obj;
        int rail = -1;
        Vector3 base {};
        bool hiddenNow = false; // 出現前の道（同上）
    };

    std::vector<std::unique_ptr<MeshSlot>> slots_;
    size_t slotsUsed_ = 0;
    std::vector<std::unique_ptr<JointSlot>> joints_;
    size_t jointsUsed_ = 0;

    Camera*  camera_ = nullptr; // Build 中に使うカメラ（BeginBuild で設定）
    uint32_t atlasSrv_ = 0;     // 道アトラスのSRV（BeginBuild で設定）
    bool     cullNone_ = true;  // true=両面描画（従来） / false=背面カリング

    size_t lastVertexCount_ = 0;   // 直近 Build の総頂点数（表示用）
    size_t lastTriangleCount_ = 0; // 直近 Build の総三角形数（表示用）
};
