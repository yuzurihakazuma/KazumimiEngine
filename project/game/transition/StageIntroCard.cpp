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
    constexpr const char* kLayoutPath = "resources/kamishibai/intro_layout.json";
    constexpr const char* kContentDir = "resources/stage_intro/";
    constexpr float kEyeDistance = 1500.0f;  // 台紙が後ろへ倒れる時の奥行きの強さ（カードの px）
    constexpr float kBarHeight = 0.095f;     // 上下の帯の太さ（画面の高さ=1）
    constexpr float kSlideTilt = 0.12f;      // 台紙が入る時の傾き(rad)
    // 巻いた布（make_kamishibai_assets.py の INTRO_ROLL_* / INTRO_BANNER_SIZE と同じ）
    constexpr float kRollPeriod = 120.0f;
    constexpr float kRollHeight = 278.0f;
    constexpr float kBannerClothWidth = 1240.0f; // 帯の布の幅
    constexpr float kRollDiameterStart = 112.0f; // 巻いた布の太さ：広がるほど細くなる
    constexpr float kRollDiameterEnd = 74.0f;
    const char* kEnemies[] = { "enemy_ground", "enemy_air", "enemy_plant" };

    // 色（JSON は見た目どおりの sRGB の値）→ 掛け算に使う値。描き先が sRGB なので、そのまま掛けると色が淡くなる
    float ToLinear(float value){ return std::pow(( std::max )( value, 0.0f ), 2.2f); }
    bool ReadColor(const json& value, Vector3& out){
        if ( !value.is_array() || value.size() < 3 ) { return false; }
        out = { ToLinear(value[0].get<float>()), ToLinear(value[1].get<float>()), ToLinear(value[2].get<float>()) };
        return true;
    }
}

StageIntroCard::StageIntroCard() = default;
StageIntroCard::~StageIntroCard() = default;

// resources/kamishibai/intro_*.png を全部読み、置き場所（intro_layout.json）から部品を作る
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
    slit_ = create("intro_cut.png", { 0.0f, 0.0f });
    if ( slit_ ) { slitImageWidth_ = slit_->GetSize().x; }
    loaded_ = true;

    std::ifstream file(kLayoutPath);
    json layout = json::object();
    if ( file.is_open() ) {
        try { file >> layout; } catch ( ... ) { layout = json::object(); }
    }
    Configure(layout);
}

void StageIntroCard::Finalize(){
    elements_.clear();
    roll_.reset();
    rollShade_.reset();
    barTop_.reset();
    barBottom_.reset();
    slit_.reset();
    scissors_.reset();
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

// 置き場所：部品（並べた順に描く＝後ろの物から）と、切れ目・はさみの寸法
void StageIntroCard::Configure(const json& layout){
    elements_.clear();
    margin_ = layout.value("margin", 0.0f);
    const json cut = layout.value("cut", json::object());
    cutY_ = cut.value("y", kImageHeight);
    topHeight_ = cut.value("topHeight", kImageHeight);
    bottomTop_ = cut.value("bottomTop", kImageHeight);
    bottomHeight_ = cut.value("bottomHeight", 0.0f);
    slitTop_ = cut.value("slitTop", cutY_);
    slitHeight_ = cut.value("slitHeight", 0.0f);
    const json scissors = layout.value("scissors", json::object());
    scissorsTipX_ = scissors.value("tipX", 0.0f);
    scissorsTipY_ = scissors.value("tipY", 0.0f);
    scissorsWidth_ = scissors.value("width", 0.0f);
    scissorsHeight_ = scissors.value("height", 0.0f);
    const auto scissorsTexture = textures_.find(scissors.value("file", std::string()));
    if ( scissorsTexture != textures_.end() ) {
        scissors_ = Sprite::Create(scissorsTexture->second, {}, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f });
    }

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
        element.bob = entry.value("bob", 0.0f);
        element.blink = entry.value("blink", -1);
        element.stamp = entry.value("stamp", false);
        element.bottom = ( entry.value("part", std::string("top")) == "bottom" );
        ReadColor(entry.value("color", json()), element.defaultColor);
        element.color = element.defaultColor;
        for ( const json& layer : entry.value("layers", json::array()) ) {
            std::string file = layer.value("file", std::string());
            if ( file == "@enemy" ) { // 敵は中身で変わる：全部の敵の切り絵を持っておき、描く時に選ぶ
                for ( const char* enemy : kEnemies ) { AddLayer(element, std::string("intro_char_") + enemy + ".png", false); }
                continue;
            }
            AddLayer(element, file, layer.value("tinted", false));
        }
        elements_.push_back(std::move(element));
    }
}

