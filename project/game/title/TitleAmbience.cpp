#include "game/title/TitleAmbience.h"
#include "game/title/TitleLayout.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/utils/EditorManager.h"
#include "engine/utils/Level/LevelEditor.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
    constexpr float kPi = 3.14159265358979323846f;

    // --- 風 ---
    constexpr float kGrassSway  = 0.11f;  // 草がゆれる角度(rad)
    constexpr float kFlowerSway = 0.08f;
    constexpr float kTreeSway   = 0.02f;  // 木は重いので少しだけ
    constexpr float kCloudSwing = 0.035f; // 雲は糸の先でゆらゆら
    constexpr float kCloudDrift = 0.45f * TitleLayout::kScale; // 雲が左右に流れる幅(m)
    constexpr float kWindTravel = 0.22f;  // 風が横へ伝わる細かさ（大きいほど隣との時間差が大きい）

    // --- 恐竜への反応 ---
    constexpr float kPushRadius = 1.7f;   // この距離(m)より近い草・花が押される
    constexpr float kPushAngle  = 0.55f;  // 真横を通った時に倒れる角度(rad)
    constexpr float kSpring     = 140.0f; // 元へ戻るばねの強さ
    constexpr float kDamping    = 7.0f;   // 揺れの収まりやすさ（小さいほど長くぷるぷるする）

    // --- 紙の波 ---
    constexpr float kWaveWidth    = 46.0f * TitleLayout::kScale; // 1列の横幅（画面の外まで届く長さ）
    constexpr float kWaveStep     = 0.25f;  // 波の形を作る刻み(m)
    constexpr float kWaveLength   = 2.6f;   // 波1つの幅(m)
    constexpr float kWaveCrest    = 0.34f;  // 波の山の高さ(m)
    constexpr const char* kWaveTexture = "resources/craft/textures/paper_blue.png";

    bool StartsWith(const std::string& text, const char* prefix){
        return text.rfind(prefix, 0) == 0;
    }

    // 波1列の形：下はまっすぐ、上は山が並んだ形の板（カメラ側を向く）
    Model::ModelData MakeWaveShape(float bodyHeight){
        Model::ModelData data;
        const int segments = ( int ) ( kWaveWidth / kWaveStep );
        const float totalHeight = bodyHeight + kWaveCrest;
        for ( int i = 0; i <= segments; ++i ) {
            const float x = -kWaveWidth * 0.5f + kWaveStep * ( float ) i;
            // 山が丸く、谷がとがる形（紙を波型に切った時の形）
            const float top = bodyHeight + kWaveCrest * std::abs(std::sin(x * kPi / kWaveLength));
            Model::VertexData bottomVertex {};
            bottomVertex.position = { x, 0.0f, 0.0f, 1.0f };
            bottomVertex.texcoord = { x * 0.25f, 1.0f };
            bottomVertex.normal = { 0.0f, 0.0f, -1.0f };
            bottomVertex.influence.weights[0] = 1.0f;
            Model::VertexData topVertex = bottomVertex;
            topVertex.position.y = top;
            topVertex.texcoord.y = 1.0f - top / totalHeight;
            data.vertices.push_back(bottomVertex);
            data.vertices.push_back(topVertex);
        }
        for ( int i = 0; i < segments; ++i ) {
            const uint32_t base = ( uint32_t ) ( i * 2 );
            // 左下→左上→右下、右下→左上→右上（平面モデルと同じ向き＝カメラ側が表）
            data.indices.insert(data.indices.end(), { base, base + 1, base + 2, base + 2, base + 1, base + 3 });
        }
        data.material.textureFilePath = kWaveTexture;
        return data;
    }
}

TitleAmbience::TitleAmbience() = default;
TitleAmbience::~TitleAmbience() = default;

void TitleAmbience::Initialize(Camera* camera){
    time_ = 0.0f;
    movers_.clear();
    boundObjectCount_ = -1;
    boundMapVersion_ = -1;
    wasMoving_ = false;
    CreateWaves(camera);
}

