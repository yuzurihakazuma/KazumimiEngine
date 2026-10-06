#pragma once
// =====================================================================
//  CraftStageView：CraftStage（箱庭の配置データ）を描く。
//   物1個 = Obj3d 1個。データが変わったら（CraftStage::GetVersion が進んだら）id で突き合わせて、
//   増えた物は作り、消えた物は捨て、残った物は位置を合わせ直す（作り直さない）。
//   一覧に無い物（manifest に無い asset）は赤い箱で出す（データは消さない）。
//
//   見た目だけの動き（風でゆれる等）は SetDisplayModifier で差し込む。置いた位置（データ）は変えない
//
//   背景の追従（パララックス）：params に "parallax"（0〜1）を持つ物は、カメラが基準の位置（ステージの
//   environment.backdropAnchor）から動いた分の parallax 倍だけ一緒に動く。1＝空のようにカメラに付いてくる /
//   0.6 くらい＝奥の山のようにカメラより遅く動いて奥行きが出る。レールに沿って長く続く本編ステージ用
// =====================================================================
#include "game/craft/CraftStage.h"

#include <functional>
#include <memory>
#include <unordered_map>

class Camera;
class Obj3d;

class CraftStageView {
public:
    CraftStageView();
    ~CraftStageView();

    void Initialize(Camera* camera);
    void Finalize();
    // データに合わせる（毎フレーム呼ぶ。データが変わった時だけ作り直す）＋表示の動きを足す
    void Update(const CraftStage& stage);
    void Draw();

    // 層ごとの表示
    void SetLayerVisible(int layer, bool visible){ if ( layer >= 0 && layer < CraftLayer_Count ) { layerVisible_[layer] = visible; } }
    bool IsLayerVisible(int layer) const{ return layer >= 0 && layer < CraftLayer_Count && layerVisible_[layer]; }

    // 表示だけを動かす処理（置いた位置 position・回転 rotationRad を受け取り、書き換えて返す）
    using DisplayModifier = std::function<void(const CraftObject& object, Vector3& position, Vector3& rotationRad)>;
    void SetDisplayModifier(DisplayModifier modifier){ modifier_ = std::move(modifier); }

    // 背景の追従に使う今のカメラ位置（毎フレーム、Update の前に渡す）
    void SetCameraPosition(const Vector3& position){ cameraPosition_ = position; }

    // その物の表示（ギズモで直接動かす時に使う）。無ければ nullptr
    Obj3d* FindObject(uint64_t id) const;
    // 編集中（ギズモでつかんでいる間など）は、この物だけデータから位置を戻さない
    void SetHeldObject(uint64_t id){ heldId_ = id; }

private:
    void Rebuild(const CraftStage& stage);

    Camera* camera_ = nullptr;
    struct Visual {
        std::unique_ptr<Obj3d> object;
        std::string asset;
        int layer = 0;
    };
    std::unordered_map<uint64_t, Visual> visuals_;
    int  builtVersion_ = -1;
    bool layerVisible_[CraftLayer_Count] = { true, true, true, true };
    uint64_t heldId_ = 0;
    Vector3  cameraPosition_ {};
    DisplayModifier modifier_;
};
