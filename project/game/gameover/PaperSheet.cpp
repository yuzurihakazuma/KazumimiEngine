#include "game/gameover/PaperSheet.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/3d/obj/Obj3dCommon.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
    constexpr int   kCells = 48;              // 格子の細かさ（片側）
    constexpr float kShrink = 0.42f;          // いちばん縮んだ時に、真ん中へ寄る割合
    constexpr float kShrinkJitter = 0.16f;    // 縮み方のむら（ふちがギザギザになる）
    constexpr float kShift = 0.07f;           // 縮む時の横ずれ（紙の幅に対して。折り重なる）
    constexpr float kCreaseDepth = 0.11f;     // 折れ目の深さ（紙の幅に対して）
    constexpr float kBulge = 0.05f;           // 全体のゆるいふくらみ（紙の幅に対して）
    constexpr float kBallCrease = 0.28f;      // 紙玉の凸凹（半径に対して）
    constexpr float kPi = 3.14159265358979323846f;
    constexpr uint32_t kSeed = 20261027u;     // くしゃくしゃと広げで同じシード（シワの位置が一致する）
    constexpr const char* kBackTexture = "resources/craft/textures/kraft.png";

    Vector3 Add(const Vector3& a, const Vector3& b){ return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    Vector3 Sub(const Vector3& a, const Vector3& b){ return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    Vector3 Mul(const Vector3& a, float s){ return { a.x * s, a.y * s, a.z * s }; }
    float Dot(const Vector3& a, const Vector3& b){ return a.x * b.x + a.y * b.y + a.z * b.z; }
    Vector3 Cross(const Vector3& a, const Vector3& b){ return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    Vector3 Normalize(const Vector3& v){
        const float length = std::sqrt(Dot(v, v));
        return length > 1e-6f ? Mul(v, 1.0f / length) : Vector3 { 0.0f, 0.0f, -1.0f };
    }
    float Smooth(float t){ t = std::clamp(t, 0.0f, 1.0f); return t * t * ( 3.0f - 2.0f * t ); }

    // 格子の値ノイズ（シード固定）
    float Hash(int x, int y, uint32_t salt){
        uint32_t h = ( uint32_t ) x * 374761393u + ( uint32_t ) y * 668265263u + ( kSeed + salt ) * 2246822519u;
        h = ( h ^ ( h >> 13 ) ) * 1274126177u;
        return ( float ) ( ( h ^ ( h >> 16 ) ) & 0xffffu ) / 65535.0f;
    }
    float ValueNoise(float x, float y, uint32_t salt){
        const int ix = ( int ) std::floor(x), iy = ( int ) std::floor(y);
        const float fx = Smooth(x - ( float ) ix), fy = Smooth(y - ( float ) iy);
        const float a = Hash(ix, iy, salt), b = Hash(ix + 1, iy, salt), c = Hash(ix, iy + 1, salt), d = Hash(ix + 1, iy + 1, salt);
        const float top = a + ( b - a ) * fx, bottom = c + ( d - c ) * fx;
        return top + ( bottom - top ) * fy;
    }
    // 尾根の立ったノイズ（折れ目が鋭い）。-1〜1（谷折り・山折り）
    float Ridged(float x, float y){
        float sum = 0.0f, amp = 0.6f, freq = 1.0f;
        for ( int octave = 0; octave < 3; ++octave ) {
            const float n = 1.0f - std::fabs(ValueNoise(x * freq, y * freq, 1u + octave) * 2.0f - 1.0f);
            sum += ( n * n - 0.45f ) * amp;
            freq *= 2.2f;
            amp *= 0.5f;
        }
        return std::clamp(sum * 1.6f, -1.0f, 1.0f);
    }
}

PaperSheet::PaperSheet() = default;
PaperSheet::~PaperSheet() = default;

void PaperSheet::Initialize(uint32_t frontSrv){
    const int count = ( kCells + 1 ) * ( kCells + 1 );
    crease_.resize(count);
    shift_.resize(count);
    for ( int row = 0; row <= kCells; ++row ) {
        for ( int column = 0; column <= kCells; ++column ) {
            const float u = ( float ) column / kCells, v = ( float ) row / kCells;
            const int index = row * ( kCells + 1 ) + column;
            crease_[index] = Ridged(u * 7.0f, v * 4.5f);
            shift_[index] = { ValueNoise(u * 3.0f, v * 2.0f, 11u) * 2.0f - 1.0f, ValueNoise(u * 3.0f + 5.0f, v * 2.0f, 12u) * 2.0f - 1.0f };
        }
    }
    grid_.resize(count);
    BuildLayers();
    SetFrontTexture(frontSrv);
    SetCoverTexture(frontSrv);
}

void PaperSheet::BuildLayers(){
    layers_.clear();
    struct Setup { const char* texture; float offset; bool reversed; bool lit; };
    const Setup setups[LayerCount] = {
        { "resources/block/white1x1.png", 0.0f, false, true },     // 表（写し取った絵・光を当てる）
        { "resources/block/white1x1.png", 0.003f, false, false },  // 重ねる絵（光なし）
        { kBackTexture, -0.004f, true, true },                     // 裏（段ボール）
    };
    const uint32_t capacity = kCells * kCells * 6;
    for ( const Setup& setup : setups ) {
        LayerData layer;
        layer.model = std::make_unique<Model>();
        layer.model->InitializeDynamic(ModelManager::GetInstance()->GetModelCommon(), capacity, capacity, setup.texture);
        if ( Model::Material* material = layer.model->GetMaterial() ) {
            material->color = { 1.0f, 1.0f, 1.0f, 1.0f };
            material->enableLighting = setup.lit ? 1 : 0;
            material->matte = 1.0f;
            material->environmentCoefficient = 0.0f;
        }
        layer.obj = std::make_unique<Obj3d>();
        layer.obj->Initialize(layer.model.get());
        layer.obj->SetCamera(Obj3dCommon::GetInstance()->GetDefaultCamera()); // Initialize ではカメラが付かない
        layer.offset = setup.offset;
        layer.reversed = setup.reversed;
        layers_.push_back(std::move(layer));
    }
}

void PaperSheet::Finalize(){
    layers_.clear();
    grid_.clear();
    crease_.clear();
    shift_.clear();
    visible_ = false;
}

void PaperSheet::SetFrontTexture(uint32_t srv){
    if ( !layers_.empty() ) { layers_[Front].model->SetTexture(srv); }
}

void PaperSheet::SetCoverTexture(uint32_t srv){
    if ( !layers_.empty() ) { layers_[Cover].model->SetTexture(srv); }
}

void PaperSheet::Place(const Vector3& bottom, const Vector3& right, const Vector3& up, const Vector3& normal, float width, float height){
    bottom_ = bottom;
    right_ = Normalize(right);
    up_ = Normalize(up);
    normal_ = Normalize(normal);
    width_ = width;
    height_ = height;
}

// 格子点 (column, row) の今の位置：真ん中へ縮む（むらあり・横ずれあり）＋ 折れ目の山と谷
Vector3 PaperSheet::Deform(int column, int row) const{
    const float u = ( float ) column / kCells, v = ( float ) row / kCells;
    const int index = row * ( kCells + 1 ) + column;
    const float c = crumple_;
    float x = ( u - 0.5f ) * width_, y = ( v - 0.5f ) * height_;
    if ( c > 0.0f ) {
        const Vector2 shift = shift_[index];
        const float squeeze = 1.0f - c * ( kShrink + kShrinkJitter * shift.x );
        x = x * squeeze + c * kShift * width_ * shift.x;
        y = y * squeeze + c * kShift * width_ * shift.y;
    }
    // 折れ目：山折り・谷折りが交互に立つ。真ん中ほど深く、ふくらむ
    const float edge = std::fabs(u - 0.5f) * 2.0f + std::fabs(v - 0.5f) * 2.0f;
    const float bulge = ( 1.0f - std::clamp(edge * 0.5f, 0.0f, 1.0f) ) * kBulge;
    const float depth = c * width_ * ( crease_[index] * kCreaseDepth + bulge );
    const Vector3 center = Add(bottom_, Mul(up_, height_ * 0.5f));
    const Vector3 flat = Add(center, Add(Add(Mul(right_, x), Mul(up_, y)), Mul(normal_, depth)));
    if ( ball_ <= 0.0f ) { return flat; }

    // 紙玉：表の真ん中が手前の極、ふちが裏側へ回り込む球（折れ目で半径を凸凹に）
    const float px = ( u - 0.5f ) * 2.0f, py = ( v - 0.5f ) * 2.0f;
    const float rho = std::clamp(std::sqrt(px * px + py * py) / 1.4142f, 0.0f, 1.0f);
    const float phi = rho * kPi * 0.94f;
    const float theta = std::atan2(py, px) + ballSpin_;
    const Vector3 around = Add(Mul(right_, std::cos(theta)), Mul(up_, std::sin(theta)));
    const Vector3 dir = Add(Mul(normal_, std::cos(phi)), Mul(around, std::sin(phi)));
    const float bump = 1.0f + kBallCrease * crease_[index];
    const Vector3 onBall = Add(center, Mul(dir, ballRadius_ * bump));
    // ふちから先に丸まる（真ん中は最後まで平ら＝アイリスのように外側から閉じる）
    const float k = Smooth(ball_ * 1.5f - 0.5f * ( 1.0f - rho ));
    return Add(flat, Mul(Sub(onBall, flat), k));
}

void PaperSheet::Update(){
    if ( layers_.empty() || ( !visible_ && !meshFilled_ ) ) { return; } // 見えていない間は計算しない（空にした後は何もしない）
    meshFilled_ = visible_;
    for ( int row = 0; row <= kCells; ++row ) {
        for ( int column = 0; column <= kCells; ++column ) { grid_[row * ( kCells + 1 ) + column] = Deform(column, row); }
    }

    std::vector<Model::VertexData> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(kCells * kCells * 6);
    indices.reserve(kCells * kCells * 6);
    for ( int index = 0; index < LayerCount; ++index ) {
        LayerData& layer = layers_[index];
        vertices.clear();
        indices.clear();
        const bool needed = visible_ && ( index != Cover || coverAlpha_ > 0.0f );
        if ( needed ) {
            for ( int row = 0; row < kCells; ++row ) {
                for ( int column = 0; column < kCells; ++column ) {
                    auto at = [&](int c, int r){ return grid_[r * ( kCells + 1 ) + c]; };
                    auto uv = [&](int c, int r){ return Vector2 { ( float ) c / kCells, 1.0f - ( float ) r / kCells }; };
                    const int tris[2][3][2] = { { { column, row }, { column, row + 1 }, { column + 1, row } },
                                                { { column + 1, row }, { column, row + 1 }, { column + 1, row + 1 } } };
                    for ( const auto& tri : tris ) {
                        const Vector3 a = at(tri[0][0], tri[0][1]), b = at(tri[1][0], tri[1][1]), c = at(tri[2][0], tri[2][1]);
                        Vector3 normal = Normalize(Cross(Sub(b, a), Sub(c, a))); // 面ごとの法線（カクカクした折れ目に）
                        const Vector3 lift = Mul(normal, layer.offset);
                        if ( layer.reversed ) { normal = Mul(normal, -1.0f); }
                        const int order[3] = { 0, layer.reversed ? 2 : 1, layer.reversed ? 1 : 2 };
                        for ( int k : order ) {
                            const Vector3 p = Add(at(tri[k][0], tri[k][1]), lift);
                            Model::VertexData vertex {};
                            vertex.position = { p.x, p.y, p.z, 1.0f };
                            vertex.texcoord = uv(tri[k][0], tri[k][1]);
                            vertex.normal = normal;
                            indices.push_back(( uint32_t ) vertices.size());
                            vertices.push_back(vertex);
                        }
                    }
                }
            }
        }
        layer.model->UpdateMesh(vertices, indices);
        if ( Model::Material* material = layer.model->GetMaterial() ) {
            material->color.w = index == Cover ? coverAlpha_ : 1.0f; // 重ねる絵は濃さで
        }
        layer.obj->SetTranslation({ 0.0f, 0.0f, 0.0f });
        layer.obj->Update();
    }
}

void PaperSheet::Draw(){
    if ( !visible_ ) { return; }
    for ( int index = 0; index < LayerCount; ++index ) {
        if ( index == Cover && coverAlpha_ <= 0.0f ) { continue; }
        layers_[index].obj->Draw();
    }
}

Vector3 PaperSheet::PointAt(float u, float v) const{
    const int column = std::clamp(( int ) std::round(u * kCells), 0, kCells);
    const int row = std::clamp(( int ) std::round(v * kCells), 0, kCells);
    return grid_.empty() ? bottom_ : grid_[row * ( kCells + 1 ) + column];
}

Vector3 PaperSheet::Center() const{
    return Add(bottom_, Mul(up_, height_ * 0.5f));
}
