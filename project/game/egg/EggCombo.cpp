#include "game/egg/EggCombo.h"

#include "game/egg/Egg.h"
#include "game/egg/EggSystem.h"
#include "game/stage/CoinSystem.h"
#include "game/stage/StageBackdrop.h"
#include "engine/audio/AudioManager.h"
#include "engine/camera/Camera.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/VectorMath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

using namespace VectorMath;
using namespace MatrixMath;

namespace {
    constexpr float kMaxReach      = 30.0f;  // プレイヤーからこれより遠い物は狙えない(m)
    constexpr float kMinDepth      = 0.8f;   // プレイヤーより、カメラから見てこれだけ奥の物だけ（奥へ投げる）
    constexpr float kAssistPixels  = 40.0f;  // カーソルが少し外れていても拾う（画面の画素）
    constexpr float kEggSpeed      = 19.0f;  // 奥へ投げる・跳ね返る卵の速さ(m/s)
    constexpr float kChainReach    = 9.0f;   // 跳ね返って次に当たれる距離(m)
    constexpr int   kMaxCombo      = 8;
    constexpr float kComboShowTime = 1.6f;   // 最後に当ててから札を出しておく秒
    constexpr float kWobbleTime    = 0.7f;
    constexpr float kPi            = 3.14159265f;

    uint32_t Hash(uint32_t x){ x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }

    // 線分 a→b が、中心 c・半径 r の球に触れるか（触れた点も返す）
    bool SegmentSphere(const Vector3& a, const Vector3& b, const Vector3& c, float r, Vector3& outAt){
        const Vector3 ab = Subtract(b, a);
        const float len2 = Dot(ab, ab);
        float t = len2 > 1e-8f ? Dot(Subtract(c, a), ab) / len2 : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        const Vector3 closest = Add(a, Multiply(t, ab));
        if ( Length(Subtract(c, closest)) > r ) { return false; }
        outAt = closest;
        return true;
    }
}

bool EggCombo::IsTargetModel(const std::string& model, bool& outTree){
    outTree = model.rfind("tree_", 0) == 0;
    return outTree || model == "fence_sticks" || model.rfind("paper_rock_", 0) == 0 || model == "sign_arrow"
        || model.rfind("flower_", 0) == 0;
}

// 背景の中から的になる物を集める（木は葉の所を狙う。一部の木にかくれ花）
void EggCombo::BuildTargets(){
    targets_.clear();
    flowersTotal_ = 0;
    if ( !backdrop_ ) { return; }
    const auto& items = backdrop_->Items();
    builtFor_ = items.size();
    for ( int i = 0; i < ( int ) items.size(); ++i ) {
        if ( items[i].removed ) { continue; }
        bool tree = false;
        if ( !IsTargetModel(items[i].model, tree) ) { continue; }
        Vector3 lo, hi;
        if ( !backdrop_->WorldBounds(i, lo, hi) ) { continue; }
        Target target;
        target.item = i;
        target.tree = tree;
        const Vector3 size = Subtract(hi, lo);
        target.center = { ( lo.x + hi.x ) * 0.5f, lo.y + size.y * ( tree ? 0.62f : 0.5f ), ( lo.z + hi.z ) * 0.5f };
        target.radius = std::clamp(( std::max )( size.x, size.y ) * ( tree ? 0.32f : 0.45f ), 0.5f, 2.0f);
        // かくれ花：木の5本に1本くらい（置いた位置から決まる＝毎回同じ木。tools/hud/find_hidden_flowers.py でも同じ計算）
        const uint32_t h = Hash(( uint32_t ) ( std::lround(items[i].pos.x * 10.0f) * 73856093 ) ^ ( uint32_t ) ( std::lround(items[i].pos.z * 10.0f) * 19349663 ));
        target.flower = tree && ( h % 5u ) == 0u;
        if ( target.flower ) { ++flowersTotal_; }
        targets_.push_back(target);
    }
}

void EggCombo::Reset(){
    RestoreWobbles();
    chains_.clear();
    shownCombo_ = 0;
    comboTimer_ = 0.0f;
    comboPop_ = 0.0f;
    flowerPop_ = 0.0f;
    flowersFound_ = 0;
    BuildTargets();
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
    return best >= 0;
}

