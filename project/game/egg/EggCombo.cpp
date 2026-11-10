#include "game/egg/EggCombo.h"

#include "game/egg/Egg.h"
#include "game/egg/EggSystem.h"
#include "game/stage/CoinSystem.h"
#include "game/stage/StageBackdrop.h"
#include "game/stage/StageRecords.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/audio/AudioManager.h"
#include "engine/graphics/DebugDraw.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
#include "engine/camera/Camera.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/VectorMath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

using namespace VectorMath;
using namespace MatrixMath;

namespace {
    constexpr int   kProxyCount    = 20;     // 代役の数（光る1つ＋飛んでいる物＋生え戻り）
    constexpr float kMaxReach      = 30.0f;  // プレイヤーからこれより遠い物は狙えない(m)
    constexpr float kMinDepth      = 0.8f;   // プレイヤーより、カメラから見てこれだけ奥の物だけ（奥へ投げる）
    constexpr float kAssistPixels  = 40.0f;  // カーソルが少し外れていても拾う（画面の画素）
    constexpr float kEggSpeed      = 19.0f;  // 奥へ投げる卵の速さ(m/s)
    constexpr float kChainReach    = 9.0f;   // はじけた物が次に当たれる距離(m)
    constexpr int   kMaxCombo      = 8;
    constexpr float kGravity       = 16.0f;  // 飛ぶ物の重力
    constexpr float kDissolveTime  = 0.8f;   // 溶けて消えるまで
    constexpr float kRegrowDelay   = 4.0f;   // 飛んでから生え戻るまで(秒)
    constexpr float kRegrowTime    = 0.35f;  // ぽんと生え戻る演出
    constexpr float kComboShowTime = 1.6f;   // 最後に当ててから札を出しておく秒
    constexpr float kConeDeg       = 60.0f;  // はじける向きから、次の物を探す角度（まずこの中）
    constexpr float kWideConeDeg   = 100.0f; // 上で見つからなければ、ここまで広げる（少し選ばれにくい）
    constexpr float kAimSpread     = 0.8f;   // 狙える所：物の真ん中から、大きさのこの割合まで
    constexpr float kPi            = 3.14159265f;

