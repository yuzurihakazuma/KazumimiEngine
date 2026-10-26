#include "game/worldmap/WorldMapPanel.h"

#include "engine/2d/Sprite.h"
#include "engine/base/WindowProc.h"
#include "engine/graphics/TextureManager.h"
#include "engine/utils/Easing.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace {
    constexpr const char* kPanelTexture = "resources/worldmap/panel.png";
    constexpr const char* kLockedTexture = "resources/worldmap/panel_locked.png";
    constexpr const char* kDigitsTexture = "resources/worldmap/digits.png";
    constexpr const char* kDigitChars = "0123456789/-";
    constexpr float kDigitCellW = 64.0f, kDigitCellH = 96.0f;
    constexpr int   kMaxDigits = 12;
    // パネルの中の置き場所（panel.png の画素。make_worldmap_assets.py の panel() と同じ）
    constexpr float kPanelScale = 0.9f;  // 1080p での大きさ
    constexpr float kMargin = 48.0f;     // 画面の左からの余白(1080p)
    constexpr float kNameY = 104.0f, kNameWidth = 560.0f;
    constexpr float kCoinY = 228.0f, kBestY = 304.0f, kNumberX = 300.0f, kNumberH = 64.0f;
    constexpr float kSlideInTime = 0.45f, kSlideOutSpeed = 5.0f;
}

WorldMapPanel::WorldMapPanel() = default;
WorldMapPanel::~WorldMapPanel() = default;

void WorldMapPanel::Initialize(){
    TextureManager* textures = TextureManager::GetInstance();
    panel_ = Sprite::Create(textures->Load(kPanelTexture).srvIndex, { 0.0f, 0.0f }, { 1, 1, 1, 1 }, { 0.0f, 0.0f });
    panelLocked_ = Sprite::Create(textures->Load(kLockedTexture).srvIndex, { 0.0f, 0.0f }, { 1, 1, 1, 1 }, { 0.0f, 0.0f });
    digitsTexture_ = textures->Load(kDigitsTexture).srvIndex;
    digits_.clear();
    for ( int i = 0; i < kMaxDigits; ++i ) {
        digits_.push_back(Sprite::Create(digitsTexture_, { 0.0f, 0.0f }, { 1, 1, 1, 1 }, { 0.0f, 0.5f }));
    }
    slide_ = 0.0f;
    shown_ = false;
}

void WorldMapPanel::Finalize(){
    panel_.reset();
    panelLocked_.reset();
    names_.clear();
    digits_.clear();
}

void WorldMapPanel::SetContent(const std::string& stage, bool unlocked, int coinTotal, int coinGot){
    stage_ = stage;
    unlocked_ = unlocked;
    coinTotal_ = coinTotal;
    coinGot_ = coinGot;
    // 名前の画像（ステージ開始カードと同じ物）。無ければ名前なし
    if ( unlocked && !stage.empty() && !names_.contains(stage) ) {
        const std::string path = "resources/kamishibai/intro_name_" + stage + ".png";
        std::unique_ptr<Sprite> name;
        if ( std::filesystem::exists(path) ) {
            name = Sprite::Create(TextureManager::GetInstance()->Load(path).srvIndex, { 0.0f, 0.0f }, { 1, 1, 1, 1 }, { 0.5f, 0.5f });
        }
        names_[stage] = std::move(name);
    }
}

void WorldMapPanel::Update(float deltaTime, bool shown){
    if ( shown && !shown_ ) { slideTime_ = 0.0f; }
    shown_ = shown;
    if ( shown ) {
        slideTime_ += deltaTime;
        slide_ = Easing::EaseOutBack(Easing::Clamp01(slideTime_ / kSlideInTime)); // 少し行き過ぎて戻る
    } else {
        slide_ = ( std::max )( 0.0f, ( std::min )( slide_, 1.0f ) - deltaTime * kSlideOutSpeed );
    }
}

void WorldMapPanel::DrawNumber(const std::string& text, float x, float y, float height){
    const float width = height * kDigitCellW / kDigitCellH;
    const float step = width * 0.78f; // 字のふちが重なるくらいに詰める
    // 1枚のスプライトを1フレームに2回描くと、後の位置で上書きされるので、数字ごとに別の物を使う
    for ( char c : text ) {
        if ( nextDigit_ >= ( int ) digits_.size() ) { break; }
        const char* found = std::strchr(kDigitChars, c);
        if ( c == ' ' || !found ) { x += step * 0.5f; continue; }
        Sprite* sprite = digits_[nextDigit_++].get();
        sprite->SetTextureRect(( float ) ( found - kDigitChars ) * kDigitCellW, 0.0f, kDigitCellW, kDigitCellH);
        sprite->SetPosition({ x, y });
        sprite->SetSize({ width, height });
        sprite->Update();
        sprite->Draw();
        x += step;
    }
}

void WorldMapPanel::Draw(){
    if ( slide_ <= 0.001f || !panel_ ) { return; }
    nextDigit_ = 0;
    const float screenH = ( float ) WindowProc::GetInstance()->GetClientHeight();
    const float k = screenH / 1080.0f * kPanelScale;
    Sprite* board = unlocked_ ? panel_.get() : panelLocked_.get();
    const Vector2 size { 636.0f * k, 486.0f * k };
    // 左の外（隠れている）→ 余白の位置（出ている）
    const float left = -size.x - 20.0f + ( size.x + 20.0f + kMargin * k / kPanelScale ) * slide_;
    const float top = screenH * 0.5f - size.y * 0.62f;
    board->SetPosition({ left, top });
    board->SetSize(size);
    board->Update();
    board->Draw();
    if ( !unlocked_ ) { return; }

    // 名前（ステージ開始カードの名前の紙）
    auto name = names_.find(stage_);
    if ( name != names_.end() && name->second ) {
        Sprite* sprite = name->second.get();
        const float scale = kNameWidth / 864.0f * k;
        sprite->SetPosition({ left + 318.0f * k, top + kNameY * k });
        sprite->SetSize({ 864.0f * scale, 230.0f * scale });
        sprite->Update();
        sprite->Draw();
    }
    // コイン：とった数 / 全部の数。ベストはまだ記録が無いので「--」
    DrawNumber(std::to_string(coinGot_) + "/" + std::to_string(coinTotal_), left + kNumberX * k, top + kCoinY * k, kNumberH * k);
    DrawNumber("--", left + kNumberX * k, top + kBestY * k, kNumberH * k);
}
