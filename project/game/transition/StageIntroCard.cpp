#include "game/transition/StageIntroCard.h"

#include "engine/2d/Sprite.h"
#include "engine/graphics/TextureManager.h"
#include "engine/utils/Easing.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kImageWidth = 1920.0f;   // カードの座標の基準
    constexpr float kImageHeight = 1080.0f;
    constexpr const char* kImageDir = "resources/kamishibai/";
    constexpr const char* kContentDir = "resources/stage_intro/";
    constexpr float kEyeDistance = 1500.0f;  // 台紙が後ろへ倒れる時の奥行きの強さ（カードの px）
    constexpr float kBarHeight = 0.095f;     // 上下の帯の太さ（画面の高さ=1）
    // 巻いた布（make_kamishibai_assets.py の INTRO_ROLL_* / INTRO_BANNER_SIZE と同じ）
    constexpr float kRollPeriod = 120.0f;
    constexpr float kRollHeight = 278.0f;
    constexpr float kBannerClothWidth = 1240.0f; // 帯の布の幅（画像は左右に 30px の余白）
    constexpr float kBannerPad = 30.0f;
    constexpr float kRollDiameterStart = 112.0f; // 巻いた布の太さ：広がるほど細くなる
    constexpr float kRollDiameterEnd = 74.0f;
    const char* kEnemies[] = { "enemy_ground", "enemy_air", "enemy_plant" };

    // 色（JSON は見た目どおりの sRGB の値）→ 掛け算に使う値。描き先が sRGB なので、そのまま掛けると色が淡くなる
    float ToLinear(float value){ return std::pow(( std::max )( value, 0.0f ), 2.2f); }
    Vector3 ReadColor(const json& value, const Vector3& fallback){
        if ( !value.is_array() || value.size() < 3 ) { return { ToLinear(fallback.x), ToLinear(fallback.y), ToLinear(fallback.z) }; }
        return { ToLinear(value[0].get<float>()), ToLinear(value[1].get<float>()), ToLinear(value[2].get<float>()) };
    }
}

StageIntroCard::StageIntroCard() = default;
StageIntroCard::~StageIntroCard() = default;

// resources/kamishibai/intro_*.png を全部読んでおく（どのステージの名前も、どの敵も）
void StageIntroCard::Load(){
    if ( loaded_ ) { return; }
    TextureManager* textures = TextureManager::GetInstance();
    std::error_code error;
    for ( const auto& entry : std::filesystem::directory_iterator(kImageDir, error) ) {
        const std::string name = entry.path().filename().string();
        if ( name.rfind("intro_", 0) == 0 && entry.path().extension() == ".png" ) {
            textures_[name] = textures->Load(kImageDir + name).srvIndex;
        }
    }
    auto create = [&](const std::string& file, const Vector2& anchor) -> std::unique_ptr<Sprite> {
        const auto found = textures_.find(file);
        return ( found != textures_.end() ) ? Sprite::Create(found->second, {}, { 1.0f, 1.0f, 1.0f, 1.0f }, anchor) : nullptr;
    };
    roll_ = create("intro_roll.png", { 0.5f, 0.5f });
    rollShade_ = create("intro_roll_shade.png", { 0.5f, 0.5f });
    barTop_ = create("intro_bar.png", { 0.0f, 0.0f });
    barBottom_ = create("intro_bar.png", { 0.0f, 0.0f });
    if ( barBottom_ ) { barBottom_->SetIsFlipY(true); } // 下の帯は、内側のふちが上になるように
    for ( const char* enemy : kEnemies ) { enemyFiles_[enemy] = std::string("intro_char_") + enemy + ".png"; }
    loaded_ = true;
}

void StageIntroCard::Finalize(){
    elements_.clear();
    layoutKey_.clear();
    roll_.reset();
    rollShade_.reset();
    barTop_.reset();
    barBottom_.reset();
    loaded_ = false;
}

void StageIntroCard::AddLayer(Element& element, const std::string& file, bool tinted){
    const auto found = textures_.find(file);
    if ( found == textures_.end() ) { return; }
    Layer layer;
    layer.sprite = Sprite::Create(found->second, {}, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.5f, 0.5f });
    layer.width = layer.sprite->GetSize().x;
    layer.height = layer.sprite->GetSize().y;
    layer.tinted = tinted;
    element.layers.push_back(std::move(layer));
}

