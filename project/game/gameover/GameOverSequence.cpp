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
#include "engine/graphics/TextureManager.h"
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
    constexpr const char* kBasaSe = "resources/gameover/se_basa.wav";
    constexpr const char* kBoardTexture = "resources/gameover/screen_board.png";
    constexpr const char* kScreenSe = "resources/kamishibai/se_patan.wav";
    constexpr const char* kCrumpleSe = "resources/gameover/se_crumple.wav";
    constexpr const char* kSelectSe = "resources/kamishibai/se_pon.wav";
    constexpr const char* kKraft = "resources/craft/textures/kraft.png";
    constexpr const char* kDizzySe = "resources/gameover/se_dizzy.wav";
    constexpr const char* kThudSe = "resources/kamishibai/se_patan.wav";
    constexpr const char* kStarTexture = "resources/kamishibai/intro_star.png";
    // やられの演技の段（止まる段の長さに対する割合）
    //   着地（一瞬止まる）→ 仰け反りから起き上がる → くらくら（星が回る・ふらふら）→ しりもち → あおむけにぺたん → じっと
    constexpr float kHitEnd = 0.06f, kTurnEnd = 0.2f, kDizzyEnd = 0.62f, kSitEnd = 0.73f, kFallEnd = 0.88f;
    constexpr float kStarRadius = 0.55f;   // 星が回る輪の半径(m)
    constexpr float kStarSize = 0.5f;      // 星の大きさ(m)
    constexpr float kStarSpin = 5.5f;      // 星が回る速さ(rad/秒)
    constexpr float kStarAbove = 0.75f;    // 頭の骨（顔の真ん中あたり）からどれだけ上で回るか(m)
    // 星の色（白いフェルトの星に掛ける。黄色・ピンク・水色）
    const Vector4 kStarColors[] = { { 1.0f, 0.78f, 0.12f, 1.0f }, { 1.0f, 0.42f, 0.62f, 1.0f }, { 0.35f, 0.75f, 1.0f, 1.0f } };
    constexpr float kFastForward = 6.0f;   // 早送りの速さ
    constexpr float kCoverFade = 0.35f;    // くしゃくしゃの始め、写したままの色から陰影の付いた色へ変わる割合
    constexpr float kShake = 0.06f;        // くしゃくしゃの瞬間の揺れ(m)
    constexpr float kBoardBehind = 1.5f;   // シールの奥のフェルトまでの距離(m)
    constexpr float kScreenFadeIn = 0.25f; // ゲームオーバーの画面の地（スプライト）を、奥のフェルトに重ねてなじませる時間
    constexpr float kBallShare = 0.075f;   // 紙玉の半径（画面の高さに対して）
    constexpr float kBallSpin = 2.2f;      // 丸まる間に紙玉が回る角度(rad)
    constexpr float kHoldShare = 0.3f;     // 開く段のはじめ、紙玉のまま弾んでいる割合
    constexpr float kFlatShare = 0.35f;    // 開く段の終わり、光なしの絵（写したままの色）へなじませる割合
    constexpr float kUnscreenTime = 0.3f;  // 文字とボタンをしまう時間（看板は残す）

    Vector3 Add(const Vector3& a, const Vector3& b){ return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    Vector3 Sub(const Vector3& a, const Vector3& b){ return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    Vector3 Mul(const Vector3& a, float s){ return { a.x * s, a.y * s, a.z * s }; }
    float Dot(const Vector3& a, const Vector3& b){ return a.x * b.x + a.y * b.y + a.z * b.z; }
    Vector3 Cross(const Vector3& a, const Vector3& b){ return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    Vector3 Normalize(const Vector3& v){
        const float length = std::sqrt(Dot(v, v));
        return length > 1e-6f ? Mul(v, 1.0f / length) : Vector3 { 0.0f, 0.0f, 1.0f };
    }
    // 軸 axis（長さ1）のまわりに angle 回す
    Vector3 Rotate(const Vector3& v, const Vector3& axis, float angle){
        const float c = std::cos(angle), s = std::sin(angle);
        return Add(Add(Mul(v, c), Mul(Cross(axis, v), s)), Mul(axis, Dot(axis, v) * ( 1.0f - c )));
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
    // シールの下：ゲームオーバーの画面の地と同じ紫のフェルト（光なし。画面ぴったりに置くので、画面の地にそのまま切り替わる）
    board_ = MakeTiledBox("go_board", { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
    if ( Model* model = ModelManager::GetInstance()->FindModel("go_board") ) {
        model->SetTexture("resources/gameover/screen_bg.png");
        if ( Model::Material* material = model->GetMaterial() ) { material->enableLighting = 0; }
    }
    screen_.Initialize();
    AudioManager* audio = AudioManager::GetInstance();
    for ( const char* se : { kFreezeSe, kRevealSe, kScreenSe, kCrumpleSe, kBasaSe, kDizzySe, kThudSe } ) { audio->LoadWave(se); }
    // 頭の上で回る星：白いフェルトの星の板に色を付ける（光なし・いつもカメラの方を向く）
    stars_.clear();
    for ( int i = 0; i < 3; ++i ) {
        const std::string name = "go_star_" + std::to_string(i);
        ModelManager* modelManager = ModelManager::GetInstance();
        if ( !modelManager->FindModel(name) ) { modelManager->CreatePlaneModel(name, 1.0f, 1.0f); }
        if ( Model* model = modelManager->FindModel(name) ) {
            model->SetTexture(kStarTexture);
            if ( Model::Material* material = model->GetMaterial() ) {
                material->color = kStarColors[i];
                material->enableLighting = 0;
                material->matte = 1.0f;
            }
        }
        auto star = Obj3d::Create(name);
        if ( star ) {
            star->SetIsBillboard(true);
            star->SetPipelineType(PipelineType::Object3D_CullNone);
            stars_.push_back(std::move(star));
        }
    }
    boardSrv_ = TextureManager::GetInstance()->Load(kBoardTexture).srvIndex;
    phase_ = Phase::None;
}

void GameOverSequence::Finalize(){
    sheet_.Finalize();
    screen_.Finalize();
    capture_.reset();
    resume_.reset();
    previous_.reset();
    board_.reset();
    stars_.clear();
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
        timing.open = t.value("open", timing.open);
        timing.screen = t.value("screen", timing.screen);
        timing.rebuild = t.value("rebuild", timing.rebuild);
    };
    shortTiming_ = { 0.3f, 0.0f, 0.55f, 0.5f, 0.5f, 0.55f };
    read("full", full_);
    read("short", shortTiming_);
    hearts_ = content.value("hearts", hearts_);
    damage_ = content.value("damage", damage_);
    sheetDistance_ = content.value("sheetDistance", sheetDistance_);
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
    case Phase::Open:     return t.open;
    case Phase::Screen:   return t.screen;
    case Phase::Unscreen: return kUnscreenTime;
    case Phase::Fold:     return t.crumple * 0.75f;
    case Phase::Rebuild:  return t.rebuild;
    default:              return 0.0f;
    }
}

// 紙を、写し取った時のカメラの前（画面の真ん中）に、幅 width・高さ height(m) で置く
void GameOverSequence::PlaceSheet(float width, float height){
    const Vector3 center = Add(camPos_, Mul(forward_, sheetDistance_));
    sheet_.Place(Sub(center, Mul(up_, height * 0.5f)), right_, up_, Mul(forward_, -1.0f), width, height);
}

void GameOverSequence::PlaceSheetFull(){ PlaceSheet(sheetWidth_, sheetHeight_); }

// ゲームオーバーの看板（スプライト）と同じ所・同じ大きさ：画面の真ん中に、1920×1080 基準の画素 × 縮め（GameOverScreen と同じ計算）
void GameOverSequence::PlaceSheetBoard(){
    const float width = ( float ) ( std::max )( 1, WindowProc::GetInstance()->GetClientWidth() );
    const float height = ( float ) ( std::max )( 1, WindowProc::GetInstance()->GetClientHeight() );
    const float k = ( std::min )( width / 1920.0f, height / 1080.0f );
    const float metersPerPixel = sheetHeight_ / height;
    PlaceSheet(screen_.BoardSize().x * k * metersPerPixel, screen_.BoardSize().y * k * metersPerPixel);
}

void GameOverSequence::SetBall(float amount, float spin){
    sheet_.SetBall(amount, sheetHeight_ * kBallShare, spin);
    sheet_.SetCrumple(( std::min )( amount * 1.6f, 1.0f )); // 丸まりはじめから折れ目を立てる
}

void GameOverSequence::Enter(Phase phase){
    phase_ = phase;
    time_ = 0.0f;
    startPos_ = camPos_;
    startFov_ = camFov_;
    AudioManager* audio = AudioManager::GetInstance();
    switch ( phase ) {
    case Phase::Freeze:
        audio->PlayWave(kFreezeSe, false, 0.9f); // ぶつかった
        defeatPrev_ = 0.0f;
        starAngle_ = 0.0f;
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
        PlaceSheetFull();
        SetBall(0.0f, 0.0f);
        sheet_.SetCoverAlpha(1.0f);
        sheet_.SetVisible(true);
        if ( board_ ) {
            // シールの下の紫のフェルト：少し奥に、画面ぴったりの大きさで（ゲームオーバーの画面の地と同じ見え方）
            const float spread = ( sheetDistance_ + kBoardBehind ) / sheetDistance_;
            board_->SetTranslation(Add(camPos_, Mul(forward_, sheetDistance_ + kBoardBehind)));
            board_->SetRotation(RotationOf(forward_));
            board_->SetScale({ sheetWidth_ * spread, sheetHeight_ * spread, 0.05f });
        }
        restoreRequested_ = true; // 見えていない間に、シーンはチェックポイントへ戻す
        audio->PlayWave(kCrumpleSe, false, 1.0f);
        break;
    }
    case Phase::Open:
        // 紙玉の中身を看板の絵に替える（丸まり切って見えない間に）。大きさも看板に合わせる（紙玉の大きさは変わらない）
        sheet_.SetFrontTexture(boardSrv_);
        sheet_.SetCoverTexture(boardSrv_);
        sheet_.SetCoverAlpha(0.0f);
        PlaceSheetBoard();
        break;
    case Phase::Screen:
        screen_.SetBackground(0.0f); // 奥のフェルトと同じ絵を、少しずつ重ねる（Update）
        screen_.Open(PhaseLength(Phase::Screen), true); // 看板はもう開いている
        audio->PlayWave(kScreenSe, false, 0.9f);
        break;
    case Phase::ResumeShot:
        resumeShotRequested_ = true; // この1フレームだけ、再開のカメラで 3D を描いて写す
        resumeShotTaken_ = false;
        break;
    case Phase::Unscreen:
        screen_.Close(kUnscreenTime, true); // 文字とボタンだけしまう（看板は残す）
        break;
    case Phase::Fold:
        // 看板（スプライト）を、同じ所・同じ大きさの紙に替えてから丸める。画面の地（スプライト）も外す
        //   （スプライトは 3D の紙より手前に描かれるので、残すと紙が隠れる。奥に同じ絵の板があるので見た目は変わらない）
        screen_.SetBoardVisible(false);
        screen_.SetBackground(0.0f);
        sheet_.SetFrontTexture(boardSrv_);
        sheet_.SetCoverTexture(boardSrv_);
        sheet_.SetCoverAlpha(1.0f);
        PlaceSheetBoard();
        SetBall(0.0f, 0.0f);
        sheet_.SetVisible(true);
        audio->PlayWave(kCrumpleSe, false, 0.9f);
        break;
    case Phase::Rebuild:
        // 紙玉の中身を再開する所の絵に替え、大きさを画面ぴったりに（丸まり切って見えない間に）
        sheet_.SetFrontTexture(resume_->GetSrvIndex());
        sheet_.SetCoverTexture(resume_->GetSrvIndex());
        sheet_.SetCoverAlpha(0.0f);
        PlaceSheetFull();
        break;
    default:
        break;
    }
}

bool GameOverSequence::HidesScene() const{
    return phase_ >= Phase::Crumple && phase_ <= Phase::Rebuild && phase_ != Phase::ResumeShot;
}

// やられの演技：ぶつかって一瞬止まる → こっちを向いて「あっ」と跳ねる → 頭の上で星がくるくる回り、ふらふら揺れる
//   → しりもち → あおむけにぺたん（地面で1回弾む）→ そのまま、じっと（星だけゆっくり回る）
bool GameOverSequence::DefeatPose(DefeatPoseData& out) const{
    if ( phase_ != Phase::Freeze && phase_ != Phase::Reveal && phase_ != Phase::Capture ) { return false; }
    const float length = ( std::max )( PhaseLength(Phase::Freeze), 0.01f );
    const float u = phase_ == Phase::Freeze ? Easing::Clamp01(time_ / length) : 1.0f;
    auto part = [&](float a, float b){ return Easing::Clamp01(( u - a ) / ( b - a )); };
    out = DefeatPoseData {};

    // 仰け反った形（とどめのスローの間の形）から起き上がる。着いた所で少しつぶれ、起きながら縦に伸びる
    const float turn = part(kHitEnd, kTurnEnd);
    out.pitch = kBendPitch * ( 1.0f - Easing::EaseOutBack(turn) );
    out.squash = 1.0f - 0.1f * std::sin(part(0.0f, kHitEnd + 0.04f) * kPi) + 0.08f * std::sin(turn * kPi);
    // くらくら：左右にふらふら（だんだん大きく）
    const float dizzy = part(kTurnEnd, kDizzyEnd);
    if ( dizzy > 0.0f && dizzy < 1.0f ) {
        out.roll = 0.22f * std::sin(dizzy * kPi * 5.0f) * std::sin(dizzy * kPi) * ( 0.6f + 0.4f * dizzy );
        out.wobble = 0.3f * std::sin(dizzy * kPi * 3.0f) * std::sin(dizzy * kPi); // 行きつ戻りつしながら
    }
    // 目を回して、ふらふらと画面から見て横向きへ（倒れる所が縮んで見えないように。横向きならほとんど回らない）
    out.spin = Easing::EaseInOutSine(dizzy);
    // しりもち：すとんと腰を落とす（縦につぶれる）
    const float sit = part(kDizzyEnd, kSitEnd);
    if ( sit > 0.0f ) {
        out.squash = 1.0f - 0.22f * Easing::EaseOutBack(sit);
    }
    // あおむけにぺたん：後ろへ倒れて、地面で1回弾む。倒れながら、縦のつぶれは元へ
    const float fall = part(kSitEnd, kFallEnd);
    if ( fall > 0.0f ) {
        out.pitch = -1.1f * Easing::EaseOutBounce(fall); // 倒れ切ると顔が隠れるので、少し起きた所で止める
        out.squash = 1.0f - 0.22f * ( 1.0f - Easing::EaseOutQuad(fall) );
    }
    return true;
}

// 頭の上で星がくるくる回る（くらくらの間にポンと出て、倒れた後もゆっくり回り続ける）
void GameOverSequence::UpdateStars(float deltaTime){
    const bool acting = phase_ == Phase::Freeze || phase_ == Phase::Reveal || phase_ == Phase::Capture;
    const float length = ( std::max )( PhaseLength(Phase::Freeze), 0.01f );
    const float u = phase_ == Phase::Freeze ? Easing::Clamp01(time_ / length) : 1.0f;
    // 音：くらくらの始めに「ぴよぴよ」、倒れて地面に着いた時に「ぺたん」
    if ( phase_ == Phase::Freeze ) {
        if ( defeatPrev_ < kTurnEnd && u >= kTurnEnd ) { AudioManager::GetInstance()->PlayWave(kDizzySe, false, 0.7f); }
        const float land = kSitEnd + ( kFallEnd - kSitEnd ) * 0.36f; // EaseOutBounce が初めて地面に着く所
        if ( defeatPrev_ < land && u >= land ) { AudioManager::GetInstance()->PlayWave(kThudSe, false, 0.9f); }
        defeatPrev_ = u;
    }
    const float appear = Easing::EaseOutBack(Easing::Clamp01(( u - kTurnEnd ) / 0.08f));
    const float speed = u < kSitEnd ? kStarSpin : kStarSpin * 0.45f; // 倒れたらゆっくり
    starAngle_ += deltaTime * speed;
    starSize_ = acting ? kStarSize * appear : 0.0f;
}

// 星の位置は描く直前に決める（演出のカメラと恐竜の頭の位置が、そのフレームの物に決まった後。
// 先に決めると1フレーム前のカメラで画面へ写り、カメラが動いた時に星だけ画面に付いてくるように見える）
void GameOverSequence::DrawTransparent(){
    if ( phase_ != Phase::Freeze && phase_ != Phase::Reveal && phase_ != Phase::Capture ) { return; }
    // 輪はカメラから見た横・奥行きで回す（恐竜がどっちを向いていても、画面では同じ横長の輪）
    Vector3 depth { forward_.x, 0.0f, forward_.z };
    depth = depth.x * depth.x + depth.z * depth.z > 1e-8f ? Normalize(depth) : Vector3 { 0.0f, 0.0f, 1.0f };
    const Vector3 side { depth.z, 0.0f, -depth.x };
    for ( size_t i = 0; i < stars_.size(); ++i ) {
        const float angle = starAngle_ + ( float ) i * 2.0f * kPi / ( float ) stars_.size();
        Vector3 at = Add(head_, Add(Mul(side, std::cos(angle) * kStarRadius), Mul(depth, std::sin(angle) * kStarRadius * 0.55f)));
        at.y += kStarAbove + 0.06f * std::sin(angle * 2.0f);
        stars_[i]->SetTranslation(at);
        stars_[i]->SetScale({ starSize_, starSize_, starSize_ });
        stars_[i]->SetRotation({ 0.0f, 0.0f, angle * 0.6f });
        stars_[i]->Update();
        stars_[i]->Draw();
    }
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
    case Phase::Freeze: {
        // ぶつかった瞬間：画面が少しだけ揺れる
        const float hit = Easing::Clamp01(t / kHitEnd);
        const float shake = 0.05f * ( 1.0f - hit ) * std::sin(time_ * 80.0f);
        camPos_ = Add(startPos_, Mul(right_, shake));
        if ( t >= 1.0f ) { camPos_ = startPos_; Enter(Phase::Reveal); }
        break;
    }
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
        // 画面が、ふちから真ん中へ丸まって紙玉になる（アイリスアウトのように）。写したままの色から、陰影の付いた紙へ
        const float e = Easing::EaseInOutCubic(t);
        SetBall(e, kBallSpin * e);
        sheet_.SetCoverAlpha(1.0f - Easing::EaseInOutSine(Easing::Clamp01(t / kCoverFade)));
        // グシャッとした瞬間に、画面が少し揺れる
        const float shake = kShake * ( 1.0f - Easing::Clamp01(t / 0.4f) ) * std::sin(time_ * 70.0f);
        camPos_ = Add(startPos_, Add(Mul(right_, shake), Mul(up_, shake * 0.6f)));
        if ( t >= 1.0f ) { camPos_ = startPos_; Enter(Phase::Open); }
        break;
    }
    case Phase::Open: {
        // 紙玉がポンと弾んで、開くと看板になっている
        const float hold = Easing::Clamp01(t / kHoldShare);
        const float open = Easing::Clamp01(( t - kHoldShare ) / ( 1.0f - kHoldShare ));
        if ( t >= kHoldShare && time_ - dt < length * kHoldShare ) { AudioManager::GetInstance()->PlayWave(kBasaSe, false, 0.9f); }
        const float pop = 1.0f + 0.25f * std::sin(hold * kPi); // 弾む（紙玉が少しふくらむ）
        const float e = Easing::EaseOutCubic(open);
        sheet_.SetBall(1.0f - e, sheetHeight_ * kBallShare * pop, kBallSpin * ( 1.0f - e ));
        sheet_.SetCrumple(( std::min )( ( 1.0f - e ) * 1.6f, 1.0f ));
        sheet_.SetCoverAlpha(Easing::Clamp01(( open - ( 1.0f - kFlatShare ) ) / kFlatShare));
        if ( t >= 1.0f ) { sheet_.SetVisible(false); Enter(Phase::Screen); }
        break;
    }
    case Phase::Screen:
        // 画面の地（スプライト）を、奥のフェルトの上に重ねてなじませる（同じ絵なので、切り替わりは見えない）
        screen_.SetBackground(Easing::Clamp01(time_ / kScreenFadeIn));
        if ( screen_.IsOpened() ) { Enter(Phase::Choose); }
        break;
    case Phase::ResumeShot:
        // 再開する所の絵を写した（そのフレームは前の画面を出し直したので見えない）
        if ( resumeShotTaken_ ) {
            Enter(Phase::Unscreen);
        }
        break;
    case Phase::Unscreen:
        // 文字とボタンがしまわれる（看板と画面の地は残す）
        if ( t >= 1.0f ) { Enter(Phase::Fold); }
        break;
    case Phase::Fold: {
        // 看板が真ん中へ丸まって紙玉になる
        const float e = Easing::EaseInOutCubic(t);
        SetBall(e, kBallSpin * e);
        sheet_.SetCoverAlpha(1.0f - Easing::EaseInOutSine(Easing::Clamp01(t / kCoverFade)));
        if ( t >= 1.0f ) { Enter(Phase::Rebuild); }
        break;
    }
    case Phase::Rebuild: {
        // 紙玉がポンと弾んで開くと、再開する所の画面が画面いっぱいに広がる
        const float hold = Easing::Clamp01(t / kHoldShare);
        const float open = Easing::Clamp01(( t - kHoldShare ) / ( 1.0f - kHoldShare ));
        if ( t >= kHoldShare && time_ - dt < length * kHoldShare ) { AudioManager::GetInstance()->PlayWave(kBasaSe, false, 0.9f); }
        const float pop = 1.0f + 0.25f * std::sin(hold * kPi);
        const float e = Easing::EaseOutCubic(open);
        sheet_.SetBall(1.0f - e, sheetHeight_ * kBallShare * pop, -kBallSpin * ( 1.0f - e ));
        sheet_.SetCrumple(( std::min )( ( 1.0f - e ) * 1.6f, 1.0f ));
        sheet_.SetCoverAlpha(Easing::Clamp01(( open - ( 1.0f - kFlatShare ) ) / kFlatShare));
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

    UpdateStars(dt);
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
    static const char* kNames[] = { "なし", "P0 止まる", "P1 種明かし", "P4 写し取る", "P3/P5 丸める", "開いて看板",
                                    "P7 ゲームオーバーの画面", "選ぶ", "P8 再開の絵", "文字をしまう", "P8 看板を丸める", "P8 開いて再開", "ステージ選択へ" };
    ImGui::Text("段：%s（%.2f 秒）%s", kNames[( int ) phase_], time_, short_ ? " 短縮版" : "");
    if ( ImGui::Button("秒数を読み直す (gameover.json)") ) { LoadSettings(); }
#endif
}
