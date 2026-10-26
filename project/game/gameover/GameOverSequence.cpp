#include "game/gameover/GameOverSequence.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/audio/AudioManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Input.h"
#include "engine/base/WindowProc.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/PipelineType.h"
#include "engine/graphics/RenderTexture.h"
#include "engine/graphics/SrvManager.h"
#include "engine/math/Matrix4x4.h"
#include "engine/utils/Easing.h"
#include "externals/nlohmann/json.hpp"
#include "game/transition/KamishibaiTransition.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>
#include <fstream>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr const char* kSettingsFile = "resources/gameover/gameover.json";
    constexpr const char* kFreezeSe = "resources/kamishibai/se_pon.wav";
    constexpr const char* kRevealSe = "resources/kamishibai/se_whoosh.wav";
    constexpr const char* kTossSe = "resources/kamishibai/se_whoosh.wav";
    constexpr const char* kScreenSe = "resources/kamishibai/se_patan.wav";
    constexpr const char* kCrumpleSe = "resources/gameover/se_crumple.wav";
    constexpr const char* kBasaSe = "resources/gameover/se_basa.wav";
    constexpr const char* kSelectSe = "resources/kamishibai/se_pon.wav";
    constexpr const char* kKraft = "resources/craft/textures/kraft.png";
    constexpr float kFastForward = 6.0f;   // 早送りの速さ
    constexpr float kCoverFade = 0.35f;    // くしゃくしゃの始め、写したままの色から陰影の付いた色へ変わる割合
    constexpr float kShake = 0.06f;        // くしゃくしゃの瞬間の揺れ(m)
    constexpr float kBoardBehind = 1.5f;   // 紙の奥の板までの距離(m)
    constexpr float kUnscreenTime = 0.5f;  // ゲームオーバーの画面をしまう時間（前半で板がちぢみ、後半で背景が消える）
    constexpr float kReturnShare = 0.5f;   // 組み上げの前に、紙をどれだけ奥に置いておくか（捨てた距離に対して）

    Vector3 Add(const Vector3& a, const Vector3& b){ return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    Vector3 Sub(const Vector3& a, const Vector3& b){ return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    Vector3 Mul(const Vector3& a, float s){ return { a.x * s, a.y * s, a.z * s }; }
    float Dot(const Vector3& a, const Vector3& b){ return a.x * b.x + a.y * b.y + a.z * b.z; }
    Vector3 Cross(const Vector3& a, const Vector3& b){ return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    Vector3 Normalize(const Vector3& v){
        const float length = std::sqrt(Dot(v, v));
        return length > 1e-6f ? Mul(v, 1.0f / length) : Vector3 { 0.0f, 0.0f, 1.0f };
    }
    // カメラの回転（X＝見下ろす角・Y＝向き）にする
    Vector3 RotationOf(const Vector3& forward){
        return { std::atan2(-forward.y, std::sqrt(forward.x * forward.x + forward.z * forward.z)), std::atan2(forward.x, forward.z), 0.0f };
    }
    std::unique_ptr<Obj3d> MakeTiledBox(const std::string& name, const Vector3& tiles, const Vector4& color){
        ModelManager* modelManager = ModelManager::GetInstance();
        if ( !modelManager->FindModel(name) ) { modelManager->CreateCubeModel(name, 1.0f); }
        if ( Model* model = modelManager->FindModel(name) ) {
            model->SetTexture(kKraft);
            if ( Model::Material* material = model->GetMaterial() ) {
                material->matte = 1.0f;
                material->color = color;
                material->uvTransform = MatrixMath::MakeScale(tiles);
            }
        }
        auto obj = Obj3d::Create(name);
        if ( obj ) { obj->SetPipelineType(PipelineType::Object3D_WrapUV); }
        return obj;
    }
    void Transition(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after){
        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        commandList->ResourceBarrier(1, &barrier);
    }
}

GameOverSequence::GameOverSequence() = default;
GameOverSequence::~GameOverSequence() = default;

void GameOverSequence::Initialize(){
    LoadSettings();
    const uint32_t width = ( std::max )( 1u, ( uint32_t ) WindowProc::GetInstance()->GetClientWidth() );
    const uint32_t height = ( std::max )( 1u, ( uint32_t ) WindowProc::GetInstance()->GetClientHeight() );
    for ( auto* target : { &capture_, &resume_, &previous_ } ) {
        *target = std::make_unique<RenderTexture>();
        ( *target )->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), width, height);
    }
    sheet_.Initialize(capture_->GetSrvIndex());
    // 紙の奥の板：濃い段ボール（縮んだ紙のまわりに見える。紙の裏の明るい段ボールと見分ける）
    board_ = MakeTiledBox("go_board", { 12.0f, 7.0f, 1.0f }, { 0.42f, 0.3f, 0.24f, 1.0f });
    screen_.Initialize();
    AudioManager* audio = AudioManager::GetInstance();
    for ( const char* se : { kFreezeSe, kRevealSe, kScreenSe, kCrumpleSe, kBasaSe } ) { audio->LoadWave(se); }
    phase_ = Phase::None;
}

