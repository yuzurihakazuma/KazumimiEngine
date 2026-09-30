#pragma once
// =====================================================================
//  TitleAmbience：タイトルの背景を「生きている」ように見せる動き。
//   物を増やすのではなく、もう置いてある物を少しずつ動かす：
//   ・風 … マップに置いた草・花・木が、左から右へ風が通るようにゆれる。雲は糸で吊られてゆっくり流れる
//   ・恐竜への反応 … 恐竜がそばを通ると、草と花が押されて倒れ、ぷるぷる揺れて戻る
//   ・紙の波 … 草原と丘の間に、紙で作った波の列が左右にすれ違う（舞台の波の仕掛け）。置く物はこれだけ増やす
//
//  草・花・木・雲はマップ（title.json）に置いた物をそのまま動かす。置いた位置（保存される値）は変えず、
//  表示だけを動かすので、エディタで保存しても揺れが書き込まれることはない。
//  エディタを出している間は止める（物をつかんで動かす時に揺れていると作業しづらいため）
// =====================================================================
#include "engine/math/struct.h"

#include <memory>
#include <vector>

class Camera;
class Model;
class Obj3d;

class TitleAmbience {
public:
    TitleAmbience();
    ~TitleAmbience();

    void Initialize(Camera* camera);
    void Finalize();
    // dinoPosition：恐竜の足元（草・花が押される中心）
    void Update(float deltaTime, const Vector3& dinoPosition);
    void Draw(); // 紙の波（草・花などはマップの物なのでエディタ側が描く）

    // インスペクターから切り替える
    bool& RefWindEnabled(){ return windEnabled_; }
    bool& RefWavesEnabled(){ return wavesEnabled_; }
    bool& RefMoveWhileEditing(){ return moveWhileEditing_; }
    float& RefWindStrength(){ return windStrength_; }

private:
    // 動かす物の種類（マップの type 名から決める）
    enum class Kind { Grass, Flower, Tree, Cloud };
    struct Mover {
        int   index = 0;       // マップの配置物の番号
        Kind  kind = Kind::Grass;
        float bend = 0.0f;     // 恐竜に押されて倒れている角度
        float bendSpeed = 0.0f;
    };
    // 紙の波1列
    struct WaveRow {
        std::unique_ptr<Model> model;
        std::unique_ptr<Obj3d> object;
        Vector3 basePosition {};
        float   slide = 0.0f;  // 左右に動く幅(m)
        float   speed = 0.0f;  // 動く速さ
        float   phase = 0.0f;
    };

    void BindMapObjects();                 // マップの配置物から、動かす物を探し直す
    void UpdateMapObjects(float deltaTime, const Vector3& dinoPosition, bool moving);
    void CreateWaves(Camera* camera);

    std::vector<Mover> movers_;
    int  boundObjectCount_ = -1;   // 探した時の配置物の数（変わったら探し直す）
    int  boundMapVersion_ = -1;    // 探した時のマップ読込回数
    bool wasMoving_ = false;       // 前フレームに動かしていたか（止めた瞬間に元の位置へ戻す）

    std::vector<WaveRow> waves_;
    float time_ = 0.0f;

    bool  windEnabled_ = true;
    bool  wavesEnabled_ = true;
    bool  moveWhileEditing_ = false; // エディタを出している間も動かす
    float windStrength_ = 1.0f;
};
