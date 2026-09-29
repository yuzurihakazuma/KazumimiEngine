#include "game/rail/RailMotion.h"
#include "engine/rail/SplineRail.h"

#include <algorithm>
#include <cmath>

namespace {
    const float kPi    = 3.14159265f;
    const float kTwoPi = 2.0f * kPi;

    Vector3 Scaled(const Vector3& amp, float w){ return { amp.x * w, amp.y * w, amp.z * w }; }

    // 後から出現する道：発動レールに乗ったら出現（以後は下からせり上がるアニメで現れる）
    void UpdateAppear(SplineRail& rail, float dt, int ridingRail){
        if ( rail.appearTrigger < 0 ) return;
        if ( !rail.appeared && ( ridingRail == rail.appearTrigger || ridingRail == RailMotion::kStartAll ) ) {
            rail.appeared = true;
        }
        if ( rail.appeared && rail.appearAnim < 1.0f ) {
            rail.appearAnim = ( std::min )( 1.0f, rail.appearAnim + dt / 0.6f );
        }
    }

    // 1: 端で一時停止つき往復（各端で周期の15%停止。移動はコサインで滑らか）
    float DwellSwing(float u){
        const float dwell = 0.15f;
        const float move  = 0.5f - dwell;
        if ( u < move )        { return -std::cos(( u / move ) * kPi); }           // -1 → +1
        if ( u < 0.5f )        { return 1.0f; }                                     // +端で停止
        if ( u < 0.5f + move ) { return  std::cos(( ( u - 0.5f ) / move ) * kPi); } // +1 → -1
        return -1.0f;                                                               // -端で停止
    }

    // 3: ガイドレール追従：別レールの経路に沿って動く（ヨッシー1-1の列車式）。
    //   周期=1周期の秒数（一周ループ=1周 / 往復=行って帰る1往復）/ 位相=スタート位置 /
    //   区間=ガイドのここからここまで。
    //   guideMode 0=一周ループ（終点→始点へ戻る） / 1=往復（コサイン緩急で滑らかに折り返す） / 2=片道
    void FollowGuide(SplineRail& rail, const std::vector<SplineRail>& rails, float period, float u){
        int g = rail.guideRail;
        if ( g < 0 || g >= ( int ) rails.size() || &rails[g] == &rail || rails[g].GetLength() <= 0.0f ) {
            rail.animOffset = { 0.0f, 0.0f, 0.0f };
            return;
        }
        const SplineRail& guide = rails[g];
        const float guideLen = guide.GetLength();
        float s0 = std::clamp(rail.guideStart, 0.0f, guideLen);
        float s1 = ( rail.guideEnd < 0.0f ) ? guideLen : std::clamp(rail.guideEnd, 0.0f, guideLen);
        if ( s1 < s0 ) { std::swap(s0, s1); }
        if ( s1 - s0 < 0.01f ) { s0 = 0.0f; s1 = guideLen; } // 区間が潰れていたら全区間

        const float dwell = ( std::max )( rail.guideDwell, 0.0f );
        float w;
        if ( rail.guideMode == 1 ) {
            // 往復：行き→(停車)→帰り→(停車)。移動はコサイン緩急＝両端で速度0。
            //   周期=1往復の移動秒数（停車時間は別枠で足される）
            float half = period * 0.5f;
            float cycle = period + dwell * 2.0f;
            float t = std::fmod(rail.motionTime + rail.motionPhase * cycle, cycle);
            if      ( t < half )           { w = 0.5f - 0.5f * std::cos(( t / half ) * kPi); }                  // 行き 0→1
            else if ( t < half + dwell )   { w = 1.0f; }                                                        // 終点で停車
            else if ( t < period + dwell ) { w = 0.5f + 0.5f * std::cos(( ( t - half - dwell ) / half ) * kPi); } // 帰り 1→0
            else                           { w = 0.0f; }                                                        // 始点で停車
        } else if ( rail.guideMode == 2 ) {
            // 片道：出発まで dwell 秒待ち→走行→到着したら止まる（出発・到着ともコサイン緩急）。
            //   周期=片道の秒数。u は周回でラップしているため経過時間から直接進行度を求める
            float progress = std::clamp(( rail.motionTime - dwell ) / period + rail.motionPhase, 0.0f, 1.0f);
            w = 0.5f - 0.5f * std::cos(progress * kPi);
        } else {
            // 一周ループ：区間の端まで行くと始点へ戻って回り続ける（閉じたガイド向け）
            w = u;
        }
        const float d = s0 + ( s1 - s0 ) * w;
        Vector3 p  = guide.GetPositionByDistance(d);
        Vector3 p0 = guide.GetPositionByDistance(s0);
        rail.animOffset = { p.x - p0.x, p.y - p0.y, p.z - p0.z };

        // 列車式：ガイドの進行方向に足場の向きを合わせて転回（縦向き⇔横向きも滑らか）。
        //   ほぼ真上/真下の区間では向きを保つ（水平成分が無いとヨーが定まらないため）
        if ( rail.guideAlign != 1 ) return;
        Vector3 guideTan = guide.GetTangentByDistance(d);
        float horizLen = std::sqrt(guideTan.x * guideTan.x + guideTan.z * guideTan.z);
        if ( horizLen <= 0.15f ) return;
        float targetYaw = std::atan2(guideTan.x, guideTan.z) - rail.restYaw;
        // 最短角で連続に追従（±180°の境目でクルッと一回転しない）
        float deltaYaw = targetYaw - rail.animYaw;
        while ( deltaYaw >  kPi ) { deltaYaw -= kTwoPi; }
        while ( deltaYaw < -kPi ) { deltaYaw += kTwoPi; }
        rail.animYaw += deltaYaw;
    }
}

