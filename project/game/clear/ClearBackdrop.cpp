#include "game/clear/ClearBackdrop.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/utils/Easing.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr const char* kKitModelDir = "resources/craft/models"; // クラフトのキット（StageBackdrop と同じ）
    constexpr float kRiseDepth = 18.0f;   // せり上がる前の深さ(m)
    constexpr float kRiseTime = 0.9f;     // せり上がる時間(秒)
    constexpr float kRiseStagger = 0.06f; // 1 つずつの遅れ(秒)
}

ClearBackdrop::ClearBackdrop() = default;
ClearBackdrop::~ClearBackdrop() = default;

void ClearBackdrop::Initialize(const std::vector<ClearBackdropItem>& items){
    ModelManager* modelManager = ModelManager::GetInstance();
    pieces_.clear();
    for ( const ClearBackdropItem& item : items ) {
        if ( !modelManager->FindModel(item.model) ) {
            modelManager->LoadModel(item.model, kKitModelDir, item.model + ".gltf", true); // 底面原点のまま
            if ( Model* model = modelManager->FindModel(item.model) ) {
                if ( model->GetMaterial() ) { model->GetMaterial()->matte = 1.0f; } // 紙・フェルトなので照り返しなし
            }
        }
        Piece piece;
        piece.object = Obj3d::Create(item.model);
        piece.item = item;
        if ( piece.object ) { pieces_.push_back(std::move(piece)); }
    }
}

void ClearBackdrop::Begin(const Vector3& origin, const Vector3& back){
    origin_ = origin;
    const float length = std::sqrt(back.x * back.x + back.z * back.z);
    back_ = ( length > 1e-4f ) ? Vector3 { back.x / length, 0.0f, back.z / length } : Vector3 { 1.0f, 0.0f, 0.0f };
    right_ = { back_.z, 0.0f, -back_.x };              // カメラから見て右
    faceYaw_ = std::atan2(-back_.x, -back_.z);           // 表（+Z）をカメラの方へ
    time_ = 0.0f;
    visible_ = true;
    Update(0.0f);
}

void ClearBackdrop::Hide(){
    visible_ = false;
}

void ClearBackdrop::Update(float deltaTime){
    if ( !visible_ ) { return; }
    time_ += deltaTime;
    for ( size_t i = 0; i < pieces_.size(); ++i ) {
        Piece& piece = pieces_[i];
        const ClearBackdropItem& item = piece.item;
        // 下からせり上がる（行き過ぎて戻る）。奥の物から順に
        const float progress = Easing::Clamp01(( time_ - kRiseStagger * static_cast<float>( i ) ) / kRiseTime);
        const float sink = kRiseDepth * ( 1.0f - Easing::EaseOutBack(progress) );
        const Vector3 position = origin_ + back_ * item.back + right_ * item.side + Vector3 { 0.0f, item.up - sink, 0.0f };
        piece.object->SetTranslation(position);
        piece.object->SetRotation({ 0.0f, faceYaw_ + item.turn, 0.0f });
        piece.object->SetScale(item.scale);
        piece.object->Update();
    }
}

void ClearBackdrop::Draw(){
    if ( !visible_ ) { return; }
    for ( Piece& piece : pieces_ ) { piece.object->Draw(); }
}
