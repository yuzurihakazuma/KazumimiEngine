#include "game/worldmap/PaperFx.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/InstancedGroup.h"
#include "engine/math/Matrix4x4.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr uint32_t kCapacity = 192;      // 1色あたりの最大数
    constexpr float kGravity = -6.0f;        // 紙吹雪の重力(m/秒²)
    constexpr float kFallLimit = -0.9f;      // 紙は空気でゆっくり落ちる(m/秒)
    constexpr float kDrag = 2.2f;            // 横の空気の抵抗
    constexpr float kFlutter = 0.7f;         // ひらひら横に揺れる速さ(m/秒)
    constexpr float kShrinkTime = 0.35f;     // 消える前に小さくなる時間(秒)
    constexpr const char* kPaperTexture = "resources/block/white1x1.png";
    constexpr const char* kFeltTexture = "resources/road/cloth/cloth_felt_plain.png";
    constexpr float kFeltBrightness = 0.46f; // フェルトの画像の平均の明るさ（linear）

    // 色紙（sRGB）
    const Vector3 kPaperColors[] = {
        { 0.94f, 0.34f, 0.3f }, { 0.98f, 0.82f, 0.3f }, { 0.46f, 0.78f, 0.38f },
        { 0.42f, 0.66f, 0.94f }, { 0.97f, 0.62f, 0.76f }, { 0.98f, 0.6f, 0.26f },
    };
    const Vector3 kPuffColors[] = { { 0.98f, 0.97f, 0.94f }, { 0.82f, 0.66f, 0.46f } }; // 白い綿 / 段ボール色

    float Linear(float c, float brightness){ return std::pow(c, 2.2f) / brightness; }

    std::string MakeModel(const std::string& name, bool sphere, const Vector3& color){
        ModelManager* modelManager = ModelManager::GetInstance();
        if ( !modelManager->FindModel(name) ) {
            if ( sphere ) { modelManager->CreateSphereModel(name, 12); }
            else          { modelManager->CreateCubeModel(name, 1.0f); }
        }
        if ( Model* model = modelManager->FindModel(name) ) {
            const float brightness = sphere ? kFeltBrightness : 1.0f;
            model->SetTexture(sphere ? kFeltTexture : kPaperTexture);
            if ( model->GetMaterial() ) {
                model->GetMaterial()->color = { Linear(color.x, brightness), Linear(color.y, brightness), Linear(color.z, brightness), 1.0f };
                model->GetMaterial()->matte = 1.0f;
            }
        }
        return name;
    }
}

struct PaperFx::Group {
    std::unique_ptr<InstancedGroup> batch;
    std::vector<Obj3d::TransformationMatrix> matrices;
};

PaperFx::PaperFx() = default;
PaperFx::~PaperFx() = default;

void PaperFx::Initialize(uint32_t noiseSrv){
    groups_.clear();
    pieces_.clear();
    auto add = [&](const std::string& model){
        Group group;
        group.batch = std::make_unique<InstancedGroup>();
        group.batch->Initialize(model, kCapacity);
        group.batch->SetNoiseTexture(noiseSrv);
        groups_.push_back(std::move(group));
    };
    int index = 0;
    for ( const Vector3& color : kPaperColors ) { add(MakeModel("fx_paper_" + std::to_string(index++), false, color)); }
    confettiGroups_ = ( int ) groups_.size();
    index = 0;
    for ( const Vector3& color : kPuffColors ) { add(MakeModel("fx_puff_" + std::to_string(index++), true, color)); }
}

void PaperFx::Clear(){
    pieces_.clear();
    groups_.clear();
}

float PaperFx::Random(float lo, float hi){
    return std::uniform_real_distribution<float>(lo, hi)(random_);
}

void PaperFx::Confetti(const Vector3& at, int count, float speed, float size){
    if ( confettiGroups_ == 0 ) { return; }
    for ( int i = 0; i < count; ++i ) {
        Piece piece;
        piece.group = i % confettiGroups_;
        piece.confetti = true;
        const float angle = Random(0.0f, 2.0f * kPi);
        const float out = Random(0.25f, 0.6f) * speed;
        piece.position = { at.x + std::cos(angle) * size, at.y, at.z + std::sin(angle) * size };
        piece.velocity = { std::cos(angle) * out, Random(0.7f, 1.15f) * speed, std::sin(angle) * out };
        piece.rotation = { Random(0.0f, kPi), Random(0.0f, kPi), Random(0.0f, kPi) };
        piece.spin = { Random(-9.0f, 9.0f), Random(-4.0f, 4.0f), Random(-9.0f, 9.0f) };
        piece.size = size * Random(0.75f, 1.25f);
        piece.life = Random(1.3f, 2.0f);
        piece.phase = Random(0.0f, 2.0f * kPi);
        pieces_.push_back(piece);
    }
}