bool RailMotion::Advance(std::vector<SplineRail>& rails, float dt, int ridingRail){
    bool anyMotion = false;
    for ( size_t railIndex = 0; railIndex < rails.size(); ++railIndex ) {
        SplineRail& rail = rails[railIndex];
        UpdateAppear(rail, dt, ridingRail);
        if ( !rail.HasMotion() ) continue;
        anyMotion = true;
        // 乗ったら動き出す：発動するまで待機（animOffsetは基準位置のまま）。
        //   一度発動したら降りても動き続ける（ヨッシーの列車と同じ）。
        //   エディタのプレビュー（kStartAll）では全レール強制発動＝動きを確認できる
        if ( rail.motionTrigger == 1 && !rail.motionStarted ) {
            if ( ridingRail != ( int ) railIndex && ridingRail != kStartAll ) continue;
            rail.motionStarted = true;
        }
        rail.motionTime += dt; // 発動してからの経過時間（最初から動くレールはPlay開始からの時間）
        const float period = ( rail.motionPeriod > 0.1f ) ? rail.motionPeriod : 0.1f;
        float u = rail.motionTime / period + rail.motionPhase;
        u -= std::floor(u); // 0〜1 の周期位置

        switch ( rail.motionType ) {
        case 1:  rail.animOffset = Scaled(rail.motionAmp, DwellSwing(u)); break;
        case 2: { // 円運動（XZ楕円＋Y。半径は amp の各成分。amp.x と amp.z で円になる）
            const float th = u * kTwoPi;
            const Vector3& amp = rail.motionAmp;
            rail.animOffset = { amp.x * std::cos(th), amp.y * std::sin(th), amp.z * std::sin(th) };
            break;
        }
        case 3:  FollowGuide(rail, rails, period, u); break;
        default: rail.animOffset = Scaled(rail.motionAmp, std::sin(u * kTwoPi)); break; // 0: サイン往復
        }
    }
    return anyMotion;
}

void RailMotion::Reset(std::vector<SplineRail>& rails){
    for ( auto& rail : rails ) {
        rail.animOffset = { 0.0f, 0.0f, 0.0f };
        rail.motionTime = 0.0f;
        rail.motionStarted = false;
        rail.appeared   = ( rail.appearTrigger < 0 );
        rail.appearAnim = rail.appeared ? 1.0f : 0.0f;
        rail.animYaw = 0.0f; // 列車式の回転も基準向きへ戻す
    }
}
