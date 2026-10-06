#pragma once
// =====================================================================
//  TitleAmbience：タイトルの背景を「生きている」ように見せる動き。
//   物を増やすのではなく、もう置いてある物を少しずつ動かす：
//   ・風 … マップに置いた草・花・木が、左から右へ風が通るようにゆれる。雲は糸で吊られてゆっくり流れる
//   ・恐竜への反応 … 恐竜がそばを通ると、草と花が押されて倒れ、ぷるぷる揺れて戻る
//   ・紙の波 … 草原と丘の間に、紙で作った波の列が左右にすれ違う（舞台の波の仕掛け）。置く物はこれだけ増やす
//
//  草・花・木・雲は箱庭のステージ（title.stage.json）に置いた物をそのまま動かす。置いた位置（保存される値）は変えず、
//  表示だけを動かすので、エディタで保存しても揺れが書き込まれることはない。
//  エディタを出している間は止める（物をつかんで動かす時に揺れていると作業しづらいため）
// =====================================================================
#include "engine/math/struct.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

class Camera;
class CraftStageView;
class Model;
class Obj3d;
struct CraftObject;

class TitleAmbience {
public:
    TitleAmbience();
    ~TitleAmbience();

    void Initialize(Camera* camera);
    void Finalize();
    // dinoPosition：恐竜の足元（草・花が押される中心）
    void Update(float deltaTime, const Vector3& dinoPosition);
    void Draw(); // 紙の波（草・花などは箱庭の物なので CraftStageView が描く）
    // 箱庭の描画へ「表示だけの動き」を差し込む（シーンの初期化で1回呼ぶ）
    void AttachTo(CraftStageView* view);

    // インスペクターから切り替える
    bool& RefWindEnabled(){ return windEnabled_; }
    bool& RefWavesEnabled(){ return wavesEnabled_; }
    bool& RefMoveWhileEditing(){ return moveWhileEditing_; }
    float& RefWindStrength(){ return windStrength_; }

private:
    // 動かす物の種類（asset 名から決める）
    enum class Kind { Grass, Flower, Tree, Cloud };
    // 恐竜に押されて倒れている角度（物の id ごと）
    struct Bend { float angle = 0.0f; float speed = 0.0f; };
    // 紙の波1列
    struct WaveRow {
        std::unique_ptr<Model> model;
        std::unique_ptr<Obj3d> object;
        Vector3 basePosition {};
        float   slide = 0.0f;  // 左右に動く幅(m)
        float   speed = 0.0f;  // 動く速さ
        float   phase = 0.0f;
    };

    void ApplySway(const CraftObject& placed, Vector3& position, Vector3& rotation);
    void CreateWaves(Camera* camera);

    std::unordered_map<uint64_t, Bend> bends_;
    bool    moving_ = false;       // 今フレーム動かすか（風 ON かつエディタを出していない）
    float   deltaTime_ = 0.0f;
    Vector3 dinoPosition_ {};

    std::vector<WaveRow> waves_;
    float time_ = 0.0f;

    bool  windEnabled_ = true;
    bool  wavesEnabled_ = true;
    bool  moveWhileEditing_ = false; // エディタを出している間も動かす
    float windStrength_ = 1.0f;
};