void GameOverSequence::Finalize(){
    sheet_.Finalize();
    screen_.Finalize();
    capture_.reset();
    resume_.reset();
    previous_.reset();
    board_.reset();
    walls_.clear();
    phase_ = Phase::None;
}

void GameOverSequence::LoadSettings(){
    std::ifstream file(kSettingsFile);
    nlohmann::json content = nlohmann::json::object();
    if ( file.is_open() ) {
        try { file >> content; } catch ( ... ) { content = nlohmann::json::object(); }
    }
    auto read = [&](const char* key, Timing& timing){
        const nlohmann::json t = content.value(key, nlohmann::json::object());
        timing.freeze = t.value("freeze", timing.freeze);
        timing.reveal = t.value("reveal", timing.reveal);
        timing.crumple = t.value("crumple", timing.crumple);
        timing.toss = t.value("toss", timing.toss);
        timing.screen = t.value("screen", timing.screen);
        timing.rebuild = t.value("rebuild", timing.rebuild);
    };
    shortTiming_ = { 0.3f, 0.0f, 0.5f, 0.35f, 0.5f, 0.6f };
    read("full", full_);
    read("short", shortTiming_);
    hearts_ = content.value("hearts", hearts_);
    damage_ = content.value("damage", damage_);
    sheetDistance_ = content.value("sheetDistance", sheetDistance_);
    tossDistance_ = content.value("tossDistance", tossDistance_);
    boxRim_ = content.value("boxRim", boxRim_);
    const nlohmann::json reveal = content.value("reveal", nlohmann::json::object());
    revealBack_ = reveal.value("back", revealBack_);
    revealUp_ = reveal.value("up", revealUp_);
    revealFov_ = reveal.value("fov", revealFov_);
}

void GameOverSequence::SetStageBounds(const Vector3& min, const Vector3& max, float groundTop){
    boundsMin_ = min;
    boundsMax_ = max;
    groundTop_ = groundTop;
    hasBounds_ = true;
    BuildBox();
}

