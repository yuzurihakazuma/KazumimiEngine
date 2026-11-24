#include "game/rail/RailField.h"
#include "engine/base/TimeManager.h"
#include "game/rail/RailConnections.h"

#include "engine/utils/EditorManager.h"
#include "engine/graphics/DebugDraw.h"

#include <cmath>
#include <algorithm>

using namespace KazumimiEngine;

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
    for ( SplineRail& rail : rails_ ) { RailMotion::SetupRollBoard(rail); } // ころころの転がり出すはし（つながりが決まってから）
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
        rail.SetNodes(lines[i]);
        rail.SetLineMode(ValueAt(lineModes, i, 0));
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
    const auto& trains         = editor.GetEditorRailTrains();
    for ( size_t i = 0; i < rails_.size(); ++i ) {
        SplineRail& rail = rails_[i];
        if ( i < motions.size() ) {
            rail.SetMotionAmp({ motions[i].x, motions[i].y, motions[i].z });
            rail.SetMotionPeriod(( motions[i].w > 0.1f ) ? motions[i].w : 0.1f);
        }
        rail.SetMotionType(ValueAt(motionTypes, i, 0));
        rail.SetGuideRail(ValueAt(guideRails, i, -1));
        rail.SetMotionPhase(ValueAt(motionPhases, i, 0.0f));
        rail.SetGuideStart(ValueAt(guideStarts, i, 0.0f));
        rail.SetGuideEnd(ValueAt(guideEnds, i, -1.0f));
        rail.SetGuideMode(ValueAt(guideModes, i, 0));
        rail.SetGuideDwell(ValueAt(guideDwells, i, 0.0f));
        rail.SetMotionTrigger(ValueAt(motionTriggers, i, 0));
        rail.SetAppearTrigger(ValueAt(appearTriggers, i, -1));
        rail.SetGuideAlign(ValueAt(guideAligns, i, 0));
        const RailTrain train = ( i < trains.size() ) ? trains[i] : RailTrain {};
        rail.SetTrain(train.enabled);
        rail.SetTrainDepart(train.departSec);
        rail.SetTrainReturn(train.returnSec);
        rail.SetTrainEngine(train.engine);
        rail.SetTrainSpeed(( std::max )( train.speed, 0.1f ));
        rail.SetTrainSpeedMuls(train.speedMuls);
        rail.SetIsTrack(false);
        rail.SetBoardTrain(( train.boardFor >= 0 && train.boardFor != ( int ) i ) ? train.boardFor : -1);
        rail.SetBoardKind(train.boardMode == 1 ? 2 : ( rail.Board().boardTrain >= 0 ? 1 : 0 ));
        rail.SetBoardTriggered(false);
        rail.SetBoardOpen(0.0f);
        // 列車式回転の基準：基準位置でのレール中点と向きを1回だけ計測（この時点で回転・移動はゼロ）
        if ( rail.Nodes().size() >= 2 && rail.GetLength() > 0.0f ) {
            float mid = rail.GetLength() * 0.5f;
            rail.SetAnimPivot(rail.GetPositionByDistance(mid));
            Vector3 midTan = rail.GetTangentByDistance(mid);
            float horizLen = std::sqrt(midTan.x * midTan.x + midTan.z * midTan.z);
            rail.SetRestYaw(( horizLen > 1e-4f ) ? std::atan2(midTan.x, midTan.z) : 0.0f);
        }
    }
    // 列車：ガイドは線路（歩けない・つながない）。元の駅と向きを決める
    for ( SplineRail& rail : rails_ ) {
        if ( !rail.IsTrain() || rail.Motion().guideRail >= ( int ) rails_.size() ) { continue; }
        rails_[rail.Motion().guideRail].SetIsTrack(true);
        RailMotion::SetupTrain(rail, rails_);
    }
    for ( SplineRail& rail : rails_ ) { RailMotion::SetupBoard(rail, rails_); } // 渡し板（列車の駅が決まってから）
    RailMotion::Reset(rails_); // 動きの経過・発動・出現の状態を基準へ（列車は元の駅へ）
}

