#include "game/clear/ResultScreen.h"
#include "game/ui/UiScale.h"

#include "engine/2d/Sprite.h"
#include "engine/audio/AudioManager.h"
#include "engine/base/WindowProc.h"
#include "engine/graphics/TextureManager.h"
#include "engine/utils/Easing.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace {
    // 基準の画面（置き場所はこの大きさで決めてある）

    constexpr const char* kFolder = "resources/clear/";
    constexpr const char* kWhiteTexture = "resources/block/white1x1.png";
    constexpr const char* kNameTexture = "resources/kamishibai/intro_name_";     // ＋ステージ名＋ .png（ステージ開始カードと同じ絵）
    constexpr const char* kComboFlowerTexture = "resources/ui/hud_combo_flower.png";
    constexpr const char* kHiddenFlowerTexture = "resources/ui/hud_flower.png";
    constexpr const char* kFlowerSound = "resources/kamishibai/se_pon.wav";
    // 数字の絵（HUD と同じ。× は 'x' で引く）
    constexpr const char* kDigitsTexture = "resources/ui/hud_digits.png";
    constexpr const char* kDigitChars = "0123456789/x+";
    constexpr float kDigitCellWidth = 64.0f, kDigitCellHeight = 96.0f;
    constexpr int   kDigitCapacity = 48;

    constexpr int   kMaxBandFlowers = 8;        // 帯に並べられる花の数（これより多い時は、数字だけで出す）
    constexpr float kFastSpeed = 8.0f;          // 早送りの速さ（倍）
    constexpr float kMatDropFrom = -1000.0f;    // マットがすべり込む前の高さ（画面の上の外）
    constexpr float kStarSpin = 1.6f;           // 星が出る時の回り始めの角度(ラジアン)。くるっと回って止まる
    constexpr float kStarHadAlpha = 0.45f;      // 前のプレイで取った星（今回は取れなかった）の、うすさ
    constexpr float kCountShortest = 0.35f;     // 数え上げ：小さい数でもこの割合の時間はかける
    constexpr float kCountFullAt = 10.0f;       // 数え上げ：この数以上で、決めた時間いっぱい
    constexpr float kPromptBob = 6.0f;          // 案内の上下のゆれ(画素)
    constexpr float kPromptBobRate = 1.2f;      // 案内のゆれの速さ(回/秒)
    constexpr float kHidden = -1.0f;            // 「まだ出していない」の印
    const Vector4 kCrayonRed { 0.67f, 0.03f, 0.025f, 1.0f }; // 花丸のクレヨンの赤（リニア）
    constexpr float kCountVolume = 0.5f, kStarVolume = 0.8f, kHanamaruVolume = 0.8f, kFlowerVolume = 0.6f;

    std::unique_ptr<Sprite> MakeSprite(const std::string& path, const Vector2& anchor = { 0.5f, 0.5f }){
        return Sprite::Create(TextureManager::GetInstance()->Load(path).srvIndex, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, anchor);
    }
    std::string File(const char* name){ return std::string(kFolder) + name; }
}

ResultScreen::ResultScreen() = default;
ResultScreen::~ResultScreen() = default;