// 種明かしの段ボール箱：ステージの地面の範囲を囲む4枚の壁（ふちが地面から少し上に出る）
void GameOverSequence::BuildBox(){
    walls_.clear();
    if ( !hasBounds_ ) { return; }
    const float thickness = 0.6f;
    const float bottom = boundsMin_.y - 1.0f;
    const float top = groundTop_ + boxRim_;
    const float height = top - bottom;
    const float midY = ( top + bottom ) * 0.5f;
    const float sizeX = boundsMax_.x - boundsMin_.x + thickness * 2.0f;
    const float sizeZ = boundsMax_.z - boundsMin_.z;
    const float cx = ( boundsMin_.x + boundsMax_.x ) * 0.5f, cz = ( boundsMin_.z + boundsMax_.z ) * 0.5f;
    struct Wall { Vector3 center, size; };
    const Wall walls[4] = {
        { { cx, midY, boundsMin_.z - thickness * 0.5f }, { sizeX, height, thickness } },
        { { cx, midY, boundsMax_.z + thickness * 0.5f }, { sizeX, height, thickness } },
        { { boundsMin_.x - thickness * 0.5f, midY, cz }, { thickness, height, sizeZ } },
        { { boundsMax_.x + thickness * 0.5f, midY, cz }, { thickness, height, sizeZ } },
    };
    for ( int i = 0; i < 4; ++i ) {
        const Wall& wall = walls[i];
        const float length = ( std::max )( wall.size.x, wall.size.z );
        auto obj = MakeTiledBox("go_wall_" + std::to_string(i), { length / 4.0f, height / 4.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
        if ( !obj ) { continue; }
        obj->SetTranslation(wall.center);
        obj->SetScale(wall.size);
        walls_.push_back(std::move(obj));
    }
}

void GameOverSequence::Start(Camera* camera, bool shortVersion){
    if ( !camera ) { return; }
    camera_ = camera;
    short_ = shortVersion;
    speed_ = 1.0f;
    restoreRequested_ = finished_ = false;
    captureRequested_ = captured_ = false;
    resumeShotRequested_ = resumeShotTaken_ = false;
    previousValid_ = false;
    choice_ = 0;
    push_ = 0.0f;
    // 今のカメラ（プレイ中のカメラ）から始める
    const Matrix4x4& projection = camera->GetProjectionMatrix();
    camFov_ = 2.0f * std::atan(1.0f / projection.m[1][1]);
    aspect_ = projection.m[1][1] / projection.m[0][0];
    const Vector3 rotation = camera->GetRotation();
    forward_ = { std::sin(rotation.y) * std::cos(rotation.x), -std::sin(rotation.x), std::cos(rotation.y) * std::cos(rotation.x) };
    camPos_ = camera->GetWorldPosition();
    camLook_ = Add(camPos_, Mul(forward_, sheetDistance_));
    sheet_.SetFrontTexture(capture_->GetSrvIndex());
    sheet_.SetCoverTexture(capture_->GetSrvIndex());
    sheet_.SetCoverAlpha(1.0f);
    sheet_.SetCrumple(0.0f);
    sheet_.SetVisible(false);
    screen_.SetBackground(0.0f);
    Enter(Phase::Freeze);
}

void GameOverSequence::Abort(){
    phase_ = Phase::None;
    sheet_.SetVisible(false);
    screen_.SetBackground(0.0f);
    screen_.Close(0.01f);
    captureRequested_ = resumeShotRequested_ = false;
    restoreRequested_ = finished_ = false;
}

float GameOverSequence::PhaseLength(Phase phase) const{
    const Timing& t = short_ ? shortTiming_ : full_;
    switch ( phase ) {
    case Phase::Freeze:   return t.freeze;
    case Phase::Reveal:   return t.reveal;
    case Phase::Crumple:  return t.crumple;
    case Phase::Toss:     return t.toss;
    case Phase::Screen:   return t.screen;
    case Phase::Unscreen: return kUnscreenTime;
    case Phase::Rebuild:  return t.rebuild;
    default:              return 0.0f;
    }
}

// 紙を、写し取った時のカメラの前（push だけ奥）に、画面ぴったりの大きさで置く
void GameOverSequence::PlaceSheet(float push){
    push_ = push;
    const Vector3 bottom = Sub(Add(camPos_, Mul(forward_, sheetDistance_ + push)), Mul(up_, sheetHeight_ * 0.5f));
    sheet_.Place(bottom, right_, up_, Mul(forward_, -1.0f), sheetWidth_, sheetHeight_);
}

void GameOverSequence::Enter(Phase phase){
    phase_ = phase;
    time_ = 0.0f;
    startPos_ = camPos_;
    startFov_ = camFov_;
    AudioManager* audio = AudioManager::GetInstance();
    switch ( phase ) {
    case Phase::Freeze:
        audio->PlayWave(kFreezeSe, false, 0.9f);
        break;
    case Phase::Reveal:
        if ( PhaseLength(Phase::Reveal) <= 0.0f ) { Enter(Phase::Capture); return; }
        audio->PlayWave(kRevealSe, false, 0.7f);
        break;
    case Phase::Capture:
        captureRequested_ = true; // この描画で写し取る（まだ 3D のまま）
        captured_ = false;
        break;
    case Phase::Crumple: {
        // 写し取った時のカメラの前に、画面ぴったりの紙を置く（見た目は途切れない）
        const Vector3 worldUp { 0.0f, 1.0f, 0.0f };
        forward_ = Normalize(Sub(camLook_, camPos_));
        right_ = Normalize(Cross(worldUp, forward_));
        up_ = Cross(forward_, right_);
        sheetHeight_ = 2.0f * sheetDistance_ * std::tan(camFov_ * 0.5f);
        sheetWidth_ = sheetHeight_ * aspect_;
        PlaceSheet(0.0f);
        sheet_.SetCrumple(0.0f);
        sheet_.SetCoverAlpha(1.0f);
        sheet_.SetVisible(true);
        if ( board_ ) {
            board_->SetTranslation(Add(camPos_, Mul(forward_, sheetDistance_ + tossDistance_ + kBoardBehind)));
            board_->SetRotation(RotationOf(forward_));
            const float spread = ( sheetDistance_ + tossDistance_ + kBoardBehind ) / sheetDistance_; // 奥にあるので、その分だけ大きく
            board_->SetScale({ sheetWidth_ * spread * 1.5f, sheetHeight_ * spread * 1.5f, 0.1f });
        }
        restoreRequested_ = true; // 見えていない間に、シーンはチェックポイントへ戻す
        audio->PlayWave(kCrumpleSe, false, 1.0f);
        break;
    }
    case Phase::Toss:
        audio->PlayWave(kTossSe, false, 0.6f);
        break;
    case Phase::Screen:
        screen_.SetBackground(1.0f);
        screen_.Open(PhaseLength(Phase::Screen));
        audio->PlayWave(kScreenSe, false, 0.9f);
        break;
    case Phase::ResumeShot:
        resumeShotRequested_ = true; // この1フレームだけ、再開のカメラで 3D を描いて写す
        resumeShotTaken_ = false;
        break;
    case Phase::Unscreen:
        screen_.Close(kUnscreenTime * 0.5f);
        break;
    case Phase::Rebuild:
        audio->PlayWave(kBasaSe, false, 0.9f);
        break;
    default:
        break;
    }
}

bool GameOverSequence::HidesScene() const{
    return phase_ >= Phase::Crumple && phase_ <= Phase::Rebuild && phase_ != Phase::ResumeShot;
}

bool GameOverSequence::DefeatPose(float& outLift, float& outSquash, float& outTilt) const{
    if ( phase_ != Phase::Freeze && phase_ != Phase::Reveal && phase_ != Phase::Capture ) { return false; }
    const float t = phase_ == Phase::Freeze ? time_ : 10.0f;
    // その場で小さく跳ねて、あおむけに倒れる
    const float hop = Easing::Clamp01(t / 0.35f);
    outLift = 0.5f * std::sin(hop * kPi);
    outSquash = 1.0f - 0.15f * std::sin(Easing::Clamp01(t / 0.2f) * kPi);
    outTilt = -1.35f * Easing::EaseOutBack(hop);
    return true;
}

bool GameOverSequence::ConsumeRestoreRequest(){
    const bool requested = restoreRequested_;
    restoreRequested_ = false;
    return requested;
}

void GameOverSequence::SetResumeCamera(const Vector3& position, const Vector3& rotation, const Matrix4x4& projection){
    resumePosition_ = position;
    resumeRotation_ = rotation;
    resumeFov_ = 2.0f * std::atan(1.0f / projection.m[1][1]);
}

bool GameOverSequence::ConsumeFinished(){
    const bool done = finished_;
    finished_ = false;
    return done;
}

void GameOverSequence::UpdateInput(){
    Input* input = Input::GetInstance();
    // 早送り：選ぶ所まで速く進めて、「やりなおす」にする（企画書：決定ボタンで P8 へ）
    if ( phase_ < Phase::Choose && input->Triggerkey(DIK_RETURN) ) { speed_ = kFastForward; }
    if ( phase_ != Phase::Choose ) { return; }
    if ( speed_ > 1.0f ) { choice_ = 0; Enter(Phase::ResumeShot); return; }
    const bool upKey = input->Triggerkey(DIK_W) || input->Triggerkey(DIK_UP) || input->Triggerkey(DIK_A) || input->Triggerkey(DIK_LEFT);
    const bool downKey = input->Triggerkey(DIK_S) || input->Triggerkey(DIK_DOWN) || input->Triggerkey(DIK_D) || input->Triggerkey(DIK_RIGHT);
    if ( ( upKey && choice_ != 0 ) || ( downKey && choice_ != 1 ) ) {
        choice_ = upKey ? 0 : 1;
        screen_.SetChoice(choice_);
        AudioManager::GetInstance()->PlayWave(kSelectSe, false, 0.5f);
    }
    if ( input->Triggerkey(DIK_SPACE) || input->Triggerkey(DIK_RETURN) ) {
        if ( choice_ == 0 ) {
            Enter(Phase::ResumeShot); // まず再開する所の絵を写してから、画面をしまって紙を広げる
        } else {
            Enter(Phase::Leave);
            KamishibaiTransition::ChangeSceneWithCard("STAGESELECT"); // 組み上げずにステージ選択へ
        }
    }
}

void GameOverSequence::Update(float deltaTime){
    if ( phase_ == Phase::None || !camera_ ) { return; }
    UpdateInput();
    if ( phase_ == Phase::None ) { return; }
    const float dt = deltaTime * speed_;
    time_ += dt;
    screen_.Update(dt);
    const float length = PhaseLength(phase_);
    const float t = length > 0.0f ? Easing::Clamp01(time_ / length) : 1.0f;
    const Vector3 worldUp { 0.0f, 1.0f, 0.0f };

    switch ( phase_ ) {
    case Phase::Freeze:
        if ( t >= 1.0f ) { Enter(Phase::Reveal); }
        break;
    case Phase::Reveal: {
        // 注視点はそのまま、手前へ引いて上がり、画角を広げる → 箱のふちが見える
        const float e = Easing::EaseInOutSine(t);
        camPos_ = Add(Add(startPos_, Mul(forward_, -revealBack_ * e)), Mul(worldUp, revealUp_ * e));
        camFov_ = startFov_ + revealFov_ * e;
        if ( t >= 1.0f ) { Enter(Phase::Capture); }
        break;
    }
    case Phase::Capture:
        if ( captured_ ) { Enter(Phase::Crumple); }
        break;
    case Phase::Crumple: {
        // 画面がぎゅっと縮んで、折れ目だらけになる（少し行き過ぎて戻る）。写したままの色から、陰影の付いた紙へ
        sheet_.SetCrumple(( std::min )( Easing::EaseOutBack(Easing::Clamp01(t / 0.7f)), 1.08f ));
        sheet_.SetCoverAlpha(1.0f - Easing::EaseInOutSine(Easing::Clamp01(t / kCoverFade)));
        // グシャッとした瞬間に、画面が少し揺れる
        const float shake = kShake * ( 1.0f - Easing::Clamp01(t / 0.4f) ) * std::sin(time_ * 70.0f);
        camPos_ = Add(startPos_, Add(Mul(right_, shake), Mul(up_, shake * 0.6f)));
        if ( t >= 1.0f ) { camPos_ = startPos_; Enter(Phase::Toss); }
        break;
    }
    case Phase::Toss:
        // くしゃくしゃの紙が奥へ飛んでいき、ゲームオーバーの画面の背景が出てくる
        sheet_.SetCrumple(1.0f);
        PlaceSheet(tossDistance_ * Easing::EaseInQuad(t));
        screen_.SetBackground(Easing::EaseInOutSine(Easing::Clamp01(( t - 0.3f ) / 0.7f)));
        if ( t >= 1.0f ) { Enter(Phase::Screen); }
        break;
    case Phase::Screen:
        if ( screen_.IsOpened() ) { Enter(Phase::Choose); }
        break;
    case Phase::ResumeShot:
        // 再開する所の絵を写した（そのフレームは前の画面を出し直したので見えない）
        if ( resumeShotTaken_ ) {
            // くしゃくしゃの紙の絵を再開する所の絵にして、少し奥へ置いておく（画面をしまうと、そこから戻ってくる）
            sheet_.SetFrontTexture(resume_->GetSrvIndex());
            sheet_.SetCoverTexture(resume_->GetSrvIndex());
            sheet_.SetCoverAlpha(0.0f);
            sheet_.SetCrumple(1.0f);
            PlaceSheet(tossDistance_ * kReturnShare);
            Enter(Phase::Unscreen);
        }
        break;
    case Phase::Unscreen:
        // 板がちぢんで消え、背景が消えると、奥にくしゃくしゃの紙が見える
        screen_.SetBackground(1.0f - Easing::EaseInOutSine(Easing::Clamp01(( t - 0.5f ) / 0.5f)));
        if ( t >= 1.0f ) { screen_.SetBackground(0.0f); Enter(Phase::Rebuild); }
        break;
    case Phase::Rebuild: {
        // くしゃくしゃの紙が手前へ戻ってきながら広がって、画面ぴったりの再開の絵になる
        const float back = Easing::EaseOutCubic(Easing::Clamp01(t / 0.6f));
        PlaceSheet(tossDistance_ * kReturnShare * ( 1.0f - back ));
        sheet_.SetCrumple(1.0f - Easing::EaseInOutSine(t));
        sheet_.SetCoverAlpha(Easing::EaseInOutSine(Easing::Clamp01(( t - 0.6f ) / 0.4f)));
        if ( t >= 1.0f ) {
            // 紙は画面ぴったりで再開の絵になった：再開のカメラへ切り替えて、紙を外す（見た目は同じ）
            sheet_.SetVisible(false);
            camera_->SetTranslation(resumePosition_);
            camera_->SetRotation(resumeRotation_);
            camera_->SetFovY(resumeFov_);
            camera_->Update();
            phase_ = Phase::None;
            finished_ = true;
            return;
        }
        break;
    }
    default:
        break;
    }

    ApplyCamera();
    sheet_.Update();
    if ( board_ ) { board_->Update(); }
    for ( auto& wall : walls_ ) { wall->Update(); }
}

void GameOverSequence::ApplyCamera(){
    if ( phase_ == Phase::ResumeShot ) {
        // 再開の絵を写すフレームは、再開のカメラそのもの
        camera_->SetTranslation(resumePosition_);
        camera_->SetRotation(resumeRotation_);
        camera_->SetFovY(resumeFov_);
    } else {
        camera_->SetTranslation(camPos_);
        camera_->SetRotation(RotationOf(Normalize(Sub(camLook_, camPos_))));
        camera_->SetFovY(camFov_);
    }
    camera_->Update();
}

void GameOverSequence::Copy(ID3D12GraphicsCommandList* commandList, ID3D12Resource* source, ID3D12Resource* destination){
    Transition(commandList, source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
    Transition(commandList, destination, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->CopyResource(destination, source);
    Transition(commandList, destination, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Transition(commandList, source, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void GameOverSequence::OnFrameComposed(ID3D12GraphicsCommandList* commandList, ID3D12Resource* finalTarget){
    if ( phase_ == Phase::None || !finalTarget || !capture_ ) { return; }
    // 画面の大きさが変わっていたら作り直す
    const D3D12_RESOURCE_DESC desc = finalTarget->GetDesc();
    const D3D12_RESOURCE_DESC mine = capture_->GetResource()->GetDesc();
    if ( desc.Width != mine.Width || desc.Height != mine.Height ) {
        for ( auto* target : { &capture_, &resume_, &previous_ } ) {
            ( *target )->Resize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), ( uint32_t ) desc.Width, desc.Height);
        }
        sheet_.SetFrontTexture(capture_->GetSrvIndex());
        sheet_.SetCoverTexture(capture_->GetSrvIndex());
        previousValid_ = false;
    }
    if ( captureRequested_ ) {
        Copy(commandList, finalTarget, capture_->GetResource().Get());
        captureRequested_ = false;
        captured_ = true;
    }
    if ( resumeShotRequested_ ) {
        // 再開の絵を写して、画面には前のフレームを出し直す（このフレームは見えない）
        Copy(commandList, finalTarget, resume_->GetResource().Get());
        if ( previousValid_ ) { Copy(commandList, previous_->GetResource().Get(), finalTarget); }
        resumeShotRequested_ = false;
        resumeShotTaken_ = true;
        return;
    }
    Copy(commandList, finalTarget, previous_->GetResource().Get());
    previousValid_ = true;
}

// 初めて使う描き方（タイル貼りの段ボール・紙の動的メッシュ）は、初回の描画でドライバ・デバッグレイヤーの準備が走り、
//   Debug では 0.3 秒ほど止まる（ゲームオーバーの最初に一瞬固まって、画面が黒く写った）。
//   シーンの始め（読み込みカードの裏）の数フレームで、見えない大きさで一度描いておく
void GameOverSequence::Warmup(){
    --warmupFrames_;
    for ( auto& wall : walls_ ) {
        const Vector3 scale = wall->GetScale();
        wall->SetScale({ 0.0f, 0.0f, 0.0f });
        wall->Update();
        wall->Draw();
        wall->SetScale(scale);
        wall->Update();
    }
    if ( board_ ) {
        board_->SetScale({ 0.0f, 0.0f, 0.0f });
        board_->Update();
        board_->Draw();
    }
}

void GameOverSequence::DrawOpaque(){
    if ( warmupFrames_ > 0 ) { Warmup(); }
    if ( phase_ == Phase::None ) { return; }
    if ( !HidesScene() ) {
        // 種明かしの箱（3D のステージと一緒に）
        if ( phase_ <= Phase::Capture && PhaseLength(Phase::Reveal) > 0.0f ) { // 種明かしをしない時は出さない
            for ( auto& wall : walls_ ) { wall->Draw(); }
        }
        return;
    }
    if ( board_ ) { board_->Draw(); }
    sheet_.Draw();
}

void GameOverSequence::DrawUI(){
    if ( phase_ != Phase::None ) { screen_.Draw(); }
}

void GameOverSequence::DrawDebugUI(){
#ifdef USE_IMGUI
    static const char* kNames[] = { "なし", "P0 止まる", "P1 種明かし", "P4 写し取る", "P3/P5 くしゃくしゃ", "捨てる",
                                    "P7 ゲームオーバーの画面", "選ぶ", "P8 再開の絵", "画面をしまう", "P8 広がって再開", "ステージ選択へ" };
    ImGui::Text("段：%s（%.2f 秒）%s", kNames[( int ) phase_], time_, short_ ? " 短縮版" : "");
    if ( ImGui::Button("秒数を読み直す (gameover.json)") ) { LoadSettings(); }
#endif
}
