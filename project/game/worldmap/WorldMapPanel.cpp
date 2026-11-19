#include "game/worldmap/WorldMapPanel.h"
#include "game/ui/UiScale.h"

#include "engine/2d/Sprite.h"
#include "engine/base/WindowProc.h"
#include "engine/graphics/TextureManager.h"
#include "engine/utils/Easing.h"

#include <algorithm>
#include <filesystem>

using namespace KazumimiEngine;

namespace {
    constexpr const char* kPanelTexture = "resources/worldmap/panel.png";
    constexpr const char* kLockedTexture = "resources/worldmap/panel_locked.png";
    constexpr const char* kDigitsTexture = "resources/worldmap/digits.png";
    constexpr const char* kDigitChars = "0123456789/-";
    constexpr float kDigitCellW = 64.0f, kDigitCellH = 96.0f;
    constexpr int   kMaxDigits = 24;
    // パネルの中の置き場所（panel.png の画素。make_worldmap_assets.py の panel() と同じ）
    constexpr float kPanelScale = 0.9f;  // 1080p での大きさ
    constexpr float kMargin = 48.0f;     // 画面の左からの余白(1080p)
    constexpr Vector2 kPanelSize { 636.0f, 556.0f }; // 絵の大きさ（板 620×540＋影）
    constexpr Vector2 kNameSize { 864.0f, 230.0f };  // 名前の絵の大きさ（intro_name_<stage>.png）
    constexpr float kNameX = 318.0f, kNameY = 104.0f, kNameWidth = 560.0f;
    constexpr float kCoinY = 228.0f, kBestY = 304.0f, kStampY = 380.0f, kNumberX = 300.0f, kNumberH = 64.0f;
    constexpr float kTopRatio = 0.62f;   // 画面の真ん中から、パネルの高さのこの割合だけ上に置く
    constexpr float kHiddenGap = 20.0f;  // 隠れている時、画面の左の外へ余分に出す量(画素)
    constexpr float kSlideInTime = 0.45f, kSlideOutSpeed = 5.0f;
}

WorldMapPanel::WorldMapPanel() = default;
WorldMapPanel::~WorldMapPanel() = default;

void WorldMapPanel::Initialize(){
    TextureManager* textures = TextureManager::GetInstance();
    panel_ = Sprite::Create(textures->Load(kPanelTexture).srvIndex, { 0.0f, 0.0f }, { 1, 1, 1, 1 }, { 0.0f, 0.0f });
    panelLocked_ = Sprite::Create(textures->Load(kLockedTexture).srvIndex, { 0.0f, 0.0f }, { 1, 1, 1, 1 }, { 0.0f, 0.0f });
    digits_.Initialize(kDigitsTexture, kDigitChars, kDigitCellW, kDigitCellH, kMaxDigits);
    slide_ = 0.0f;
    shown_ = false;
}

void WorldMapPanel::Finalize(){
    panel_.reset();
    panelLocked_.reset();
    names_.clear();
    digits_.Finalize();
}

void WorldMapPanel::SetContent(const std::string& stage, bool unlocked, const Counts& counts){
    stage_ = stage;
    unlocked_ = unlocked;
    counts_ = counts;
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

void WorldMapPanel::Draw(){
    if ( slide_ <= 0.001f || !panel_ ) { return; }
    digits_.BeginFrame();
    const float screenH = ( float ) WindowProc::GetInstance()->GetClientHeight();
    const float k = UiScale::FromHeight(screenH) * kPanelScale;
    Sprite* board = unlocked_ ? panel_.get() : panelLocked_.get();
    const Vector2 size { kPanelSize.x * k, kPanelSize.y * k };
    // 左の外（隠れている）→ 余白の位置（出ている）
    const float left = -size.x - kHiddenGap + ( size.x + kHiddenGap + kMargin * k / kPanelScale ) * slide_;
    const float top = screenH * 0.5f - size.y * kTopRatio;
    board->SetPosition({ left, top });
    board->SetSize(size);
    board->Update();
    board->Draw();
    if ( !unlocked_ ) { return; }

    // 名前（ステージ開始カードの名前の紙）
    auto name = names_.find(stage_);
    if ( name != names_.end() && name->second ) {
        Sprite* sprite = name->second.get();
        const float scale = kNameWidth / kNameSize.x * k;
        sprite->SetPosition({ left + kNameX * k, top + kNameY * k });
        sprite->SetSize({ kNameSize.x * scale, kNameSize.y * scale });
        sprite->Update();
        sprite->Draw();
    }
    // コイン：いちばんためた数。コンボ花：見つけた数 / 全体（コンボの的が無いステージは「--」）
    //   星：取った数 / 3（まだクリアしていないステージは「--」）
    auto fraction = [](int found, int total){
        return total > 0 ? std::to_string(found) + "/" + std::to_string(total) : std::string("--");
    };
    const float numberX = left + kNumberX * k, numberHeight = kNumberH * k;
    digits_.Draw(std::to_string(counts_.coins), numberX, top + kCoinY * k, numberHeight);
    digits_.Draw(fraction(counts_.comboFound, counts_.comboTotal), numberX, top + kBestY * k, numberHeight);
    digits_.Draw(fraction(counts_.stars, counts_.starTotal), numberX, top + kStampY * k, numberHeight);
}
