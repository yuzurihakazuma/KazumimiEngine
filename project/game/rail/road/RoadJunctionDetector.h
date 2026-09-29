#pragma once
#include "engine/math/struct.h"
#include "game/rail/road/RoadJunction.h"
#include <array>
#include <vector>

class SplineRail;

// =====================================================================
//  RoadJunctionDetector：レール群から「道をつなぐ場所」を検出するクラス（GUIDE_ジャンクション生成）。
//   ・端の丸広場（endPlaza）… 腕1本のジャンクション
//   ・端点溶接（2本）… 150°以上のゆるい角は掃引コネクタ / 鋭い角はパッチ
//   ・T字分岐（branchPoints）と本体×本体の交差 … 常にパッチ
//   検出したジャンクションの t_cut（入口までの距離）を決めて cuts に登録する
//   （掃引側はその範囲に面を張らない）。メッシュ生成はしない＝結果を渡すだけ
//   ・roadMode=1（道なし）／非表示のレールは接続相手からも除外する（§4）
// =====================================================================
class RoadJunctionDetector {
public:
    // ゆるい2本溶接：掃引コネクタの5点と、中央に置くジョイント用の2本腕ジャンクション
    struct Connector {
        std::array<Vector3, 5> nodes {}; // 助走/入口/ノード/出口/助走（RoadSweepBuilder::BuildConnector へ）
        RoadJunction joint;              // ジョイント配置用（followRail はコネクタメッシュの追従先も兼ねる）
    };

    // 道を敷く対象のレールか（roadMode=1「道なし」・見えないレールは接続相手としても数えない）
    static bool IsRoadRail(const std::vector<SplineRail>& rails, int idx);

    // 全検出を行う。cuts は rails.size() ぶん確保済みであること（各レールの切り詰め範囲を追記する）
    void Detect(const std::vector<SplineRail>& rails, std::vector<std::vector<RoadCut>>& cuts);

    // 検出結果（Detect の呼び出し順＝生成順：丸広場 → コネクタ → パッチ）
    const std::vector<RoadJunction>& Plazas() const{ return plazas_; }
    const std::vector<Connector>&    Connectors() const{ return connectors_; }
    const std::vector<RoadJunction>& Junctions() const{ return junctions_; } // t_cut 決定済み

private:
    // 端の丸広場（エディタ指定）
    void DetectEndPlazas(const std::vector<SplineRail>& rails, std::vector<std::vector<RoadCut>>& cuts);
    // A) 端点溶接（2本の共有ノード）
    void DetectWelds(const std::vector<SplineRail>& rails, std::vector<std::vector<RoadCut>>& cuts);
    // B) T字分岐（branchPoints）
    void DetectBranches(const std::vector<SplineRail>& rails);
    // C) 本体×本体の交差（乗り換えポイント）
    void DetectCrossings(const std::vector<SplineRail>& rails);

    // 1ジャンクションの t_cut 計算（ウェッジのマイター交点）＋Cut登録
    static void ComputeArmCuts(const std::vector<SplineRail>& rails, RoadJunction& junc,
                               std::vector<std::vector<RoadCut>>& cuts);

    std::vector<RoadJunction> plazas_;
    std::vector<Connector>    connectors_;
    std::vector<RoadJunction> junctions_;
};
