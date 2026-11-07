#include "game/flip/FlipDial.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"

#include <cmath>

namespace {
    constexpr float kPi = 3.14159265f;
    // 部品のモデル（材質の色はモデルごとなので、色ごとに別の名前）
    constexpr const char* kPaper     = "flipDialPaper";     // 黄色い紙皿
    constexpr const char* kPaperCool = "flipDialPaperCool"; // 使えない間の紙皿（灰色）
    constexpr const char* kDot       = "flipDialDot";       // オレンジの点
    constexpr const char* kKnob      = "flipDialKnob";      // 緑のつまみ
    constexpr const char* kKnobCool  = "flipDialKnobCool";
    constexpr const char* kHand      = "flipDialHand";      // 青緑の針
    constexpr const char* kCotton    = "flipDialCotton";    // 白い綿
    constexpr const char* kArm       = "flipDialArm";       // ピンクのモール
    constexpr const char* kPost      = "flipDialPost";      // 段ボールの筒

    constexpr int kArmBeads = 9;    // モール1本の玉の数
    constexpr int kHandBeads = 4;   // 針の玉の数
    constexpr float kHandAngle = 0.75f; // 針の向き（真上から時計回り、rad）

    // 2次ベジェ（モールの曲がり）
    Vector3 Bezier(const Vector3& a, const Vector3& b, const Vector3& c, float t){
        const float u = 1.0f - t;
        return { u * u * a.x + 2.0f * u * t * b.x + t * t * c.x,
                 u * u * a.y + 2.0f * u * t * b.y + t * t * c.y,
                 u * u * a.z + 2.0f * u * t * b.z + t * t * c.z };
    }
}

FlipDial::FlipDial() = default;
FlipDial::~FlipDial() = default;

void FlipDial::LoadModels(){
    ModelManager* models = ModelManager::GetInstance();
    // 色は linear（画面では少し明るく出る）。紙・フェルトなので照り返しは出さない
    auto make = [&](const char* name, bool cylinder, const char* texture, const Vector4& color){
        if ( !models->FindModel(name) ) {
            if ( cylinder ) { models->CreateCylinderModel(name, 20, 1.0f, 1.0f); }
            else { models->CreateSphereModel(name, 16); }
        }
        if ( Model* model = models->FindModel(name) ) {
            model->SetTexture(texture);
            if ( Model::Material* material = model->GetMaterial() ) {
                material->color = color;
                material->enableLighting = 1;
                material->matte = 1.0f;
            }
        }
    };
    make(kPaper,     false, "resources/craft/textures/paper_yellow.png", { 1.0f, 0.92f, 0.45f, 1.0f });
    make(kPaperCool, false, "resources/craft/textures/paper_white.png",  { 0.45f, 0.44f, 0.46f, 1.0f });
    make(kDot,       false, "resources/craft/textures/paper_orange.png", { 1.0f, 0.55f, 0.2f, 1.0f });
    make(kKnob,      false, "resources/craft/textures/felt_leaf.png",    { 0.35f, 0.85f, 0.3f, 1.0f });
    make(kKnobCool,  false, "resources/craft/textures/paper_white.png",  { 0.35f, 0.35f, 0.36f, 1.0f });
    make(kHand,      false, "resources/craft/textures/paper_blue.png",   { 0.1f, 0.55f, 0.6f, 1.0f });
    make(kCotton,    false, "resources/craft/textures/paper_white.png",  { 1.0f, 1.0f, 1.0f, 1.0f });
    make(kArm,       false, "resources/craft/textures/paper_white.png",  { 1.0f, 0.38f, 0.58f, 1.0f });
    make(kPost,      true,  "resources/craft/textures/kraft.png",        { 0.85f, 0.7f, 0.5f, 1.0f });
}

FlipDial::Part& FlipDial::Add(const char* model, const Vector3& local, const Vector3& scale){
    Part part;
    part.obj = Obj3d::Create(model);
    part.local = local;
    part.scale = scale;
    parts_.push_back(std::move(part));
    return parts_.back();
}