void PaperFx::Puff(const Vector3& at, const Vector3& velocity, float size, float life, PuffColor color){
    if ( groups_.empty() ) { return; }
    Piece piece;
    piece.group = confettiGroups_ + ( color == PuffColor::White ? 0 : 1 );
    piece.confetti = false;
    piece.position = at;
    piece.velocity = velocity;
    piece.rotation = { 0.0f, Random(0.0f, 2.0f * kPi), 0.0f };
    piece.size = size * Random(0.85f, 1.15f);
    piece.life = life * Random(0.85f, 1.15f);
    pieces_.push_back(piece);
}

void PaperFx::PuffRing(const Vector3& at, int count, float speed, float size, PuffColor color){
    for ( int i = 0; i < count; ++i ) {
        const float angle = 2.0f * kPi * ( ( float ) i + Random(-0.2f, 0.2f) ) / ( float ) count;
        Puff({ at.x + std::cos(angle) * size * 0.9f, at.y + size * 0.3f, at.z + std::sin(angle) * size * 0.9f },
             { std::cos(angle) * speed, speed * 0.35f, std::sin(angle) * speed }, size, 0.55f, color);
    }
}

void PaperFx::Update(float deltaTime, const Camera* camera){
    for ( Piece& piece : pieces_ ) {
        piece.age += deltaTime;
        if ( piece.confetti ) {
            // 舞い上がって、空気でゆっくりに。横にひらひら揺れながら、くるくる回って落ちる
            piece.velocity.y = ( std::max )( piece.velocity.y + kGravity * deltaTime, kFallLimit );
            const float drag = ( std::max )( 0.0f, 1.0f - kDrag * deltaTime );
            piece.velocity.x *= drag;
            piece.velocity.z *= drag;
            const float sway = std::sin(piece.age * 6.0f + piece.phase) * kFlutter;
            piece.position.x += ( piece.velocity.x + sway ) * deltaTime;
            piece.position.y += piece.velocity.y * deltaTime;
            piece.position.z += ( piece.velocity.z + sway * 0.5f ) * deltaTime;
            piece.rotation = { piece.rotation.x + piece.spin.x * deltaTime, piece.rotation.y + piece.spin.y * deltaTime,
                               piece.rotation.z + piece.spin.z * deltaTime };
        } else {
            // 綿のけむり：ゆっくりになりながら上がる
            const float drag = ( std::max )( 0.0f, 1.0f - 1.8f * deltaTime );
            piece.velocity = { piece.velocity.x * drag, piece.velocity.y * drag, piece.velocity.z * drag };
            piece.position = { piece.position.x + piece.velocity.x * deltaTime, piece.position.y + piece.velocity.y * deltaTime,
                               piece.position.z + piece.velocity.z * deltaTime };
        }
    }
    pieces_.erase(std::remove_if(pieces_.begin(), pieces_.end(), [](const Piece& piece){ return piece.age >= piece.life; }), pieces_.end());

    if ( !camera ) { return; }
    const Matrix4x4& viewProjection = camera->GetViewProjectionMatrix();
    for ( Group& group : groups_ ) { group.matrices.clear(); }
    for ( const Piece& piece : pieces_ ) {
        Group& group = groups_[piece.group];
        if ( group.matrices.size() >= kCapacity ) { continue; }
        Vector3 scale;
        if ( piece.confetti ) {
            const float fade = ( std::max )( 0.02f, ( std::min )( ( piece.life - piece.age ) / kShrinkTime, 1.0f ) );
            scale = { piece.size * fade, piece.size * 0.06f * fade, piece.size * 0.7f * fade }; // 薄い長方形の紙
        } else {
            // ぽわっとふくらんで、しぼんで消える（半径1の球なので半分）
            const float u = piece.age / piece.life;
            const float grow = u < 0.3f ? std::sin(u / 0.3f * kPi * 0.5f) : std::cos(( u - 0.3f ) / 0.7f * kPi * 0.5f);
            const float r = piece.size * 0.5f * ( 0.4f + 0.6f * grow ) * ( u < 0.3f ? 1.0f : grow );
            scale = { ( std::max )( r, 1e-3f ), ( std::max )( r * 0.9f, 1e-3f ), ( std::max )( r, 1e-3f ) };
        }
        Obj3d::TransformationMatrix matrix;
        matrix.World = MatrixMath::MakeAffine(scale, piece.rotation, piece.position);
        matrix.WorldInverseTranspose = MatrixMath::Transpose(MatrixMath::Inverse(matrix.World));
        matrix.WVP = MatrixMath::Multiply(matrix.World, viewProjection);
        group.matrices.push_back(matrix);
    }
    for ( Group& group : groups_ ) {
        if ( !group.matrices.empty() ) { group.batch->Update(group.matrices); }
    }
}

void PaperFx::Draw(const Camera* camera){
    for ( Group& group : groups_ ) {
        if ( !group.matrices.empty() ) { group.batch->Draw(camera); }
    }
}
