#pragma once
// =====================================================================
//  PaperSheet：写し取った画面を貼った「1枚の紙」（ゲームオーバー演出 企画・仕様書の板メッシュ）。
//   細かく区切った板（動的メッシュ）を CPU で毎フレーム曲げる：
//    ・くしゃくしゃ（画面のまま、真ん中へぎゅっと縮みながら、折れ目がたくさん立つ。ふちもギザギザになる）
//    ・広げる（くしゃくしゃを戻す。同じノイズ・同じシードなので、シワは丸めた時と同じ所に残る）
//   面ごとの法線（頂点を共有しない）で、カクカクした折れ目の見た目にする。
//   層：表（写し取った絵・光を当てて折れ目の陰影を出す）／重ねる絵（光なし。写したままの色・次の絵へ切り替える）／裏（段ボール）
// =====================================================================
#include "engine/math/struct.h"

#include <cstdint>
#include <memory>
#include <vector>

class Model;
class Obj3d;

class PaperSheet {
public:
    PaperSheet();
    ~PaperSheet();

    void Initialize(uint32_t frontSrv);
    void Finalize();

    // 置き場所：bottom＝下の辺の真ん中、right＝右、up＝下の辺から上の辺へ、normal＝表の向き。width/height＝大きさ(m)
    void Place(const Vector3& bottom, const Vector3& right, const Vector3& up, const Vector3& normal, float width, float height);
    void SetFrontTexture(uint32_t srv);   // 表（光を当てる）
    void SetCoverTexture(uint32_t srv);   // 重ねる絵（光なし）

    // くしゃくしゃの強さ（0＝平ら 1＝いちばん縮んで折れ目だらけ）
    void SetCrumple(float amount){ crumple_ = amount; }
    // 重ねる絵の濃さ（0＝表だけ 1＝重ねる絵だけ）
    enum Layer { Front, Cover, Back, LayerCount };
    void SetCoverAlpha(float alpha){ coverAlpha_ = alpha; }
    void SetVisible(bool visible){ visible_ = visible; }

    void Update();
    void Draw();

    // 今の形で、紙のどこ（u：左0〜右1、v：下0〜上1）が世界のどこにあるか（恐竜がくわえる所など）
    Vector3 PointAt(float u, float v) const;
    Vector3 Center() const;

private:
    struct LayerData {
        std::unique_ptr<Model> model;
        std::unique_ptr<Obj3d> obj;
        float offset = 0.0f;    // 表の向きへ浮かせる量（m）
        bool  reversed = false; // 裏面（向きを逆に）
    };
    Vector3 Deform(int column, int row) const;
    void BuildLayers();

    std::vector<LayerData> layers_;
    std::vector<Vector3> grid_;      // 曲げた後の格子点
    std::vector<float> crease_;      // 折れ目の深さ（格子点ごと。シードは固定）
    std::vector<Vector2> shift_;     // 縮む時の横ずれ（折り重なる所）
    float coverAlpha_ = 1.0f;

    Vector3 bottom_ {}, right_ { 1, 0, 0 }, up_ { 0, 1, 0 }, normal_ { 0, 0, -1 };
    float width_ = 1.0f, height_ = 1.0f;
    float crumple_ = 0.0f;
    bool  visible_ = false;
    bool  meshFilled_ = true; // メッシュに形が入っている（隠した後、1回だけ空にする）
};
