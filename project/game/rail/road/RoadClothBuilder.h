#pragma once
#include "engine/math/struct.h"
#include <vector>

class SplineRail;
class RoadMeshBuilder;

// =====================================================================
//  RoadClothBuilder：レール1本ぶんの「布の道」（布ロード企画書 フェーズ3）を生成するクラス。
//   ・移動に使うのと同じレール（弧長 s と RMF フレーム）から作るので、布と歩ける所がずれない
//   ・断面は6点：左のすそ（地面の中へ少し）→ 左のめくれ → 左のステッチの内側 → 右 … の帯。
//     地面から lift だけ浮かせる（Z ファイティング防止）。布＋土台は土台の上面に載る
//   ・UV：u = 弧長（曲がり・坂でも柄が伸び縮みしない）/ v = 幅の方向（端のステッチは v=0.1/0.9）
//   ・端：つながっている端は端の向きへ半幅ぶん伸ばして、相手の布と重ねる（継ぎ目が重なりとして見える）。
//     つながっていない端は丸く、丸広場の指定があれば円く敷く。穴・展開の途中で切れる所は平らな切り口
//   ・visible＝敷く範囲（展開率。始点からの範囲の並び）。穴の区間は自動で抜く
//   ・頂点はレールの「基準位置」（FrameCache）で作る。動くレールは RoadRenderSlots が毎フレームずらす
// =====================================================================
class RoadClothBuilder {
public:
    struct Range { float s0, s1; };

    // extraLift：重なり順の高さ（つなぎ目で相手と同じ高さにならないよう、呼ぶ側が決める）
    void Build(const std::vector<SplineRail>& rails, int railIndex, const std::vector<Range>& visible,
               float extraLift, RoadMeshBuilder& out) const;

    static constexpr float kUvPerMeter = 1.0f / 1.2f; // 布の柄の繰り返し（1.2m ごと）

private:
    struct EndInfo { bool connected; bool plaza; };
    static EndInfo EndOf(const std::vector<SplineRail>& rails, int railIndex, bool front);
};
