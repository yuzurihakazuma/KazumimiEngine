#include "game/ui/DigitStrip.h"

#include "engine/2d/Sprite.h"
#include "engine/graphics/TextureManager.h"

using namespace KazumimiEngine;

DigitStrip::DigitStrip() = default;
DigitStrip::~DigitStrip() = default;

void DigitStrip::Initialize(const std::string& texturePath, const std::string& characters, float cellWidth, float cellHeight,
                            int capacity){
    characters_ = characters;
    cellWidth_ = cellWidth;
    cellHeight_ = cellHeight;
    next_ = 0;
    sprites_.clear();
    const uint32_t texture = TextureManager::GetInstance()->Load(texturePath).srvIndex;
    for ( int i = 0; i < capacity; ++i ) {
        // 左はし・真ん中の高さを基準に置く
        sprites_.push_back(Sprite::Create(texture, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.5f }));
    }
}

void DigitStrip::Finalize(){
    sprites_.clear();
    next_ = 0;
}

float DigitStrip::Draw(const std::string& text, float x, float y, float height, float alpha){
    const float width = height * cellWidth_ / cellHeight_;
    const float step = width * kAdvance;
    for ( const char character : text ) {
        const size_t index = characters_.find(character);
        if ( index == std::string::npos ) { x += step * 0.5f; continue; }
        if ( next_ >= static_cast<int>( sprites_.size() ) ) { break; }
        Sprite* sprite = sprites_[next_++].get();
        sprite->SetTextureRect(static_cast<float>( index ) * cellWidth_, 0.0f, cellWidth_, cellHeight_);
        sprite->SetPosition({ x, y });
        sprite->SetSize({ width, height });
        sprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
        sprite->Update();
        sprite->Draw();
        x += step;
    }
    return x + ( width - step );
}

float DigitStrip::Width(const std::string& text, float height) const{
    const float width = height * cellWidth_ / cellHeight_;
    const float step = width * kAdvance;
    float total = 0.0f;
    for ( const char character : text ) {
        total += ( characters_.find(character) == std::string::npos ) ? step * 0.5f : step;
    }
    return text.empty() ? 0.0f : total + ( width - step );
}
