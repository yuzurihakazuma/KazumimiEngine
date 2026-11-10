#include "game/egg/EggSystem.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/audio/AudioManager.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/sdf/SDFVolumeObject.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

EggSystem::~EggSystem() = default; // unique_ptr<Obj3d> のため cpp 側で定義

namespace {
    // -1.0〜1.0 の簡易乱数（トレイルのばらつき用）
    float Rand11(){ return ( ( float ) std::rand() / RAND_MAX ) * 2.0f - 1.0f; }

    // --- 産卵演出（卵が芯から育つ出現エロージョン＋クロスフェード）の定数 ---
    //   ※以前は「敵ボール→卵」のモーフを見せていたが、出現だけのシンプルな演出に変更
    //     （敵の変換は踏みつけ時のSDF消滅演出＝CombatSystem 側が担当する）
    const float kBirthFxDuration = 0.35f; // 卵が育ちきるまでの秒数
    const float kBirthFadeTime   = 0.15f; // 育ちきった後、実体メッシュへクロスフェードする秒数
    const float kBirthFxScale    = 0.91f; // メッシュ卵(モデル高さ1.3×スケール0.7=0.91m)と同じ大きさ
    const float kBirthFullErode  = 0.5f * kBirthFxScale; // これだけ痩せると完全に消える量
    const float kBirthDepthBias  = 0.03f; // 実体へ交代する間だけ、SDF の卵の深度をこれだけ手前へ（同じ形の実体と面を取り合わないように）
    const float kEggHalfHeight   = 0.65f; // 卵のモデルの高さの半分（egg.obj。SDF の卵に実体と同じ UV で模様を貼るのに使う）
    const Vector4 kBirthFxColor  = { 0.98f, 0.96f, 0.86f, 1.0f }; // 予備色（αはフェードで使う）

    // 壁・ブロック・道に対する当たりの大きさ（敵に対する半径への倍率）。
    //   敵には当てやすく大きめの半径を使うが、壁にも同じ大きさを使うと
    //   壁際や足場の上から投げた瞬間に自分の足元へ引っかかって割れてしまう
    const float kObstacleRadiusScale = 0.6f;

    // 後ろの卵の並び（恐竜が通った道のりに沿った距離）
    const float kFollowFirst   = 0.8f;  // 一番近い卵まで(m)
    const float kFollowGap     = 0.6f;  // 卵どうしの間(m)
    const float kTrailStep     = 0.08f; // 足あとを残す間隔(m)
    const float kTrailWarp     = 3.0f;  // 1フレームにこれより動いたら、足あとを捨てる（リスポーン・乗り換えの飛び）
    const float kGatherFirst   = 0.65f; // 止まった時：卵をまとめる所までの道のり(m)（しっぽの後ろ）
    const float kGatherStack   = 0.05f; // 止まった時：重ねた卵を 1 個ごとに奥へずらす量(m)（ぴったり同じ所だと面がちらつく）
    const float kFollowStagger = 0.07f; // 並び方が変わった時、1 個ずつ遅れて動き出す秒
    const float kStillSpeed    = 0.6f;  // 横の速さがこれより小さいと「止まっている」(m/秒)
    const float kStillDelay    = 0.15f; // 止まってから、まとまり始めるまで(秒)
    const float kHopDelay      = 0.09f; // 止まっている時：恐竜が跳ぶと、卵が 1 個ずつこれだけ遅れて同じように跳ぶ(秒)
    const float kWalkHop       = 0.14f; // 歩いている時：卵がぴょこぴょこ跳ねる高さ(m)
    const float kWalkHopStride = 0.7f;  // 歩いている時：1 回跳ねる間に進む道のり(m)
    const float kHistoryTime   = 1.2f;  // 恐竜の足の高さを覚えておく秒
}

void EggSystem::Initialize(){
    eggs_.clear();
    // 表示中パフの Obj3d はプールへ返してから消す。捨てるとモード切替のたびに
    // プールが痩せていき、いずれ初回バーストのカクつきが再発してしまう
    for ( auto& puff : puffs_ ) {
        if ( puff.obj ) {
            auto& pool = puffPool_[puff.model];
            if ( pool.size() < 32 ) { pool.push_back(std::move(puff.obj)); }
        }
    }
    puffs_.clear();
    spits_.clear();
    stomach_    = 0;
    trailTimer_ = 0.0f;
    trail_.clear();
    trailReady_ = false;
    followStopped_ = true;
    stillTime_ = modeTime_ = 0.0f;
    footHistory_.clear();
    clock_ = walkedTotal_ = 0.0f;
    // 産卵演出の状態もリセット（birthFx_ 本体は読み込み済みのまま使い回す）
    birthEgg_   = nullptr;
    birthTimer_ = -1.0f;
}

