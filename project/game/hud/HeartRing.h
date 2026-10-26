#pragma once
// =====================================================================
//  HeartRing：体力の♡（ヨッシー クラフトワールドの HP の出し方）。
//   ふだんは出さず、ダメージを受けた時（や回復・やり直しの時）だけ、恐竜のまわりに
//   小さなハートが輪になって並ぶ。減った分のハートは、輪からはじけて落ちる。
//   輪の大きさは、画面に映った恐竜の大きさに合わせる。残りが少ない時は、ずっと出したまま脈打つ。
//   体力の数そのものもここで持つ（ダメージ・満タンに戻す・ゼロになったか）
// =====================================================================
#include "engine/math/struct.h"

#include <memory>
#include <vector>

class Camera;
class Sprite;

class HeartRing {
public:
    HeartRing();
    ~HeartRing();

    void Initialize(int maxHearts);
    void Finalize();

    // amount 個減らす。ゼロになったら true
    bool Damage(int amount);
    void Refill();          // 満タンに戻す（見せる）
    void Show();            // 今の数を見せる
    void HideNow();         // すぐ隠す（ゲームオーバー演出の間など）
    int  Count() const{ return count_; }
    int  Max() const{ return max_; }

    // feet＝恐竜の足元、height＝背の高さ(m)。画面へ投影して、そのまわりに輪を作る
    void Update(float deltaTime, const Vector3& feet, float height, const Camera& camera);
    void Draw();

private:
    struct Heart {
        std::unique_ptr<Sprite> sprite;
        bool  broken = false;    // 減った
        float breakTime = 0.0f;  // はじけてからの秒数（負＝はじけるまでの待ち）
        float driftX = 0.0f;     // はじけて飛ぶ横の向き
    };
    std::vector<Heart> hearts_;
    int   max_ = 12;
    int   count_ = 12;
    float showTime_ = -1.0f; // 見せ始めてからの秒数（負＝隠れている）
    Vector2 center_ {};      // 輪の真ん中（画面）
    float radius_ = 80.0f;   // 輪の半径（画面の画素）
    bool  valid_ = false;
    float pulse_ = 0.0f;
};