void EggCombo::Launch(Egg* egg, int target){
    if ( !egg || target < 0 || target >= ( int ) targets_.size() ) { return; }
    Chain chain;
    chain.egg = egg;
    chain.target = target;
    const Vector3 dir = Normalize(Subtract(targets_[target].center, egg->GetPosition()));
    egg->Redirect(dir, kEggSpeed, 3.0f);
    chains_.push_back(chain);
}

// ===== 当たる・跳ね返る =====
int EggCombo::NextTarget(const Chain& chain, const Vector3& from) const{
    int best = -1;
    float bestDistance = kChainReach;
    for ( int k = 0; k < ( int ) targets_.size(); ++k ) {
        if ( std::find(chain.visited.begin(), chain.visited.end(), k) != chain.visited.end() ) { continue; }
        const float d = Length(Subtract(targets_[k].center, from));
        if ( d < 1.0f ) { continue; } // すぐとなり（同じ木の別の部品など）は飛ばない
        // まだ当てていない物を少し優先（同じ所ばかり往復しない）
        const float score = d - ( targets_[k].rewarded ? 0.0f : 1.5f );
        if ( score < bestDistance ) { bestDistance = score; best = k; }
    }
    return best;
}

void EggCombo::Hit(Chain& chain, const Vector3& at, const Vector3& playerPos){
    Target& target = targets_[chain.target];
    chain.count++;
    chain.visited.push_back(chain.target);
    shownCombo_ = chain.count;
    comboTimer_ = 0.0f;
    comboPop_ = 1.0f;
    StartWobble(target.item);

    // 散る物：木は葉っぱ（緑）、ほかは木くず（茶）。コンボが続くほど多く
    const Vector4 chip = target.tree ? Vector4 { 0.35f, 0.75f, 0.3f, 1.0f } : Vector4 { 0.75f, 0.55f, 0.3f, 1.0f };
    const int pieces = 8 + chain.count * 2;
    for ( int i = 0; i < pieces; ++i ) {
        const float a = ( float ) i / pieces * 2.0f * kPi;
        eggs_->SpawnPuff(at, { std::cos(a) * 2.6f, 1.5f + ( i % 3 ) * 0.9f, std::sin(a) * 2.6f }, chip, 0.26f, 0.7f);
    }
    // 初めて当てた物からはコイン（コンボの数だけ）。金の粒が飛び出す
    if ( !target.rewarded ) {
        target.rewarded = true;
        if ( coins_ ) { coins_->AddBonus(chain.count); }
        for ( int i = 0; i < 4 + chain.count * 2; ++i ) {
            const float a = ( float ) i * 2.4f;
            eggs_->SpawnPuff(at, { std::cos(a) * 1.4f, 3.2f + ( i % 3 ) * 0.6f, std::sin(a) * 1.4f },
                             { 1.0f, 0.85f, 0.2f, 1.0f }, 0.2f, 0.7f);
        }
        // かくれ花：花が飛び出す
        if ( target.flower ) {
            flowersFound_++;
            flowerPop_ = 1.0f;
            for ( int i = 0; i < 3; ++i ) {
                eggs_->SpawnPuff(at, { ( i - 1 ) * 1.5f, 6.0f, 0.0f }, { 1.0f, 0.6f, 0.75f, 1.0f }, 3.2f, 1.2f, "flower_red");
            }
            for ( int i = 0; i < 14; ++i ) {
                const float a = ( float ) i / 14.0f * 2.0f * kPi;
                eggs_->SpawnPuff(at, { std::cos(a) * 3.0f, 2.0f + ( i % 2 ) * 1.5f, std::sin(a) * 3.0f },
                                 { 1.0f, 0.55f, 0.75f, 1.0f }, 0.32f, 0.9f);
            }
            AudioManager::GetInstance()->PlayWave("resources/kamishibai/se_pon.wav", false, 0.7f);
        }
    }
    AudioManager::GetInstance()->PlayWave("resources/se/eggHit.wav", false, ( std::min )( 0.35f + chain.count * 0.08f, 0.8f));

    // 次へ跳ね返る。無ければ（または最大数）、プレイヤーの所へ戻ってくる
    const int next = chain.count < kMaxCombo ? NextTarget(chain, at) : -1;
    if ( next >= 0 ) {
        chain.target = next;
        chain.egg->Redirect(Normalize(Subtract(targets_[next].center, at)), kEggSpeed, 2.0f);
    } else {
        chain.returning = true;
        chain.timer = 0.0f;
        const Vector3 home { playerPos.x, playerPos.y + 1.2f, playerPos.z };
        chain.egg->Redirect(Normalize(Subtract(home, at)), kEggSpeed * 0.9f, 3.0f);
    }
}

