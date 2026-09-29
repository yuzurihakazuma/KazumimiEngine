#include "game/rail/RailField.h"
#include "game/rail/RailConnections.h"

#include "engine/utils/EditorManager.h"
#include "engine/graphics/DebugDraw.h"

#include <cmath>
#include <algorithm>

namespace {
    // エディタの並列配列（レールごとの設定）から i 番目を読む。足りなければ既定値
    template <class T, class U>
    T ValueAt(const std::vector<U>& values, size_t i, T fallback){
        return ( i < values.size() ) ? static_cast< T >( values[i] ) : fallback;
    }
}

// エディタ保持の最新レール節点から rails_ を作り直す。
void RailField::Sync(Camera* camera, uint32_t whiteTexIndex){
    camera_ = camera;
    whiteTexIndex_ = whiteTexIndex;

    const EditorManager& editor = *EditorManager::GetInstance();
    BuildRailsFromEditor(editor);
    ApplyMotionSettings(editor);
    ApplyRailTypes(editor);
    RailConnections::Build(rails_); // 接続・分岐・ループ（動くレールは静的連結から除外）
    ApplySurfaceSettings(editor);
    ResolveStartGoal(editor);

    markers_.Build(rails_, camera_, whiteTexIndex_);
    lastVersion_ = editor.GetRailEditVersion();
}

void RailField::BuildRailsFromEditor(const EditorManager& editor){
    const auto& lines     = editor.GetEditorRailLines();
    // 線のつなぎ方（0=スプライン/1=直線）は距離テーブルの計測結果を変えるので、
    // 必ず BuildDistanceTable より先に割り当てる
    const auto& lineModes = editor.GetEditorRailLineModes();
    rails_.clear();
    for ( size_t i = 0; i < lines.size(); ++i ) {
        SplineRail rail;
        rail.nodes = lines[i];
        rail.lineMode = ValueAt(lineModes, i, 0);
        rail.BuildDistanceTable();
        rails_.push_back(rail);
    }
}

void RailField::ApplyMotionSettings(const EditorManager& editor){
    const auto& motions        = editor.GetEditorRailMotions();
    const auto& motionTypes    = editor.GetEditorRailMotionTypes();
    const auto& guideRails     = editor.GetEditorRailGuideRails();
    const auto& motionPhases   = editor.GetEditorRailMotionPhases();
    const auto& guideStarts    = editor.GetEditorRailGuideStarts();
    const auto& guideEnds      = editor.GetEditorRailGuideEnds();
    const auto& guideModes     = editor.GetEditorRailGuideModes();
    const auto& motionTriggers = editor.GetEditorRailMotionTriggers();
    const auto& appearTriggers = editor.GetEditorRailAppearTriggers();
    const auto& guideAligns    = editor.GetEditorRailGuideAligns();
    const auto& guideDwells    = editor.GetEditorRailGuideDwells();
    for ( size_t i = 0; i < rails_.size(); ++i ) {
        SplineRail& rail = rails_[i];
        if ( i < motions.size() ) {
            rail.motionAmp    = { motions[i].x, motions[i].y, motions[i].z };
            rail.motionPeriod = ( motions[i].w > 0.1f ) ? motions[i].w : 0.1f;
        }
        rail.motionType    = ValueAt(motionTypes, i, 0);
        rail.guideRail     = ValueAt(guideRails, i, -1);
        rail.motionPhase   = ValueAt(motionPhases, i, 0.0f);
        rail.guideStart    = ValueAt(guideStarts, i, 0.0f);
        rail.guideEnd      = ValueAt(guideEnds, i, -1.0f);
        rail.guideMode     = ValueAt(guideModes, i, 0);
        rail.guideDwell    = ValueAt(guideDwells, i, 0.0f);
        rail.motionTrigger = ValueAt(motionTriggers, i, 0);
        rail.appearTrigger = ValueAt(appearTriggers, i, -1);
        rail.guideAlign    = ValueAt(guideAligns, i, 0);
        // 列車式回転の基準：基準位置でのレール中点と向きを1回だけ計測（この時点で回転・移動はゼロ）
        if ( rail.nodes.size() >= 2 && rail.GetLength() > 0.0f ) {
            float mid = rail.GetLength() * 0.5f;
            rail.animPivot = rail.GetPositionByDistance(mid);
            Vector3 midTan = rail.GetTangentByDistance(mid);
            float horizLen = std::sqrt(midTan.x * midTan.x + midTan.z * midTan.z);
            rail.restYaw = ( horizLen > 1e-4f ) ? std::atan2(midTan.x, midTan.z) : 0.0f;
        }
    }
    RailMotion::Reset(rails_); // 動きの経過・発動・出現の状態を基準へ
}