// 中身：名前の画像・帯の色・星の色・出す敵（書いていない色は、置き場所の既定の色）
void StageIntroCard::SetContent(const std::string& content){
    std::ifstream file(std::string(kContentDir) + content + ".json");
    json data = json::object();
    if ( file.is_open() ) {
        try { file >> data; } catch ( ... ) { data = json::object(); }
    }
    const json starColors = data.value("starColors", json::array());
    enemy_ = data.value("enemy", std::string("enemy_ground"));
    for ( Element& element : elements_ ) {
        element.color = element.defaultColor;
        if ( element.id == "banner" ) { ReadColor(data.value("bannerColor", json()), element.color); }
        if ( element.id.rfind("star", 0) == 0 ) {
            const size_t index = ( size_t ) std::atoi(element.id.c_str() + 4);
            if ( index < starColors.size() ) { ReadColor(starColors[index], element.color); }
        }
        if ( element.id == "name" && !element.layers.empty() ) { // 名前の画像を差し替える（無ければ名前を出さない）
            const auto found = textures_.find("intro_name_" + content + ".png");
            element.disabled = ( found == textures_.end() );
            if ( !element.disabled ) {
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
    cutProgress_ = snipTime_ = 0.0f;
    scissorsX_ = -1.0f;
    topLift_ = topTurn_ = bottomDrop_ = bottomTurn_ = 0.0f;
    bars_ = 0.0f;
    fall_ = 0.0f;
    lift_ = 0.0f;
    cardAlpha_ = 1.0f;
    cardVisible_ = false;
}

// 部品の動き：「速く出て、少し行き過ぎて、戻る」が基本（紙を手で貼るような軽さ）
void StageIntroCard::Animate(const std::string& id, const std::string& motion, float progress, float elapsed){
    const float p = Easing::Clamp01(progress);
    for ( Element& target : elements_ ) {
        if ( target.id != id ) { continue; }
        Element* element = &target;
        element->visible = true;
        cardVisible_ = true;
        const float height = element->layers.empty() ? 0.0f : element->layers[0].height * element->baseScale;
        if ( motion == "slideIn" ) {
            // 台紙：右からすべって来て、少し行き過ぎて止まる（左右の段ボールの切れ目が先頭）
            element->offsetX = 1.15f * kImageWidth * ( 1.0f - Easing::EaseOutBack(p) );
            element->angle = ( 1.0f - Easing::EaseOutCubic(p) ) * kSlideTilt;
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
        // show：ただ出す（何も動かさない）
    }
}

// はさみ：下の折り目の線を左から右へ切り進む（切り終えても止まらず、そのまま右へ抜ける）。
//   切り終えたら、上の部分はゆっくり傾きながら上へ、下の帯は下へ落ちる
void StageIntroCard::SetCut(float elapsed, float cutTime, float flyTime, float lift, float drop){
    cutTime = ( std::max )( cutTime, 0.01f );
    flyTime = ( std::max )( flyTime, 0.01f );
    snipTime_ = elapsed;
    cutProgress_ = Easing::Clamp01(elapsed / cutTime);
    const float travel = elapsed / cutTime;
    scissorsX_ = ( travel < 1.8f ) ? travel : -1.0f;
    const float fly = Easing::Clamp01(( elapsed - cutTime ) / flyTime);
    topLift_ = lift * Easing::EaseInCubic(fly);
    topTurn_ = -0.10f * Easing::EaseInQuad(fly);
    bottomDrop_ = drop * Easing::EaseInQuad(fly);
    bottomTurn_ = 0.12f * Easing::EaseInQuad(fly);
    if ( fly >= 1.0f ) { cardVisible_ = false; }
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

// 切れ目で分かれた後の位置：上の部分・下の帯それぞれの中心のまわりに回って、上・下へずれる
Vector2 StageIntroCard::Split(float x, float y, bool bottom) const{
    const float pivotX = kImageWidth * 0.5f;
    const float pivotY = bottom ? bottomTop_ + bottomHeight_ * 0.5f : topHeight_ * 0.5f;
    const float turn = bottom ? bottomTurn_ : topTurn_;
    const float dx = x - pivotX, dy = y - pivotY;
    const float c = std::cos(turn), s = std::sin(turn);
    const float shift = ( bottom ? bottomDrop_ : -topLift_ ) * kImageHeight;
    return { pivotX + dx * c - dy * s, pivotY + dx * s + dy * c + shift };
}

// 台紙は下の辺を軸に、奥へ倒れる。手前（画面）から見た遠近で、上の方ほど小さく下へ寄る
Vector2 StageIntroCard::Project(float x, float y, bool bottom, float width, float height) const{
    const Vector2 split = Split(x, y, bottom);
    const float v = kImageHeight - split.y;                     // 下の辺からの高さ
    const float lifted = kImageHeight - v * std::cos(fall_);    // 倒れた後の高さ（画面上の位置）
    const float depth = v * std::sin(fall_);                    // 奥へ行った距離
    const float k = kEyeDistance / ( kEyeDistance + depth );
    const float px = kImageWidth * 0.5f + ( split.x - kImageWidth * 0.5f ) * k;
    const float py = kImageHeight * 0.5f + ( lifted - kImageHeight * 0.5f ) * k - lift_;
    return { px * width / kImageWidth, py * height / kImageHeight };
}

void StageIntroCard::DrawElement(const Element& element, float width, float height, float time){
    // 「…」：時間で1つずつ増えて、全部出たら消える
    if ( element.blink >= 0 && ( int ) ( time * 3.0f ) % 4 <= element.blink ) { return; }
    const float sway = element.swayAmount * std::sin(time * element.swaySpeed + element.swayPhase);
    float moveY = 0.0f;
    if ( element.stamp && element.arrived ) { moveY = -std::abs(std::sin(time * 9.0f)) * 7.0f; } // 足踏み
    if ( element.bob > 0.0f ) { moveY -= std::abs(std::sin(time * element.swaySpeed + element.swayPhase)) * element.bob; } // ぴょこぴょこ
    const float angle = element.baseAngle + element.angle + sway;
    const float c = std::cos(angle), s = std::sin(angle);
    const float cx = element.baseX + element.offsetX;
    const float cy = element.baseY + element.offsetY + moveY;
    const float shade = 1.0f - 0.35f * std::sin(fall_); // 倒れるほど暗く
    for ( size_t i = 0; i < element.layers.size(); ++i ) {
        const Layer& layer = element.layers[i];
        if ( !layer.sprite ) { continue; }
        // 敵は、中身で選んだ1体だけ描く
        if ( element.id == "enemy" && element.layers.size() > 1 ) {
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
            return Project(cx + lx * c - ly * s, cy + lx * s + ly * c, element.bottom, width, height);
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
    auto corner = [&](float lx, float ly){ return Project(cx + lx, cy + ly, banner->bottom, width, height); };
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

// 切れ目：はさみが進んだところまで開いた線（分かれ始めたら、その間は次のシーン）
void StageIntroCard::DrawSlit(float width, float height){
    const bool splitting = ( topLift_ > 0.0f || bottomDrop_ > 0.0f );
    if ( cutProgress_ <= 0.0f || splitting || !slit_ ) { return; }
    const float shown = ( std::min )( margin_ + cutProgress_ * kImageWidth, slitImageWidth_ );
    slit_->SetTextureRect(0.0f, 0.0f, shown, slitHeight_);
    const float x0 = -margin_, x1 = -margin_ + shown, y0 = slitTop_, y1 = slitTop_ + slitHeight_;
    slit_->SetCorners(Project(x0, y0, false, width, height), Project(x1, y0, false, width, height),
                      Project(x0, y1, false, width, height), Project(x1, y1, false, width, height));
    slit_->SetPosition({ 0.0f, 0.0f });
    slit_->SetColor({ 1.0f, 1.0f, 1.0f, cardAlpha_ });
    slit_->Update();
    slit_->Draw();
}

// はさみ：刃先が切れ目の先。チョキチョキ（刃の開け閉め）に合わせて小さく揺れる
void StageIntroCard::DrawScissors(float width, float height){
    if ( scissorsX_ < 0.0f || !scissors_ ) { return; }
    const float snip = std::sin(snipTime_ * 40.0f);
    const float tipX = scissorsX_ * kImageWidth, tipY = cutY_ + snip * 3.0f;
    const float angle = snip * 0.12f;
    const float c = std::cos(angle), s = std::sin(angle);
    auto corner = [&](float lx, float ly){
        const float rx = lx * c - ly * s, ry = lx * s + ly * c;
        return Project(tipX + rx, tipY + ry, false, width, height);
    };
    const float left = -scissorsTipX_, right = scissorsWidth_ - scissorsTipX_;
    const float top = -scissorsTipY_, bottom = scissorsHeight_ - scissorsTipY_;
    scissors_->SetCorners(corner(left, top), corner(right, top), corner(left, bottom), corner(right, bottom));
    scissors_->SetPosition({ 0.0f, 0.0f });
    scissors_->SetColor({ 1.0f, 1.0f, 1.0f, cardAlpha_ });
    scissors_->Update();
    scissors_->Draw();
}

void StageIntroCard::Draw(float width, float height){
    if ( cardVisible_ ) {
        // 台紙 → 切れ目 → その上の部品（切れ目にかかったシールは、切れ目をまたいで見える）→ はさみ
        for ( const Element& element : elements_ ) {
            if ( element.visible && element.id == "backing" ) { DrawElement(element, width, height, time_); }
        }
        DrawSlit(width, height);
        for ( const Element& element : elements_ ) {
            if ( !element.visible || element.disabled || element.id == "backing" ) { continue; }
            DrawElement(element, width, height, time_);
            if ( element.id == "banner" ) { DrawRoll(width, height); } // 巻いた布は帯のすぐ上
        }
        DrawScissors(width, height);
    }
    // 上下の帯（プレイヤーの演技の間だけ。カードが分かれる時に入り、カメラが戻る時に引っ込む）
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