// 産卵演出のセットアップ。
//   卵(egg.sdf3d)単体を読み込み、産む瞬間に「芯から育てて」出現させる。
//   （旧実装の敵ボール→卵モーフは廃止。モーフ用の2ボリューム目も読まないぶん軽い）
void EggSystem::InitializeBirthFx(ID3D12GraphicsCommandList* commandList){
    birthFx_ = std::make_unique<SDFVolumeObject>();
    birthLookReady_ = false; // 作り直したら、見た目の設定もやり直す
    if ( !birthFx_->Load("resources/sdf3d/egg.sdf3d", commandList) ) {
        birthFx_.reset(); // 無ければ演出なし（従来のポン出しに自動フォールバック）
        return;
    }
    birthFx_->SetScale(kBirthFxScale);
    birthFx_->SetColor({ 0.98f, 0.96f, 0.86f, 1.0f }); // カラーボリュームが無い時の予備色
}

// 演出中のSDF卵の描画（専用PSOに切り替えるためMRTパスの最後で呼ばれる）
void EggSystem::DrawBirthFx(ID3D12GraphicsCommandList* commandList){
    if ( !birthFx_ ) { return; }
    if ( birthTimer_ >= 0.0f ) { birthFx_->Draw(commandList); }
    else if ( birthWarmupDraws_ > 0 ) { --birthWarmupDraws_; birthFx_->Draw(commandList); }
}

void EggSystem::WarmupBirthFx(){
    if ( !birthFx_ || birthTimer_ >= 0.0f ) { return; }
    birthFx_->SetTranslation({ 0.0f, -10000.0f, 0.0f });
    birthFx_->Update();
    birthWarmupDraws_ = 3;
}

// パフ用 Obj3d の事前生成：ロード時にプールへ積んでおく（初回バーストの固まり対策）
void EggSystem::PrewarmPuffPool(const std::string& modelName, int count){
    auto& pool = puffPool_[modelName];
    while ( ( int ) pool.size() < count ) {
        auto obj = Obj3d::Create(modelName);
        if ( !obj ) break; // モデル未登録なら何もしない（SpawnPuff 側と同じ扱い）
        pool.push_back(std::move(obj));
    }
}

// 実体パフを1個出す（fxSphere=色付きの粒／eggShell=殻の欠片）。縮みながら消えるので加算でなくても確実に見える。
void EggSystem::SpawnPuff(const Vector3& pos, const Vector3& vel, const Vector4& color, float scale, float life,
                          const std::string& modelName){
    TrailPuff puff;
    // 同じモデルの使い終わった Obj3d があれば使い回す（Create はGPUリソース確保を伴い、
    // 敵撃破や卵割れの一斉バーストでその瞬間だけ重くなるため）
    auto poolIt = puffPool_.find(modelName);
    if ( poolIt != puffPool_.end() && !poolIt->second.empty() ) {
        puff.obj = std::move(poolIt->second.back());
        poolIt->second.pop_back();
    } else {
        puff.obj = Obj3d::Create(modelName); // 既定カメラが自動バインド
    }
    puff.model = modelName;
    if ( puff.obj ) {
        puff.obj->SetScale({ scale, scale, scale });
        // 殻の欠片(eggShell)は自前の色をそのまま見せる。汎用の粒(fxSphere)だけ呼び出し側の色を付ける。
        if ( modelName == "fxSphere" && puff.obj->GetModel() && puff.obj->GetModel()->GetMaterial() ) {
            puff.obj->GetModel()->GetMaterial()->color = color;
        }
        // 殻は薄い湾曲面なので両面表示（裏返っても消えない）
        if ( modelName == "eggShell" ) {
            puff.obj->SetPipelineType(PipelineType::Object3D_CullNone);
        }
        puff.obj->SetTranslation(pos);
        puff.obj->Update();
    }
    puff.pos = pos; puff.vel = vel; puff.life = life; puff.maxLife = life; puff.baseScale = scale;
    puff.rot = { Rand11() * 3.14f, Rand11() * 3.14f, Rand11() * 3.14f };       // ランダムな初期姿勢
    puff.rotSpeed = { Rand11() * 8.0f, Rand11() * 8.0f, Rand11() * 8.0f };     // くるくる回りながら舞う
    puffs_.push_back(std::move(puff));
}

// 敵を飲み込んだ → お腹に1匹ためる（卵にするのは LayEgg）。
void EggSystem::AddToBelly(){
    if ( stomach_ < kMaxEggs ) { ++stomach_; }
}

