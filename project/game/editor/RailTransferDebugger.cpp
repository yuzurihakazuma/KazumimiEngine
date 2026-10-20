#include "game/editor/RailTransferDebugger.h"

#include "engine/base/TimeManager.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/math/VectorMath.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/EditorManager.h"
#include "engine/utils/Level/RailEditor.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>

using namespace VectorMath;

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr size_t kSampleCount = 600;     // 約10秒（60fps）
    constexpr size_t kLogLimit = 1000;
    constexpr float kTrailSeconds = 3.0f;
    constexpr float kJoinReach = 1.2f;       // Player の合流（FindJoin）と同じ距離

    // 判定の目安（企画書の初期値：位置 1mm・角度 1° 未満で緑）
    constexpr float kGapGreen = 0.001f, kGapYellow = 0.03f;
    constexpr float kDegGreen = 1.0f,   kDegYellow = 10.0f;
    constexpr float kCornerDeg = 45.0f; // これ以上曲がっている接続は「曲がり角」（わざと曲げた所なので赤にしない）

    const Vector4 kGreen  { 0.25f, 1.0f, 0.35f, 1.0f };
    const Vector4 kYellow { 1.0f, 0.85f, 0.15f, 1.0f };
    const Vector4 kRed    { 1.0f, 0.25f, 0.2f, 1.0f };

    float AngleDeg(const Vector3& a, const Vector3& b){
        const float la = Length(a), lb = Length(b);
        if ( la < 1e-6f || lb < 1e-6f ) return 0.0f;
        const float c = std::clamp(Dot(a, b) / ( la * lb ), -1.0f, 1.0f);
        return std::acos(c) * 180.0f / kPi;
    }
    Vector3 Scale(const Vector3& v, float s){ return { v.x * s, v.y * s, v.z * s }; }
    Vector3 Plus(const Vector3& a, const Vector3& b){ return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    Vector3 Minus(const Vector3& a, const Vector3& b){ return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    float Dist3(const Vector3& a, const Vector3& b){ return Length(Minus(a, b)); }

    const char* KindName(Player::TransferKind kind){
        switch ( kind ) {
        case Player::TransferKind::Continue: return "連結";
        case Player::TransferKind::Join:     return "合流";
        case Player::TransferKind::Switch:   return "乗換キー";
        case Player::TransferKind::Branch:   return "分岐";
        case Player::TransferKind::Land:     return "着地";
        case Player::TransferKind::ToAir:    return "空中へ";
        default:                             return "端で停止";
        }
    }
    bool IsUsable(const SplineRail& rail){ return rail.nodes.size() >= 2 && rail.GetLength() > 0.0f; }
    int StatusOf(float gap, float maxDeg, bool checkAngle){
        if ( gap >= kGapYellow || ( checkAngle && maxDeg >= kDegYellow ) ) return 2;
        if ( gap >= kGapGreen  || ( checkAngle && maxDeg >= kDegGreen ) )  return 1;
        return 0;
    }
}

// ===== 接続点の計測 =====
std::vector<RailTransferDebugger::Connection> RailTransferDebugger::MeasureConnections(const std::vector<SplineRail>& rails){
    std::vector<Connection> result;
    const int count = ( int ) rails.size();
    for ( int a = 0; a < count; ++a ) {
        const SplineRail& railA = rails[a];
        if ( !IsUsable(railA) || railA.isLoop || railA.HasMotion() ) continue; // 動くレールはその場の位置で合流する（固定の接続ではない）
        for ( int end = 0; end < 2; ++end ) {
            const bool aFront = ( end == 0 );
            const float aDist = aFront ? 0.0f : railA.GetLength();
            const int b = aFront ? railA.frontConnIndex : railA.backConnIndex;
            const Vector3 posA = railA.GetPositionByDistance(aDist);
            // A の端から出て行く向き（front から出る時は接線の逆）と、その時の「進む向きの右」
            const SplineRail::RailFrame frameA = railA.GetFrameAtDistance(aDist);
            const float signA = aFront ? -1.0f : 1.0f;
            const Vector3 dirA = Scale(railA.GetTangentByDistance(aDist), signA);
            const Vector3 rightA = Scale(frameA.right, signA);

            if ( b >= 0 && b < count && IsUsable(rails[b]) ) {
                // 端どうし：同じ組は1回だけ（a<b、または相手が別の端を指している片思い）
                const SplineRail& railB = rails[b];
                const bool bFront = aFront ? railA.frontConnToFront : railA.backConnToFront;
                const int backRef = bFront ? railB.frontConnIndex : railB.backConnIndex;
                if ( backRef == a && b < a ) continue;
                const float bDist = bFront ? 0.0f : railB.GetLength();
                const Vector3 posB = railB.GetPositionByDistance(bDist);
                const SplineRail::RailFrame frameB = railB.GetFrameAtDistance(bDist);
                const float signB = bFront ? 1.0f : -1.0f; // B へ入って進む向き
                const Vector3 dirB = Scale(railB.GetTangentByDistance(bDist), signB);
                const Vector3 rightB = Scale(frameB.right, signB);
                Connection c;
                c.railA = a; c.aFront = aFront; c.aDist = aDist;
                c.railB = b; c.bFront = bFront; c.bDist = bDist;
                c.pos = posA;
                c.gap = Dist3(posA, posB);
                c.tangentDeg = AngleDeg(dirA, dirB);
                c.upDeg = AngleDeg(frameA.up, frameB.up);
                c.rightDeg = AngleDeg(rightA, rightB);
                c.status = StatusOf(c.gap, ( std::max )( { c.tangentDeg, c.upDeg, c.rightDeg } ), true);
                if ( c.tangentDeg >= kCornerDeg && c.gap < kGapYellow ) { c.status = 3; } // 曲がり角
                result.push_back(c);
                continue;
            }
            // つながっていない端：前の 1.2m 以内に別レールの本体があれば合流（T字）になる
            int best = -1; float bestDist = kJoinReach, bestS = 0.0f;
            for ( int j = 0; j < count; ++j ) {
                if ( j == a || !IsUsable(rails[j]) || rails[j].IsRideBlocked() || rails[j].HasMotion() ) continue;
                const float s = rails[j].GetClosestDistance(posA);
                const Vector3 p = rails[j].GetPositionByDistance(s);
                const Vector3 toTarget { p.x - posA.x, 0.0f, p.z - posA.z };
                const float d = Dist3(p, posA);
                if ( d >= bestDist ) continue;
                const float lh = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
                if ( lh > 1e-3f && ( toTarget.x * dirA.x + toTarget.z * dirA.z ) / lh < 0.1f
                     && Length({ dirA.x, 0.0f, dirA.z }) > 1e-3f ) continue; // 前方だけ（Player と同じ）
                best = j; bestDist = d; bestS = s;
            }
            if ( best < 0 ) continue;
            Connection c;
            c.endToBody = true;
            c.railA = a; c.aFront = aFront; c.aDist = aDist;
            c.railB = best; c.bDist = bestS;
            c.bodyPos = rails[best].GetPositionByDistance(bestS);
            c.pos = posA;
            c.gap = bestDist;
            c.status = StatusOf(c.gap, 0.0f, false);
            result.push_back(c);
        }
    }
    return result;
}

RailTransferDebugger::~RailTransferDebugger(){
    // 自動の書き出し（環境変数 CG2_RAIL_DEBUG_DUMP=1 の時だけ。テストで直す前後を比べる用）
    char* value = nullptr;
    size_t length = 0;
    if ( _dupenv_s(&value, &length, "CG2_RAIL_DEBUG_DUMP") == 0 && value ) {
        free(value);
        WriteCsv();
        namespace fs = std::filesystem;
        std::error_code errorCode;
        fs::create_directories("debug", errorCode);
        std::ofstream file("debug/rail_connections.csv");
        file << "status,kind,railA,aEnd,railB,bEnd_or_s,gap_mm,tangent_deg,up_deg,right_deg\n";
        for ( const Connection& c : lastConnections_ ) {
            file << c.status << "," << ( c.endToBody ? "end-body" : "end-end" ) << "," << c.railA << "," << ( c.aFront ? "front" : "back" ) << ","
                 << c.railB << "," << ( c.endToBody ? std::to_string(c.bDist) : std::string(c.bFront ? "front" : "back") ) << ","
                 << c.gap * 1000.0f << "," << c.tangentDeg << "," << c.upDeg << "," << c.rightDeg << "\n";
        }
    }
}

// ===== 記録 =====
void RailTransferDebugger::BeforePlayerUpdate(Player& player, const std::vector<SplineRail>& rails){
    // 環境変数 CG2_RAIL_DEBUG_AUTOTEST=1：Play が始まって少ししたら全接続の自動テストを1回だけ流す
    if ( autoDelay_ == 0 ) {
        char* value = nullptr;
        size_t length = 0;
        autoDelay_ = -1;
        if ( _dupenv_s(&value, &length, "CG2_RAIL_DEBUG_AUTOTEST") == 0 && value ) { free(value); autoDelay_ = 90; }
    }
    if ( autoDelay_ > 0 && --autoDelay_ == 0 ) { autoStartRequested_ = true; autoDelay_ = -1; }
    if ( autoStartRequested_ ) { autoStartRequested_ = false; StartAutoTest(rails); }
    if ( autoIndex_ >= 0 ) { StepAutoTest(player); return; }
    if ( replayStartPending_ ) {
        replayStartPending_ = false;
        player.DebugPlaceAt(recordStartRail_, recordStartDist_);
        replayIndex_ = 0;
        hasLast_ = false;
    }
    if ( replaying_ ) {
        if ( replayIndex_ < recorded_.size() ) {
            player.SetAutoPilot(true, recorded_[replayIndex_++]);
        } else {
            replaying_ = false;
            player.SetAutoPilot(false);
            status_ = "再生が終わった";
        }
        return;
    }
    if ( recording_ ) { recorded_.push_back(player.CaptureIntent()); }
}

void RailTransferDebugger::Record(const Player& player, const std::vector<SplineRail>& rails, const Camera* camera, float dt){
    if ( dt <= 0.0f ) return;
    lastDt_ = dt;
    if ( lastConnections_.empty() || ( Time::GetInstance()->GetFrameCount() % 120 ) == 0 ) { lastConnections_ = MeasureConnections(rails); }
    const Player::DebugStep& step = player.GetDebugStep();
    Sample sample;
    sample.frame = Time::GetInstance()->GetFrameCount();
    sample.position = step.position;
    sample.base = step.base;
    sample.rail = step.rail;
    sample.inAir = step.inAir;
    sample.transfer = step.transferCount > 0;
    sample.clamped = step.clampedAtEnd;
    if ( hasLast_ ) {
        sample.move = Dist3(sample.position, last_.position);
        sample.speed = sample.move / dt;
    }
    if ( !step.inAir && step.rail >= 0 && step.rail < ( int ) rails.size() && IsUsable(rails[step.rail]) ) {
        const SplineRail& rail = rails[step.rail];
        const Vector3 onRail = rail.GetPositionByDistance(step.dist);
        const SplineRail::RailFrame frame = rail.GetFrameAtDistance(step.dist);
        const Vector3 diff { step.position.x - onRail.x, 0.0f, step.position.z - onRail.z };
        sample.offRail = std::sqrt(diff.x * diff.x + diff.z * diff.z);
        sample.lateral = diff.x * frame.right.x + diff.z * frame.right.z;
    }
    if ( camera ) {
        const float yaw = camera->GetRotation().y;
        float turn = ( yaw - lastCameraYaw_ ) * 180.0f / kPi;
        while ( turn > 180.0f ) turn -= 360.0f;
        while ( turn < -180.0f ) turn += 360.0f;
        sample.cameraTurn = hasLast_ ? std::abs(turn) : 0.0f;
        lastCameraYaw_ = yaw;
    }
    if ( samples_.size() < kSampleCount ) { samples_.push_back(sample); }
    else { samples_[sampleHead_] = sample; sampleHead_ = ( sampleHead_ + 1 ) % kSampleCount; }

    if ( step.clampedAtEnd && step.clampedLoss > 1e-4f && step.rail >= 0 && step.rail < ( int ) rails.size() ) {
        // 端で止められた（はみ出しを捨てた）。つながっている端なら「1フレーム止まる」違和感の元
        const SplineRail& rail = rails[step.rail];
        const bool atFront = step.dist < 0.01f;
        const bool connected = ( atFront ? rail.frontConnIndex : rail.backConnIndex ) >= 0;
        if ( connected ) {
            LogRow row;
            row.frame = sample.frame;
            row.kind = Player::TransferKind::None;
            row.fromRail = row.toRail = step.rail;
            row.fromDist = row.toDist = step.dist;
            row.overshoot = step.clampedLoss;
            row.before = hasLast_ ? last_.position : step.position;
            row.after = step.position;
            row.speedBefore = hasLast_ ? last_.speed : 0.0f;
            row.speedAfter = sample.speed;
            if ( log_.size() >= kLogLimit ) { log_.erase(log_.begin()); }
            log_.push_back(row);
        }
    }
    if ( sample.transfer ) {
        const Player::DebugTransfer& t = step.transfer;
        LogRow row;
        row.frame = sample.frame;
        row.kind = t.kind;
        row.fromRail = t.fromRail; row.fromDist = t.fromDist;
        row.toRail = t.toRail;     row.toDist = t.toDist;
        row.overshoot = t.overshoot;
        row.carried = t.carried;
        row.warp = ( std::max )( 0.0f, Dist3(t.fromPos, t.toPos) - t.carried );
        row.visibleOffset = Length(step.smooth);
        row.before = hasLast_ ? last_.position : t.fromPos;
        row.after = step.position;
        row.speedBefore = hasLast_ ? last_.speed : 0.0f;
        row.speedAfter = sample.speed;
        row.lateralBefore = hasLast_ ? last_.lateral : 0.0f;
        row.lateralAfter = sample.lateral;
        if ( log_.size() >= kLogLimit ) { log_.erase(log_.begin()); }
        log_.push_back(row);
        if ( t.kind != Player::TransferKind::ToAir ) { transferHappened_ = true; }
    }
    last_ = sample;
    hasLast_ = true;
}

// ===== 時間 =====
void RailTransferDebugger::UpdateTimeControl(bool playing){
    static const float kScales[] = { 1.0f, 0.5f, 0.25f, 0.125f };
    Time* time = Time::GetInstance();
    if ( !playing ) {
        transferHappened_ = false;
        if ( controlledLastFrame_ ) { time->SetTimeScale(1.0f); controlledLastFrame_ = false; }
        return;
    }
    if ( pauseOnTransfer_ && transferHappened_ ) { paused_ = true; status_ = "乗り換えで止めた（コマ送りで続きを見る）"; }
    transferHappened_ = false;
    if ( stepping_ ) { stepping_ = false; } // 1コマ進めた後は止める
    if ( stepRequested_ ) { stepRequested_ = false; stepping_ = true; }
    // このフレームで決めた倍率は、次のフレームの dt に効く
    const bool control = paused_ || slowIndex_ > 0;
    if ( paused_ && !stepping_ ) { time->SetTimeScale(0.0f); }
    else if ( control )          { time->SetTimeScale(kScales[slowIndex_]); }
    else if ( controlledLastFrame_ ) { time->SetTimeScale(1.0f); } // スロー・一時停止をやめた時だけ戻す（カメラの回転待ちなどは触らない）
    controlledLastFrame_ = control;
}

// ===== 重ね描き =====
void RailTransferDebugger::DrawOverlay(const std::vector<SplineRail>& rails, const Player* player, bool playing){
    if ( !showOverlay_ ) return;
    DebugDraw* draw = DebugDraw::GetInstance();
    const Vector3 focus = player ? player->GetPosition() : Vector3 { 0.0f, 0.0f, 0.0f };
    const float nearRange = 30.0f; // フレームの矢印はプレイヤーのまわりだけ（線が多すぎないように）

    for ( const SplineRail& rail : rails ) {
        if ( !IsUsable(rail) ) continue;
        const float length = rail.GetLength();
        const Vector4 lineColor = rail.visible ? Vector4 { 0.3f, 0.9f, 1.0f, 0.9f } : Vector4 { 0.7f, 0.5f, 1.0f, 0.6f };
        Vector3 prevCenter {}, prevLeft {}, prevRight {};
        const float step = 0.5f;
        for ( float s = 0.0f; ; s += step ) {
            const float d = ( std::min )( s, length );
            const Vector3 center = rail.GetPositionByDistance(d);
            const SplineRail::RailFrame frame = rail.GetFrameAtDistance(d);
            const Vector3 lift { 0.0f, 0.03f, 0.0f };
            const Vector3 left = Plus(Plus(center, Scale(frame.right, -halfWidth_)), lift);
            const Vector3 right = Plus(Plus(center, Scale(frame.right, halfWidth_)), lift);
            if ( s > 0.0f ) {
                draw->Line(Plus(prevCenter, lift), Plus(center, lift), lineColor);
                if ( showBounds_ && rail.visible && rail.roadMode == 0 ) {
                    draw->Line(prevLeft, left, { 1.0f, 1.0f, 1.0f, 0.5f });
                    draw->Line(prevRight, right, { 1.0f, 1.0f, 1.0f, 0.5f });
                }
            }
            if ( showFrames_ && Dist3(center, focus) < nearRange && std::fmod(s, 1.0f) < 0.01f ) {
                draw->Line(center, Plus(center, Scale(frame.tangent, 0.4f)), { 1.0f, 0.3f, 0.3f, 1.0f }); // 接線（赤）
                draw->Line(center, Plus(center, Scale(frame.right, 0.4f)), { 0.3f, 1.0f, 0.3f, 1.0f });   // 横（緑）
                draw->Line(center, Plus(center, Scale(frame.up, 0.4f)), { 0.3f, 0.5f, 1.0f, 1.0f });      // 上（青）
            }
            prevCenter = center; prevLeft = left; prevRight = right;
            if ( d >= length ) break;
        }
    }
    // 接続点
    for ( const Connection& c : MeasureConnections(rails) ) {
        if ( showOnlyBad_ && c.status == 0 ) continue;
        const Vector4 color = c.status == 0 ? kGreen : c.status == 1 ? kYellow : c.status == 2 ? kRed : Vector4 { 0.4f, 0.7f, 1.0f, 1.0f };
        draw->Sphere(Plus(c.pos, { 0.0f, 0.1f, 0.0f }), c.status == 0 ? 0.12f : 0.22f, color, 10);
        if ( c.gap > 0.001f && c.railB >= 0 ) {
            draw->Line(c.pos, rails[c.railB].GetPositionByDistance(c.bDist), color);
        }
    }
    if ( !player ) return;
    const Player::DebugStep& step = player->GetDebugStep();
    // 足元 → 所属レールの点（離れている距離が見える）
    if ( !step.inAir && step.rail >= 0 && step.rail < ( int ) rails.size() && IsUsable(rails[step.rail]) ) {
        const Vector3 onRail = rails[step.rail].GetPositionByDistance(step.dist);
        draw->Line(step.position, onRail, { 1.0f, 0.4f, 1.0f, 1.0f });
        draw->Sphere(onRail, 0.06f, { 1.0f, 0.4f, 1.0f, 1.0f }, 8);
    }
    // 軌跡（直近の数秒。乗り換えたフレームは赤、端で止められたフレームは黄）
    if ( showTrail_ && playing && !samples_.empty() ) {
        const size_t count = samples_.size();
        const size_t keep = ( std::min )( count, ( size_t ) ( kTrailSeconds / ( std::max )( lastDt_, 1e-3f ) ) );
        for ( size_t k = 0; k < keep; ++k ) {
            const size_t index = ( sampleHead_ + count - 1 - k ) % count;
            const Sample& sample = samples_[index];
            const Vector4 color = sample.transfer ? kRed : sample.clamped ? kYellow : Vector4 { 1.0f, 1.0f, 1.0f, 0.8f };
            draw->Sphere(Plus(sample.position, { 0.0f, 0.05f, 0.0f }), sample.transfer ? 0.1f : 0.035f, color, 6);
        }
    }
}

// ===== パネル =====
void RailTransferDebugger::DrawWindow(const std::vector<SplineRail>& rails, Player* player, RailEditor* railEditor, EditorManager* editor){
#ifdef USE_IMGUI
    if ( !editor->IsPanelVisible(EditorManager::Panel_RailDebug) ) return;
    if ( !ImGui::Begin("乗り換えデバッガ (Rail Transfer)") ) { ImGui::End(); return; }
    const bool playing = ( editor->GetMode() == EngineMode::Play );
    const bool editing = ( editor->GetMode() == EngineMode::Edit );

    if ( ImGui::CollapsingHeader("重ね描き", ImGuiTreeNodeFlags_DefaultOpen) ) {
        ImGui::Checkbox("表示する##rtd", &showOverlay_); ImGui::SameLine();
        ImGui::Checkbox("フレームの矢印##rtd", &showFrames_); ImGui::SameLine();
        ImGui::Checkbox("道の端##rtd", &showBounds_); ImGui::SameLine();
        ImGui::Checkbox("軌跡##rtd", &showTrail_);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::DragFloat("道の幅の半分##rtd", &halfWidth_, 0.01f, 0.2f, 3.0f, "%.2f m");
        ImGui::TextDisabled("矢印：赤=接線 緑=横 青=上 ／ 接続点：緑・黄・赤 ／ 桃色の線＝足元→所属レールの点\n"
                            "軌跡：白=通常 赤=乗り換えたフレーム 黄=端で止められたフレーム");
    }

    if ( ImGui::CollapsingHeader("接続点インスペクタ", ImGuiTreeNodeFlags_DefaultOpen) ) {
        DrawConnectionTable(rails, railEditor, editing);
    }
    if ( ImGui::CollapsingHeader("乗り換えログ", ImGuiTreeNodeFlags_DefaultOpen) ) { DrawLog(); }
    if ( ImGui::CollapsingHeader("時系列グラフ", ImGuiTreeNodeFlags_DefaultOpen) ) { DrawGraphs(); }
    if ( ImGui::CollapsingHeader("再現の補助", ImGuiTreeNodeFlags_DefaultOpen) ) { DrawReplay(player, playing); }
    if ( !status_.empty() ) { ImGui::TextWrapped("%s", status_.c_str()); }
    ImGui::End();
#else
    ( void ) rails; ( void ) player; ( void ) railEditor; ( void ) editor;
#endif
}

void RailTransferDebugger::DrawConnectionTable(const std::vector<SplineRail>& rails, RailEditor* railEditor, bool editing){
#ifdef USE_IMGUI
    const std::vector<Connection> connections = MeasureConnections(rails);
    int counts[4] = {};
    for ( const Connection& c : connections ) { ++counts[c.status]; }
    ImGui::Text("接続 %d か所：", ( int ) connections.size());
    ImGui::SameLine(); ImGui::TextColored({ 0.3f, 1.0f, 0.4f, 1.0f }, "緑 %d", counts[0]);
    ImGui::SameLine(); ImGui::TextColored({ 1.0f, 0.85f, 0.2f, 1.0f }, "黄 %d", counts[1]);
    ImGui::SameLine(); ImGui::TextColored({ 1.0f, 0.3f, 0.25f, 1.0f }, "赤 %d", counts[2]);
    ImGui::SameLine(); ImGui::TextColored({ 0.4f, 0.7f, 1.0f, 1.0f }, "角 %d", counts[3]);
    ImGui::SameLine(); ImGui::Checkbox("赤・黄だけ##rtd", &showOnlyBad_);
    ImGui::TextDisabled("緑：位置のずれ 1mm 未満・角度 1° 未満 ／ 赤：3cm 以上 か 10° 以上 ／ 角：45° 以上曲がっている（わざと曲げた角）。\n"
                        "端→本体（T字の合流）は位置だけ見る。動くレールはその場の位置で合流するので除く");
    if ( !editing ) { ImGui::TextDisabled("（直すボタンは Edit モードの時だけ）"); }

    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
    if ( ImGui::BeginTable("##connections", 8, flags, { 0.0f, 220.0f }) ) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("状態");
        ImGui::TableSetupColumn("接続");
        ImGui::TableSetupColumn("ずれ(mm)");
        ImGui::TableSetupColumn("接線°");
        ImGui::TableSetupColumn("上°");
        ImGui::TableSetupColumn("横°");
        ImGui::TableSetupColumn("直す");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        for ( int i = 0; i < ( int ) connections.size(); ++i ) {
            const Connection& c = connections[i];
            if ( showOnlyBad_ && c.status == 0 ) continue;
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const ImVec4 color = c.status == 0 ? ImVec4 { 0.3f, 1.0f, 0.4f, 1.0f } : c.status == 1 ? ImVec4 { 1.0f, 0.85f, 0.2f, 1.0f }
                               : c.status == 2 ? ImVec4 { 1.0f, 0.3f, 0.25f, 1.0f } : ImVec4 { 0.4f, 0.7f, 1.0f, 1.0f };
            ImGui::TextColored(color, c.status == 0 ? "緑" : c.status == 1 ? "黄" : c.status == 2 ? "赤" : "角");
            ImGui::TableNextColumn();
            char label[96];
            if ( c.endToBody ) {
                snprintf(label, sizeof(label), "R%d %s → R%d 本体 %.1fm", c.railA, c.aFront ? "始点" : "終点", c.railB, c.bDist);
            } else {
                snprintf(label, sizeof(label), "R%d %s ↔ R%d %s", c.railA, c.aFront ? "始点" : "終点", c.railB, c.bFront ? "始点" : "終点");
            }
            if ( ImGui::Selectable(label, selectedConnection_ == i, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap) ) {
                selectedConnection_ = i;
                focusRequested_ = true;
                focusRail_ = c.railA;
                focusDist_ = c.aDist;
            }
            ImGui::TableNextColumn(); ImGui::Text("%.1f", c.gap * 1000.0f);
            ImGui::TableNextColumn(); if ( c.endToBody ) ImGui::TextDisabled("-"); else ImGui::Text("%.1f", c.tangentDeg);
            ImGui::TableNextColumn(); if ( c.endToBody ) ImGui::TextDisabled("-"); else ImGui::Text("%.1f", c.upDeg);
            ImGui::TableNextColumn(); if ( c.endToBody ) ImGui::TextDisabled("-"); else ImGui::Text("%.1f", c.rightDeg);
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(!editing || !railEditor || c.status == 0 || c.status == 3);
            if ( c.gap >= kGapGreen && ImGui::SmallButton("端点を一致") ) { FixGap(c, railEditor); }
            if ( !c.endToBody && c.tangentDeg >= kDegGreen ) {
                if ( c.gap >= kGapGreen ) ImGui::SameLine();
                if ( ImGui::SmallButton("接線を揃える") ) { FixTangent(c, rails, railEditor); }
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::BeginDisabled(!editing || !railEditor);
    if ( ImGui::Button("端点のずれを全部直す##rtd") ) {
        int fixed = 0;
        for ( const Connection& c : connections ) {
            if ( c.gap >= kGapGreen ) { FixGap(c, railEditor); ++fixed; }
        }
        status_ = std::to_string(fixed) + " か所の端点を一致させた（元に戻す：Ctrl+Z）";
    }
    ImGui::EndDisabled();
#else
    ( void ) rails; ( void ) railEditor; ( void ) editing;
#endif
}

// 端点を一致させる：端→端は B の端を A の端へ。端→本体は A の端を本体の上の点へ（エディタのデータを直す）
void RailTransferDebugger::FixGap(const Connection& c, RailEditor* railEditor){
    if ( !railEditor ) return;
    const auto& lines = railEditor->GetRailLines();
    if ( c.railA < 0 || c.railA >= ( int ) lines.size() || c.railB < 0 || c.railB >= ( int ) lines.size() ) return;
    if ( c.endToBody ) {
        // 端を、合流する本体の上の点へ（T字の端がぴったり本体に載る）
        const int index = c.aFront ? 0 : ( int ) lines[c.railA].size() - 1;
        railEditor->SetNodePosOf(c.railA, index, c.bodyPos);
        return;
    }
    const auto& lineA = lines[c.railA];
    const Vector3 endA = c.aFront ? lineA.front() : lineA.back();
    const int indexB = c.bFront ? 0 : ( int ) lines[c.railB].size() - 1;
    railEditor->SetNodePosOf(c.railB, indexB, endA);
}

// 接線を揃える：B の端の次の点を、A から出て行く向きの上に置く（遠い時は端の近くに点を足す＝短い補間カーブ）
void RailTransferDebugger::FixTangent(const Connection& c, const std::vector<SplineRail>& rails, RailEditor* railEditor){
    if ( !railEditor || c.endToBody ) return;
    const auto& lines = railEditor->GetRailLines();
    if ( c.railA >= ( int ) lines.size() || c.railB >= ( int ) lines.size() ) return;
    const auto& lineA = lines[c.railA];
    const auto& lineB = lines[c.railB];
    if ( lineA.size() < 2 || lineB.size() < 2 ) return;
    // A の端の向き（データの折れ線の最後の区間＝Catmull-Rom の端の接線と同じ向き）
    const Vector3 endA = c.aFront ? lineA[0] : lineA.back();
    const Vector3 nextA = c.aFront ? lineA[1] : lineA[lineA.size() - 2];
    Vector3 dir = Normalize(Minus(endA, nextA)); // A から出て行く向き
    const int endIndexB = c.bFront ? 0 : ( int ) lineB.size() - 1;
    const int nextIndexB = c.bFront ? 1 : ( int ) lineB.size() - 2;
    const Vector3 endB = lineB[endIndexB];
    const float span = Dist3(lineB[nextIndexB], endB);
    ( void ) rails;
    if ( span > 1.5f ) {
        // 遠い：端から 0.6m の所に点を足して、そこまでを A の向きにそろえる（形は大きく変えない）
        const Vector3 added = Plus(endB, Scale(dir, 0.6f));
        railEditor->InsertNodeOf(c.railB, c.bFront ? 1 : ( int ) lineB.size() - 1, added);
    } else {
        railEditor->SetNodePosOf(c.railB, nextIndexB, Plus(endB, Scale(dir, span)));
    }
    status_ = "R" + std::to_string(c.railB) + " の端の向きを R" + std::to_string(c.railA) + " にそろえた（元に戻す：Ctrl+Z）";
}

void RailTransferDebugger::DrawLog(){
#ifdef USE_IMGUI
    ImGui::Text("乗り換え %d 回", ( int ) log_.size());
    ImGui::SameLine();
    if ( ImGui::SmallButton("CSV に書き出す##rtd") ) { WriteCsv(); }
    ImGui::SameLine();
    if ( ImGui::SmallButton("消す##rtdlog") ) { log_.clear(); }
    ImGui::TextDisabled("飛び＝レール上の点が飛んだ距離（はみ出しを持ち越した分を除く）／隠し＝見た目の平滑化で隠したずれ");
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
    if ( ImGui::BeginTable("##transferlog", 9, flags, { 0.0f, 200.0f }) ) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for ( const char* name : { "フレーム", "種類", "元 R/s", "先 R/s", "はみ出し", "持ち越し", "飛び(cm)", "隠し(cm)", "速度 前→後" } ) {
            ImGui::TableSetupColumn(name);
        }
        ImGui::TableHeadersRow();
        for ( int i = ( int ) log_.size() - 1; i >= 0; --i ) {
            const LogRow& row = log_[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%llu", ( unsigned long long ) row.frame);
            ImGui::TableNextColumn(); ImGui::Text("%s", KindName(row.kind));
            ImGui::TableNextColumn(); ImGui::Text("R%d %.2f", row.fromRail, row.fromDist);
            ImGui::TableNextColumn(); ImGui::Text("R%d %.2f", row.toRail, row.toDist);
            ImGui::TableNextColumn(); ImGui::Text("%.3f", row.overshoot);
            ImGui::TableNextColumn(); ImGui::Text("%.3f", row.carried);
            ImGui::TableNextColumn();
            const ImVec4 warpColor = row.warp < 0.005f ? ImVec4 { 0.3f, 1.0f, 0.4f, 1.0f } : row.warp < 0.03f ? ImVec4 { 1.0f, 0.85f, 0.2f, 1.0f } : ImVec4 { 1.0f, 0.3f, 0.25f, 1.0f };
            ImGui::TextColored(warpColor, "%.1f", row.warp * 100.0f);
            ImGui::TableNextColumn(); ImGui::Text("%.1f", row.visibleOffset * 100.0f);
            ImGui::TableNextColumn(); ImGui::Text("%.2f→%.2f", row.speedBefore, row.speedAfter);
        }
        ImGui::EndTable();
    }
#endif
}

void RailTransferDebugger::WriteCsv(){
    namespace fs = std::filesystem;
    std::error_code errorCode;
    fs::create_directories("debug", errorCode);
    const std::time_t now = std::time(nullptr);
    std::tm local {};
    localtime_s(&local, &now);
    char name[96];
    std::strftime(name, sizeof(name), "debug/rail_transfer_%Y%m%d_%H%M%S.csv", &local);
    lastCsvPath_ = name;
    std::ofstream file(name);
    if ( !file.is_open() ) { status_ = std::string("書き出せませんでした: ") + name; return; }
    file << "frame,kind,fromRail,fromS,toRail,toS,overshoot,carried,warp_m,hidden_m,"
            "beforeX,beforeY,beforeZ,afterX,afterY,afterZ,delta_m,speedBefore,speedAfter,lateralBefore,lateralAfter\n";
    for ( const LogRow& row : log_ ) {
        file << row.frame << "," << KindName(row.kind) << "," << row.fromRail << "," << row.fromDist << ","
             << row.toRail << "," << row.toDist << "," << row.overshoot << "," << row.carried << ","
             << row.warp << "," << row.visibleOffset << ","
             << row.before.x << "," << row.before.y << "," << row.before.z << ","
             << row.after.x << "," << row.after.y << "," << row.after.z << ","
             << Dist3(row.before, row.after) << "," << row.speedBefore << "," << row.speedAfter << ","
             << row.lateralBefore << "," << row.lateralAfter << "\n";
    }
    status_ = std::string("書き出した: project/") + name;
}

void RailTransferDebugger::DrawGraphs(){
#ifdef USE_IMGUI
    const size_t count = samples_.size();
    if ( count < 2 ) { ImGui::TextDisabled("Play 中に動くと記録される"); return; }
    std::vector<float> move(count), speed(count), lateral(count), off(count), camera(count);
    std::vector<int> marks; // 乗り換えたフレームの位置
    for ( size_t k = 0; k < count; ++k ) {
        const Sample& s = samples_[( sampleHead_ + k ) % count];
        move[k] = s.move * 100.0f;
        speed[k] = s.speed;
        lateral[k] = s.lateral * 100.0f;
        off[k] = s.offRail * 100.0f;
        camera[k] = s.cameraTurn;
        if ( s.transfer ) { marks.push_back(( int ) k); }
    }
    auto plot = [&](const char* label, const std::vector<float>& values, const char* unit){
        float lo = *std::min_element(values.begin(), values.end());
        float hi = *std::max_element(values.begin(), values.end());
        if ( hi - lo < 1e-3f ) { hi = lo + 1.0f; }
        char overlay[64];
        snprintf(overlay, sizeof(overlay), "最大 %.2f %s", hi, unit);
        ImGui::PlotLines(label, values.data(), ( int ) values.size(), 0, overlay, lo, hi, { -110.0f, 60.0f });
        // 乗り換えたフレームに縦線
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 size = ImGui::GetItemRectSize();
        const float plotWidth = size.x - ImGui::CalcTextSize(label, nullptr, true).x - ImGui::GetStyle().ItemInnerSpacing.x;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        for ( int mark : marks ) {
            const float x = min.x + plotWidth * ( ( float ) mark / ( float ) ( values.size() - 1 ) );
            drawList->AddLine({ x, min.y }, { x, min.y + size.y }, IM_COL32(255, 70, 60, 170), 1.0f);
        }
    };
    ImGui::TextDisabled("直近 %d フレーム。赤い縦線＝乗り換えたフレーム（違和感がある所はここでグラフが跳ねる）", ( int ) count);
    plot("移動量(cm/フレーム)", move, "cm");
    plot("速度(m/s)", speed, "m/s");
    plot("横のずれ(cm)", lateral, "cm");
    plot("レール中心から(cm)", off, "cm");
    plot("カメラの向き(°/フレーム)", camera, "°");
#endif
}

void RailTransferDebugger::DrawReplay(Player* player, bool playing){
#ifdef USE_IMGUI
    static const char* kSlowNames[] = { "等速", "1/2", "1/4", "1/8" };
    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("スロー##rtd", &slowIndex_, kSlowNames, 4);
    ImGui::SameLine();
    if ( ImGui::Button(paused_ ? "再開##rtd" : "一時停止##rtd") ) { paused_ = !paused_; }
    ImGui::SameLine();
    ImGui::BeginDisabled(!paused_);
    if ( ImGui::Button("コマ送り##rtd") ) { stepRequested_ = true; }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("乗り換えで一時停止##rtd", &pauseOnTransfer_);
    if ( !playing ) { ImGui::TextDisabled("（スロー・一時停止は Play 中だけ効く）"); }

    // 入力の記録と再生（同じ動きを、直す前と後で比べる）
    ImGui::BeginDisabled(!playing || !player || replaying_);
    if ( !recording_ ) {
        if ( ImGui::Button("入力の記録を始める##rtd") ) {
            recorded_.clear();
            recordStartRail_ = player->GetDebugStep().rail;
            recordStartDist_ = player->GetDebugStep().dist;
            recording_ = true;
            status_ = "記録中（もう一度押すと止まる）";
        }
    } else if ( ImGui::Button("記録を止める##rtd") ) {
        recording_ = false;
        status_ = std::to_string(recorded_.size()) + " フレーム記録した";
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!playing || !player || recording_ || recorded_.empty());
    if ( !replaying_ ) {
        if ( ImGui::Button("同じ所から再生##rtd") ) {
            replaying_ = true;
            replayStartPending_ = true;
            log_.clear();
            status_ = "再生中（記録した所に戻って、同じ入力で動く）";
        }
    } else if ( ImGui::Button("再生を止める##rtd") ) {
        replaying_ = false;
        if ( player ) { player->SetAutoPilot(false); }
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("記録 %d フレーム（開始 R%d %.2fm）", ( int ) recorded_.size(), recordStartRail_, recordStartDist_);
    ImGui::BeginDisabled(!playing || !player);
    if ( autoIndex_ < 0 ) {
        if ( ImGui::Button("全接続を自動で試す##rtd") ) { autoStartRequested_ = true; }
        if ( ImGui::IsItemHovered() ) { ImGui::SetTooltip("つながっている所の手前に1か所ずつ立たせて、端へ向かって歩かせる。結果は乗り換えログに入る"); }
    } else {
        ImGui::Text("自動テスト %d / %d: %s", autoIndex_ + 1, ( int ) autoCases_.size(), autoCases_[autoIndex_].label.c_str());
        ImGui::SameLine();
        if ( ImGui::SmallButton("止める##rtdauto") ) { autoIndex_ = -1; if ( player ) { player->SetAutoPilot(false); } }
    }
    ImGui::EndDisabled();
#else
    ( void ) player; ( void ) playing;
#endif
}

// ===== 全接続の自動テスト =====
void RailTransferDebugger::StartAutoTest(const std::vector<SplineRail>& rails){
    autoCases_.clear();
    for ( const Connection& c : MeasureConnections(rails) ) {
        const SplineRail& rail = rails[c.railA];
        const float length = rail.GetLength();
        const float run = ( std::min )( 1.5f, length * 0.5f );
        const float startDist = c.aFront ? run : length - run;
        // 端へ向かう向き（ワールドの意思）。横レールは X、縦レールは Z で歩く（Player と同じ）
        const Vector3 tangent = rail.GetTangentByDistance(startDist);
        const float toward = c.aFront ? -1.0f : 1.0f;
        AutoCase autoCase;
        autoCase.rail = c.railA;
        autoCase.startDist = startDist;
        autoCase.moveX = 0.0f;
        autoCase.moveZ = 0.0f;
        if ( rail.type == SplineRail::RailType::Horizontal ) { autoCase.moveX = ( tangent.x * toward >= 0.0f ) ? 1.0f : -1.0f; }
        else                                                 { autoCase.moveZ = ( tangent.z * toward >= 0.0f ) ? 1.0f : -1.0f; }
        char label[64];
        snprintf(label, sizeof(label), "R%d %s → R%d", c.railA, c.aFront ? "始点" : "終点", c.railB);
        autoCase.label = label;
        autoCases_.push_back(autoCase);
    }
    log_.clear();
    autoIndex_ = autoCases_.empty() ? -1 : 0;
    autoFrame_ = 0;
    status_ = std::to_string(autoCases_.size()) + " か所を自動で試す";
}

void RailTransferDebugger::StepAutoTest(Player& player){
    const AutoCase& autoCase = autoCases_[autoIndex_];
    if ( autoFrame_ == 0 ) {
        player.DebugPlaceAt(autoCase.rail, autoCase.startDist);
        hasLast_ = false;
    }
    PlayerInput::AutoPilot pilot;
    // 立たせた直後の数フレームは何も押さない（前のテストの勢いを切る）。その後 0.9 秒歩く
    if ( autoFrame_ >= 3 ) { pilot.moveX = autoCase.moveX; pilot.moveZ = autoCase.moveZ; }
    player.SetAutoPilot(true, pilot);
    if ( ++autoFrame_ > 3 + 54 ) {
        autoFrame_ = 0;
        if ( ++autoIndex_ >= ( int ) autoCases_.size() ) {
            autoIndex_ = -1;
            player.SetAutoPilot(false);
            status_ = "自動テストが終わった（乗り換えログを見る・CSV に書き出せる）";
        }
    }
}

bool RailTransferDebugger::ConsumeFocusRequest(int& outRail, float& outDist){
    if ( !focusRequested_ ) return false;
    focusRequested_ = false;
    outRail = focusRail_;
    outDist = focusDist_;
    return true;
}
