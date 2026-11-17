#include "game/clear/ClearFlow.h"

#include "engine/audio/AudioManager.h"
#include "engine/base/Input.h"
#include "engine/base/TimeManager.h"
#include "engine/camera/Camera.h"
#include "engine/math/MathUtil.h"
#include "engine/math/VectorMath.h"
#include "engine/rail/SplineRail.h"
#include "game/egg/EggCombo.h"
#include "game/gameover/GameOverFlow.h"
#include "game/player/Player.h"
#include "game/player/PlayerAvatar.h"
#include "game/player/PlayerFacing.h"
#include "game/rail/RailField.h"
#include "game/stage/StageRecords.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>
#include <dinput.h>

namespace {
    constexpr const char* kNextScene = "STAGESELECT"; // 終わったら戻る先（ステージ選択）
    constexpr float kEndMargin = 0.2f;                // 着地する所は、レールの端からこれより内側(m)
    constexpr float kClearTurnRate = 9.0f;            // 演出の間の、恐竜の向きを変える速さ（ふつうより少しゆっくり）
    // 卵スタンプの名前（StageRecords に残す）
    constexpr const char* kStampBand = "band";        // ＋番号
    constexpr const char* kStampCombo = "combo";
    constexpr const char* kStampHealth = "health";
    constexpr const char* kStampEnemies = "enemies";

    // 決定ボタン（ゲームオーバーの画面と同じ）
    bool DecidePressed(){
        Input* input = Input::GetInstance();
        return input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN);
    }
}

ClearFlow::ClearFlow() = default;
ClearFlow::~ClearFlow() = default;

void ClearFlow::Initialize(const Context& context, const std::string& stage, uint32_t noiseSrv){
    scene_ = context;
    stage_ = stage;
    settings_.Load();
    stageSettings_.Load(stage_);

    gate_.Initialize();
    props_.Initialize(settings_);
    backdrop_.Initialize(settings_.backdrop);
    result_.Initialize(settings_, stage_);
    paperFx_.Initialize(noiseSrv);
    AudioManager::GetInstance()->LoadWave(settings_.sounds.gate);

    // 場面が使う物（ずっと同じ物）
    context_ = {};
    context_.player = scene_.player;
    context_.avatar = scene_.avatar;
    context_.railField = scene_.railField;
    context_.camera = scene_.camera;
    context_.eggSystem = scene_.eggSystem;
    context_.gate = &gate_;
    context_.clearCamera = &camera_;
    context_.props = &props_;
    context_.result = &result_;
    context_.paperFx = &paperFx_;
    context_.settings = &settings_;
    context_.stage = &stageSettings_;
    OnRailsSynced();
}

void ClearFlow::Finalize(){
    active_ = false;
    sequence_.Clear();
    result_.Finalize();
    paperFx_.Clear();
}

void ClearFlow::OnRailsSynced(){
    if ( !scene_.railField ) { return; }
    gate_.Place(*scene_.railField, settings_.gate, stageSettings_.gateOffset, stageSettings_.gateTurn);
    hasPrevious_ = false;
}

void ClearFlow::OnPlayStart(int enemyTotal){
    if ( active_ ) { Abort(); }
    tally_.Reset(enemyTotal);
    gate_.Close();
    hasPrevious_ = false;
}

void ClearFlow::OnEditStart(){
    if ( active_ ) { Abort(); }
    gate_.Close();
    hasPrevious_ = false;
}

void ClearFlow::Abort(){
    sequence_.Abort(context_);
    active_ = false;
    props_.Hide();
    backdrop_.Hide();
    result_.Close();
    paperFx_.Clear();
    context_.resultOpen = context_.leaving = false;
    if ( scene_.player ) {
        scene_.player->SetAutoPilot(false);
        scene_.player->SetFaceOverride(false, 0.0f);
        scene_.player->SetTurnRate(PlayerFacing::kDefaultTurnRate);
    }
    if ( scene_.avatar ) { scene_.avatar->ClearIntroClip(); }
}

// ---------------------------------------------------------------------
//  ゴールの判定 → 演出の開始
// ---------------------------------------------------------------------
bool ClearFlow::CheckGoal(){
    if ( active_ || !scene_.player || !gate_.IsPlaced() ) { return false; }
    const Vector3 foot = scene_.player->GetPosition();
    float direction = 1.0f;
    const bool passed = hasPrevious_ && gate_.CheckPass(previousFoot_, foot, direction);
    previousFoot_ = foot;
    hasPrevious_ = true;
    if ( !passed ) { return false; }
    Begin(direction);
    return true;
}