// しゃがみ等で産む：お腹を1減らして、birthPos に卵(Held)を1個生む。
//   持っている卵が満杯なら一番古い卵を割って捨ててから生む。
bool EggSystem::LayEgg(const Vector3& birthPos){
    if ( stomach_ <= 0 ) return false;
    --stomach_;

    if ( HeldCount() >= kMaxEggs ) {
        for ( auto& heldEgg : eggs_ ) {        // 先頭から順＝一番古い保持卵を捨てる
            if ( heldEgg->IsHeld() ) { heldEgg->Break(); break; }
        }
    }

    auto egg = std::make_unique<Egg>(birthPos);

    // 実体（見た目）：専用の卵モデル（resources/egg/egg.obj）を使う。
    //   ※以前は "sphere" を潰して代用していたが、"sphere" は敵とも共有しているモデルのため
    //   色を変えると敵まで一緒に染まってしまっていた。専用モデルにしたことでその干渉も無くなる。
    const Vector3 baseScale = { 0.7f, 0.7f, 0.7f }; // モデル自体が卵の形なので均等スケールでOK
    if ( auto obj = Obj3d::Create("egg") ) { // 既定カメラが自動でバインドされる
        obj->SetScale(baseScale);
        egg->AttachVisual(std::move(obj), baseScale);
    }
    eggs_.push_back(std::move(egg));

    // 産卵エロージョン演出を開始：実体メッシュを隠し、SDFの卵を「芯から育てる」。
    //   育ちきったら Update が実体メッシュへバトンタッチする
    if ( birthFx_ ) {
        // 連続産卵対策：前の演出対象がまだ非表示のままなら、先に実体表示へ戻す
        //   （戻さないと演出が新しい卵へ移った時、前の卵が永久に見えなくなる）
        if ( birthEgg_ ) {
            for ( auto& egg : eggs_ ) {
                if ( egg.get() == birthEgg_ ) { birthEgg_->SetVisualHidden(false); birthEgg_->SetSpinPaused(false); break; }
            }
        }
        // SDF の卵を、実体の卵と同じ見た目（同じテクスチャ・同じ光・同じつや）にする。交代しても模様・明るさが変わらない
        if ( !birthLookReady_ ) {
            if ( Model* model = ModelManager::GetInstance()->FindModel("egg") ) {
                const float shininess = model->GetMaterial() ? model->GetMaterial()->shininess : 32.0f;
                birthFx_->SetMeshLook(true, model->GetTextureIndex(), kEggHalfHeight, shininess);
                birthLookReady_ = true;
            }
        }
        birthEgg_   = eggs_.back().get();
        birthTimer_ = 0.0f;
        birthEgg_->SetVisualHidden(true);
        birthEgg_->SetSpinPaused(true); // 交代が終わるまで回さない（SDF の卵と模様の向きをそろえる）
    }
    return true;
}

// 恐竜の足あとを残す（少し動くたびに 1 点）。古い分は、卵が並ぶ長さより先を捨てる
void EggSystem::UpdateTrail(const Vector3& playerPos, const Vector3& facing, float dt){
    if ( trailReady_ ) {
        const Vector3 jump { playerPos.x - trailHead_.x, playerPos.y - trailHead_.y, playerPos.z - trailHead_.z };
        if ( jump.x * jump.x + jump.y * jump.y + jump.z * jump.z > kTrailWarp * kTrailWarp ) { trail_.clear(); } // 飛んだ：並び直す
        // 止まっているか：横の速さが小さいまま少し続いたら「止まった」（跳んでいても、横へ動いていなければ止まった扱い）
        const float hx = playerPos.x - trailHead_.x, hz = playerPos.z - trailHead_.z;
        const float speed = dt > 1e-5f ? std::sqrt(hx * hx + hz * hz) / dt : 0.0f;
        stillTime_ = ( speed < kStillSpeed ) ? stillTime_ + dt : 0.0f;
    }
    const bool stopped = stillTime_ >= kStillDelay;
    if ( stopped != followStopped_ ) { followStopped_ = stopped; modeTime_ = 0.0f; } // 並び方が変わった：近い卵から 1 個ずつ動く
    modeTime_ += dt;
    if ( followGrounded_ ) { lastGroundFootY_ = playerPos.y; } // 恐竜が最後に立っていた高さ（止まった時の卵の高さ）
    // 恐竜の足の高さを時間で覚えておく（止まっている時に跳ぶと、卵が 1 個ずつ遅れて同じ高さを跳ぶ）
    clock_ += dt;
    footHistory_.push_back({ clock_, playerPos.y });
    while ( footHistory_.size() > 2 && footHistory_[1].time < clock_ - kHistoryTime ) { footHistory_.erase(footHistory_.begin()); }
    if ( trailReady_ ) {
        const float hx = playerPos.x - trailHead_.x, hz = playerPos.z - trailHead_.z;
        walkedTotal_ += std::sqrt(hx * hx + hz * hz); // 歩いた道のり（卵がぴょこぴょこ跳ねる拍子）
    }
    trailHead_ = playerPos;
    trailFacing_ = facing;
    trailReady_ = true;
    const TrailPoint now { playerPos, followGroundY_, followHasGround_ };
    if ( trail_.empty() ) { trail_.push_back(now); return; }
    // 道のりは横の動きで数える（その場で跳ぶ・落ちるだけでは足あとを増やさない＝卵が足元へ寄ってこない）
    const float dx = playerPos.x - trail_[0].pos.x, dz = playerPos.z - trail_[0].pos.z;
    if ( dx * dx + dz * dz < kTrailStep * kTrailStep ) {
        trail_[0] = now; // 止まっている・真上へ跳んでいる間：一番新しい足あとを今の所にする
        return;
    }
    trail_.insert(trail_.begin(), now);
    // 一番遠い卵の所より先の足あとは捨てる
    const float keep = kFollowFirst + kFollowGap * ( kMaxEggs + 1 );
    float length = 0.0f;
    for ( size_t i = 1; i < trail_.size(); ++i ) {
        const float sx = trail_[i].pos.x - trail_[i - 1].pos.x, sz = trail_[i].pos.z - trail_[i - 1].pos.z;
        length += std::sqrt(sx * sx + sz * sz);
        if ( length > keep ) { trail_.resize(i + 1); break; }
    }
}

