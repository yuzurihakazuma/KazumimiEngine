#include "game/gameover/GameOverScreen.h"

#include "engine/2d/Sprite.h"
#include "engine/base/WindowProc.h"
#include "engine/graphics/TextureManager.h"
#include "engine/utils/Easing.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    // 1920×1080 での置き場所
    constexpr float kTitleY = 330.0f;
    constexpr float kButtonY[2] = { 610.0f, 750.0f };
    // 手袋と寝ころぶ恐竜が重ならないように（手袋を小さくしてボタンの左のふちに指先を合わせ、恐竜はその左に収める）
    //   ボタンの左のふち＝960−280＝680。手袋の指先は中心から +108×大きさ、恐竜の右のふちは中心から +190×大きさ（絵の見えている所）
    constexpr float kHandScale = 0.8f;
    constexpr float kHandX = 680.0f + 15.0f - 108.0f * kHandScale;
    // 恐竜と敵は、板の枠（ギザギザの縁）の内側に収める。板の内側＝x 314〜1606・y 194〜866（screen_board.png の縁 54 と影の余白から）
    constexpr float kDinoX = 423.0f, kDinoY = 760.0f, kDinoScale = 0.45f;
    constexpr float kEnemyX = 1500.0f, kEnemyY = 782.0f, kEnemyScale = 0.42f;

    std::unique_ptr<Sprite> Make(const char* path, Vector2& outSize){
        auto sprite = Sprite::Create(TextureManager::GetInstance()->Load(path).srvIndex, { 0.0f, 0.0f });
        outSize = sprite->GetSize();
        return sprite;
    }
    // 段の進み（start〜start+length を 0〜1 に）
    float Part(float t, float start, float length){ return Easing::Clamp01(( t - start ) / length); }
}

GameOverScreen::GameOverScreen() = default;
GameOverScreen::~GameOverScreen() = default;

void GameOverScreen::Initialize(){
    Vector2 unused;
    background_ = Make("resources/gameover/screen_bg.png", unused);
    board_ = Make("resources/gameover/screen_board.png", boardSize_);
    title_ = Make("resources/gameover/screen_title.png", titleSize_);
    dino_ = Make("resources/gameover/screen_dino.png", dinoSize_);
    enemy_ = Make("resources/kamishibai/intro_char_enemy_ground.png", enemySize_);
    hand_ = Make("resources/gameover/cursor_hand.png", handSize_);
    buttons_[0] = Make("resources/gameover/button_retry.png", buttonSize_);
    buttons_[1] = Make("resources/gameover/button_stage.png", buttonSize_);
    buttonsOn_[0] = Make("resources/gameover/button_retry_on.png", buttonSize_);
    buttonsOn_[1] = Make("resources/gameover/button_stage_on.png", buttonSize_);
    state_ = State::Hidden;
}

void GameOverScreen::Finalize(){
    background_.reset();
    board_.reset();
    title_.reset();
    dino_.reset();
    enemy_.reset();
    hand_.reset();
    for ( auto& button : buttons_ ) { button.reset(); }
    for ( auto& button : buttonsOn_ ) { button.reset(); }
}

void GameOverScreen::Open(float duration, bool boardShown){
    boardShown_ = boardShown;
    keepBoard_ = false;
    boardVisible_ = true;
    state_ = State::Opening;
    time_ = 0.0f;
    openDuration_ = ( std::max )( duration, 0.1f );
    choice_ = 0;
    choiceTime_ = 1.0f;
    handY_ = kButtonY[0];
}

void GameOverScreen::Close(float duration, bool keepBoard){
    keepBoard_ = keepBoard;
    state_ = State::Closed;
    closeTime_ = 0.0f;
    closeDuration_ = ( std::max )( duration, 0.05f );
}

void GameOverScreen::SetChoice(int choice){
    if ( choice == choice_ ) { return; }
    choice_ = choice;
    choiceTime_ = 0.0f;
}

void GameOverScreen::Update(float deltaTime){
    time_ += deltaTime;
    choiceTime_ += deltaTime;
    if ( state_ == State::Opening && time_ >= openDuration_ ) { state_ = State::Opened; }
    if ( state_ == State::Closed ) { closeTime_ += deltaTime; }
    handY_ += ( kButtonY[choice_] - handY_ ) * ( std::min )( deltaTime * 18.0f, 1.0f );
}