// 型：部品の置き場所。並べた順に描く（後ろの物から）
void StageIntroCard::Configure(const json& layout){
    if ( !loaded_ ) { return; }
    // 置き場所が前と同じなら作り直さない（ステージに入るたびに部品を作り直さない）
    const std::string key = layout.dump();
    if ( key == layoutKey_ && !elements_.empty() ) { return; }
    layoutKey_ = key;
    elements_.clear();
    for ( const json& entry : layout.value("elements", json::array()) ) {
        Element element;
        element.id = entry.value("id", std::string());
        element.baseX = entry.value("x", kImageWidth * 0.5f);
        element.baseY = entry.value("y", kImageHeight * 0.5f);
        element.baseScale = entry.value("scale", 1.0f);
        element.baseAngle = entry.value("angle", 0.0f) * kPi / 180.0f;
        element.swayAmount = entry.value("sway", 2.0f) * kPi / 180.0f;
        element.swaySpeed = entry.value("swaySpeed", 1.6f);
        element.swayPhase = entry.value("swayPhase", 0.0f);
        element.stamp = entry.value("stamp", false);
        for ( const json& layer : entry.value("layers", json::array()) ) {
            std::string file = layer.value("file", std::string());
            if ( file == "@enemy" ) { // 敵はステージごとに変わる：全部の敵の切り絵を持っておき、描く時に選ぶ
                for ( const char* enemy : kEnemies ) { AddLayer(element, enemyFiles_[enemy], false); }
                continue;
            }
            AddLayer(element, file, layer.value("tinted", false));
        }
        elements_.push_back(std::move(element));
    }
    if ( !stage_.empty() ) { SetContent(stage_); }
}

// 中身：ステージ名の画像・帯の色・星の色・出す敵
void StageIntroCard::SetContent(const std::string& stage){
    stage_ = stage;
    std::ifstream file(std::string(kContentDir) + stage + ".json");
    json content = json::object();
    if ( file.is_open() ) {
        try { file >> content; } catch ( ... ) { content = json::object(); }
    }
    const Vector3 bannerColor = ReadColor(content.value("bannerColor", json()), { 0.62f, 0.84f, 0.24f });
    const json starColors = content.value("starColors", json::array());
    enemy_ = content.value("enemy", std::string("enemy_ground"));
    for ( Element& element : elements_ ) {
        if ( element.id == "banner" ) { element.color = bannerColor; }
        if ( element.id.rfind("star", 0) == 0 ) {
            const size_t index = ( size_t ) std::atoi(element.id.c_str() + 4);
            if ( index < starColors.size() ) { element.color = ReadColor(starColors[index], { 1.0f, 1.0f, 1.0f }); }
        }
        if ( element.id == "name" && !element.layers.empty() ) { // ステージ名の画像を差し替える
            const auto found = textures_.find("intro_name_" + stage + ".png");
            if ( found != textures_.end() ) {
                element.layers[0].sprite->SetTexture(found->second);
                element.layers[0].width = element.layers[0].sprite->GetSize().x;
                element.layers[0].height = element.layers[0].sprite->GetSize().y;
            }
        }
    }
}

void StageIntroCard::Reset(){
    for ( Element& element : elements_ ) {
        element.visible = false;
        element.offsetX = element.offsetY = element.angle = 0.0f;
        element.scaleX = element.scaleY = 1.0f;
        element.reveal = 1.0f;
        element.arrived = false;
    }
    rollVisible_ = false;
    rollBounce_ = rollScroll_ = 0.0f;
    bars_ = 0.0f;
    fall_ = 0.0f;
    lift_ = 0.0f;
    cardAlpha_ = 1.0f;
    cardVisible_ = false;
}

StageIntroCard::Element* StageIntroCard::Find(const std::string& id){
    for ( Element& element : elements_ ) { if ( element.id == id ) { return &element; } }
    return nullptr;
}