// 足あとを、恐竜から道のり along だけ戻った点（足の高さ。跳んで通った所は跳んだ高さ）と、そこでの進む向き（横の向き）。
//   足あとが足りない時は、一番古い足あとから、向きの真後ろへ延ばす
void EggSystem::TrailPointAt(float along, Vector3& outPos, Vector3& outDir, TrailPoint* outGround) const{
    Vector3 prev = trailHead_;
    TrailPoint last { trailHead_, followGroundY_, followHasGround_ };
    outDir = trailFacing_;
    float walked = 0.0f;
    for ( const TrailPoint& point : trail_ ) {
        const Vector3 s { point.pos.x - prev.x, point.pos.y - prev.y, point.pos.z - prev.z };
        const float step = std::sqrt(s.x * s.x + s.z * s.z);
        if ( step > 1e-5f ) { outDir = { -s.x / step, 0.0f, -s.z / step }; } // 恐竜が進んだ向き
        if ( walked + step >= along && step > 1e-5f ) {
            const float k = ( along - walked ) / step;
            outPos = { prev.x + s.x * k, prev.y + s.y * k, prev.z + s.z * k };
            if ( outGround ) { *outGround = ( k >= 0.5f ) ? point : last; } // 近い方の足あとの地面
            return;
        }
        walked += step;
        prev = point.pos;
        last = point;
    }
    const float rest = along - walked;
    outPos = { prev.x - trailFacing_.x * rest, prev.y, prev.z - trailFacing_.z * rest };
    if ( outGround ) { *outGround = last; outGround->hasGround = false; } // 足あとの先（通っていない所）：地面は分からない
}

// 足あとを fromAlong〜maxAlong 戻る間で、恐竜が今より高い面に立っていた（足が地面に着いていた）最初の所
bool EggSystem::StandingPointBehind(float fromAlong, float maxAlong, Vector3& outPos) const{
    Vector3 prev = trailHead_;
    float walked = 0.0f;
    for ( const TrailPoint& point : trail_ ) {
        const float sx = point.pos.x - prev.x, sz = point.pos.z - prev.z;
        walked += std::sqrt(sx * sx + sz * sz);
        prev = point.pos;
        if ( walked > maxAlong ) { break; }
        if ( walked < fromAlong ) { continue; }
        if ( point.hasGround && std::abs(point.pos.y - point.groundY) < 0.08f && point.groundY > lastGroundFootY_ + 0.3f ) {
            outPos = { point.pos.x, point.groundY, point.pos.z };
            return true;
        }
    }
    return false;
}

// delay 秒前の恐竜の足の高さ
float EggSystem::FootYAgo(float delay) const{
    if ( footHistory_.empty() ) { return lastGroundFootY_; }
    const float t = clock_ - delay;
    if ( t <= footHistory_.front().time ) { return footHistory_.front().y; }
    for ( size_t i = 1; i < footHistory_.size(); ++i ) {
        const FootSample& a = footHistory_[i - 1];
        const FootSample& b = footHistory_[i];
        if ( t <= b.time ) {
            const float span = b.time - a.time;
            return span > 1e-6f ? a.y + ( b.y - a.y ) * ( t - a.time ) / span : b.y;
        }
    }
    return footHistory_.back().y;
}

// 止まった時に卵を並べられる長さ：恐竜が今立っている面の上（足あとの高さが、立っている高さから 0.3m 以内）を、どこまで戻れるか。
//   段を跳び上がってすぐ止まった時に、下の道や跳んだ途中の宙へ卵を置かない
float EggSystem::FlatLengthBehind() const{
    Vector3 prev = trailHead_;
    float walked = 0.0f;
    for ( const TrailPoint& point : trail_ ) {
        if ( std::abs(point.pos.y - lastGroundFootY_) > 0.3f ) { return walked; }
        const float sx = point.pos.x - prev.x, sz = point.pos.z - prev.z;
        walked += std::sqrt(sx * sx + sz * sz);
        prev = point.pos;
    }
    return walked; // 足あとの先（通っていない所）は数えない
}