void FlipDial::Initialize(){
    LoadModels();
    parts_.clear();
    hand_.clear();
    leftArm_.clear();
    rightArm_.clear();

    // 綿の雲：文字盤の後ろに、まわりへはみ出すようにもこもこ
    Add(kCotton, { 0.0f, 0.05f, 0.32f }, { 0.62f, 0.55f, 0.35f });
    for ( int i = 0; i < 9; ++i ) {
        const float a = ( float ) i / 9.0f * 2.0f * kPi + 0.3f;
        const float r = kRadius * ( 0.92f + 0.1f * std::sin(a * 3.0f) );
        const float size = 0.26f + 0.07f * std::cos(a * 2.0f) + ( std::sin(a) > 0.3f ? 0.06f : 0.0f ); // 上の方を大きく
        Add(kCotton, { std::cos(a) * r, std::sin(a) * r * 0.9f + 0.04f, 0.16f }, { size, size * 0.92f, size * 0.8f });
    }
    // 文字盤（紙皿）：平たい球
    disc_ = ( int ) parts_.size();
    Add(kPaper, { 0.0f, 0.0f, 0.0f }, { kRadius, kRadius, 0.07f });
    // ふちのオレンジの点
    for ( int i = 0; i < 5; ++i ) {
        const float a = kPi * 0.5f + ( float ) i / 5.0f * 2.0f * kPi + 0.35f;
        Add(kDot, { std::cos(a) * kRadius * 0.72f, std::sin(a) * kRadius * 0.72f, -0.06f }, { 0.065f, 0.065f, 0.03f });
    }
    // 針（根元から先へ細く）
    for ( int i = 0; i < kHandBeads; ++i ) {
        hand_.push_back(( int ) parts_.size());
        const float size = 0.1f - 0.016f * ( float ) i;
        Add(kHand, {}, { size, size, 0.035f });
    }
    // 真ん中のつまみ
    knob_ = ( int ) parts_.size();
    Add(kKnob, { 0.0f, 0.0f, -0.09f }, { 0.11f, 0.11f, 0.08f });
    // モールの腕（左右）：位置は Update で揺らしながら決める
    for ( int side = 0; side < 2; ++side ) {
        for ( int i = 0; i < kArmBeads; ++i ) {
            ( side == 0 ? leftArm_ : rightArm_ ).push_back(( int ) parts_.size());
            Add(kArm, {}, { 0.095f, 0.095f, 0.095f });
        }
    }
    // 段ボールの筒の柱
    post_ = ( int ) parts_.size();
    Add(kPost, {}, { 0.1f, 1.0f, 0.1f });
}

void FlipDial::Update(const Vector3& center, float groundY, float yaw, float spin, bool cooling, float time){
    if ( parts_.empty() ) { return; }
    ModelManager* models = ModelManager::GetInstance();
    // 当たった時：くるっと1回転・少しふくらむ・針が一回り・腕がぱたぱた
    const float turn = spin > 0.0f ? ( 1.0f - spin ) * 2.0f * kPi : 0.0f;
    const float pop = 1.0f + 0.22f * std::sin(spin * kPi);
    const float facing = yaw + turn;
    const float c = std::cos(facing), s = std::sin(facing);
    auto toWorld = [&](const Vector3& local){ // 行ベクトル×RotY と同じ回し方（+X→(c,0,-s) / +Z→(s,0,c)）
        const Vector3 p { local.x * pop, local.y * pop, local.z * pop };
        return Vector3 { center.x + p.x * c + p.z * s, center.y + p.y, center.z - p.x * s + p.z * c };
    };

    // 針：真上から時計回りに kHandAngle、当たると一回り
    const float handAngle = kHandAngle + ( spin > 0.0f ? ( 1.0f - spin ) * 2.0f * kPi : 0.0f );
    for ( int i = 0; i < ( int ) hand_.size(); ++i ) {
        const float r = 0.08f + 0.085f * ( float ) i;
        parts_[hand_[i]].local = { std::sin(handAngle) * r, std::cos(handAngle) * r, -0.075f };
    }
    // モール：雲のわきから外へ出て、先が下へたれる。ふだんはゆっくり、当たるとぱたぱた
    const float flap = 0.06f * std::sin(time * 2.0f) + 0.22f * std::sin(spin * kPi * 6.0f) * spin;
    for ( int side = 0; side < 2; ++side ) {
        const float sign = side == 0 ? -1.0f : 1.0f;
        const Vector3 a { sign * 0.62f, 0.02f, 0.12f };
        const Vector3 b { sign * 1.02f, 0.3f + flap, 0.14f };
        const Vector3 e { sign * 1.22f, -0.22f + flap * 1.6f, 0.1f };
        const auto& arm = side == 0 ? leftArm_ : rightArm_;
        for ( int i = 0; i < ( int ) arm.size(); ++i ) {
            parts_[arm[i]].local = Bezier(a, b, e, ( float ) i / ( float ) ( arm.size() - 1 ));
        }
    }

    for ( Part& part : parts_ ) {
        if ( !part.obj ) { continue; }
        part.obj->SetTranslation(toWorld(part.local));
        part.obj->SetScale({ part.scale.x * pop, part.scale.y * pop, part.scale.z * pop });
        part.obj->SetRotation({ 0.0f, facing, 0.0f });
    }
    // 使えない間は文字盤とつまみを灰色に
    parts_[disc_].obj->SetModel(models->FindModel(cooling ? kPaperCool : kPaper));
    parts_[knob_].obj->SetModel(models->FindModel(cooling ? kKnobCool : kKnob));
    // 柱：雲の下から道まで（回さない）
    const float top = center.y - 0.35f;
    const float length = ( std::max )( top - groundY, 0.05f );
    Part& post = parts_[post_];
    post.obj->SetTranslation({ center.x + 0.1f * s, groundY + length * 0.5f, center.z + 0.1f * c }); // 雲の少し奥
    post.obj->SetScale({ 0.1f, length, 0.1f });
    post.obj->SetRotation({ 0.0f, 0.0f, 0.0f });

    for ( Part& part : parts_ ) {
        if ( part.obj ) { part.obj->Update(); }
    }
}

void FlipDial::Draw(){
    for ( Part& part : parts_ ) {
        if ( part.obj ) { part.obj->Draw(); }
    }
}
