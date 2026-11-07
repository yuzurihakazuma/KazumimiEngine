#pragma once
// =====================================================================
//  CraftLighting：光のプロフィール（ステージごとの太陽の向き・色・強さ）。
//   ステージの environment.lighting に名前（"day" / "evening"）を書くと、そのシーンの間だけ光を切り替える。
//   紙・フェルトの切り絵は正面から光が当たらないと暗く沈むので、真上からではなく手前上から当てる。
//   点光源・スポットライトは場所でムラが出るので切る。シーンを出る時に元へ戻す
// =====================================================================
#include "engine/3d/obj/Obj3dCommon.h"

#include <string>

namespace CraftLighting {
    // 切り替える前の光（戻す時に使う）
    struct Saved {
        Obj3dCommon::DirectionalLightData directional {};
        float pointIntensity = 0.0f;
        float spotIntensity = 0.0f;
        bool  valid = false;
    };
    // 名前の光に切り替える（知らない名前なら何もしない＝false）
    bool Apply(const std::string& profile, Saved& saved);
    void Restore(Saved& saved);
    // 選べる名前（インスペクター用）
    inline constexpr const char* kProfiles[] = { "day", "evening" };
}
