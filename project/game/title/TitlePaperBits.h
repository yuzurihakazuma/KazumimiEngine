#pragma once
// =====================================================================
//  TitlePaperBits：タイトルの紙ふぶき（小さな色紙の切れ端）。
//   光る粒ではなく「紙」なので、クラフトの雰囲気を壊さずに動きを足せる。
//   ・Burst … その場から少しだけ舞い上がって、ひらひら落ちる（看板が刺さった時・的にベロが当たった時など）
//   ・ただよう紙 … 画面の上から、ゆっくり斜めに降り続ける（数枚だけ。ON/OFF できる）
//   あらかじめ決まった枚数の板（Obj3d）を用意して使い回す。実行中に作ったり消したりしない
// =====================================================================
#include "engine/math/struct.h"

#include <memory>
#include <vector>

class Obj3d;

class TitlePaperBits {
public:
    TitlePaperBits();
    ~TitlePaperBits();

    void Initialize();
    void Finalize();
    void Update(float deltaTime);
    void Draw();

    // position から count 枚を散らす。spread=横へ広がる速さ / lift=舞い上がる速さ / size=1枚の大きさ(m)
    void Burst(const Vector3& position, int count, float spread, float lift, float size = 0.24f);
    void Clear(); // 全部消す（演出をやり直す時）

    void SetAmbient(bool enabled){ ambient_ = enabled; }
    bool IsAmbient() const{ return ambient_; }

private:
    struct Bit {
        std::unique_ptr<Obj3d> object;
        bool    alive = false;
        bool    drifting = false; // ただよう紙（ゆっくり降り続ける）か
        Vector3 position {};
        Vector3 velocity {};
        Vector3 rotation {};
        Vector3 spin {};          // 回る速さ(rad/s)
        float   size = 0.16f;
        float   age = 0.0f;
        float   life = 1.0f;
        float   swayPhase = 0.0f; // 左右にゆれる位相
    };

    Bit* FindFree();
    void SpawnDrifting(Bit& bit, bool anywhere);

    std::vector<Bit> bits_;
    bool  ambient_ = true;
    float ambientTimer_ = 0.0f;
};