// index 番目の卵が並ぶ所。
//   動いている時：足あとを一列に付いてくる（恐竜から 0.8m・0.6m おき）。高さは恐竜が通った高さ（跳んで越えた所は卵も跳ねて越える）。
//   止まった時：恐竜のすぐ後ろの 1 か所に、全部の卵が重なってまとまる（宙や下の道に残らない・ブロックの中に入らない）。
//   並び方が変わったら、近い卵から 1 個ずつ（0.07 秒ずつ遅れて）動き出す
Vector3 EggSystem::FollowSlot(int index) const{
    if ( !trailReady_ ) { return { 0.0f, kHeldEggHeight, 0.0f }; }
    const bool stopped = ( modeTime_ >= kFollowStagger * index ) ? followStopped_ : !followStopped_;
    if ( !stopped ) {
        const float along = kFollowFirst + kFollowGap * ( float ) index;
        Vector3 at, dir;
        TrailPointAt(along, at, dir, nullptr);
        // ぴょこぴょこ：同じ所で跳ねる（前の卵が跳ねた所へ来ると、後ろの卵も跳ねる＝1 個ずつ順に跳ねて付いてくる）
        const float hop = kWalkHop * std::abs(std::sin(( walkedTotal_ - along ) * 3.14159265f / kWalkHopStride));
        return { at.x, at.y + kHeldEggHeight + hop, at.z };
    }
    // まとまる：全部の卵を、恐竜のすぐ後ろの 1 か所に重ねる（1 個ごとに少しだけ奥へずらす）。
    //   置く所は、恐竜が通って来た道を kGatherFirst 戻った所。そこが
    //     ・今立っている面と同じ高さ → そこ（地面の上）
    //     ・高い所（段から降りて来た） → もう少し戻って、恐竜が段の上に立っていた所（段の上に置く。下へ下ろすとブロックの中になる）
    //     ・低い所・地面が無い（段へ跳び上がった・穴を越えた）→ 立っている面の上を戻れる所まで戻り、足りない分は奥へずらす
    //   どれも恐竜が立てた所か、その真横なので、ブロックの中にならない
    Vector3 at, dir;
    TrailPoint ground {};
    TrailPointAt(kGatherFirst, at, dir, &ground);
    float baseY = lastGroundFootY_;
    Vector3 standing;
    if ( ground.hasGround && std::abs(ground.groundY - lastGroundFootY_) <= 0.3f ) {
        // 同じ高さ：そのまま地面の上
    } else if ( ground.hasGround && ground.groundY > lastGroundFootY_ && StandingPointBehind(kGatherFirst, kGatherFirst + 1.5f, standing) ) {
        at = standing;         // 段の上で恐竜が立っていた所
        baseY = standing.y;
    } else {
        const float along = ( std::min )( kGatherFirst, FlatLengthBehind() );
        TrailPointAt(along, at, dir, nullptr);
        const float push = std::sqrt(( std::max )( kGatherFirst * kGatherFirst - along * along, 0.0f ));
        at.x += followAway_.x * push;
        at.z += followAway_.z * push;
    }
    at.x += followAway_.x * kGatherStack * ( float ) index;
    at.z += followAway_.z * kGatherStack * ( float ) index;
    // 高さ：少し前の恐竜の足の高さ（恐竜が跳ぶと、近い卵から 1 個ずつ遅れて同じように跳ぶ）。置く面より下へは行かない
    const float y = ( std::max )( FootYAgo(kHopDelay * ( index + 1 )), baseY );
    return { at.x, y + kHeldEggHeight, at.z };
}