// 0/1 ならそれを使い、-1(自動)や未設定は主軸で自動判定
void RailField::ApplyRailTypes(const EditorManager& editor){
    const auto& types = editor.GetEditorRailTypes();
    for ( size_t i = 0; i < rails_.size(); ++i ) {
        int type = ValueAt(types, i, -1);
        if ( type == 0 )      rails_[i].type = SplineRail::RailType::Horizontal;
        else if ( type == 1 ) rails_[i].type = SplineRail::RailType::Vertical;
        else                  rails_[i].AutoDetectType();
    }
}

// 地面タイプ・穴・表示フラグ・道モード・端の丸広場・片方向・速度倍率
void RailField::ApplySurfaceSettings(const EditorManager& editor){
    const auto& grounds   = editor.GetEditorRailGroundTypes();
    const auto& holes     = editor.GetEditorRailNodeHoles();
    const auto& visible   = editor.GetEditorRailVisible();
    const auto& roadModes = editor.GetEditorRailRoadModes();
    const auto& endPlazas = editor.GetEditorRailEndPlazas();
    const auto& oneWays   = editor.GetEditorRailOneWay();
    const auto& speedMuls = editor.GetEditorRailSpeedMuls();
    for ( size_t i = 0; i < rails_.size(); ++i ) {
        SplineRail& rail = rails_[i];
        rail.groundType = static_cast< SplineRail::GroundType >( ValueAt(grounds, i, 0) );
        if ( i < holes.size() ) rail.nodeHole = holes[i];
        else                    rail.nodeHole.clear();
        rail.visible  = ValueAt(visible, i, 1) != 0;
        rail.roadMode = ValueAt(roadModes, i, 0);
        rail.endPlaza = ValueAt(endPlazas, i, 0);
        rail.oneWay   = ValueAt(oneWays, i, 0);
        rail.speedMul = ( i < speedMuls.size() && speedMuls[i] > 0.05f ) ? speedMuls[i] : 1.0f;
    }
}

// スタート/ゴール地点（レール番号＋ノード番号 → レール上の距離へ変換）
void RailField::ResolveStartGoal(const EditorManager& editor){
    auto nodeToDist = [&](int rail, int node, int& outRail, float& outDist){
        outRail = 0; outDist = 0.0f;
        if ( rail < 0 || rail >= ( int ) rails_.size() ) { outRail = -1; return; }
        outRail = rail;
        int maxNode = ( int ) rails_[rail].nodes.size() - 1;
        int n = std::clamp(node, 0, ( std::max )( maxNode, 0 ));
        outDist = rails_[rail].GetDistanceFromT(( float ) n);
    };
    nodeToDist(editor.GetEditorStartRail(), editor.GetEditorStartNode(), startRail_, startDist_);
    if ( startRail_ < 0 ) { startRail_ = 0; startDist_ = 0.0f; } // 未設定はレール0の先頭
    nodeToDist(editor.GetEditorGoalRail(), editor.GetEditorGoalNode(), goalRail_, goalDist_);
}

// 動くレールの時間を進めて animOffset を更新し、緑線マーカーも追従させる
void RailField::UpdateMotion(float dt, int ridingRail){
    if ( RailMotion::Advance(rails_, dt, ridingRail) ) { markers_.UpdatePositions(rails_); }
}

void RailField::ResetMotion(){
    RailMotion::Reset(rails_);
    markers_.UpdatePositions(rails_);
}

// 動くレールのエディタプレビュー（OFFにした瞬間に基準位置へ戻す）
void RailField::UpdateEditorPreview(bool enabled){
    if ( enabled ) { UpdateMotion(1.0f / 60.0f, kMotionStartAll); }
    else if ( prevEditorPreview_ ) { ResetMotion(); }
    prevEditorPreview_ = enabled;
}

// 「乗ったら動き出す」で待機中のリフトへ金色の「！」目印（乗れば動くことが一目で分かる）
void RailField::DrawWaitingLiftMarkers(float dt){
    liftMarkerTime_ += dt;
    const Vector4 gold { 1.0f, 0.85f, 0.2f, 1.0f };
    for ( const auto& rail : rails_ ) {
        if ( !rail.HasMotion() || rail.motionTrigger != 1 || rail.motionStarted ) continue;
        if ( rail.nodes.size() < 2 || !rail.visible ) continue;
        Vector3 markPos = rail.GetPositionByDistance(rail.GetLength() * 0.5f);
        float bob = std::sin(liftMarkerTime_ * 4.0f) * 0.08f;
        float baseY = markPos.y + 1.4f + bob;
        DebugDraw::GetInstance()->Line({ markPos.x, baseY + 0.5f, markPos.z },
                                       { markPos.x, baseY + 0.18f, markPos.z }, gold);
        DebugDraw::GetInstance()->Sphere({ markPos.x, baseY, markPos.z }, 0.06f, gold, 6);
    }
}