// 着地する所（門から、くぐった向きへ landAhead）・くぐった向き・演出カメラを回す側
void ClearFlow::BuildStage(float direction){
    const auto& rails = scene_.railField->GetRails();
    const SplineRail& rail = rails[gate_.Rail()];
    context_.landRail = gate_.Rail();
    context_.landDistance = std::clamp(gate_.RailDistance() + stageSettings_.landAhead * direction, kEndMargin,
                                       ( std::max )( rail.GetLength() - kEndMargin, kEndMargin ));
    context_.landPoint = rail.GetPositionByDistance(context_.landDistance);
    context_.forward = gate_.Forward() * direction;
    context_.facingSign = ( VectorMath::Dot(gate_.Front(), context_.forward) >= 0.0f ) ? 1.0f : -1.0f;
    // 演出カメラは、遊んでいる時のカメラがいた側へ回り込む（反対へ回ると、道の向こうの何もない所から写す）
    const Vector3 side { context_.forward.z, 0.0f, -context_.forward.x };
    const Vector3 fromLand = scene_.camera ? scene_.camera->GetWorldPosition() - context_.landPoint : side * -1.0f;
    cameraSide_ = ( VectorMath::Dot(fromLand, side) >= 0.0f ) ? side : side * -1.0f;
}

void ClearFlow::ApplyStageCamera(){
    // 終点：着地する所から、くぐった向きを遊んでいる時のカメラの側へ yaw 度回した方へ distance・上へ height。
    //   見る点は、恐竜から門の方へ lookBack 戻った所の上（恐竜と、後ろの門の両方が入る）
    const float yaw = Math::ToRadians(stageSettings_.cameraYaw);
    context_.toCamera = VectorMath::NormalizeSafe(context_.forward * std::cos(yaw) + cameraSide_ * std::sin(yaw), context_.forward);
    const Vector3 endPosition = context_.landPoint + context_.toCamera * stageSettings_.cameraDistance
                              + Vector3 { 0.0f, stageSettings_.cameraHeight, 0.0f };
    const Vector3 endLook = context_.landPoint - context_.forward * stageSettings_.lookBack
                          + Vector3 { 0.0f, stageSettings_.lookHeight, 0.0f };
    camera_.SetEnd(endPosition, endLook, stageSettings_.fovY);
}

// 集計を確定して、リザルトの中身を作る。卵スタンプとクリアの回数を記録へ足す
ResultScreen::Data ClearFlow::MakeResult(bool& outShort){
    StageRecords* records = StageRecords::GetInstance();
    const StageTally::Result& tally = tally_.GetResult();
    ResultScreen::Data data;
    data.stage = stage_;

    // 帯：ステージの中の集める物
    data.bandKind = stageSettings_.band;
    if ( data.bandKind == "comboFlowers" ) {
        data.bandFound = records->ComboFlowersFound(stage_);
        data.bandTotal = StageRecords::CountComboTargets(stage_);
    } else if ( data.bandKind == "flowers" && scene_.eggCombo ) {
        data.bandFound = scene_.eggCombo->FlowersFound();
        data.bandTotal = scene_.eggCombo->FlowersTotal();
    }
    if ( data.bandTotal <= 0 ) { data.bandKind = "none"; data.bandFound = data.bandTotal = 0; }
    data.bandFound = ( std::min )( data.bandFound, data.bandTotal );

    // 3 行
    data.maxCombo = tally.maxCombo;
    data.health = scene_.health ? scene_.health->Hearts() : 0;
    data.healthMax = scene_.health ? scene_.health->MaxHearts() : 0;
    data.enemiesDefeated = tally.enemiesDefeated;
    data.enemyTotal = tally.enemyTotal;
    data.achieved[ResultScreen::Row_Combo] = tally.maxCombo >= stageSettings_.comboTarget;
    data.achieved[ResultScreen::Row_Health] = ( stageSettings_.healthRule == "noDamage" )
        ? ( tally.damageCount == 0 ) : ( data.healthMax > 0 && data.health >= data.healthMax );
    data.achieved[ResultScreen::Row_Enemy] = tally.enemiesDefeated >= tally.enemyTotal;
    data.hanamaru = data.bandFound >= data.bandTotal && data.achieved[ResultScreen::Row_Combo]
        && data.achieved[ResultScreen::Row_Health] && data.achieved[ResultScreen::Row_Enemy];

    // 卵スタンプ：帯は見つけた分がそのまま、行は達成した分
    std::vector<std::string> stamps;
    for ( int i = 0; i < data.bandFound; ++i ) { stamps.push_back(kStampBand + std::to_string(i)); }
    if ( data.achieved[ResultScreen::Row_Combo] )  { stamps.push_back(kStampCombo); }
    if ( data.achieved[ResultScreen::Row_Health] ) { stamps.push_back(kStampHealth); }
    if ( data.achieved[ResultScreen::Row_Enemy] )  { stamps.push_back(kStampEnemies); }
    data.ownedBefore = records->TotalStamps();
    data.newStamps = records->AddStamps(stage_, stamps, data.bandTotal + ResultScreen::Row_Count, data.hanamaru);

    outShort = records->IsCleared(stage_); // 同じステージの 2 回目から短縮版
    records->NoteClear(stage_);
    return data;
}