void EggSystem::Update(const Vector3& playerPos, const Vector3& facing, float dt){
    // 保持中の卵は、恐竜が通った所（足あと）を少し遅れて付いてくる（増えるほど後ろへ一列に並ぶ）。
    //   各卵が自分でそこへ寄っていく（整列＆追従）
    UpdateTrail(playerPos, facing, dt);
    int held = 0;
    for ( auto& egg : eggs_ ) {
        if ( !egg->IsHeld() ) continue;
        egg->SetTarget(FollowSlot(held));
        ++held;
    }

    // 飛行トレイル：一定間隔で、飛んでいる卵の位置に煙パフ（白い小球）を置く。
    trailTimer_ += dt;
    bool emitTrail = false;
    if ( trailTimer_ >= 0.03f ) { trailTimer_ -= 0.03f; emitTrail = true; }

    // 各卵の状態と見た目を進める＋トレイル／割れた瞬間の殻飛び散り
    for ( auto& egg : eggs_ ) {
        if ( egg->IsFlying() && emitTrail ) {
            Vector3 eggPos = egg->GetPosition();
            Vector3 jitter = { Rand11() * 0.1f, Rand11() * 0.1f, Rand11() * 0.1f };
            SpawnPuff({ eggPos.x + jitter.x, eggPos.y + jitter.y, eggPos.z + jitter.z },
                      { Rand11() * 0.4f, 0.4f + Rand11() * 0.2f, Rand11() * 0.4f },
                      { 0.95f, 0.97f, 1.0f, 1.0f }, 0.28f, 0.4f); // 白い煙
        }
        egg->Update(dt);
        // 飛行中：動いた区間で壁・ブロック・道に当たったら、当たった位置で止める
        if ( egg->IsFlying() && !egg->HitObstacle() ) {
            if ( obstacleQuery_ ) {
                Vector3 hitPos {};
                if ( obstacleQuery_(egg->GetPrevPosition(), egg->GetPosition(),
                                    egg->GetRadius() * kObstacleRadiusScale, hitPos) ) {
                    egg->StopAtObstacle(hitPos);
                }
            } else if ( egg->GetPosition().y <= egg->GetRadius() ) {
                // 問い合わせ先が無い時の保険：高さ0の床で止まる（従来の挙動）
                Vector3 floorPos = egg->GetPosition();
                floorPos.y = egg->GetRadius();
                egg->StopAtObstacle(floorPos);
            }
        }
        if ( egg->JustBroke() ) { // 着弾／時間切れ：殻の欠片が飛び散り、黄身が splash する
            AudioManager::GetInstance()->PlayWave("resources/se/eggBreak.wav", false, 0.4f); // 割れる音
            Vector3 breakPos = egg->GetPosition();
            for ( int i = 0; i < 6; ++i ) { // 殻の欠片（くるくる回りながら舞う）
                SpawnPuff(breakPos, { Rand11() * 3.5f, 2.0f + Rand11() * 2.0f, Rand11() * 3.5f },
                          { 1.0f, 1.0f, 1.0f, 1.0f }, 0.22f, 0.45f, "eggShell");
            }
            for ( int i = 0; i < 6; ++i ) { // 黄身の飛沫（小さい黄色の粒）
                SpawnPuff(breakPos, { Rand11() * 2.5f, 1.5f + Rand11() * 1.5f, Rand11() * 2.5f },
                          { 1.0f, 0.85f, 0.2f, 1.0f }, 0.12f, 0.35f);
            }
            egg->ClearJustBroke();
        }
    }

    // --- 吐き出し弾の更新（軽い放物線＋転がり回転。地面/時間切れで煙になって消える）---
    for ( auto& spit : spits_ ) {
        if ( spit.dead ) continue;
        spit.life  -= dt;
        bool stopped = spit.hitObstacle; // 前の更新で壁/地面に当たって止まっていた
        if ( !stopped ) {
            spit.prevPos = spit.pos;
            spit.vel.y -= 10.0f * dt; // 軽い重力＝少し先で落ちる放物線
            spit.pos.x += spit.vel.x * dt;
            spit.pos.y += spit.vel.y * dt;
            spit.pos.z += spit.vel.z * dt;
            spit.spin  += 12.0f * dt;

            // 動いた区間で壁・ブロック・道に当たったら、当たった位置で止める
            //   （消えるのは次の更新。その前に敵の判定が1回通る＝手前の敵への命中を取りこぼさない）
            if ( obstacleQuery_ ) {
                Vector3 hitPos {};
                if ( obstacleQuery_(spit.prevPos, spit.pos, spit.radius * kObstacleRadiusScale, hitPos) ) {
                    spit.pos = hitPos;
                    spit.vel = { 0.0f, 0.0f, 0.0f };
                    spit.hitObstacle = true;
                }
            } else if ( spit.pos.y - spit.radius <= 0.0f ) {
                stopped = true; // 問い合わせ先が無い時の保険：高さ0の床（従来の挙動）
            }
        }

        if ( stopped || spit.life <= 0.0f || spit.pos.y < -30.0f ) {
            // 壁/地面に当たった or 時間切れ → 白い煙を出して消える
            for ( int i = 0; i < 5; ++i ) {
                SpawnPuff(spit.pos, { Rand11() * 1.5f, 0.8f + Rand11() * 0.6f, Rand11() * 1.5f },
                          { 0.95f, 0.97f, 1.0f, 1.0f }, 0.22f, 0.35f);
            }
            spit.dead = true;
            continue;
        }
        if ( spit.obj ) {
            spit.obj->SetTranslation(spit.pos);
            spit.obj->SetRotation({ spit.spin, 0.0f, 0.0f }); // 進行方向へ転がる見た目
            spit.obj->Update();
        }
    }
    spits_.erase(std::remove_if(spits_.begin(), spits_.end(),
        [](const SpitBall& spit){ return spit.dead; }), spits_.end());

    // 実体パフの更新（移動＋軽い重力＋回転＋縮小、寿命切れで削除）
    for ( auto& puff : puffs_ ) {
        puff.life   -= dt;
        puff.vel.y  -= 3.0f * dt;
        puff.pos.x  += puff.vel.x * dt;
        puff.pos.y  += puff.vel.y * dt;
        puff.pos.z  += puff.vel.z * dt;
        puff.rot.x  += puff.rotSpeed.x * dt;
        puff.rot.y  += puff.rotSpeed.y * dt;
        puff.rot.z  += puff.rotSpeed.z * dt;
        float lifeRatio = ( puff.maxLife > 0.0f ) ? ( puff.life / puff.maxLife ) : 0.0f;
        if ( lifeRatio < 0.0f ) lifeRatio = 0.0f;
        float shrunkScale = puff.baseScale * lifeRatio;
        if ( puff.obj ) {
            puff.obj->SetTranslation(puff.pos);
            puff.obj->SetRotation(puff.rot);
            puff.obj->SetScale({ shrunkScale, shrunkScale, shrunkScale });
            puff.obj->Update();
        }
    }
    // 寿命切れのパフは Obj3d をプールへ返してから消す（次のバーストで使い回す）
    for ( auto& puff : puffs_ ) {
        if ( puff.life <= 0.0f && puff.obj ) {
            auto& pool = puffPool_[puff.model];
            if ( pool.size() < 32 ) { pool.push_back(std::move(puff.obj)); }
        }
    }
    puffs_.erase(std::remove_if(puffs_.begin(), puffs_.end(),
        [](const TrailPuff& puff){ return puff.life <= 0.0f; }), puffs_.end());

    // 割れて消えてよくなった卵を後始末
    eggs_.erase(
        std::remove_if(eggs_.begin(), eggs_.end(),
            [](const std::unique_ptr<Egg>& egg){ return egg->IsDead(); }),
        eggs_.end());

    // --- 産卵演出の進行 ---
    //   ①SDFの卵が芯から育つ → ②実体メッシュを出した上に SDF卵を重ねたまま
    //   透明化（クロスフェード）→ 完全交代。
    //   パッと差し替えず重ねて溶かすので、質感の微差が切り替わりとして見えない。
    if ( birthTimer_ >= 0.0f && birthFx_ ) {
        // 対象の卵がまだ存在するか検証（満杯で割られた等でポインタが死ぬのを防ぐ）
        bool exists = false;
        for ( auto& egg : eggs_ ) { if ( egg.get() == birthEgg_ ) { exists = true; break; } }

        birthTimer_ += dt;
        float tGrow = birthTimer_ / kBirthFxDuration;                         // 0〜1: 出現（芯から育つ）
        float tFade = ( birthTimer_ - kBirthFxDuration ) / kBirthFadeTime;    // 0〜1: クロスフェード

        if ( !exists || !birthEgg_->IsHeld() || tFade >= 1.0f ) {
            // 演出終了（or 途中で投げられた/割られた）→ 完全に実体メッシュへ交代
            if ( exists ) { birthEgg_->SetVisualHidden(false); birthEgg_->SetSpinPaused(false); }
            birthEgg_   = nullptr;
            birthTimer_ = -1.0f;
        } else if ( tGrow >= 1.0f ) {
            // ②クロスフェード：実体メッシュを表示した上に、育ちきったSDF卵を重ねて透明化していく
            birthEgg_->SetVisualHidden(false);
            Vector4 fadeColor = kBirthFxColor;
            fadeColor.w = 1.0f - std::clamp(tFade, 0.0f, 1.0f);
            birthFx_->SetColor(fadeColor);
            birthFx_->SetErode(0.0f);
            // 実体の卵と同じ形・同じ場所に重ねるので、深度だけ少し手前へずらす（面の取り合いでちらつかない）
            birthFx_->SetDepthBias(kBirthDepthBias);
            birthFx_->SetTranslation(birthEgg_->GetPosition());
            birthFx_->Update();
        } else {
            // ①出現：エロージョンを解きながら卵を芯から育てる
            float grow = 1.0f - ( 1.0f - tGrow ) * ( 1.0f - tGrow ); // 2次アウト（最初勢いよく→ゆっくり整う）

            birthFx_->SetColor(kBirthFxColor); // α=1（次の産卵に備えて毎回リセット）
            birthFx_->SetDepthBias(0.0f);
            birthFx_->SetTranslation(birthEgg_->GetPosition());
            birthFx_->SetErode(kBirthFullErode * ( 1.0f - grow ));
            birthFx_->Update(); // カメラ行列＋CBを毎フレーム焼き直す
        }
    }
}