void ResultScreen::Initialize(const ClearSettings& settings, const std::string& stage){
    layout_ = settings.result;
    sounds_ = settings.sounds;

    // ステージの名前の絵（ステージ開始カードと同じ物）。無ければ名前なし
    const std::string namePath = kNameTexture + stage + ".png";
    name_.reset();
    if ( std::filesystem::exists(namePath) ) {
        name_ = MakeSprite(namePath);
        nameSize_ = name_->GetSize();
    }

    dim_ = MakeSprite(kWhiteTexture, { 0.0f, 0.0f });
    mat_ = MakeSprite(File("result_mat.png"));
    matSize_ = mat_->GetSize();
    title_ = MakeSprite(File("result_title.png"));
    titleSize_ = title_->GetSize();
    hanamaru_ = MakeSprite(File("hanamaru.png"));
    prompt_ = MakeSprite(File("prompt_next.png"));
    promptSize_ = prompt_->GetSize();

    // 行の枠と名前（[0]＝帯、[1..]＝3 行）
    const char* labelFiles[Row_Count + 1] = { "label_band.png", "label_combo.png", "label_health.png", "label_enemy.png" };
    for ( int i = 0; i <= Row_Count; ++i ) {
        rowBoards_[i] = MakeSprite(File("result_row.png"));
        labels_[i] = MakeSprite(File(labelFiles[i]), { 0.0f, 0.5f });
        labelSizes_[i] = labels_[i]->GetSize();
    }
    rowSize_ = rowBoards_[0]->GetSize();
    flowerLabel_ = MakeSprite(File("label_band_flowers.png"), { 0.0f, 0.5f });
    flowerLabelSize_ = flowerLabel_->GetSize();

    const char* iconFiles[Row_Count] = { "icon_combo.png", "icon_health.png", "icon_enemy.png" };
    for ( int i = 0; i < Row_Count; ++i ) {
        icons_[i] = MakeSprite(File(iconFiles[i]));
        stampEmpty_[i] = MakeSprite(File("star_empty.png")); // 星の空欄（点線の星）
        stamps_[i] = MakeSprite(File("star_full.png"));      // 星（フェルトの金色の星）
    }
    for ( int i = 0; i < kMaxBandFlowers; ++i ) {
        flowerEmpty_.push_back(MakeSprite(File("flower_empty.png")));
        comboFlowers_.push_back(MakeSprite(kComboFlowerTexture));
        hiddenFlowers_.push_back(MakeSprite(kHiddenFlowerTexture));
    }
    digits_.Initialize(kDigitsTexture, kDigitChars, kDigitCellWidth, kDigitCellHeight, kDigitCapacity);

    AudioManager* audio = AudioManager::GetInstance();
    for ( const std::string& sound : { sounds_.count, sounds_.star, sounds_.hanamaru, std::string(kFlowerSound) } ) {
        audio->LoadWave(sound);
    }
    Close();
}

void ResultScreen::Finalize(){
    // スプライトは GPU の物を持つので、デバイスより先に放す
    dim_.reset(); mat_.reset(); title_.reset(); hanamaru_.reset(); prompt_.reset();
    flowerLabel_.reset();
    name_.reset();
    for ( auto& sprite : rowBoards_ ) { sprite.reset(); }
    for ( auto& sprite : labels_ ) { sprite.reset(); }
    for ( auto& sprite : icons_ ) { sprite.reset(); }
    for ( auto& sprite : stampEmpty_ ) { sprite.reset(); }
    for ( auto& sprite : stamps_ ) { sprite.reset(); }
    flowerEmpty_.clear(); comboFlowers_.clear(); hiddenFlowers_.clear();
    digits_.Finalize();
    phase_ = Phase::Closed;
}

void ResultScreen::Close(){
    phase_ = Phase::Closed;
    rows_.Clear();
}

void ResultScreen::Open(const Data& data){
    data_ = data;
    data_.bandTotal = ( data_.bandKind == "none" ) ? 0 : ( std::max )( data_.bandTotal, 0 );
    data_.bandFound = std::clamp(data_.bandFound, 0, data_.bandTotal);
    phase_ = Phase::Showing;
    speed_ = 1.0f;
    openTime_ = time_ = 0.0f;
    titleShown_ = hanamaruProgress_ = promptShown_ = kHidden;
    rowShown_.fill(kHidden);
    stampShown_.fill(kHidden);
    rowValue_.fill(0.0f);
    flowerShown_.assign(static_cast<size_t>( ( std::min )( data_.bandTotal, kMaxBandFlowers ) ), kHidden);
    lastTick_ = 0;

    BuildRows();
    rows_.Start(*this);
}

int ResultScreen::RowTarget(Row row) const{
    switch ( row ) {
    case Row_Combo:  return data_.maxCombo;
    case Row_Health: return data_.health;
    case Row_Enemy:  return data_.enemiesDefeated;
    default:         return 0;
    }
}

std::string ResultScreen::RowText(Row row) const{
    const std::string value = std::to_string(static_cast<int>( rowValue_[row] ));
    switch ( row ) {
    case Row_Combo:  return "x" + value;
    case Row_Health: return value + "/" + std::to_string(data_.healthMax);
    case Row_Enemy:  return value + "/" + std::to_string(data_.enemyTotal);
    default:         return value;
    }
}