// 部品の動き：「速く出て、少し行き過ぎて、戻る」が基本（紙を手で貼るような軽さ）
void StageIntroCard::Animate(const std::string& id, const std::string& motion, float progress, float elapsed){
    Element* element = Find(id);
    if ( !element ) { return; }
    const float p = Easing::Clamp01(progress);
    element->visible = true;
    cardVisible_ = true;
    const float height = element->layers.empty() ? 0.0f : element->layers[0].height * element->baseScale;
    if ( motion == "slideIn" ) {
        // 台紙：右からすべって来て、少し行き過ぎて止まる（左右の段ボールの切れ目が先頭）
        element->offsetX = 1.15f * kImageWidth * ( 1.0f - Easing::EaseOutBack(p) );
        element->angle = ( 1.0f - Easing::EaseOutCubic(p) ) * 0.12f;
    } else if ( motion == "pop" ) {
        // ポン：0から大きくなって、少し行き過ぎて戻る。少しひねりながら
        const float size = Easing::EaseOutBack(p);
        element->scaleX = element->scaleY = size;
        element->angle = -( 1.0f - Easing::EaseOutCubic(p) ) * 0.5f;
    } else if ( motion == "unroll" ) {
        // 巻いた布が左から右へ転がり、通った所に布が広がる。端で止まって小さく跳ねる
        const float spread = Easing::EaseOutQuad(Easing::Clamp01(p / 0.85f));
        element->reveal = ( std::max )( spread, 0.001f );
        const float left = element->baseX - kBannerClothWidth * 0.5f * element->baseScale;
        const float traveled = spread * kBannerClothWidth * element->baseScale;
        rollVisible_ = true;
        rollDiameter_ = kRollDiameterStart + ( kRollDiameterEnd - kRollDiameterStart ) * spread;
        rollX_ = left + traveled;
        // 転がった距離 ÷ 半径 ＝ 回った角度（滑らずに転がって見える）。模様は半周で1周期
        rollScroll_ = traveled / ( kPi * rollDiameter_ * 0.5f ) * kRollPeriod;
        rollBounce_ = ( p > 0.85f ) ? -std::sin(( p - 0.85f ) / 0.15f * kPi) * 16.0f : 0.0f;
    } else if ( motion == "jumpIn" ) {
        // 左から山なりにジャンプして来て、着地で少しつぶれる（足元はそのまま）
        const float u = Easing::Clamp01(p / 0.78f);
        element->offsetX = -560.0f * ( 1.0f - u );
        element->offsetY = -320.0f * 4.0f * u * ( 1.0f - u );
        element->angle = -0.3f * ( 1.0f - u );
        const float squash = ( p > 0.78f ) ? std::sin(( p - 0.78f ) / 0.22f * kPi) : 0.0f;
        element->scaleY = 1.0f - 0.22f * squash;
        element->scaleX = 1.0f + 0.14f * squash;
        element->offsetY += height * 0.5f * ( 1.0f - element->scaleY );
    } else if ( motion == "runIn" ) {
        // 右から走り込んで止まる（走っている間は上下に弾む）。止まった後は足踏み
        const float u = Easing::EaseOutCubic(p);
        element->offsetX = 700.0f * ( 1.0f - u );
        element->offsetY = -std::abs(std::sin(elapsed * 16.0f)) * 14.0f * ( 1.0f - u );
        element->angle = 0.08f * ( 1.0f - u );
        element->arrived = ( p >= 1.0f );
    } else if ( motion == "rollIn" ) {
        // 左から転がってきて止まる（転がった距離 ÷ 半径 ＝ 回った角度）
        const float distance = 620.0f * ( 1.0f - Easing::EaseOutCubic(p) );
        element->offsetX = -distance;
        element->angle = -distance / ( std::max )( height * 0.45f, 1.0f );
    }
}

void StageIntroCard::SetFall(float progress){
    const float p = Easing::Clamp01(progress);
    fall_ = kPi * 0.5f * Easing::EaseInQuad(Easing::Clamp01(p / 0.8f)); // 重さで加速しながら倒れる
    cardAlpha_ = ( p < 0.8f ) ? 1.0f : 1.0f - ( p - 0.8f ) / 0.2f;    // 倒れ切ったら消える
    if ( p >= 1.0f ) { cardVisible_ = false; }
}

// 上へ引き抜く：ゆっくり動き始めて、すっと抜ける
void StageIntroCard::SetLift(float progress){
    const float p = Easing::Clamp01(progress);
    lift_ = Easing::EaseInQuad(p) * ( kImageHeight + 80.0f ); // 影の分だけ余分に上げて、下の端まで画面の外へ
    if ( p >= 1.0f ) { cardVisible_ = false; }
}

void StageIntroCard::SetBars(float amount){ bars_ = Easing::Clamp01(amount); }
void StageIntroCard::Update(float time){ time_ = time; }

// 台紙は下の辺を軸に、奥へ倒れる。手前（画面）から見た遠近で、上の方ほど小さく下へ寄る
Vector2 StageIntroCard::Project(float x, float y, float width, float height) const{
    const float v = kImageHeight - y;                       // 下の辺からの高さ
    const float lifted = kImageHeight - v * std::cos(fall_); // 倒れた後の高さ（画面上の位置）
    const float depth = v * std::sin(fall_);                 // 奥へ行った距離
    const float k = kEyeDistance / ( kEyeDistance + depth );
    const float px = kImageWidth * 0.5f + ( x - kImageWidth * 0.5f ) * k;
    const float py = kImageHeight * 0.5f + ( lifted - kImageHeight * 0.5f ) * k - lift_;
    return { px * width / kImageWidth, py * height / kImageHeight };
}