void EggCombo::Update(const Vector3& playerPos, float dt){
    if ( !eggs_ ) { return; }
    for ( auto it = chains_.begin(); it != chains_.end(); ) {
        Chain& chain = *it;
        if ( !eggs_->Contains(chain.egg) || !chain.egg->IsFlying() ) { it = chains_.erase(it); continue; }
        chain.timer += dt;
        if ( chain.returning ) {
            // 戻ってきた：プレイヤーのそばで割れる（キラッと）
            const Vector3 home { playerPos.x, playerPos.y + 1.2f, playerPos.z };
            if ( Length(Subtract(chain.egg->GetPosition(), home)) < 1.2f || chain.timer > 2.5f ) {
                chain.egg->Break();
                it = chains_.erase(it);
                continue;
            }
            chain.egg->Redirect(Normalize(Subtract(home, chain.egg->GetPosition())), kEggSpeed * 0.9f, 3.0f); // 動くプレイヤーを追う
        } else if ( chain.target >= 0 && chain.target < ( int ) targets_.size() ) {
            Vector3 at;
            const Target& target = targets_[chain.target];
            if ( SegmentSphere(chain.egg->GetPrevPosition(), chain.egg->GetPosition(), target.center, target.radius, at) ) {
                Hit(chain, at, playerPos);
            }
        }
        ++it;
    }
    if ( shownCombo_ > 0 ) {
        comboTimer_ += dt;
        if ( comboTimer_ > kComboShowTime && chains_.empty() ) { shownCombo_ = 0; }
    }
    comboPop_ = ( std::max )( 0.0f, comboPop_ - dt / 0.25f );
    flowerPop_ = ( std::max )( 0.0f, flowerPop_ - dt / 0.5f );
    UpdateWobbles(dt);
}

float EggCombo::ComboAlpha() const{
    if ( shownCombo_ <= 0 ) { return 0.0f; }
    const float fade = kComboShowTime - 0.4f;
    return comboTimer_ < fade ? 1.0f : ( std::max )( 0.0f, 1.0f - ( comboTimer_ - fade ) / 0.4f );
}

// ===== 当たった物のゆれ =====
void EggCombo::StartWobble(int item){
    if ( !backdrop_ || item < 0 || item >= ( int ) backdrop_->Items().size() ) { return; }
    for ( Wobble& wobble : wobbles_ ) {
        if ( wobble.item == item ) { wobble.t = 0.0f; return; }
    }
    const BackdropItem& source = backdrop_->Items()[item];
    wobbles_.push_back({ item, 0.0f, source.rot, source.scale });
}

void EggCombo::UpdateWobbles(float dt){
    if ( !backdrop_ ) { wobbles_.clear(); return; }
    for ( auto it = wobbles_.begin(); it != wobbles_.end(); ) {
        it->t += dt;
        if ( it->item >= ( int ) backdrop_->Items().size() ) { it = wobbles_.erase(it); continue; }
        BackdropItem item = backdrop_->Items()[it->item];
        if ( it->t >= kWobbleTime ) {
            item.rot = it->baseRot;
            item.scale = it->baseScale;
            backdrop_->SetItem(it->item, item);
            it = wobbles_.erase(it);
            continue;
        }
        // 根元を軸に、ゆらゆら（だんだん小さく）＋当たった瞬間に少しつぶれる
        const float k = std::exp(-5.0f * it->t);
        const float sway = std::sin(it->t * 28.0f) * 9.0f * k; // 度
        const float squash = 1.0f - 0.12f * std::exp(-14.0f * it->t);
        item.rot = { it->baseRot.x, it->baseRot.y, it->baseRot.z + sway };
        item.scale = { it->baseScale.x * ( 2.0f - squash ), it->baseScale.y * squash, it->baseScale.z * ( 2.0f - squash ) };
        backdrop_->SetItem(it->item, item);
        ++it;
    }
}

void EggCombo::RestoreWobbles(){
    if ( backdrop_ ) {
        for ( const Wobble& wobble : wobbles_ ) {
            if ( wobble.item >= ( int ) backdrop_->Items().size() ) { continue; }
            BackdropItem item = backdrop_->Items()[wobble.item];
            item.rot = wobble.baseRot;
            item.scale = wobble.baseScale;
            backdrop_->SetItem(wobble.item, item);
        }
    }
    wobbles_.clear();
}