// 0/1 ならそれを使い、-1(自動)や未設定は主軸で自動判定
void RailField::ApplyRailTypes(const EditorManager& editor){
    const auto& types = editor.GetEditorRailTypes();
    for ( size_t i = 0; i < rails_.size(); ++i ) {
        int type = ValueAt(types, i, -1);
        if ( type == 0 )      rails_[i].SetType(SplineRail::RailType::Horizontal);
        else if ( type == 1 ) rails_[i].SetType(SplineRail::RailType::Vertical);
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
    const auto& skins     = editor.GetEditorRailSkins();
    for ( size_t i = 0; i < rails_.size(); ++i ) {
        SplineRail& rail = rails_[i];
        rail.SetGroundType(static_cast< SplineRail::GroundType >( ValueAt(grounds, i, 0) ));
        if ( i < holes.size() ) rail.SetNodeHole(holes[i]);
        else                    rail.ClearNodeHole();
        rail.SetVisible(ValueAt(visible, i, 1) != 0);
        rail.SetRoadMode(ValueAt(roadModes, i, 0));
        rail.SetEndPlaza(ValueAt(endPlazas, i, 0));
        rail.SetOneWay(ValueAt(oneWays, i, 0));
        rail.SetSpeedMul(( i < speedMuls.size() && speedMuls[i] > 0.05f ) ? speedMuls[i] : 1.0f);
        rail.SetSkin(( i < skins.size() ) ? skins[i] : RailSkin {});
        // 列車の足場と線路には道も布も敷かない（列車と線路は TrainVisuals が描く。布は roadMode を見ないので見た目も消す）
        if ( rail.IsTrain() || rail.Train().isTrack || rail.IsBoard() ) { // 渡し板も板の模型を描く
            rail.SetRoadMode(1);
            RailSkin hidden = rail.Config().skin;
            hidden.style = RailSkin::Hidden;
            rail.SetSkin(hidden);
        }
    }
}

// スタート/ゴール地点（レール番号＋ノード番号 → レール上の距離へ変換）
void RailField::ResolveStartGoal(const EditorManager& editor){
    auto nodeToDist = [&](int rail, int node, int& outRail, float& outDist){
        outRail = 0; outDist = 0.0f;
        if ( rail < 0 || rail >= ( int ) rails_.size() ) { outRail = -1; return; }
        outRail = rail;
        int maxNode = ( int ) rails_[rail].Nodes().size() - 1;
        int n = std::clamp(node, 0, ( std::max )( maxNode, 0 ));
        outDist = rails_[rail].GetDistanceFromT(( float ) n);
    };
    nodeToDist(editor.GetEditorStartRail(), editor.GetEditorStartNode(), startRail_, startDist_);
    if ( startRail_ < 0 ) { startRail_ = 0; startDist_ = 0.0f; } // 未設定はレール0の先頭
    nodeToDist(editor.GetEditorGoalRail(), editor.GetEditorGoalNode(), goalRail_, goalDist_);
}

// 動くレールの時間を進めて animOffset を更新し、緑線マーカーも追従させる
void RailField::UpdateMotion(float dt, int ridingRail, int playerRail){
    if ( RailMotion::Advance(rails_, dt, ridingRail, playerRail) ) { markers_.UpdatePositions(rails_); }
}

void RailField::ResetMotion(bool keepRolls){
    RailMotion::Reset(rails_, keepRolls);
    markers_.UpdatePositions(rails_);
}

bool RailField::HasRolls() const{
    for ( const SplineRail& rail : rails_ ) { if ( rail.IsRollBoard() ) { return true; } }
    return false;
}

// 卵 × 巻いたマット：線分（卵の動いた所）と筒の真ん中の球（少し大きめ）
bool RailField::HitRoll(const Vector3& from, const Vector3& to, float radius){
    for ( SplineRail& rail : rails_ ) {
        if ( !rail.IsRollBoard() || rail.Board().boardTriggered ) { continue; }
        RailMotion::RollPose pose;
        if ( !RailMotion::GetRollPose(rail, pose) ) { continue; }
        const Vector3 d { to.x - from.x, to.y - from.y, to.z - from.z };
        const Vector3 m { pose.center.x - from.x, pose.center.y - from.y, pose.center.z - from.z };
        const float len2 = d.x * d.x + d.y * d.y + d.z * d.z;
        const float t = len2 > 1e-8f ? std::clamp(( m.x * d.x + m.y * d.y + m.z * d.z ) / len2, 0.0f, 1.0f) : 0.0f;
        const Vector3 q { from.x + d.x * t - pose.center.x, from.y + d.y * t - pose.center.y, from.z + d.z * t - pose.center.z };
        const float reach = pose.radius + radius + 0.35f; // 筒は横に長い（幅 1.8m）ので、球を少し大きく
        if ( q.x * q.x + q.y * q.y + q.z * q.z <= reach * reach ) {
            rail.SetBoardTriggered(true);
            return true;
        }
    }
    return false;
}

// 動くレールのエディタプレビュー（OFFにした瞬間に基準位置へ戻す）
void RailField::UpdateEditorPreview(bool enabled){
    if ( enabled ) { UpdateMotion(Time::kFixedDeltaTime, kMotionStartAll); }
    else if ( prevEditorPreview_ ) { ResetMotion(); }
    prevEditorPreview_ = enabled;
}

// 「乗ったら動き出す」で待機中のリフトへ金色の「！」目印（乗れば動くことが一目で分かる）
void RailField::DrawWaitingLiftMarkers(float dt){
    liftMarkerTime_ += dt;
    const Vector4 gold { 1.0f, 0.85f, 0.2f, 1.0f };
    for ( const auto& rail : rails_ ) {
        if ( !rail.HasMotion() || rail.Motion().motionTrigger != 1 || rail.Motion().motionStarted || rail.IsTrain() ) continue; // 列車は駅で待つ（目印なし）
        if ( rail.Nodes().size() < 2 || !rail.Config().visible ) continue;
        Vector3 markPos = rail.GetPositionByDistance(rail.GetLength() * 0.5f);
        float bob = std::sin(liftMarkerTime_ * 4.0f) * 0.08f;
        float baseY = markPos.y + 1.4f + bob;
        DebugDraw::GetInstance()->Line({ markPos.x, baseY + 0.5f, markPos.z },
                                       { markPos.x, baseY + 0.18f, markPos.z }, gold);
        DebugDraw::GetInstance()->Sphere({ markPos.x, baseY, markPos.z }, 0.06f, gold, 6);
    }
}