void GameOverScreen::DrawAt(Sprite* sprite, const Vector2& size, float x, float y, float scale, float alpha, float rotation){
    if ( !sprite || scale <= 0.001f || alpha <= 0.001f ) { return; }
    const float width = ( float ) WindowProc::GetInstance()->GetClientWidth();
    const float height = ( float ) WindowProc::GetInstance()->GetClientHeight();
    const float k = ( std::min )( width / 1920.0f, height / 1080.0f );
    sprite->SetPosition({ width * 0.5f + ( x - 960.0f ) * k, height * 0.5f + ( y - 540.0f ) * k });
    sprite->SetSize({ size.x * scale * k, size.y * scale * k });
    sprite->SetRotation(rotation);
    sprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
    sprite->Update();
    sprite->Draw();
}

void GameOverScreen::Draw(){
    if ( state_ == State::Hidden && backgroundAlpha_ <= 0.0f ) { return; }
    // 背景（画面いっぱい）
    if ( background_ && backgroundAlpha_ > 0.0f ) {
        const float width = ( float ) WindowProc::GetInstance()->GetClientWidth();
        const float height = ( float ) WindowProc::GetInstance()->GetClientHeight();
        background_->SetPosition({ width * 0.5f, height * 0.5f });
        background_->SetSize({ width, height });
        background_->SetColor({ 1.0f, 1.0f, 1.0f, backgroundAlpha_ });
        background_->Update();
        background_->Draw();
    }
    if ( state_ == State::Hidden ) { return; }

    // 出てくる順（開く時間に対する割合）。しまう時は全部いっしょにちぢむ
    const float t = state_ == State::Opening ? time_ / openDuration_ : 1.0f;
    const float close = state_ == State::Closed ? 1.0f - Easing::EaseInQuad(Easing::Clamp01(closeTime_ / closeDuration_)) : 1.0f;

    // 板：ポンとふくらんで出る（紙玉が開いて板になった時は、はじめから出ている）
    if ( boardVisible_ ) {
        const float board = boardShown_ ? 1.0f : Easing::EaseOutBack(Part(t, 0.0f, 0.3f));
        DrawAt(board_.get(), boardSize_, 960.0f, 540.0f, board * ( keepBoard_ ? 1.0f : close ), 1.0f);
    }
    if ( close <= 0.0f ) { return; }
    // GAME OVER：上から落ちてきて弾む
    const float drop = Part(t, 0.18f, 0.35f);
    const float bounce = drop < 1.0f ? ( 1.0f - Easing::EaseOutBounce(drop) ) : 0.0f;
    DrawAt(title_.get(), titleSize_, 960.0f, kTitleY - 520.0f * bounce, close, drop > 0.0f ? 1.0f : 0.0f);
    // ボタン：下からすべり込む。選んでいる方は明るく、少しふくらむ
    for ( int i = 0; i < 2; ++i ) {
        const float slide = Easing::EaseOutBack(Part(t, 0.42f + 0.1f * ( float ) i, 0.3f));
        const bool on = i == choice_;
        const float pop = on ? 1.0f + 0.08f * std::sin(Easing::Clamp01(choiceTime_ / 0.2f) * kPi) : 1.0f;
        DrawAt(on ? buttonsOn_[i].get() : buttons_[i].get(), buttonSize_, 960.0f, kButtonY[i] + 260.0f * ( 1.0f - slide ),
               pop * close, slide > 0.0f ? 1.0f : 0.0f);
    }
    // 寝ころぶ恐竜（ゆっくり息をする）と、勝ちほこる敵（ぴょこぴょこ）
    const float chars = Easing::EaseOutBack(Part(t, 0.6f, 0.3f));
    const float breathe = 1.0f + 0.03f * std::sin(time_ * 3.0f);
    DrawAt(dino_.get(), { dinoSize_.x, dinoSize_.y * breathe }, kDinoX, kDinoY, kDinoScale * chars * close, 1.0f);
    const float hop = std::fabs(std::sin(time_ * 4.0f)) * 14.0f;
    DrawAt(enemy_.get(), enemySize_, kEnemyX, kEnemyY - hop, kEnemyScale * chars * close, 1.0f, 0.12f * std::sin(time_ * 4.0f));
    // 手袋：選んでいるボタンの左で、ちょんちょんとさす
    const float hand = Part(t, 0.8f, 0.2f);
    const float poke = -10.0f * std::fabs(std::sin(time_ * 5.0f)); // 引いてからさす（右へは出ない＝ボタンにめり込まない）
    DrawAt(hand_.get(), handSize_, kHandX + poke, handY_, kHandScale * Easing::EaseOutBack(hand) * close, hand > 0.0f ? 1.0f : 0.0f);
}
