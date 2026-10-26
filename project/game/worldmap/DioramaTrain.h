#pragma once
// =====================================================================
//  DioramaTrain：箱庭の上をぐるぐる走る、おもちゃの汽車（ステージの目印。ステージ選択）。
//   機関車＋客車2両を、フェルトの色の箱・筒・球で組む（モデルの素材は要らない）。
//   線路（輪になったレール。箱庭の中の座標）に沿って走り、向きは線路の向き
//   DioramaRoot：箱庭の中の座標 → 世界（縮めて・回して・台座の上へ）。StageDiorama と同じ変換
// =====================================================================
#include "engine/math/struct.h"
#include "engine/rail/SplineRail.h"

#include <memory>
#include <vector>

class Obj3d;

struct DioramaRoot {
    Vector3 top {};    // 台座の上面の中心（世界）
    Vector3 pivot {};  // 箱庭の中で、台座の中心に来る所
    float   scale = 1.0f;
    float   yaw = 0.0f; // 箱庭ごと回す角度
    Vector3 Apply(const Vector3& local) const;
    Vector3 Translation() const; // 縮めて回した後に足す量（SetRoot に渡す）
};

class DioramaTrain {
public:
    DioramaTrain();
    ~DioramaTrain();

    // loop：線路の真ん中の線（輪。始点と終点が同じ所）
    void Build(const SplineRail& loop, uint32_t envSrv);
    void Clear();
    // speed：線路の上を進む速さ（箱庭の中の m/秒）
    void Update(float deltaTime, float speed, const DioramaRoot& root);
    void Draw();
    // 煙突の口（世界）。汽車が無ければ false
    bool ChimneyTop(Vector3& out) const{ out = chimney_; return hasChimney_; }

private:
    struct Part {
        std::unique_ptr<Obj3d> obj;
        Vector3 offset {}; // 車両の中の位置（x＝右 y＝上 z＝前。箱庭の中の m）
        Vector3 size {};   // 大きさ（箱庭の中の m）
        float   pitch = 0.0f; // 筒を前後に寝かせる時 π/2
    };
    struct Car {
        std::vector<Part> parts;
        float behind = 0.0f; // 機関車の先頭から、この車両の真ん中までの距離
    };
    void AddPart(Car& car, const char* shape, const Vector3& color, const Vector3& offset, const Vector3& size, float pitch = 0.0f);

    SplineRail loop_;
    std::vector<Car> cars_;
    float distance_ = 0.0f; // 機関車が線路の上を進んだ距離
    uint32_t envSrv_ = 0;
    Vector3 chimney_ {};
    bool    hasChimney_ = false;
};