void StageIntroCard::DrawElement(const Element& element, float width, float height, float time){
    float sway = element.swayAmount * std::sin(time * element.swaySpeed + element.swayPhase);
    float stampY = 0.0f;
    if ( element.stamp && element.arrived ) { stampY = -std::abs(std::sin(time * 9.0f)) * 7.0f; } // 足踏み
    const float angle = element.baseAngle + element.angle + sway;
    const float c = std::cos(angle), s = std::sin(angle);
    const float cx = element.baseX + element.offsetX;
    const float cy = element.baseY + element.offsetY + stampY;
    const float shade = 1.0f - 0.35f * std::sin(fall_); // 倒れるほど暗く
    for ( size_t i = 0; i < element.layers.size(); ++i ) {
        const Layer& layer = element.layers[i];
        if ( !layer.sprite ) { continue; }
        // 敵は、ステージの中身で選んだ1体だけ描く
        if ( element.layers.size() > 1 && layer.sprite && element.id == "enemy" ) {
            const size_t index = ( size_t ) ( std::find(std::begin(kEnemies), std::end(kEnemies), enemy_) - std::begin(kEnemies) );
            if ( i != ( index < std::size(kEnemies) ? index : 0 ) ) { continue; }
        }
        const float w = layer.width * element.baseScale * element.scaleX;
        const float h = layer.height * element.baseScale * element.scaleY;
        // 左から見せる（名前の帯）：画像も同じ割合だけ使う
        const float shown = element.reveal;
        layer.sprite->SetTextureRect(0.0f, 0.0f, layer.width * shown, layer.height);
        const float x0 = -w * 0.5f, x1 = -w * 0.5f + w * shown, y0 = -h * 0.5f, y1 = h * 0.5f;
        auto corner = [&](float lx, float ly){
            return Project(cx + lx * c - ly * s, cy + lx * s + ly * c, width, height);
        };
        layer.sprite->SetCorners(corner(x0, y0), corner(x1, y0), corner(x0, y1), corner(x1, y1));
        const Vector3 tint = layer.tinted ? element.color : Vector3 { 1.0f, 1.0f, 1.0f };
        layer.sprite->SetPosition({ 0.0f, 0.0f });
        layer.sprite->SetRotation(0.0f);
        layer.sprite->SetColor({ tint.x * shade, tint.y * shade, tint.z * shade, cardAlpha_ });
        layer.sprite->Update();
        layer.sprite->Draw();
    }
}

// 巻いた布：模様をずらして転がって見せ、上に筒の明暗を重ねる
void StageIntroCard::DrawRoll(float width, float height){
    const Element* banner = nullptr;
    for ( const Element& element : elements_ ) { if ( element.id == "banner" ) { banner = &element; } }
    if ( !rollVisible_ || !banner || !roll_ || !rollShade_ ) { return; }
    const float scale = banner->baseScale;
    const float w = rollDiameter_ * scale, h = kRollHeight * scale;
    const float cx = rollX_, cy = banner->baseY + rollBounce_;
    auto corner = [&](float lx, float ly){ return Project(cx + lx, cy + ly, width, height); };
    const Vector2 tl = corner(-w * 0.5f, -h * 0.5f), tr = corner(w * 0.5f, -h * 0.5f);
    const Vector2 bl = corner(-w * 0.5f, h * 0.5f), br = corner(w * 0.5f, h * 0.5f);
    const float shade = 1.0f - 0.35f * std::sin(fall_);
    roll_->SetTextureRect(std::fmod(rollScroll_, kRollPeriod), 0.0f, kRollPeriod, kRollHeight);
    roll_->SetCorners(tl, tr, bl, br);
    roll_->SetPosition({ 0.0f, 0.0f });
    roll_->SetColor({ banner->color.x * shade, banner->color.y * shade, banner->color.z * shade, cardAlpha_ });
    roll_->Update();
    roll_->Draw();
    rollShade_->SetCorners(tl, tr, bl, br);
    rollShade_->SetPosition({ 0.0f, 0.0f });
    rollShade_->SetColor({ shade, shade, shade, cardAlpha_ });
    rollShade_->Update();
    rollShade_->Draw();
}

void StageIntroCard::Draw(float width, float height){
    if ( cardVisible_ ) {
        for ( const Element& element : elements_ ) {
            if ( !element.visible ) { continue; }
            DrawElement(element, width, height, time_);
            if ( element.id == "banner" ) { DrawRoll(width, height); } // 巻いた布は帯のすぐ上
        }
    }
    // 上下の帯（プレイヤーの演出の間だけ。台紙が倒れる時に入り、カメラが戻る時に引っ込む）
    if ( bars_ > 0.001f && barTop_ && barBottom_ ) {
        const float barHeight = height * kBarHeight;
        const float shown = barHeight * Easing::EaseOutCubic(bars_);
        barTop_->SetPosition({ 0.0f, shown - barHeight });
        barTop_->SetSize({ width, barHeight });
        barTop_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        barTop_->Update();
        barTop_->Draw();
        barBottom_->SetPosition({ 0.0f, height - shown });
        barBottom_->SetSize({ width, barHeight });
        barBottom_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        barBottom_->Update();
        barBottom_->Draw();
    }
}