void ClearFlow::Begin(float direction){
    if ( !scene_.player || !scene_.railField || !gate_.IsPlaced() ) { return; }
    // 後始末：空中・構えの途中・ベロを出している途中でも破綻しないよう、シーンに片付けてもらう（敵は、シーンが時間を止める）
    if ( scene_.onGoal ) { scene_.onGoal(); }
    Time::GetInstance()->SetTimeScale(1.0f); // ヒットストップ・スローの途中でも、ふつうの速さから
    scene_.player->SetMovementLocked(false);
    scene_.player->SetTurnRate(kClearTurnRate);

    bool isShort = false;
    context_.resultData = MakeResult(isShort);
    context_.timing = settings_.TimingFor(isShort || previewShort_);
    context_.nextScene = kNextScene;
    context_.propsTime = 0.0f;
    context_.sinceLanding = -1.0f;
    context_.dancing = context_.resultOpen = context_.leaving = context_.decide = false;

    BuildStage(direction);
    if ( scene_.camera ) {
        camera_.Begin(*scene_.camera, context_.landPoint);
        ApplyStageCamera();
    }
    // 書き割り：演出のカメラから見て、門の後ろ（カメラの反対側）に立てる
    backdrop_.Begin(gate_.Position(), context_.toCamera * -1.0f);
    ClearSequence::Build(sequence_, context_);
    active_ = true;
    sequence_.Start(context_);
}

// ---------------------------------------------------------------------
//  演出中の更新
// ---------------------------------------------------------------------
void ClearFlow::UpdateBeforePlayer(float deltaTime){
    if ( !active_ ) { return; }
    ClearSequence::UpdateActor(context_, deltaTime);
}

void ClearFlow::UpdateAfterPlayer(float deltaTime){
    if ( !active_ ) { return; }
    ApplyStageCamera(); // インスペクターで直した値を、その場で
    context_.decide = DecidePressed();
    sequence_.Update(context_, deltaTime);
    // 小道具：出した後は、リザルトの間もゆらゆら揺れ続ける
    if ( props_.IsVisible() ) {
        context_.propsTime += deltaTime;
        props_.Update(context_.propsTime);
    }
}

void ClearFlow::UpdateVisuals(float deltaTime){
    gate_.Update(deltaTime);
    if ( active_ ) { backdrop_.Update(deltaTime); }
    paperFx_.Update(deltaTime, scene_.camera);
}

// ---------------------------------------------------------------------
//  描画
// ---------------------------------------------------------------------
void ClearFlow::DrawOpaque(){
    gate_.Draw();
    if ( active_ ) { backdrop_.Draw(); }
}

void ClearFlow::DrawInstanced(const Camera* camera){
    paperFx_.Draw(camera);
}

void ClearFlow::DrawTransparent(){
    if ( active_ ) { props_.Draw(); }
}

bool ClearFlow::DrawUI(){
    if ( !active_ ) { return false; }
    result_.Draw();
    return true;
}

