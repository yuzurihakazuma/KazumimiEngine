#pragma once
#include "engine/math/struct.h"
#include "engine/rail/SplineRail.h"
#include "game/rail/road/RoadJunction.h"
#include <array>
#include <vector>

class RoadMeshBuilder;

// =====================================================================
//  RoadSweepBuilder：レール1本ぶんの「掃引メッシュ」を生成するクラス（道システム設計書 §4）。
//   ・SplineRail::FrameCache のフレーム（位置+right/up/tangent）に12頂点の断面を掃引する
//   ・曲率適応サンプリング／内側折返しの溶接／坂UV切替（ヒステリシス＋二重リング）
//   ・穴区間は面を張らず、切り口に暗色の「奈落フタ」。手前後 warnLength の上面は
//     危険帯テクスチャ（赤ストライプ）に切り替える（仕様書_穴区間 §2）
//   ・cuts（ジャンクションに譲る区間）は面を張らず、境界に必ずリングを置いて入口と一致させる
//   ・simple=true はドラッグ中の軽量プレビュー（リング1m固定・坂UV/穴/溶接なし）
//   ・ゆるい2本溶接をつなぐ「掃引コネクタ」（その場合成の小レール）もここで掃引する
// =====================================================================
class RoadSweepBuilder {
public:
    explicit RoadSweepBuilder(float warnLength) : warnLength_(warnLength) {}

    // レール1本ぶんの掃引メッシュを out へ追加する（cuts の区間は張らない）。
    //   capFront/capBack: 自由端に平らな暗色フタを張る（丸い road_end の代わり）
    void Build(const SplineRail& rail, const std::vector<RoadCut>& cuts, bool simple,
               bool capFront, bool capBack, RoadMeshBuilder& out) const;

    // ゆるい溶接の掃引コネクタ：5点（助走/入口/ノード/出口/助走）から小さなレール（Catmull-Rom）を
    // 合成し、同じ断面で掃引する。助走区間は本線と重なるので張らない
    void BuildConnector(const std::array<Vector3, 5>& nodes, RoadMeshBuilder& out) const;

private:
    // 断面を置く位置（リング）。slope=坂UV / danger=危険帯UV
    struct Ring { float s; bool slope; bool danger; };
    // リング間の区間の分類：描く / 穴 / ジャンクションの切り詰め / 二重リング
    enum class Seg { Draw, Hole, Cut, Degen };
    using Holes = std::vector<SplineRail::HoleInterval>;

    // 距離 s が危険帯（穴の手前後 warnLength_）か
    bool DangerAt(const Holes& holes, float s) const;

    // 1. リング位置の決定（簡易＝固定1m / 本生成＝曲率適応＋坂UVヒステリシス＋境界リング）
    static std::vector<Ring> PlaceSimpleRings(float len);
    std::vector<Ring> PlaceAdaptiveRings(const SplineRail& rail, float len, const Holes& holes) const;
    void InsertBoundaryRings(std::vector<Ring>& rings, float len, const std::vector<RoadCut>& cuts,
                             const Holes& holes) const;

    // 3. 区間の分類
    static std::vector<Seg> ClassifySegments(const std::vector<Ring>& rings, const std::vector<RoadCut>& cuts,
                                             const Holes& holes);

    // 5. 切り口（穴の境界・自由端）に暗色の平らなフタを張る
    static void EmitDarkCap(const SplineRail& rail, float s, const std::array<Vector3, 12>& ringPos,
                            float facing, RoadMeshBuilder& out);

    float warnLength_ = 1.0f; // 穴の手前後に危険帯（赤ストライプ・上面のみ）を敷く長さ(m)
};