void TitleAmbience::Finalize(){
    waves_.clear();
    movers_.clear();
}

// 紙の波：草原の段差の奥・丘の手前に3列。奥ほど濃い青で、隣の列と逆向きにすれ違う
void TitleAmbience::CreateWaves(Camera* camera){
    waves_.clear();
    struct RowSetting { float z; float baseY; float bodyHeight; Vector4 color; float slide; float speed; float phase; };
    const float scale = TitleLayout::kScale;
    const RowSetting settings[] = {
        // 奥から手前の順（手前の列が奥の列の下半分を隠す）
        { 6.9f * scale, 0.55f * scale, 0.75f, { 0.50f, 0.66f, 0.90f, 1.0f }, 1.3f, 0.55f, 0.0f },
        { 6.3f * scale, 0.40f * scale, 0.70f, { 0.72f, 0.86f, 1.00f, 1.0f }, 1.6f, 0.75f, 2.1f },
        { 5.7f * scale, 0.25f * scale, 0.65f, { 0.95f, 0.99f, 1.00f, 1.0f }, 1.9f, 0.95f, 4.0f },
    };
    for ( const RowSetting& setting : settings ) {
        WaveRow row;
        row.model = std::make_unique<Model>();
        row.model->InitializePrimitive(ModelManager::GetInstance()->GetModelCommon(), MakeWaveShape(setting.bodyHeight));
        if ( row.model->GetMaterial() ) {
            row.model->GetMaterial()->color = setting.color;
            row.model->GetMaterial()->matte = 1.0f; // 紙なので照り返しなし
        }
        row.object = std::make_unique<Obj3d>();
        row.object->Initialize(row.model.get());
        row.object->SetCamera(camera);
        row.basePosition = { 0.0f, setting.baseY, setting.z };
        row.slide = setting.slide;
        row.speed = setting.speed;
        row.phase = setting.phase;
        waves_.push_back(std::move(row));
    }
}

// マップの配置物の中から、草・花・木・雲を探す（type 名＝モデル名で見分ける）
void TitleAmbience::BindMapObjects(){
    movers_.clear();
    LevelEditor* levelEditor = EditorManager::GetInstance()->GetLevelEditor();
    if ( !levelEditor ) { return; }
    const auto& objects = levelEditor->GetObjects();
    for ( int i = 0; i < ( int ) objects.size(); ++i ) {
        const std::string& type = objects[i].type;
        Mover mover;
        mover.index = i;
        if ( StartsWith(type, "grass_tuft") )       { mover.kind = Kind::Grass; }
        else if ( StartsWith(type, "flower_") )     { mover.kind = Kind::Flower; }
        else if ( StartsWith(type, "tree_") )       { mover.kind = Kind::Tree; }
        else if ( StartsWith(type, "cloud_") )      { mover.kind = Kind::Cloud; }
        else { continue; }
        movers_.push_back(mover);
    }
    boundObjectCount_ = ( int ) objects.size();
    boundMapVersion_ = levelEditor->GetMapLoadVersion();
}

