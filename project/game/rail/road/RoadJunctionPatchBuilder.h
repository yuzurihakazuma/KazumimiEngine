#pragma once
#include "engine/math/struct.h"
#include "engine/rail/SplineRail.h"
#include "game/rail/road/RoadJunction.h"
#include <vector>

class RoadMeshBuilder;

// =====================================================================
//  RoadJunctionPatchBuilder：ジャンクション1個ぶんの「パッチ」メッシュを生成するクラス
//  （GUIDE_ジャンクション生成 §4）。
//   ・上面扇＋ベベル＋壁＋底 を任意角度・任意本数の腕に対して張る
//   ・入口断面はレールの実フレームから取る → 掃引側の入口リングと完全一致（継ぎ目ゼロ）
//   ・隣り合う腕の間（ウェッジ）は角度で マイター／ほぼ直進／外周円弧 を選ぶ。
//     エディタの「曲がり角の形」（cornerStyle）で上書きできる
//   ・腕1本（端の丸広場）は 360°の円弧ウェッジ＝ヨッシー風の丸い広場になる
// =====================================================================
class RoadJunctionPatchBuilder {
public:
    // cornerStyle: 0=自動（角度で判定）/ 1=いつも丸広場（アーク）/ 2=丸なし（マイター）
    explicit RoadJunctionPatchBuilder(int cornerStyle) : cornerStyle_(cornerStyle) {}

    // 1ジャンクションのパッチを out へ追加する（arms は角度ソート済み・cutS 決定済みであること）
    void Build(const std::vector<SplineRail>& rails, const RoadJunction& junc, RoadMeshBuilder& out) const;

private:
    enum class WedgeMode { Miter, Straight, Arc };
    struct Wedge { WedgeMode mode = WedgeMode::Straight; int arcSteps = 2; };

    // パッチ1個ぶんの作業データ（各腕の入口フレーム＋ウェッジ形状）
    struct Patch {
        struct ArmGeo { SplineRail::RailFrame frame; float leftSign; };
        const RoadJunction* junc = nullptr;
        std::vector<ArmGeo> geo;
        std::vector<Wedge>  wedges;
    };

    // 各ウェッジのモードと円弧分割数を先に決める（内外の点列数を一致させるため）
    Wedge DecideWedge(const Vector3& di, const Vector3& dj) const;

    // 腕 i の入口断面上の点（lat = 左方向を+とした符号付き幅、h = 断面高さ）
    static Vector3 EntryPoint(const Patch& p, int i, float lat, float h);
    // ウェッジ (i → i+1) の境界点列。幅 w・高さ h
    static std::vector<Vector3> WedgePath(const Patch& p, int i, float w, float h);
    // 全ウェッジの境界をつないだ閉ループ
    static std::vector<Vector3> Loop(const Patch& p, float w, float h);

    // 上面/底面：境界ループをノード中心から扇状に張る（底面は裏向き＝巻き順を反転）
    static void EmitFan(const Patch& p, float w, float h, const Vector3& normal, float v0, float vSpan,
                        bool flip, RoadMeshBuilder& out);
    // ベベル＋壁（ウェッジ部分のみ。入口には張らない＝道の断面がそのまま入る）
    static void EmitBevelAndWalls(const Patch& p, RoadMeshBuilder& out);

    int cornerStyle_ = 0;
};
