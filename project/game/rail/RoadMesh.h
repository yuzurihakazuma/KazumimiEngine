#pragma once
#include "engine/math/struct.h"
#include "game/rail/road/RoadMeshBuilder.h"
#include "game/rail/road/RoadRenderSlots.h"
#include <vector>

class Camera;
class SplineRail;
struct RoadCut;
struct RoadJunction;

// =====================================================================
//  RoadMesh：レールの下に敷く「クラフト風の道」の管理クラス（道システム設計書 §4-§5）。
//   ・レール1本 = 1メッシュ（= 1ドローコール）。SplineRail::FrameCache の
//     フレーム（位置+right/up/tangent）に12頂点の断面プロファイルを掃引する
//   ・曲率適応サンプリング／内側折返しの溶接／坂UV切替（ヒステリシス＋二重リング）
//   ・穴区間は面を張らず、切り口に暗色の奈落フタ。手前後 1m の上面は
//     危険帯テクスチャ（赤ストライプ）に切り替える（仕様書_穴区間 §2）
//   ・接続ノード（溶接コーナー/T字/十字）は任意角度の「ジャンクションパッチ」
//     （上面扇+ベベル+壁+底）をその場で生成して繋ぐ（GUIDE_ジャンクション生成）。
//     ゆるい2本溶接（150°以上）だけは掃引コネクタで連続的に繋ぐ
//   ・接続ノードにはプラレール風の road_joint を自動配置（保存しない派生データ）
//   ・roadMode=1（道なし）のレールはスキップし、接続相手からも除外する（§4）
//   ・GPUバッファはレール毎の固定容量スロットを使い回す（編集中の作り直しゼロ）。
//     simple=true の軽量ビルド（ドラッグ中用）はリング1m固定・交差点/キャップ省略
//   ・動くレールへは「基準位置 + animOffset」で毎フレーム追従（再生成不要）
//
//  実体は road/ 以下のクラスに分けてあり、ここは「どの順で作って、どこへ出すか」だけを持つ：
//    RoadJunctionDetector（検出＋t_cut）→ RoadJunctionPatchBuilder（パッチ）／
//    RoadSweepBuilder（掃引）→ RoadRenderSlots（GPUスロット・追従・描画）
// =====================================================================
class RoadMesh {
public:
    // レールに沿って道を生成し直す（RailField::Sync の直後に呼ぶ）。
    // simple=true はドラッグ中の軽量プレビュー（マウスアップ後に false で本生成する）
    void Build(const std::vector<SplineRail>& rails, Camera* camera, bool simple = false);

    // 毎フレーム：動くレールへの追従＋カメラ行列の焼き直し（Edit/Play共通で呼ぶ）
    void Update(const std::vector<SplineRail>& rails);

    void Draw() const;

    int  TileCount() const { return ( int ) slots_.TileCount(); }

    // 直近 Build の総頂点数/三角形数（負荷確認用。パフォーマンスパネルに表示する）
    int  VertexCount() const { return ( int ) slots_.VertexCount(); }
    int  TriangleCount() const { return ( int ) slots_.TriangleCount(); }
    bool IsVisible() const { return visible_; }
    void SetVisible(bool v){ visible_ = v; } // デバッグUIから道のON/OFFを切り替える用

    // ジョイント（road_joint）の表示：0=エディタのみ / 1=常に / 2=非表示
    void SetJointVisible(int mode){ jointVisible_ = mode; }

    // 穴の警告帯（赤ストライプ）を敷く長さ(m)。変更後は Build で反映
    void  SetWarnLength(float m){ warnLength_ = m; }
    float GetWarnLength() const{ return warnLength_; }
    // 曲がり角の形：0=自動（角度で判定）/ 1=いつも丸広場（アーク）/ 2=丸なし（マイター）
    void  SetCornerStyle(int style){ cornerStyle_ = style; }
    int   GetCornerStyle() const{ return cornerStyle_; }

    // 背面カリングの切替（true=両面描画・従来 / false=背面カリングでオーバードロー削減）。
    // 既存スロットにも即時反映される（再生成不要）
    void SetCullNone(bool cullNone){ slots_.SetCullNone(cullNone); }
    bool IsCullNone() const{ return slots_.IsCullNone(); }

private:
    // ジャンクション（丸広場/溶接/T字/十字）の検出→パッチ・コネクタ生成→ジョイント配置。
    // 各レールの「掃引しない区間」も cuts に決まる
    void BuildJunctions(const std::vector<SplineRail>& rails, std::vector<std::vector<RoadCut>>& cuts);

    // ジャンクションのジョイント（road_joint）配置
    void PlaceJoints(const std::vector<SplineRail>& rails, const RoadJunction& junc);
    void PlaceJointPiece(const std::vector<SplineRail>& rails, int railIdx, const Vector3& railPos, float yaw);

    RoadRenderSlots slots_; // 掃引メッシュ/パッチ/ジョイントの GPU スロット

    bool visible_ = true;
    int  jointVisible_ = 1; // 0=エディタのみ / 1=常に / 2=非表示

    float warnLength_ = 1.0f; // 穴の手前後に危険帯（赤ストライプ・上面のみ）を敷く長さ(m)
    int   cornerStyle_ = 0;   // 曲がり角の形（0=自動/1=いつも丸広場/2=丸なし）

    // 静的レールのジョイントをまとめるベイク先（Build 中だけ使い、最後に EmitMesh する）
    RoadMeshBuilder bakeJoints_;
};