// 1 行ぶん：行を出す → 数え上げ → （達成していれば）卵スタンプ → 少し間
void ResultScreen::AddRowSteps(Row row){
    AudioManager* audio = AudioManager::GetInstance();
    rows_.AddCall("行を出す", [row](ResultScreen& screen){ screen.rowShown_[row + 1] = 0.0f; screen.lastTick_ = 0; });
    rows_.AddTimed("行が出る間", layout_.itemIn);
    rows_.AddUntil("数え上げ", [row, audio](ResultScreen& screen, float elapsed, float /*deltaTime*/){
        const int target = screen.RowTarget(row);
        // 小さい数は短く（1 を数えるのに 1 秒近くかけない）
        const float duration = screen.layout_.countTime
            * std::clamp(static_cast<float>( target ) / kCountFullAt, kCountShortest, 1.0f);
        const float progress = ( target > 0 && duration > 0.0f ) ? Easing::Clamp01(elapsed / duration) : 1.0f;
        screen.rowValue_[row] = static_cast<float>( target ) * progress;
        const int shown = static_cast<int>( screen.rowValue_[row] );
        if ( shown != screen.lastTick_ ) {
            screen.lastTick_ = shown;
            audio->PlayWave(screen.sounds_.count, false, kCountVolume);
        }
        return progress >= 1.0f;
    });
    if ( data_.achieved[row] ) {
        rows_.AddCall("星が出る", [row, audio](ResultScreen& screen){
            screen.stampShown_[row] = 0.0f;
            audio->PlayWave(screen.sounds_.star, false, kStarVolume);
        });
        rows_.AddTimed("星の間", layout_.stampTime);
    }
    rows_.AddTimed("行の間", layout_.rowGap);
}

// 出す順番：マット → ステージ名 → 帯（花が 1 つずつ）→ 3 行 → 花丸 → 決定ボタンの案内
void ResultScreen::BuildRows(){
    AudioManager* audio = AudioManager::GetInstance();
    rows_.Clear();
    rows_.AddTimed("マットが出る", layout_.matIn);
    rows_.AddCall("ステージ名を出す", [](ResultScreen& screen){ screen.titleShown_ = 0.0f; });
    rows_.AddTimed("ステージ名が出る間", layout_.itemIn);

    if ( data_.bandTotal > 0 ) {
        rows_.AddCall("帯を出す", [](ResultScreen& screen){ screen.rowShown_[0] = 0.0f; });
        rows_.AddTimed("帯が出る間", layout_.itemIn);
        const int lit = ( std::min )( data_.bandFound, static_cast<int>( flowerShown_.size() ) );
        for ( int i = 0; i < lit; ++i ) {
            rows_.AddCall("花が咲く", [i, audio](ResultScreen& screen){
                screen.flowerShown_[static_cast<size_t>( i )] = 0.0f;
                audio->PlayWave(kFlowerSound, false, kFlowerVolume);
            });
            rows_.AddTimed("花の間", layout_.flowerInterval);
        }
        rows_.AddTimed("帯の間", layout_.rowGap);
    }
    for ( int row = 0; row < Row_Count; ++row ) { AddRowSteps(static_cast<Row>( row )); }

    if ( data_.hanamaru ) {
        rows_.AddCall("花丸を描き始める", [audio](ResultScreen& screen){
            screen.hanamaruProgress_ = 0.0f;
            audio->PlayWave(screen.sounds_.hanamaru, false, kHanamaruVolume);
        });
        rows_.AddUntil("花丸を描く", [](ResultScreen& screen, float elapsed, float /*deltaTime*/){
            const float duration = screen.layout_.hanamaruTime;
            screen.hanamaruProgress_ = ( duration > 0.0f ) ? Easing::Clamp01(elapsed / duration) : 1.0f;
            return screen.hanamaruProgress_ >= 1.0f;
        });
    }
    rows_.AddCall("案内を出す", [](ResultScreen& screen){ screen.promptShown_ = 0.0f; });
}