// ---------------------------------------------------------------------
//  インスペクター：その場で試す・門の場所とカメラの終点を直して保存する
// ---------------------------------------------------------------------
void ClearFlow::DrawInspector(bool playing){
#ifdef USE_IMGUI
    if ( !ImGui::CollapsingHeader("クリア演出") ) { return; }
    if ( !gate_.IsPlaced() ) {
        ImGui::TextDisabled("このマップにはゴールがありません（レールエディタでゴールの点を決める）");
        return;
    }
    // --- 試す ---
    ImGui::Checkbox("短縮版で試す（2 回目からの長さ）", &previewShort_);
    if ( playing && !active_ ) {
        if ( ImGui::Button("ここでゴールした事にする") ) {
            // 門の所に立たせてから始める（どこにいても試せるように）
            scene_.player->DebugPlaceAt(gate_.Rail(), gate_.RailDistance());
            Begin(1.0f);
        }
    } else if ( active_ ) {
        if ( ImGui::Button("演出をやめる") ) { Abort(); gate_.Close(); }
        ImGui::SameLine();
        ImGui::Text("場面：%s  %.1f 秒", sequence_.CurrentName().c_str(), sequence_.CurrentElapsed());
    } else {
        ImGui::TextDisabled("Play 中に、ここからその場で試せます");
    }
    const StageTally::Result& tally = tally_.GetResult();
    ImGui::Text("集計：コンボ ×%d ／ ダメージ %d 回 ／ 敵 %d / %d", tally.maxCombo, tally.damageCount, tally.enemiesDefeated, tally.enemyTotal);

    // --- ステージごとの値（輪・着地・カメラの終点・目標）---
    ImGui::SeparatorText("このステージの置き場所");
    bool gateMoved = false;
    gateMoved |= ImGui::DragFloat("門の場所（ゴールの点から m。マイナス＝手前）", &stageSettings_.gateOffset, 0.05f, -30.0f, 30.0f, "%.2f");
    gateMoved |= ImGui::DragFloat("門の向き（入口をカメラへ回す 度）", &stageSettings_.gateTurn, 0.5f, -45.0f, 45.0f, "%.1f");
    ImGui::DragFloat("着地する所（門から先へ m）", &stageSettings_.landAhead, 0.02f, 0.0f, 6.0f, "%.2f");
    ImGui::DragFloat("カメラ：回り込む角度(度)", &stageSettings_.cameraYaw, 0.5f, -80.0f, 80.0f, "%.1f");
    ImGui::DragFloat("カメラ：距離(m)", &stageSettings_.cameraDistance, 0.05f, 2.0f, 20.0f, "%.2f");
    ImGui::DragFloat("カメラ：高さ(m)", &stageSettings_.cameraHeight, 0.02f, 0.0f, 8.0f, "%.2f");
    ImGui::DragFloat("カメラ：見る点の高さ(m)", &stageSettings_.lookHeight, 0.02f, 0.0f, 6.0f, "%.2f");
    ImGui::DragFloat("カメラ：見る点を門の方へ(m)", &stageSettings_.lookBack, 0.02f, -3.0f, 4.0f, "%.2f");
    ImGui::DragFloat("カメラ：画角(rad)", &stageSettings_.fovY, 0.005f, 0.3f, 1.4f, "%.3f");
    ImGui::DragInt("1 行目の目標のコンボ", &stageSettings_.comboTarget, 0.1f, 1, 8);
    if ( gateMoved && !active_ ) { OnRailsSynced(); } // 門を置き直す（演出の途中では動かさない）

    // --- 小道具（どのステージでも同じ値）---
    ImGui::SeparatorText("切り文字と星");
    ImGui::DragFloat("文字：大きさ", &settings_.letters.scale, 0.01f, 0.3f, 1.5f, "%.2f");
    ImGui::DragFloat("文字：並べる円の半径(m)", &settings_.letters.arcRadius, 0.02f, 0.5f, 3.5f, "%.2f");
    for ( size_t i = 0; i < settings_.letters.arcAngles.size(); ++i ) {
        ImGui::DragFloat(( "文字 " + std::to_string(i + 1) + "：角度(度)" ).c_str(), &settings_.letters.arcAngles[i], 0.5f, -120.0f, 120.0f, "%.1f");
    }
    ImGui::DragFloat("文字：アーチより手前へ(m)", &settings_.letters.front, 0.02f, -1.0f, 3.0f, "%.2f");
    ImGui::DragFloat("星：根元の横(m)", &settings_.stars.pivotSide, 0.02f, 1.0f, 8.0f, "%.2f");
    ImGui::DragFloat("星：根元の高さ(m)", &settings_.stars.pivotHeight, 0.02f, -3.0f, 3.0f, "%.2f");
    ImGui::DragFloat("星：門より手前へ(m)", &settings_.stars.front, 0.02f, -2.0f, 5.0f, "%.2f");

    if ( ImGui::Button("保存（ステージの置き場所と、文字・星）") ) {
        stageSettings_.Save(stage_);
        settings_.SaveProps();
    }
    ImGui::SameLine();
    if ( ImGui::Button("読み直す") ) {
        settings_.Load();
        stageSettings_.Load(stage_);
        if ( !active_ ) { OnRailsSynced(); }
    }
    ImGui::TextDisabled("秒数・リザルトの置き場所は resources/clear/clear.json（「読み直す」で反映）");
#else
    ( void ) playing;
#endif
}