    uint32_t Hash(uint32_t x){ x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
    float Rand01(uint32_t& seed){ seed = Hash(seed + 0x9e3779b9u); return ( seed & 0xffffff ) / float(0xffffff); }
}

EggCombo::EggCombo() = default;
EggCombo::~EggCombo() = default;

EggCombo::Kind EggCombo::KindOf(const std::string& model, bool& outTarget){
    outTarget = true;
    if ( model == "combo_target" ) { return Kind::Bullseye; }
    if ( model.rfind("tree_", 0) == 0 ) { return Kind::Tree; }
    if ( model == "fence_sticks" ) { return Kind::Fence; }
    if ( model.rfind("paper_rock_", 0) == 0 ) { return Kind::Rock; }
    if ( model == "sign_arrow" ) { return Kind::Sign; }
    if ( model.rfind("flower_", 0) == 0 ) { return Kind::Flower; }
    outTarget = false;
    return Kind::Tree;
}

// 背景の中から的になる物を集める（木は葉の所を狙う。一部の木にかくれ花）
void EggCombo::BuildTargets(){
    targets_.clear();
    flowersTotal_ = 0;
    comboFlowerTotal_ = 0;
    if ( !backdrop_ ) { return; }
    const auto& items = backdrop_->Items();
    builtFor_ = items.size();
    for ( int i = 0; i < ( int ) items.size(); ++i ) {
        if ( items[i].removed ) { continue; }
        bool isTarget = false;
        const Kind kind = KindOf(items[i].model, isTarget);
        if ( !isTarget ) { continue; }
        Vector3 lo, hi;
        if ( !backdrop_->WorldBounds(i, lo, hi) ) { continue; }
        Target target;
        target.item = i;
        target.kind = kind;
        const Vector3 size = Subtract(hi, lo);
        const bool tree = kind == Kind::Tree;
        target.center = { ( lo.x + hi.x ) * 0.5f, lo.y + size.y * ( tree ? 0.62f : 0.5f ), ( lo.z + hi.z ) * 0.5f };
        target.mid = { ( lo.x + hi.x ) * 0.5f, ( lo.y + hi.y ) * 0.5f, ( lo.z + hi.z ) * 0.5f };
        target.halfHeight = size.y * 0.5f;
        target.radius = std::clamp(( std::max )( size.x, size.y ) * ( tree ? 0.32f : 0.45f ), 0.5f, 2.0f);
        // かくれ花：木の5本に1本くらい（置いた位置から決まる＝毎回同じ木。tools/hud/find_hidden_flowers.py でも同じ計算）
        const uint32_t h = Hash(( uint32_t ) ( std::lround(items[i].pos.x * 10.0f) * 73856093 ) ^ ( uint32_t ) ( std::lround(items[i].pos.z * 10.0f) * 19349663 ));
        target.flower = tree && ( h % 5u ) == 0u;
        if ( target.flower ) { ++flowersTotal_; }
        if ( kind == Kind::Bullseye ) { ++comboFlowerTotal_; }
        targets_.push_back(target);
    }
}

// ===== 代役 =====
void EggCombo::Prewarm(uint32_t noiseSrv){
    proxies_.clear();
    for ( int i = 0; i < kProxyCount; ++i ) {
        Proxy proxy;
        proxy.obj = Obj3d::Create("tree_cutout_00"); // モデルは使う時に差し替える
        if ( !proxy.obj ) { continue; }
        // パイプラインは既定のまま（Obj3d::Draw がタイル貼りのモデルを WrapUV に切り替える。
        //   CullNone にすると切り替わらず、葉のフェルトの模様が端の色だけになって見えなくなった）
        proxy.obj->SetNoiseTexture(noiseSrv);
        proxy.obj->SetMaterialOverride(true, 1.0f, 0.0f); // 自分用のマテリアルを先に作っておく
        proxy.obj->SetMaterialOverride(false);
        proxies_.push_back(std::move(proxy));
    }
}

int EggCombo::AcquireProxy(int target){
    if ( !backdrop_ || target < 0 || target >= ( int ) targets_.size() ) { return -1; }
    const BackdropItem& item = backdrop_->Items()[targets_[target].item];
    Model* model = ModelManager::GetInstance()->FindModel(item.model);
    if ( !model ) { return -1; }
    for ( int p = 0; p < ( int ) proxies_.size(); ++p ) {
        if ( proxies_[p].item >= 0 ) { continue; }
        proxies_[p].item = targets_[target].item;
        proxies_[p].obj->SetModel(model);
        proxies_[p].obj->SetMaterialOverride(false);
        proxies_[p].obj->SetDissolveThreshold(0.0f);
        backdrop_->SetComboHidden(targets_[target].item, true);
        PlaceProxy(p, targets_[target], targets_[target].mid, { item.rot.x * kPi / 180.0f, item.rot.y * kPi / 180.0f, item.rot.z * kPi / 180.0f }, 1.0f);
        return p;
    }
    return -1;
}

void EggCombo::ReleaseProxy(int proxy, bool unhideItem){
    if ( proxy < 0 || proxy >= ( int ) proxies_.size() ) { return; }
    if ( unhideItem && backdrop_ ) { backdrop_->SetComboHidden(proxies_[proxy].item, false); }
    proxies_[proxy].item = -1;
    proxies_[proxy].obj->SetMaterialOverride(false);
    proxies_[proxy].obj->SetDissolveThreshold(0.0f);
}

// 代役を置く：回る中心 center・向き rot（ラジアン）・元の大きさに掛ける倍率 scale
void EggCombo::PlaceProxy(int proxy, const Target& target, const Vector3& center, const Vector3& rot, float scale){
    const BackdropItem& item = backdrop_->Items()[target.item];
    const Vector3 size = Multiply(scale, item.scale);
    // モデルの原点は底の真ん中。真ん中を軸に回るように、原点を「中心 − 回した（底→真ん中）」に置く
    const Matrix4x4 rotation = MakeAffine({ 1.0f, 1.0f, 1.0f }, rot, { 0.0f, 0.0f, 0.0f });
    const Vector3 up = Transforms({ 0.0f, target.halfHeight * scale, 0.0f }, rotation);
    Obj3d* obj = proxies_[proxy].obj.get();
    obj->SetTranslation(Subtract(center, up));
    obj->SetRotation(rot);
    obj->SetScale(size);
}

// ===== 光る =====
void EggCombo::ReleaseHighlight(){
    if ( highlightProxy_ >= 0 ) {
        const bool idle = highlightTarget_ >= 0 && highlightTarget_ < ( int ) targets_.size() && targets_[highlightTarget_].state == State::Idle;
        ReleaseProxy(highlightProxy_, idle);
    }
    highlightProxy_ = -1;
    highlightTarget_ = -1;
}

void EggCombo::SetAimHighlight(int target){
    if ( target >= 0 && ( target >= ( int ) targets_.size() || targets_[target].state != State::Idle ) ) { target = -1; }
    if ( target == highlightTarget_ ) { return; }
    ReleaseHighlight();
    if ( target < 0 ) { return; }
    highlightTarget_ = target;
    highlightProxy_ = AcquireProxy(target);
    highlightT_ = 0.0f;
    lockPop_ = 1.0f; // ロックした合図：一度だけぽんと大きく
}

// ===== 狙う =====
bool EggCombo::PickForAim(const Camera& camera, float cursorX, float cursorY, float screenWidth, float screenHeight,
                          const Vector3& playerPos, Pick& out){
    if ( !backdrop_ ) { return false; }
    if ( targets_.empty() || builtFor_ != backdrop_->Items().size() ) { BuildTargets(); }
    const Matrix4x4& viewProjection = camera.GetViewProjectionMatrix();
    const Vector3 eye = camera.GetWorldPosition();
    Vector3 forward = Subtract(playerPos, eye);
    forward.y = 0.0f;
    forward = Normalize(forward);
    auto project = [&](const Vector3& p, float& x, float& y) -> bool {
        Vector2 ndc;
        if ( !WorldToNdc(p, viewProjection, ndc) ) { return false; }
        x = ( ndc.x * 0.5f + 0.5f ) * screenWidth;
        y = ( 1.0f - ( ndc.y * 0.5f + 0.5f ) ) * screenHeight;
        return true;
    };
    // ゲームの物（反転スイッチなど）を狙っている時は、背景をロックしない（その物に普通に投げる）
    for ( const Blocker& blocker : blockers_ ) {
        float sx, sy, ex, ey;
        if ( !project(blocker.center, sx, sy) ) { continue; }
        const Vector3 right = Normalize(Cross({ 0.0f, 1.0f, 0.0f }, Subtract(blocker.center, eye)));
        float screenRadius = 40.0f;
        if ( project(Add(blocker.center, Multiply(blocker.radius, right)), ex, ey) ) { screenRadius = std::abs(ex - sx); }
        // スイッチは小さく、カーソルが少し外れていても「スイッチを狙っている」とみなす（後ろの木を拾わない）
        if ( std::sqrt(( sx - cursorX ) * ( sx - cursorX ) + ( sy - cursorY ) * ( sy - cursorY )) < screenRadius * 2.0f + 140.0f ) { return false; }
    }
    // 画面の上で、カーソルが的の円の中（＋少し）にある物のうち、カメラに一番近い物
    int best = -1;
    float bestDepth = 1e9f;
    for ( int k = 0; k < ( int ) targets_.size(); ++k ) {
        const Target& target = targets_[k];
        if ( target.state != State::Idle ) { continue; } // 飛んでいる・まだ生え戻っていない
        // 反転で隠している物は狙わない（光っている自分自身の代役は除く）
        if ( k != highlightTarget_ && backdrop_->IsRuntimeHidden(target.item) ) { continue; }
        const Vector3 toTarget = Subtract(target.center, playerPos);
        if ( Length(toTarget) > kMaxReach ) { continue; }
        if ( Dot({ toTarget.x, 0.0f, toTarget.z }, forward) < kMinDepth ) { continue; } // 奥の物だけ
        float sx, sy;
        if ( !project(target.center, sx, sy) ) { continue; }
        // 画面での半径：中心と、カメラの横へ radius ずらした点の差
        const Vector3 right = Normalize(Cross({ 0.0f, 1.0f, 0.0f }, Subtract(target.center, eye)));
        float ex, ey;
        float screenRadius = 30.0f;
        if ( project(Add(target.center, Multiply(target.radius, right)), ex, ey) ) { screenRadius = std::abs(ex - sx); }
        const float d = std::sqrt(( sx - cursorX ) * ( sx - cursorX ) + ( sy - cursorY ) * ( sy - cursorY ));
        if ( d > screenRadius + kAssistPixels ) { continue; }
        const float depth = Length(Subtract(target.center, eye)) + d * 0.01f;
        if ( depth < bestDepth ) {
            bestDepth = depth;
            best = k;
            out = { k, target.center, sx, sy, screenRadius };
        }
    }
    if ( best < 0 ) { return false; }

    // 当てる所：カーソルの光線が、物の真ん中に一番近づく所（真ん中から大きさの 8 割まで）
    const Target& target = targets_[best];
    const Matrix4x4 inverse = Inverse(viewProjection);
    const float ndcX = cursorX / screenWidth * 2.0f - 1.0f;
    const float ndcY = 1.0f - cursorY / screenHeight * 2.0f;
    const Vector3 nearPoint = NdcToWorld(ndcX, ndcY, 0.0f, inverse);
    const Vector3 rayDir = Normalize(Subtract(NdcToWorld(ndcX, ndcY, 1.0f, inverse), nearPoint));
    const Vector3 closest = Add(nearPoint, Multiply(Dot(Subtract(target.center, nearPoint), rayDir), rayDir));
    Vector3 offset = Subtract(closest, target.center);
    const float maxOffset = target.radius * kAimSpread;
    if ( Length(offset) > maxOffset ) { offset = Multiply(maxOffset / Length(offset), offset); }
    out.aimPoint = Add(target.center, offset);
    if ( !project(out.aimPoint, out.aimX, out.aimY) ) { out.aimX = out.screenX; out.aimY = out.screenY; }
    // はじける向き：口から当てる所へまっすぐ飛んだ卵が、物の球に最初に触れた所 → 真ん中
    const Vector3 mouth { playerPos.x, playerPos.y + 0.5f, playerPos.z };
    const Vector3 throwDir = Normalize(Subtract(out.aimPoint, mouth));
    Vector3 contact = out.aimPoint;
    EntryPoint(mouth, Add(out.aimPoint, Multiply(target.radius * 2.0f, throwDir)), target.center, target.radius, contact);
    out.bounceDir = BounceDir(target.center, contact, throwDir);
    return true;
}

Vector3 EggCombo::BounceDir(const Vector3& center, const Vector3& contact, const Vector3& fallback){
    Vector3 dir { center.x - contact.x, 0.0f, center.z - contact.z };
    if ( Length(dir) < 1e-3f ) { dir = { fallback.x, 0.0f, fallback.z }; }
    if ( Length(dir) < 1e-3f ) { return { 0.0f, 0.0f, 1.0f }; }
    return Normalize(dir);
}

bool EggCombo::EntryPoint(const Vector3& from, const Vector3& to, const Vector3& center, float radius, Vector3& out){
    const Vector3 d = Subtract(to, from);
    const Vector3 m = Subtract(from, center);
    const float a = Dot(d, d), b = 2.0f * Dot(m, d), c = Dot(m, m) - radius * radius;
    if ( c <= 0.0f ) { out = from; return true; } // もう中にいる
    if ( a < 1e-10f ) { return false; }
    const float disc = b * b - 4.0f * a * c;
    if ( disc < 0.0f ) { return false; }
    const float t = ( -b - std::sqrt(disc) ) / ( 2.0f * a );
    if ( t < 0.0f || t > 1.0f ) { return false; }
    out = Add(from, Multiply(t, d));
    return true;
}

void EggCombo::Launch(Egg* egg, int target, const Vector3& aimPoint){
    if ( !egg || target < 0 || target >= ( int ) targets_.size() ) { return; }
    Chain chain;
    chain.start = target; // どの的から始めたか（コンボの的なら、×8 でその的のコンボ花）
    chains_.push_back(chain);
    shots_.push_back({ egg, target, ( int ) chains_.size() - 1 });
    egg->Redirect(Normalize(Subtract(aimPoint, egg->GetPosition())), kEggSpeed, 3.0f);
}

// ===== はじける =====
float EggCombo::DepthOf(const Vector3& p) const{
    return Dot({ p.x - eye_.x, 0.0f, p.z - eye_.z }, forward_);
}

// ビリヤード：はじける向き outDir に一番合う物。向きのずれ（60°で 0.6）と距離（9m で 1.0）を足した点が小さい物。
//   ±60° に無ければ ±100° まで広げる（点に 1.0 足す＝選ばれにくい）。カメラの方へ 2.5m 以上戻る物は選ばない（画面をふさがない）
int EggCombo::PickNext(int from, const Vector3& outDir, const std::vector<int>& visited, bool live) const{
    const Vector3& fromPos = targets_[from].center;
    const float fromDepth = DepthOf(fromPos);
    for ( int pass = 0; pass < 2; ++pass ) {
        const float cone = ( pass == 0 ? kConeDeg : kWideConeDeg ) * kPi / 180.0f;
        int best = -1;
        float bestScore = 1e9f;
        for ( int k = 0; k < ( int ) targets_.size(); ++k ) {
            if ( k == from || std::find(visited.begin(), visited.end(), k) != visited.end() ) { continue; }
            const Target& target = targets_[k];
            if ( live ) {
                if ( target.state != State::Idle ) { continue; }
                if ( backdrop_->IsRuntimeHidden(target.item) && k != highlightTarget_ ) { continue; }
            }
            const Vector3 to = Subtract(target.center, fromPos);
            const float d = Length(to);
            if ( d < 1.0f || d > kChainReach ) { continue; } // すぐとなり（同じ物の別の部品など）は飛ばない
            if ( DepthOf(target.center) < fromDepth - 2.5f ) { continue; }
            Vector3 flat { to.x, 0.0f, to.z };
            if ( Length(flat) < 1e-3f ) { continue; }
            const float angle = std::acos(std::clamp(Dot(Normalize(flat), outDir), -1.0f, 1.0f));
            if ( angle > cone ) { continue; }
            float score = angle / ( kConeDeg * kPi / 180.0f ) * 0.6f + d / kChainReach + ( pass == 0 ? 0.0f : 1.0f );
            if ( live && !target.rewarded ) { score -= 0.1f; } // まだ当てていない物を少しだけ優先
            if ( score < bestScore ) { bestScore = score; best = k; }
        }
        if ( best >= 0 ) { return best; }
    }
    return -1;
}

// 背景エディタ：選んだ物から、真ん中・左のふち・右のふちに当てた時のはじける順（線と球）。数は小さい窓に出す
void EggCombo::DrawPreview(const std::vector<int>& items, const Camera& camera){
    if ( !backdrop_ || items.empty() ) { return; }
    BuildTargets(); // 編集中は物が動くので毎回作り直す（遊んでいない時だけ呼ばれる）
    eye_ = camera.GetWorldPosition();
    const Matrix4x4 rotation = MakeAffine({ 1.0f, 1.0f, 1.0f }, camera.GetRotation(), { 0.0f, 0.0f, 0.0f });
    Vector3 forward = Transforms({ 0.0f, 0.0f, 1.0f }, rotation);
    forward.y = 0.0f;
    forward_ = Length(forward) > 1e-3f ? Normalize(forward) : Vector3 { 0.0f, 0.0f, 1.0f };
    const Vector3 right { forward_.z, 0.0f, -forward_.x };
    DebugDraw* draw = DebugDraw::GetInstance();
    const Vector4 colors[3] = { { 0.3f, 0.7f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.55f, 0.2f, 1.0f } };
    const char* names[3] = { "左のふち", "真ん中", "右のふち" };
    std::string summary;
    for ( int item : items ) {
        int start = -1;
        for ( int k = 0; k < ( int ) targets_.size(); ++k ) { if ( targets_[k].item == item ) { start = k; break; } }
        if ( start < 0 ) { continue; }
        const Target& first = targets_[start];
        summary.clear();
        for ( int variant = 0; variant < 3; ++variant ) {
            // 卵は手前（カメラの向き）から来る。左のふちに当てると右へ、右のふちに当てると左へはじける
            const float side = ( variant - 1 ) * first.radius * kAimSpread;
            const float along = std::sqrt(( std::max )( first.radius * first.radius - side * side, 0.0f ));
            const Vector3 contact = Add(Subtract(first.center, Multiply(along, forward_)), Multiply(side, right));
            Vector3 outDir = BounceDir(first.center, contact, forward_);
            std::vector<int> visited { start };
            int current = start;
            const Vector3 lift { 0.0f, 0.12f * ( variant - 1 ), 0.0f }; // 3つの線が重ならないように少し上下にずらす
            draw->Sphere(Add(contact, lift), 0.18f, colors[variant], 8);
            while ( ( int ) visited.size() < kMaxCombo ) {
                const int next = PickNext(current, outDir, visited, false);
                if ( next < 0 ) { break; }
                draw->Line(Add(targets_[current].center, lift), Add(targets_[next].center, lift), colors[variant]);
                draw->Sphere(Add(targets_[next].center, lift), 0.25f, colors[variant], 8);
                outDir = BounceDir(targets_[next].center, targets_[current].center, outDir); // 飛んできた向きのまま
                visited.push_back(next);
                current = next;
            }
            // 次が無い時は、はじけた向きに短い線
            draw->Line(Add(targets_[current].center, lift), Add(Add(targets_[current].center, lift), Multiply(1.5f, outDir)), colors[variant]);
            summary += std::string(names[variant]) + " x" + std::to_string(visited.size()) + "  ";
        }
    }
#ifdef USE_IMGUI
    if ( !summary.empty() ) {
        ImGui::SetNextWindowBgAlpha(0.8f);
        if ( ImGui::Begin("コンボの道 (Combo Preview)", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing) ) {
            ImGui::TextUnformatted("選んだ物に卵を当てた時のコンボ数（カメラの向きから投げた時）");
            ImGui::TextColored({ 0.3f, 0.7f, 1.0f, 1.0f }, "青＝左のふち"); ImGui::SameLine();
            ImGui::TextUnformatted("白＝真ん中"); ImGui::SameLine();
            ImGui::TextColored({ 1.0f, 0.55f, 0.2f, 1.0f }, "橙＝右のふち");
            ImGui::TextUnformatted(summary.c_str());
        }
        ImGui::End();
    }
#endif
}

// 種類ごとの粒（木＝葉っぱ / 柵・看板＝木くず / 岩＝紙のかけら / 花＝花びら）
void EggCombo::SpawnChips(const Target& target, const Vector3& at, int count){
    Vector4 a { 0.35f, 0.75f, 0.30f, 1.0f }, b { 0.22f, 0.58f, 0.22f, 1.0f };
    switch ( target.kind ) {
    case Kind::Fence: case Kind::Sign: a = { 0.78f, 0.56f, 0.32f, 1.0f }; b = { 0.58f, 0.40f, 0.22f, 1.0f }; break;
    case Kind::Rock:   a = { 0.88f, 0.86f, 0.80f, 1.0f }; b = { 0.65f, 0.63f, 0.60f, 1.0f }; break;
    case Kind::Flower: a = { 1.00f, 0.55f, 0.72f, 1.0f }; b = { 1.00f, 0.75f, 0.30f, 1.0f }; break;
    default: break;
    }
    uint32_t seed = ( uint32_t ) target.item * 7919u + ( uint32_t ) count;
    for ( int i = 0; i < count; ++i ) {
        const float angle = ( float ) i / count * 2.0f * kPi + Rand01(seed) * 0.5f;
        const float speed = 1.8f + Rand01(seed) * 2.2f;
        eggs_->SpawnPuff(at, { std::cos(angle) * speed, 2.0f + Rand01(seed) * 3.0f, std::sin(angle) * speed },
                         ( i % 2 ) ? a : b, 0.18f + Rand01(seed) * 0.12f, 0.6f + Rand01(seed) * 0.3f);
    }
}

// 物がはじける：根元から外れて、次の物へ（無ければ奥と上へ）回りながら飛ぶ
void EggCombo::Burst(int targetIndex, int chainIndex, const Vector3& at, const Vector3& outDir){
    Target& target = targets_[targetIndex];
    if ( target.state != State::Idle ) { return; }
    Chain& chain = chains_[chainIndex];
    chain.count++;
    chain.visited.push_back(targetIndex);
    shownCombo_ = chain.count;
    comboTimer_ = 0.0f;
    comboPop_ = 1.0f;

    // 代役：光っていた物ならその代役をそのまま使う
    int proxy = -1;
    if ( highlightTarget_ == targetIndex && highlightProxy_ >= 0 ) {
        proxy = highlightProxy_;
        highlightProxy_ = -1;
        highlightTarget_ = -1;
    } else {
        proxy = AcquireProxy(targetIndex);
    }
    if ( proxy >= 0 ) { proxies_[proxy].obj->SetMaterialOverride(true, 1.2f, 0.0f); } // 飛んでいる間は少し明るく（奥の丘の上でも見える）
    backdrop_->SetComboHidden(target.item, true);
    target.state = State::Flying;

    SpawnChips(target, at, 10 + chain.count * 2);
    // 初めて当てた物からはコイン（コンボの数だけ）。金の粒が飛び出す
    if ( !target.rewarded ) {
        target.rewarded = true;
        if ( coins_ ) { coins_->AddBonus(chain.count); }
        for ( int i = 0; i < 4 + chain.count * 2; ++i ) {
            const float a = ( float ) i * 2.4f;
            eggs_->SpawnPuff(at, { std::cos(a) * 1.4f, 3.2f + ( i % 3 ) * 0.6f, std::sin(a) * 1.4f },
                             { 1.0f, 0.85f, 0.2f, 1.0f }, 0.2f, 0.7f);
        }
        // かくれ花：はじけた瞬間に花が飛び出す
        if ( target.flower ) {
            flowersFound_++;
            flowerPop_ = 1.0f;
            for ( int i = 0; i < 3; ++i ) {
                eggs_->SpawnPuff(target.center, { ( i - 1 ) * 1.5f, 6.0f, 0.0f }, { 1.0f, 0.6f, 0.75f, 1.0f }, 3.2f, 1.2f, "flower_red");
            }
            for ( int i = 0; i < 14; ++i ) {
                const float a = ( float ) i / 14.0f * 2.0f * kPi;
                eggs_->SpawnPuff(target.center, { std::cos(a) * 3.0f, 2.0f + ( i % 2 ) * 1.5f, std::sin(a) * 3.0f },
                                 { 1.0f, 0.55f, 0.75f, 1.0f }, 0.32f, 0.9f);
            }
            AudioManager::GetInstance()->PlayWave("resources/kamishibai/se_pon.wav", false, 0.7f);
        }
    }
    if ( chain.count >= kMaxCombo && !chain.finished ) { chain.finished = true; Finale(chain, at); }
    AudioManager::GetInstance()->PlayWave("resources/se/eggHit.wav", false, ( std::min )( 0.35f + chain.count * 0.08f, 0.8f));
    AudioManager::GetInstance()->PlayWave("resources/kamishibai/se_snip.wav", false, 0.45f); // 紙がはじける音

    // 飛ぶ物
    Flyer flyer;
    flyer.proxy = proxy;
    flyer.target = targetIndex;
    flyer.chain = chainIndex;
    flyer.pos = flyer.prev = target.mid;
    const BackdropItem& item = backdrop_->Items()[target.item];
    flyer.rot = { item.rot.x * kPi / 180.0f, item.rot.y * kPi / 180.0f, item.rot.z * kPi / 180.0f };
    uint32_t seed = ( uint32_t ) targetIndex * 2654435761u + ( uint32_t ) chain.count;
    const float side = Rand01(seed) < 0.5f ? -1.0f : 1.0f;
    const int next = chain.count < kMaxCombo ? PickNext(targetIndex, outDir, chain.visited, true) : -1;
    if ( next >= 0 ) {
        // 次の物の真ん中を通る山なり（飛ぶ時間は距離から。重力のぶん上へ）
        const Vector3 d = Subtract(targets_[next].center, flyer.pos);
        const float time = std::clamp(Length(d) / 10.0f, 0.35f, 0.85f);
        flyer.vel = { d.x / time, d.y / time + 0.5f * kGravity * time, d.z / time };
        flyer.flightTime = time;
        flyer.aim = next;
    } else {
        // 向かう先が無い：はじけた向きと上へ飛びながら溶ける
        flyer.vel = Add(Multiply(3.5f, outDir), { 0.0f, 6.5f, 0.0f });
        flyer.aim = -1;
        flyer.dissolveAt = 0.35f;
    }
    // 種類ごとの飛び方（回り方）
    switch ( target.kind ) {
    case Kind::Tree:   flyer.spin = { 0.0f, 3.0f * side, 7.0f * side }; break;                 // くるくる回りながら山なり
    case Kind::Fence:  flyer.spin = { 0.0f, 1.5f * side, 9.0f * side }; break;                 // 横に倒れながら弾かれる
    case Kind::Rock:   flyer.spin = { 12.0f, 0.0f, 0.0f };                                     // 低く跳ねて転がる
                       if ( next < 0 ) { flyer.vel = Add(Multiply(5.0f, outDir), { 0.0f, 3.5f, 0.0f }); } break;
    case Kind::Sign:   flyer.spin = { 14.0f, 0.0f, 0.0f }; break;                               // たてに回って飛ぶ
    case Kind::Bullseye: flyer.spin = { 0.0f, 16.0f * side, 0.0f }; break;                        // 的：くるくる回って飛ぶ
    case Kind::Flower: flyer.spin = { 0.0f, 8.0f, 0.0f };                                     // ぽんと上に跳ねる
                       if ( next < 0 ) { flyer.vel = Add(Multiply(1.0f, outDir), { 0.0f, 8.0f, 0.0f }); } break;
    }
    flyers_.push_back(flyer);
}

void EggCombo::Update(const Vector3& playerPos, const Camera* camera, float dt){
    ( void ) playerPos;
    if ( !eggs_ || !backdrop_ ) { return; }
    if ( camera ) {
        eye_ = camera->GetWorldPosition();
        Vector3 forward = Subtract(playerPos, eye_);
        forward.y = 0.0f;
        if ( Length(forward) > 1e-3f ) { forward_ = Normalize(forward); }
    }

    // 投げた卵：的に当たったら、卵はそこで割れて、的がはじける
    for ( auto it = shots_.begin(); it != shots_.end(); ) {
        EggShot& shot = *it;
        if ( !eggs_->Contains(shot.egg) || !shot.egg->IsFlying() ) { it = shots_.erase(it); continue; }
        Vector3 at;
        const Target& target = targets_[shot.target];
        if ( target.state == State::Idle
            && EntryPoint(shot.egg->GetPrevPosition(), shot.egg->GetPosition(), target.center, target.radius, at) ) {
            const Vector3 eggDir = Subtract(shot.egg->GetPosition(), shot.egg->GetPrevPosition());
            const Vector3 outDir = BounceDir(target.center, at, eggDir);
            shot.egg->Break();
            Burst(shot.target, shot.chain, at, outDir);
            it = shots_.erase(it);
            continue;
        }
        ++it;
    }

    // 飛んでいる物
    for ( size_t i = 0; i < flyers_.size(); ) {
        Flyer& flyer = flyers_[i];
        flyer.t += dt;
        flyer.prev = flyer.pos;
        flyer.vel.y -= kGravity * dt;
        flyer.pos = Add(flyer.pos, Multiply(dt, flyer.vel));
        flyer.rot = Add(flyer.rot, Multiply(dt, flyer.spin));
        if ( flyer.aim >= 0 ) {
            Vector3 at;
            const Target& aim = targets_[flyer.aim];
            if ( aim.state == State::Idle && EntryPoint(flyer.prev, flyer.pos, aim.center, aim.radius + 0.3f, at) ) {
                const int aimIndex = flyer.aim;
                const int chainIndex = flyer.chain;
                const Vector3 outDir = BounceDir(aim.center, targets_[flyer.target].center, flyer.vel); // 前の物の真ん中 → この物の真ん中
                flyer.aim = -1;
                flyer.dissolveAt = flyer.t;
                flyer.vel = { -flyer.vel.x * 0.25f, 2.0f, -flyer.vel.z * 0.25f }; // ぶつかって少し跳ね返り、落ちながら溶ける
                Burst(aimIndex, chainIndex, at, outDir); // flyers_ に足すので、この後は flyers_[i] で触る
            } else if ( flyer.t > flyer.flightTime + 0.25f || aim.state != State::Idle ) {
                flyer.aim = -1;
                flyer.dissolveAt = flyer.t;
            }
        }
        Flyer& current = flyers_[i];
        const float melt = current.dissolveAt >= 0.0f ? ( current.t - current.dissolveAt ) / kDissolveTime : 0.0f;
        if ( current.proxy >= 0 ) {
            Obj3d* obj = proxies_[current.proxy].obj.get();
            obj->SetDissolveThreshold(melt > 0.0f ? 0.02f + 0.98f * ( std::min )( melt, 1.0f ) : 0.0f);
            PlaceProxy(current.proxy, targets_[current.target], current.pos, current.rot, 1.0f - 0.25f * std::clamp(melt, 0.0f, 1.0f));
        }
        if ( melt >= 1.0f || current.pos.y < -20.0f || current.t > 4.0f ) {
            // 溶け切った：元の場所は隠したまま、しばらくしたら生え戻る
            ReleaseProxy(current.proxy, false);
            Target& target = targets_[current.target];
            target.state = State::Gone;
            target.timer = kRegrowDelay;
            flyers_.erase(flyers_.begin() + i);
            continue;
        }
        ++i;
    }

    // 生え戻る：数秒たったら、元の場所にぽんと出る
    for ( int k = 0; k < ( int ) targets_.size(); ++k ) {
        Target& target = targets_[k];
        if ( target.state == State::Gone ) {
            target.timer -= dt;
            if ( target.timer <= 0.0f ) {
                target.proxy = AcquireProxy(k);
                if ( target.proxy < 0 ) { backdrop_->SetComboHidden(target.item, false); target.state = State::Idle; continue; }
                target.state = State::Regrowing;
                target.timer = 0.0f;
                SpawnChips(target, { target.mid.x, target.mid.y - target.halfHeight, target.mid.z }, 6);
            }
        } else if ( target.state == State::Regrowing ) {
            target.timer += dt;
            const float t = target.timer / kRegrowTime;
            if ( t >= 1.0f ) {
                ReleaseProxy(target.proxy, true);
                target.proxy = -1;
                target.state = State::Idle;
                continue;
            }
            // 0 → 1.15 → 1（ぽんと生える）。根元から伸びるように、回る中心を下げる
            const float grow = t < 0.7f ? 1.15f * std::sin(t / 0.7f * kPi * 0.5f) : 1.15f - 0.15f * ( t - 0.7f ) / 0.3f;
            const BackdropItem& item = backdrop_->Items()[target.item];
            const Vector3 base { target.mid.x, target.mid.y - target.halfHeight, target.mid.z };
            PlaceProxy(target.proxy, target, { base.x, base.y + target.halfHeight * grow, base.z },
                       { item.rot.x * kPi / 180.0f, item.rot.y * kPi / 180.0f, item.rot.z * kPi / 180.0f }, ( std::max )( grow, 0.01f ));
        }
    }

    // 光っている物：明るく、ゆっくり脈打って光る。ロックした瞬間は一度だけぽんと大きく
    if ( highlightProxy_ >= 0 && highlightTarget_ >= 0 ) {
        highlightT_ += dt;
        lockPop_ = ( std::max )( 0.0f, lockPop_ - dt / 0.22f );
        const float pulse = 0.5f + 0.5f * std::sin(highlightT_ * 5.0f);
        proxies_[highlightProxy_].obj->SetMaterialOverride(true, 1.45f + 0.15f * pulse, 5.0f + 2.0f * pulse);
        const Target& target = targets_[highlightTarget_];
        const BackdropItem& item = backdrop_->Items()[target.item];
        const float pop = 1.0f + 0.12f * std::sin(( 1.0f - lockPop_ ) * kPi) * ( lockPop_ > 0.0f ? 1.0f : 0.0f );
        PlaceProxy(highlightProxy_, target, target.mid, { item.rot.x * kPi / 180.0f, item.rot.y * kPi / 180.0f, item.rot.z * kPi / 180.0f }, pop);
    }

    if ( shownCombo_ > 0 ) {
        comboTimer_ += dt;
        if ( comboTimer_ > kComboShowTime && flyers_.empty() && shots_.empty() ) { shownCombo_ = 0; }
    }
    comboPop_ = ( std::max )( 0.0f, comboPop_ - dt / 0.25f );
    comboFlowerPop_ = ( std::max )( 0.0f, comboFlowerPop_ - dt / 0.7f );
    flowerPop_ = ( std::max )( 0.0f, flowerPop_ - dt / 0.5f );
}

void EggCombo::UpdateVisuals(){
    for ( Proxy& proxy : proxies_ ) {
        if ( proxy.item >= 0 ) { proxy.obj->Update(); }
    }
}

void EggCombo::Draw(){
    for ( Proxy& proxy : proxies_ ) {
        if ( proxy.item >= 0 ) { proxy.obj->Draw(); }
    }
}

int EggCombo::ComboFlowersFound() const{
    return StageRecords::GetInstance()->ComboFlowersFound(stage_);
}

// ×8：♡が 1 つ戻る。コンボの的から始めたなら、その的のコンボ花（初めての時だけ。金の粒と大きな花が上へ飛ぶ）
void EggCombo::Finale(const Chain& chain, const Vector3& at){
    if ( onMaxCombo_ ) { onMaxCombo_(); }
    for ( int i = 0; i < 18; ++i ) { // ×8 の合図：金の粒が輪に広がる
        const float a = ( float ) i / 18.0f * 2.0f * kPi;
        eggs_->SpawnPuff(at, { std::cos(a) * 4.0f, 3.0f + ( i % 3 ) * 1.2f, std::sin(a) * 4.0f }, { 1.0f, 0.85f, 0.25f, 1.0f }, 0.28f, 1.0f);
    }
    if ( chain.start < 0 || chain.start >= ( int ) targets_.size() || targets_[chain.start].kind != Kind::Bullseye ) { return; }
    const BackdropItem& bullseye = backdrop_->Items()[targets_[chain.start].item];
    const std::string spot = std::to_string(std::lround(bullseye.pos.x * 10.0f)) + "," + std::to_string(std::lround(bullseye.pos.z * 10.0f));
    if ( !StageRecords::GetInstance()->FindComboFlower(stage_, spot) ) { return; } // もう見つけた
    comboFlowerPop_ = 1.0f;
    eggs_->SpawnPuff(at, { 0.0f, 7.5f, 0.0f }, { 1.0f, 0.8f, 0.2f, 1.0f }, 4.5f, 1.6f, "flower_orange"); // 大きな花がくるくる上へ
    for ( int i = 0; i < 24; ++i ) {
        const float a = ( float ) i / 24.0f * 2.0f * kPi;
        eggs_->SpawnPuff(at, { std::cos(a) * 2.5f, 5.0f + ( i % 4 ) * 1.0f, std::sin(a) * 2.5f },
                         ( i % 2 ) ? Vector4 { 1.0f, 0.9f, 0.3f, 1.0f } : Vector4 { 1.0f, 0.6f, 0.75f, 1.0f }, 0.34f, 1.3f);
    }
    AudioManager::GetInstance()->PlayWave("resources/kamishibai/se_pon.wav", false, 0.9f);
}

float EggCombo::ComboAlpha() const{
    if ( shownCombo_ <= 0 ) { return 0.0f; }
    const float fade = kComboShowTime - 0.4f;
    return comboTimer_ < fade ? 1.0f : ( std::max )( 0.0f, 1.0f - ( comboTimer_ - fade ) / 0.4f );
}

void EggCombo::Reset(){
    for ( int p = 0; p < ( int ) proxies_.size(); ++p ) { ReleaseProxy(p, false); }
    if ( backdrop_ ) { backdrop_->ClearComboHidden(); }
    highlightProxy_ = -1;
    highlightTarget_ = -1;
    chains_.clear();
    shots_.clear();
    flyers_.clear();
    shownCombo_ = 0;
    comboTimer_ = 0.0f;
    comboPop_ = 0.0f;
    flowerPop_ = 0.0f;
    flowersFound_ = 0;
    BuildTargets();
}