void ResultScreen::Update(float deltaTime, bool decide){
    if ( phase_ == Phase::Closed ) { return; }
    time_ += deltaTime;
    openTime_ += deltaTime;

    // 出してからの秒数を進める（早送り中は速く）
    const float step = deltaTime * ( phase_ == Phase::Showing ? speed_ : 1.0f );
    auto advance = [step](float& shown){ if ( shown >= 0.0f ) { shown += step; } };
    advance(titleShown_);
    advance(promptShown_);
    for ( float& shown : rowShown_ ) { advance(shown); }
    for ( float& shown : stampShown_ ) { advance(shown); }
    for ( float& shown : flowerShown_ ) { advance(shown); }

    switch ( phase_ ) {
    case Phase::Showing:
        if ( decide ) { speed_ = kFastSpeed; } // 早送り
        rows_.Update(*this, step);
        if ( rows_.IsFinished() ) { phase_ = Phase::Waiting; }
        break;
    case Phase::Waiting:
        if ( decide ) { phase_ = Phase::Finished; }
        break;
    default:
        break;
    }
}

// --------------------------------------------------------------------- 描く
float ResultScreen::Pop(float shownTime) const{
    if ( shownTime < 0.0f ) { return 0.0f; }
    return ( layout_.itemIn > 0.0f ) ? Easing::EaseOutBack(Easing::Clamp01(shownTime / layout_.itemIn)) : 1.0f;
}

void ResultScreen::Place(Sprite* sprite, float x, float y, float width, float height, float alpha, float rotation){
    if ( !sprite ) { return; }
    sprite->SetPosition({ offsetX_ + x * scale_, y * scale_ });
    sprite->SetSize({ width * scale_, height * scale_ });
    sprite->SetRotation(rotation);
    sprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
    sprite->Update();
    sprite->Draw();
}

void ResultScreen::PlaceNatural(Sprite* sprite, const Vector2& size, float x, float y, float scale, float alpha){
    Place(sprite, x, y, size.x * scale, size.y * scale, alpha);
}

void ResultScreen::DrawNumberRight(const std::string& text, float right, float y, float height){
    const float pixelHeight = height * scale_;
    const float width = digits_.Width(text, pixelHeight);
    digits_.Draw(text, offsetX_ + right * scale_ - width, y * scale_, pixelHeight);
}