void EggSystem::Draw() const{
    // 構え中は「次に投げる卵（列の先頭）」を1個描かない＝手に持っている扱い。
    // TryThrow も先頭の保持卵を投げるので、隠した卵と投げる卵は必ず一致する
    bool handEggSkipped = !aimHolding_;
    for ( const auto& egg : eggs_ ) {
        if ( !handEggSkipped && egg->IsHeld() ) { handEggSkipped = true; continue; }
        egg->Draw();
    }
    for ( const auto& spit : spits_ ) { if ( spit.obj && !spit.dead ) spit.obj->Draw(); }        // 吐き出し弾
    for ( const auto& puff : puffs_ ) { if ( puff.obj && puff.life > 0.0f ) puff.obj->Draw(); } // 煙パフ
}

// 保持中の一番古い卵を指定方向へ投げる。
bool EggSystem::TryThrow(const Vector3& playerPos, const Vector3& dir, float speed){
    return ThrowEgg(playerPos, dir, speed) != nullptr;
}

bool EggSystem::Contains(const Egg* target) const{
    for ( const auto& egg : eggs_ ) { if ( egg.get() == target ) { return true; } }
    return false;
}

Egg* EggSystem::ThrowEgg(const Vector3& playerPos, const Vector3& dir, float speed){
    for ( auto& egg : eggs_ ) {
        if ( !egg->IsHeld() ) continue;
        Vector3 from = { playerPos.x, playerPos.y + 0.5f, playerPos.z };
        egg->SetPosition(from);
        egg->Throw(dir, speed);
        // 「ぽいっ」と投げる時の白い煙ひと吹き
        for ( int i = 0; i < 6; ++i ) {
            SpawnPuff(from, { Rand11() * 1.2f, 0.5f + Rand11() * 0.5f, Rand11() * 1.2f },
                      { 0.95f, 0.97f, 1.0f, 1.0f }, 0.25f, 0.35f);
        }
        return egg.get();
    }
    return nullptr;
}

// 飲み込み：口元で緑の小球がふわっと上へ吸い込まれる（リング無し＝踏みつけと別物）。
void EggSystem::SpawnSwallowFx(const Vector3& pos){
    for ( int i = 0; i < 9; ++i ) {
        SpawnPuff(pos, { Rand11() * 1.0f, 1.6f + Rand11() * 0.8f, Rand11() * 1.0f },
                  { 0.4f, 1.0f, 0.5f, 1.0f }, 0.2f, 0.32f); // 緑・上向き・速め
    }
}

