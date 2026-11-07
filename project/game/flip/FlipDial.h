#pragma once
// =====================================================================
//  FlipDial：裏世界反転の「？」スイッチの見た目（工作の文字盤）。
//   黄色い紙皿の文字盤（ふちにオレンジの点・真ん中に緑のつまみ・青緑の針）を、白い綿の雲に貼り、
//   左右にピンクのモールの腕、下に段ボールの筒の柱。全部エンジンの球と円柱を、色ごとに別の名前で作って並べる。
//   文字盤は表のカメラ（-Z 側）を向き、反転の進み具合で一緒に回って、裏ではそちらのカメラを向く。
//   卵が当たると、くるっと回りながら針が一回りし、腕がぱたぱたする。使えない間（クールダウン）は文字盤が灰色
// =====================================================================
#include "engine/math/struct.h"

#include <memory>
#include <vector>

class Obj3d;

class FlipDial {
public:
    static constexpr float kCenterAbove = 0.6f; // スイッチの位置（底）から文字盤の真ん中までの高さ(m)
    static constexpr float kRadius = 0.55f;     // 文字盤の半径(m)

    FlipDial();
    ~FlipDial();

    static void LoadModels(); // 色ごとの部品のモデル（何度呼んでもよい）
    void Initialize();

    // center：文字盤の真ん中 / groundY：柱の下（道の高さ）/ yaw：向き（0＝表のカメラの方）
    // spin：当たった時の動き（1→0）/ cooling：使えない間（灰色）/ time：揺れ用の時間(秒)
    void Update(const Vector3& center, float groundY, float yaw, float spin, bool cooling, float time);
    void Draw();

private:
    struct Part {
        std::unique_ptr<Obj3d> obj;
        Vector3 local {};   // 文字盤の真ん中からの位置（+Z＝奥。文字盤の面は -Z を向く）
        Vector3 scale {};
    };
    Part& Add(const char* model, const Vector3& local, const Vector3& scale);

    std::vector<Part> parts_;
    int disc_ = -1, knob_ = -1, post_ = -1;
    std::vector<int> hand_;            // 針（根元から先へ）
    std::vector<int> leftArm_, rightArm_;
};