// 草・花・木・雲：置いた位置（マップの値）を基準に、表示だけをゆらす
void TitleAmbience::UpdateMapObjects(float deltaTime, const Vector3& dinoPosition, bool moving){
    LevelEditor* levelEditor = EditorManager::GetInstance()->GetLevelEditor();
    if ( !levelEditor ) { return; }
    const auto& objects = levelEditor->GetObjects();
    // 物が増減した・別のマップを読んだ時は探し直す
    if ( ( int ) objects.size() != boundObjectCount_ || levelEditor->GetMapLoadVersion() != boundMapVersion_ ) {
        BindMapObjects();
    }
    if ( !moving && !wasMoving_ ) { return; } // 止まっていて、もう元の位置に戻してある
    wasMoving_ = moving;

    for ( Mover& mover : movers_ ) {
        Obj3d* object = levelEditor->GetObject3d(mover.index);
        if ( !object ) { continue; }
        const LevelObjectData& placed = objects[mover.index];
        Vector3 position = placed.translation;
        Vector3 rotation = placed.rotation;
        if ( !moving ) {
            // 止めた：置いた位置へ戻す
            mover.bend = mover.bendSpeed = 0.0f;
            object->SetTranslation(position);
            object->SetRotation(rotation);
            continue;
        }

        // 風：左から右へ伝わる波。強くなったり弱くなったりする（そよ風の「むら」）
        const float travel = placed.translation.x * kWindTravel;
        const float gust = 0.55f + 0.45f * std::sin(time_ * 0.35f - travel * 0.4f);
        float sway = 0.0f;
        switch ( mover.kind ) {
        case Kind::Grass:
            sway = kGrassSway * gust * std::sin(time_ * 2.3f - travel + placed.translation.z);
            break;
        case Kind::Flower:
            sway = kFlowerSway * gust * std::sin(time_ * 1.7f - travel + placed.translation.z);
            break;
        case Kind::Tree:
            sway = kTreeSway * gust * std::sin(time_ * 0.9f - travel);
            break;
        case Kind::Cloud:
            // 雲は糸で吊られている：ゆっくり左右へ流れて、少し遅れて傾く
            position.x += kCloudDrift * std::sin(time_ * 0.22f + placed.translation.z);
            sway = kCloudSwing * std::sin(time_ * 0.22f + placed.translation.z + 1.2f);
            break;
        }
        sway *= windStrength_;

        // 恐竜への反応：草と花は、恐竜から離れる向きへ押されて倒れ、ばねで揺れながら戻る
        if ( mover.kind == Kind::Grass || mover.kind == Kind::Flower ) {
            const float dx = placed.translation.x - dinoPosition.x;
            const float dz = placed.translation.z - dinoPosition.z;
            const float distance = std::sqrt(dx * dx + dz * dz);
            float target = 0.0f;
            if ( distance < kPushRadius ) {
                // 画面で見て、恐竜の右にある物は右へ、左にある物は左へ倒れる（Z回転は右へ倒すと負）
                const float side = ( dx >= 0.0f ) ? -1.0f : 1.0f;
                target = side * kPushAngle * ( 1.0f - distance / kPushRadius );
            }
            mover.bendSpeed += ( ( target - mover.bend ) * kSpring - mover.bendSpeed * kDamping ) * deltaTime;
            mover.bend += mover.bendSpeed * deltaTime;
            sway += mover.bend;
        }

        rotation.z += sway;
        object->SetTranslation(position);
        object->SetRotation(rotation);
    }
}

void TitleAmbience::Update(float deltaTime, const Vector3& dinoPosition){
    time_ += deltaTime;

    // エディタを出している間は止める（物をつかんで動かす作業の邪魔をしない）
    const bool editing = EditorManager::GetInstance()->IsActive() && !moveWhileEditing_;
    UpdateMapObjects(deltaTime, dinoPosition, windEnabled_ && !editing);

    // 紙の波：列ごとに左右へ行ったり来たり。隣の列とは逆向きで、少しだけ上下にも動く
    for ( size_t i = 0; i < waves_.size(); ++i ) {
        WaveRow& row = waves_[i];
        if ( !row.object ) { continue; }
        const float direction = ( i % 2 == 0 ) ? 1.0f : -1.0f;
        Vector3 position = row.basePosition;
        position.x += direction * row.slide * std::sin(time_ * row.speed + row.phase);
        position.y += 0.06f * std::sin(time_ * row.speed * 2.0f + row.phase);
        row.object->SetTranslation(position);
        row.object->Update();
    }
}

void TitleAmbience::Draw(){
    if ( !wavesEnabled_ ) { return; }
    for ( WaveRow& row : waves_ ) {
        if ( row.object ) { row.object->Draw(); }
    }
}