// 産卵：白＆緑がぽわっと丸く広がる（生まれた感。ゆっくりめ）。
void EggSystem::SpawnLayFx(const Vector3& pos){
    for ( int i = 0; i < 11; ++i ) {
        Vector4 puffColor = ( i % 2 ) ? Vector4{ 0.5f, 1.0f, 0.6f, 1.0f }   // 緑
                                : Vector4{ 0.95f, 1.0f, 0.95f, 1.0f }; // 白
        SpawnPuff(pos, { Rand11() * 1.5f, 0.4f + Rand11() * 0.5f, Rand11() * 1.5f },
                  puffColor, 0.24f, 0.45f); // ゆっくり広がる
    }
}

// 命中：殻の欠片が勢いよく飛び散り、黄身が splash する（衝撃感。速い・多い）。
void EggSystem::SpawnHitFx(const Vector3& pos){
    for ( int i = 0; i < 8; ++i ) { // 殻の欠片（大きめ・速い・回転）
        SpawnPuff(pos, { Rand11() * 4.8f, 1.0f + Rand11() * 3.0f, Rand11() * 4.8f },
                  { 1.0f, 1.0f, 1.0f, 1.0f }, 0.24f, 0.4f, "eggShell");
    }
    for ( int i = 0; i < 8; ++i ) { // 黄身の飛沫
        SpawnPuff(pos, { Rand11() * 3.5f, 0.8f + Rand11() * 2.2f, Rand11() * 3.5f },
                  { 1.0f, 0.6f, 0.1f, 1.0f }, 0.14f, 0.32f);
    }
}

// お腹の敵を1匹、指定方向へ吐き出す（口元 from から dir 方向へ発射）
bool EggSystem::SpitOut(const Vector3& from, const Vector3& dir, float speed){
    if ( stomach_ <= 0 ) return false;
    --stomach_;

    SpitBall ball;
    ball.obj = Obj3d::Create("sphere"); // 敵と同じモンスターボール柄＝「飲んだ敵」がそのまま出てくる
    ball.pos = from;
    ball.prevPos = from;
    // 少し上向きに発射→軽い放物線を描く。dir が坂に沿って上/下を向いていれば、その分も足す
    ball.vel = { dir.x * speed, 1.5f + dir.y * speed, dir.z * speed };
    if ( ball.obj ) {
        ball.obj->SetScale({ ball.radius, ball.radius, ball.radius });
        ball.obj->SetTranslation(ball.pos);
        ball.obj->Update();
    }
    spits_.push_back(std::move(ball));

    // 「ぷはっ」と吐いた煙
    for ( int i = 0; i < 6; ++i ) {
        SpawnPuff(from, { dir.x * 1.5f + Rand11() * 0.8f, 0.6f + Rand11() * 0.5f, dir.z * 1.5f + Rand11() * 0.8f },
                  { 0.95f, 0.97f, 1.0f, 1.0f }, 0.24f, 0.35f);
    }
    return true;
}

// 飛行中の吐き出し弾を当たり判定にかけ、当たった弾を消す（敵側の処理は CombatSystem が行う）
void EggSystem::ResolveSpitHits(const SweepHit& onHit){
    for ( auto& spit : spits_ ) {
        if ( spit.dead ) continue;
        if ( onHit(spit.prevPos, spit.pos, spit.radius) ) {
            for ( int i = 0; i < 5; ++i ) { // ぶつかって弾けた煙
                SpawnPuff(spit.pos, { Rand11() * 2.0f, 0.8f + Rand11() * 1.0f, Rand11() * 2.0f },
                          { 1.0f, 0.9f, 0.8f, 1.0f }, 0.2f, 0.3f);
            }
            spit.dead = true;
        }
    }
}

// 飛行中の卵を当たり判定にかけ、当たった卵を割る（敵側の処理は onHit 内でシーンが行う）。
void EggSystem::ResolveHits(const SweepHit& onHit){
    for ( auto& egg : eggs_ ) {
        if ( !egg->IsFlying() ) continue;
        if ( onHit(egg->GetPrevPosition(), egg->GetPosition(), egg->GetRadius()) ) {
            egg->Break(); // 命中 → 割れる（星は次の Update が JustBroke を拾って出す）
        }
    }
}

// 飛んでいる卵・吐き出し弾の当たり判定の形（敵に対する球／壁に対する球）を描く
void EggSystem::DrawHitShapes(const Vector4& color) const{
    DebugDraw* debugDraw = DebugDraw::GetInstance();
    Vector4 obstacleColor = { color.x, color.y, color.z, color.w * 0.5f };
    for ( const auto& egg : eggs_ ) {
        if ( !egg->IsFlying() ) continue;
        debugDraw->Sphere(egg->GetPosition(), egg->GetRadius(), color, 12);
        debugDraw->Sphere(egg->GetPosition(), egg->GetRadius() * kObstacleRadiusScale, obstacleColor, 8);
    }
    for ( const auto& spit : spits_ ) {
        if ( spit.dead ) continue;
        debugDraw->Sphere(spit.pos, spit.radius, color, 12);
        debugDraw->Sphere(spit.pos, spit.radius * kObstacleRadiusScale, obstacleColor, 8);
    }
}

int EggSystem::HeldCount() const{
    int count = 0;
    for ( const auto& egg : eggs_ ) { if ( egg->IsHeld() ) ++count; }
    return count;
}
