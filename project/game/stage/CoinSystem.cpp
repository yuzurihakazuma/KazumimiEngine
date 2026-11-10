#include "CoinSystem.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/rail/SplineRail.h"
#include "engine/graphics/DebugDraw.h"
#include <algorithm>
#include <cmath>

namespace {
    // 取る判定は見た目どおり：コイン（円盤の半径）と、恐竜の体（足元から頭までの縦の筒）が触れたら取る。
    //   前は「足元から 1.2m の球」で、頭より上・体の横 1m 先のコインまで取れていた
    const float kCoinRadius  = 0.32f; // コインの半径(m)。見た目の円盤（SetScale の 0.32）と同じ
    const float kBodyRadius  = 0.38f; // 恐竜の体の横の半径(m)（player.gltf の胴の太さ）
    const float kBodyHeight  = 1.45f; // 恐竜の足元から頭のてっぺんまで(m)
    const float kSpinSpeed   = 2.5f;  // 回転速度(rad/s)
    const float kBobAmount   = 0.08f; // 上下のふわふわ(m)
}

void CoinSystem::Sync(const std::vector<CoinData>& coinDatas, const std::vector<SplineRail>& rails){
    coins_.clear();
    collected_ = 0;
    for ( const auto& data : coinDatas ) {
        if ( data.rail < 0 || data.rail >= ( int ) rails.size() ) continue;
        const SplineRail& rail = rails[data.rail];
        if ( rail.GetLength() <= 0.0f ) continue;
        Coin coin;
        Vector3 p = rail.GetPositionByDistance(std::clamp(data.dist, 0.0f, rail.GetLength()));
        coin.pos = { p.x, p.y + data.height, p.z };
        coin.obj = Obj3d::Create("coin");
        if ( coin.obj ) {
            coin.obj->SetTranslation(coin.pos);
            coin.obj->SetScale({ 0.32f, 0.32f, 0.10f }); // 平たい円盤＝コインらしく
            coin.obj->Update();
        }
        coins_.push_back(std::move(coin));
    }
}

void CoinSystem::ResetPlay(){
    collected_ = 0;
    for ( auto& coin : coins_ ) { coin.taken = false; }
}

void CoinSystem::Update(const Vector3& playerPos, bool playMode, float dt){
    spinT_ += dt;
    for ( auto& coin : coins_ ) {
        if ( coin.taken || !coin.obj ) continue;
        // くるくる回転＋ふわふわ上下（拾えるものだと一目で分かる動き）
        coin.obj->SetRotation({ 0.0f, spinT_ * kSpinSpeed, 0.0f });
        coin.obj->SetTranslation({ coin.pos.x,
                                   coin.pos.y + std::sin(spinT_ * 2.0f + coin.pos.x) * kBobAmount,
                                   coin.pos.z });
        coin.obj->Update();
        if ( playMode && Touches(playerPos, CurrentPos(coin)) ) {
            coin.taken = true;
            ++collected_;
        }
    }
}

// 今の（ふわふわ上下した）コインの中心
Vector3 CoinSystem::CurrentPos(const Coin& coin) const{
    return { coin.pos.x, coin.pos.y + std::sin(spinT_ * 2.0f + coin.pos.x) * kBobAmount, coin.pos.z };
}

// 恐竜の体（足元から上へ伸びる縦の筒）とコイン（球）が触れているか
bool CoinSystem::Touches(const Vector3& foot, const Vector3& center){
    const float dx = center.x - foot.x, dz = center.z - foot.z;
    const float side = ( std::max )( std::sqrt(dx * dx + dz * dz) - kBodyRadius, 0.0f );    // 筒の横から外れている分
    const float below = foot.y - center.y, above = center.y - ( foot.y + kBodyHeight );
    const float up = ( std::max )( ( std::max )( below, above ), 0.0f );                  // 筒の上下から外れている分
    return side * side + up * up < kCoinRadius * kCoinRadius;
}

// 当たり判定を表示：まだ取っていないコインの球
void CoinSystem::DrawHitShapes(const Vector4& color) const{
    DebugDraw* draw = DebugDraw::GetInstance();
    for ( const auto& coin : coins_ ) {
        if ( !coin.taken ) { draw->Sphere(CurrentPos(coin), kCoinRadius, color, 12); }
    }
}

// 当たり判定を表示：コインを取る時の恐竜の体（縦の筒）
void CoinSystem::DrawPickupBody(const Vector3& foot, const Vector4& color){
    DebugDraw* draw = DebugDraw::GetInstance();
    const int kSegments = 16;
    Vector3 prevLow {}, prevHigh {};
    for ( int i = 0; i <= kSegments; ++i ) {
        const float angle = 6.2831853f * i / kSegments;
        const Vector3 low { foot.x + std::cos(angle) * kBodyRadius, foot.y, foot.z + std::sin(angle) * kBodyRadius };
        const Vector3 high { low.x, foot.y + kBodyHeight, low.z };
        if ( i > 0 ) { draw->Line(prevLow, low, color); draw->Line(prevHigh, high, color); }
        if ( i % 4 == 0 ) { draw->Line(low, high, color); }
        prevLow = low; prevHigh = high;
    }
}

void CoinSystem::Draw() const{
    for ( const auto& coin : coins_ ) {
        if ( !coin.taken && coin.obj ) { coin.obj->Draw(); }
    }
}