void ResultScreen::Draw(){
    if ( phase_ == Phase::Closed ) { return; }
    const float screenWidth = static_cast<float>( WindowProc::GetInstance()->GetClientWidth() );
    const float screenHeight = static_cast<float>( WindowProc::GetInstance()->GetClientHeight() );
    scale_ = UiScale::FromHeight(screenHeight);
    offsetX_ = ( screenWidth - UiScale::kBaseWidth * scale_ ) * 0.5f;
    digits_.BeginFrame();

    // 景色を暗い緑に沈める
    const float matIn = ( layout_.matIn > 0.0f ) ? Easing::Clamp01(openTime_ / layout_.matIn) : 1.0f;
    dim_->SetPosition({ 0.0f, 0.0f });
    dim_->SetSize({ screenWidth, screenHeight });
    dim_->SetColor({ layout_.dim.x, layout_.dim.y, layout_.dim.z, layout_.dim.w * matIn });
    dim_->Update();
    dim_->Draw();

    // マット：上からすべり込む（少し行き過ぎて戻る）
    const float matDrop = kMatDropFrom * ( 1.0f - Easing::EaseOutBack(matIn) );
    PlaceNatural(mat_.get(), matSize_, layout_.mat.x, layout_.mat.y + matDrop, 1.0f);

    // 「リザルト」とステージ名
    if ( titleShown_ >= 0.0f ) {
        const float pop = Pop(titleShown_);
        PlaceNatural(title_.get(), titleSize_, layout_.title.x, layout_.title.y, pop);
        if ( name_ && nameSize_.x > 0.0f ) {
            PlaceNatural(name_.get(), nameSize_, layout_.name.x, layout_.name.y, pop * layout_.nameWidth / nameSize_.x);
        }
    }

    // 帯：集めた花が 1 つずつ咲く。見つけた分だけ、小さな卵スタンプが付く
    const bool hasBand = data_.bandTotal > 0 && !layout_.rowYs.empty();
    if ( hasBand && rowShown_[0] >= 0.0f ) {
        const float y = layout_.rowYs[0];
        const float pop = Pop(rowShown_[0]);
        PlaceNatural(rowBoards_[0].get(), rowSize_, layout_.rowX, y, pop);
        const bool hiddenFlowers = ( data_.bandKind == "flowers" );
        Sprite* label = hiddenFlowers ? flowerLabel_.get() : labels_[0].get();
        const Vector2& labelSize = hiddenFlowers ? flowerLabelSize_ : labelSizes_[0];
        PlaceNatural(label, labelSize, layout_.labelX, y, pop);
        int lit = 0;
        for ( size_t i = 0; i < flowerShown_.size(); ++i ) {
            const float x = layout_.flowerX + layout_.flowerGap * static_cast<float>( i );
            Place(flowerEmpty_[i].get(), x, y, layout_.flowerSize * pop, layout_.flowerSize * pop);
            if ( flowerShown_[i] < 0.0f ) { continue; }
            ++lit;
            const float bloom = layout_.flowerSize * Pop(flowerShown_[i]);
            Sprite* flower = hiddenFlowers ? hiddenFlowers_[i].get() : comboFlowers_[i].get();
            Place(flower, x, y, bloom, bloom);
        }
        // 花を並べ切れない数の時は、見つけた数をそのまま出す
        if ( flowerShown_.size() < static_cast<size_t>( data_.bandTotal ) ) { lit = data_.bandFound; }
        DrawNumberRight(std::to_string(lit) + "/" + std::to_string(data_.bandTotal), layout_.valueRight, y, layout_.valueHeight);
    }

    // 3 行：絵・名前・数・星
    for ( int row = 0; row < Row_Count; ++row ) {
        const size_t slot = static_cast<size_t>( row ) + 1;
        if ( rowShown_[slot] < 0.0f || slot >= layout_.rowYs.size() ) { continue; }
        const float y = layout_.rowYs[slot];
        const float pop = Pop(rowShown_[slot]);
        PlaceNatural(rowBoards_[slot].get(), rowSize_, layout_.rowX, y, pop);
        Place(icons_[row].get(), layout_.iconX, y, layout_.iconSize * pop, layout_.iconSize * pop);
        PlaceNatural(labels_[slot].get(), labelSizes_[slot], layout_.labelX, y, pop);
        DrawNumberRight(RowText(static_cast<Row>( row )), layout_.valueRight, y, layout_.valueHeight * pop);
        // 空欄（点線の星）：星が出きったら描かない（星の下から点線がはみ出して、ずれて見えないように）
        const bool starDone = stampShown_[row] >= layout_.stampTime;
        if ( !starDone ) { Place(stampEmpty_[row].get(), layout_.stampX, y, layout_.stampSize * pop, layout_.stampSize * pop); }
        if ( stampShown_[row] >= 0.0f ) {
            // 今回取った星：くるっと回りながら、ぽんと大きくなって止まる
            const float spin = ( layout_.stampTime > 0.0f ) ? Easing::Clamp01(stampShown_[row] / layout_.stampTime) : 1.0f;
            const float size = layout_.stampSize * Pop(stampShown_[row]);
            Place(stamps_[row].get(), layout_.stampX, y, size, size, 1.0f, kStarSpin * ( 1.0f - Easing::EaseOutCubic(spin) ));
        } else if ( data_.hadBefore[row] ) {
            // 前のプレイで取った星（今回は取れなかった）：うすく出す（取ったまま残っている）
            Place(stamps_[row].get(), layout_.stampX, y, layout_.stampSize * pop, layout_.stampSize * pop, kStarHadAlpha);
        }
    }

    // 花丸：一筆ずつ描かれる（描き順のマスク）
    if ( hanamaruProgress_ >= 0.0f ) {
        hanamaru_->SetReveal(hanamaruProgress_);
        hanamaru_->SetPosition({ offsetX_ + layout_.hanamaru.x * scale_, layout_.hanamaru.y * scale_ });
        hanamaru_->SetSize({ layout_.hanamaruSize * scale_, layout_.hanamaruSize * scale_ });
        hanamaru_->SetColor(kCrayonRed);
        hanamaru_->Update();
        hanamaru_->Draw();
    }

    // 決定ボタンの案内（ふわふわ上下する）
    if ( promptShown_ >= 0.0f ) {
        const float pop = Pop(promptShown_);
        if ( phase_ != Phase::Finished ) {
            const float bob = std::sin(time_ * 2.0f * Easing::kPi * kPromptBobRate) * kPromptBob;
            PlaceNatural(prompt_.get(), promptSize_, layout_.prompt.x, layout_.prompt.y + bob, pop);
        }
    }
}
