#include "game/rail/TrainVisuals.h"
#include "game/rail/RailMotion.h"
#include "game/rail/ToyTrain.h"

#include "engine/audio/AudioManager.h"
#include "engine/camera/Camera.h"
#include "engine/utils/Easing.h"
#include "engine/graphics/InstancedGroup.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/MathUtil.h"
#include "engine/rail/SplineRail.h"

#include <algorithm>
#include <cmath>

using namespace KazumimiEngine;

namespace {
    // 線路（ワールドマップの箱庭と同じ作り：段ボールの枕木と、灰色のフェルトのレール）
    constexpr const char* kSleeperModel = "craftLayer"; // 1m 角の段ボール層（原点＝底）
    constexpr float kSleeperStep  = 1.0f;   // 枕木の間(m)
    constexpr float kSleeperThick = 0.14f;  // 枕木の厚み（この上の面が線路の上の面）
    constexpr float kSleeperWidth = 0.45f;
    constexpr float kRailGauge    = 0.7f;   // 線路の真ん中から左右のレールまで
    constexpr float kRailWidth    = 0.3f;
    constexpr float kRailThick    = 0.06f;
    constexpr float kRailStep     = 0.5f;   // レールを曲線に沿わせる刻み(m)
    const Vector3   kRailColor { 0.62f, 0.62f, 0.66f };
    // 線路の上の面（＝車輪の下）は、足場（屋根の上の面）からこれだけ下
    constexpr float kRoofTop = ToyTrain::kCoachRoofTop;
    // けむり（ワールドマップと同じ大きさ）
    constexpr float kSmokeRun  = 0.2f;   // 走っている間の間(秒)
    constexpr float kSmokeIdle = 0.7f;   // 止まっている間
    constexpr float kSmokeSize = 1.5f;
    constexpr float kSmokeRise = 2.2f;
    constexpr float kSmokeLife = 1.2f;
    // 渡し板
    constexpr const char* kBoardSe = "resources/kamishibai/se_patan.wav"; // 倒れた時の「ぱたん」
    constexpr float kBoardWidth  = 1.8f;   // 板の幅（道の幅）
    constexpr float kBoardLand   = 0.364f; // 開き具合がここを越えた時に、板が初めて地面に着く（EaseOutBounce の最初の着地 1/2.75）
    constexpr const char* kRollSe    = "resources/kamishibai/se_pon.wav";   // 卵が巻いたマットに当たった「ぽん」
    constexpr const char* kBoardModel = "board_grass"; // 草の板 1m（tools/train/make_board.py。原点＝はしの上の面）
    constexpr const char* kRollModel  = "board_roll";  // 巻いた草のマット（半径 1・幅 1.8・軸 X）
    const Vector3   kBoardHinge { 0.22f, 0.2f, 0.22f };
    const Vector3   kArrowColor { 0.98f, 0.82f, 0.2f };

    Matrix4x4 Place(const Vector3& scale, const Vector3& rotate, const Vector3& translate){
        return MatrixMath::MakeAffine(scale, rotate, translate);
    }

    // ガイドの距離 d の点（区間の外は、はしの向きのまままっすぐ延ばす）
    Vector3 GuidePoint(const SplineRail& guide, float d){
        const float len = guide.GetLength();
        const float clamped = std::clamp(d, 0.0f, len);
        Vector3 p = guide.GetPositionByDistance(clamped);
        const float over = d - clamped;
        if ( over != 0.0f ) {
            const Vector3 t = guide.GetTangentByDistance(clamped);
            p = { p.x + t.x * over, p.y + t.y * over, p.z + t.z * over };
        }
        return p;
    }

    float YawOf(const Vector3& dir){ return std::atan2(dir.x, dir.z); }
    float PitchOf(const Vector3& dir){ return -std::atan2(dir.y, std::sqrt(dir.x * dir.x + dir.z * dir.z)); }
}

TrainVisuals::TrainVisuals() = default;
TrainVisuals::~TrainVisuals() = default;

void TrainVisuals::Push(const std::string& model, const Matrix4x4& world, bool isStatic){
    Batch& batch = batches_[model];
    if ( !isStatic ) { batch.dynamics.push_back(world); return; }
    Obj3d::TransformationMatrix matrix {};
    matrix.World = world;
    matrix.WorldInverseTranspose = MatrixMath::Transpose(MatrixMath::Inverse(world));
    batch.statics.push_back(matrix);
}

void TrainVisuals::Build(const std::vector<SplineRail>& rails, uint32_t noiseSrv){
    noiseSrv_ = noiseSrv;
    if ( !smokeReady_ ) {
        smoke_.Initialize(noiseSrv);
        AudioManager::GetInstance()->LoadWave(kBoardSe); // 渡し板の「ぱたん」
        AudioManager::GetInstance()->LoadWave(kRollSe);  // ころころに卵が当たった「ぽん」
        smokeReady_ = true;
    }
    for ( auto& [name, batch] : batches_ ) { batch.statics.clear(); batch.dynamics.clear(); }
    trains_.clear();
    boards_.clear();
    for ( int i = 0; i < ( int ) rails.size(); ++i ) {
        if ( rails[i].Train().isTrack ) { AddTrack(rails[i]); }
        if ( rails[i].IsTrain() ) { TrainState state; state.rail = i; trains_.push_back(state); }
        if ( rails[i].IsBoard() ) { BoardState state; state.rail = i; state.lastOpen = rails[i].Board().boardOpen; boards_.push_back(state); }
    }
    // 列車・渡し板の部品の数を数えて、入れ物の大きさを決める（1回置いてみる。矢印も出した形で）
    for ( TrainState& state : trains_ ) { AddTrain(rails, state); }
    for ( const BoardState& state : boards_ ) { BoardState counting = state; AddBoard(rails, counting, nullptr, 0.0f, -1, true); }
    for ( auto& [name, batch] : batches_ ) {
        const uint32_t needed = static_cast< uint32_t >( batch.statics.size() + batch.dynamics.size() );
        batch.dynamics.clear();
        if ( needed == 0 ) { continue; }
        if ( !batch.group || batch.capacity < needed ) {
            batch.capacity = ( std::max )( needed + 16u, batch.capacity );
            batch.group = std::make_unique<InstancedGroup>();
            batch.group->Initialize(name, batch.capacity);
            batch.group->SetNoiseTexture(noiseSrv_);
        }
    }
}

// 線路：枕木と 2 本のレール（ガイドは動かないので、ここで1回だけ置く）
void TrainVisuals::AddTrack(const SplineRail& guide){
    const float len = guide.GetLength();
    if ( len <= 0.0f ) { return; }
    const float top = -kRoofTop; // ガイドの高さからの線路の上の面
    for ( float s = kSleeperStep * 0.5f; s < len; s += kSleeperStep ) {
        const Vector3 p = guide.GetPositionByDistance(s);
        const Vector3 t = guide.GetTangentByDistance(s);
        Push(kSleeperModel, Place({ kRailGauge * 2.0f + 0.9f, kSleeperThick, kSleeperWidth }, { PitchOf(t), YawOf(t), 0.0f },
                                  { p.x, p.y + top - kSleeperThick, p.z }), true);
    }
    // レール：刻みごとの左右の点をつなぐ（カーブの内側と外側で長さが違っても、すき間ができない）
    const std::string railModel = ToyTrain::FeltModel("box", kRailColor);
    const int count = ( std::max )( 1, static_cast< int >( std::ceil(len / kRailStep) ) );
    std::vector<Vector3> left, right;
    for ( int i = 0; i <= count; ++i ) {
        const float s = len * static_cast< float >( i ) / static_cast< float >( count );
        const Vector3 p = guide.GetPositionByDistance(s);
        const Vector3 t = guide.GetTangentByDistance(s);
        const float h = ( std::max )( std::sqrt(t.x * t.x + t.z * t.z), 1e-4f );
        const Vector3 side { t.z / h, 0.0f, -t.x / h }; // 進む向きの右（エンジンの RotateY と同じ回り）
        const float y = p.y + top + kRailThick * 0.5f;
        left.push_back({ p.x - side.x * kRailGauge, y, p.z - side.z * kRailGauge });
        right.push_back({ p.x + side.x * kRailGauge, y, p.z + side.z * kRailGauge });
    }
    for ( const std::vector<Vector3>* line : { &left, &right } ) {
        for ( int i = 0; i < count; ++i ) {
            const Vector3& a = ( *line )[i];
            const Vector3& b = ( *line )[i + 1];
            const Vector3 d { b.x - a.x, b.y - a.y, b.z - a.z };
            const float segment = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if ( segment < 1e-4f ) { continue; }
            const Vector3 mid { ( a.x + b.x ) * 0.5f, ( a.y + b.y ) * 0.5f, ( a.z + b.z ) * 0.5f };
            Push(railModel, Place({ kRailWidth, kRailThick, segment + 0.02f }, { PitchOf(d), YawOf(d), 0.0f }, mid), true);
        }
    }
}

// 列車1本：客車（足場の長さに並べる）と、機関車
void TrainVisuals::AddTrain(const std::vector<SplineRail>& rails, TrainState& state){
    const SplineRail& rail = rails[state.rail];
    const float length = rail.GetLength();
    if ( length <= 0.0f ) { return; }
    // 車両の部品を置く（部品の高さは線路の上の面から。足場＝屋根の上の面なので kRoofTop だけ下げる。stretch＝前後の伸び縮み）
    auto addCar = [&](const std::vector<ToyTrain::Part>& parts, const Matrix4x4& frame, float stretch){
        for ( const ToyTrain::Part& part : parts ) {
            Vector3 scale = ToyTrain::ModelScale(part);
            const bool wheel = std::string(part.shape) == "ball" && part.offset.y < 0.6f; // 車輪は伸ばさない
            if ( !wheel ) { scale.z *= stretch; }
            const Vector3 offset { part.offset.x, part.offset.y - kRoofTop, part.offset.z * stretch };
            Push(ToyTrain::FeltModel(part.shape, part.color),
                 MatrixMath::Multiply(Place(scale, { part.pitch, 0.0f, 0.0f }, offset), frame), false);
        }
    };

    // 客車：足場の長さに合う両数。1両の長さを伸び縮みさせて、ちょうど足場いっぱいに並べる
    const Vector3 center { rail.Motion().animPivot.x + rail.Motion().animOffset.x, rail.Motion().animPivot.y + rail.Motion().animOffset.y, rail.Motion().animPivot.z + rail.Motion().animOffset.z };
    const float yaw = rail.Motion().restYaw + rail.Motion().animYaw;
    const Matrix4x4 car = Place({ 1.0f, 1.0f, 1.0f }, { 0.0f, yaw, 0.0f }, center);
    const float gap = ToyTrain::kCoachGap;
    const int coaches = ( std::max )( 1, static_cast< int >( std::round(( length + gap ) / ( ToyTrain::kCoachLength + gap )) ) );
    const float coachLength = ( length - gap * static_cast< float >( coaches - 1 ) ) / static_cast< float >( coaches );
    const float stretch = coachLength / ToyTrain::kCoachLength;
    for ( int i = 0; i < coaches; ++i ) {
        const float z = -length * 0.5f + coachLength * 0.5f + static_cast< float >( i ) * ( coachLength + gap );
        addCar(ToyTrain::Coach(i), MatrixMath::Multiply(Place({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, z }), car), stretch);
    }

    // 機関車：ガイドの上、向こうの駅の側（前＝足場から離れる向き）
    state.hasChimney = false;
    if ( !rail.Train().trainEngine || rail.Motion().guideRail < 0 || rail.Motion().guideRail >= ( int ) rails.size() ) { return; }
    const SplineRail& guide = rails[rail.Motion().guideRail];
    const RailMotion::TrainLayout layout = RailMotion::MakeTrainLayout(rail, rails);
    if ( !layout.valid ) { return; }
    const float engineDist = rail.Train().trainDist + layout.engineSign * ( length * 0.5f + RailMotion::kTrainCoupling + RailMotion::kTrainEngineBack );
    const Vector3 enginePos = GuidePoint(guide, engineDist);
    const Vector3 tangent = guide.GetTangentByDistance(std::clamp(engineDist, 0.0f, guide.GetLength()));
    const float engineYaw = YawOf({ tangent.x * layout.engineSign, 0.0f, tangent.z * layout.engineSign });
    const Matrix4x4 engine = Place({ 1.0f, 1.0f, 1.0f }, { 0.0f, engineYaw, 0.0f }, enginePos);
    addCar(ToyTrain::Engine(), engine, 1.0f);
    const Vector3 chimney { ToyTrain::kChimneyTop.x, ToyTrain::kChimneyTop.y - kRoofTop, ToyTrain::kChimneyTop.z };
    state.chimney = MatrixMath::Transforms(chimney, engine);
    state.hasChimney = true;
}

// 草の板を hinge から along の向きへ length だけ並べる（1m ずつ。最後は visible で切る）
void TrainVisuals::AddBoardPlanks(const Matrix4x4& frame, float length, float visible){
    const int count = ( std::max )( 1, static_cast< int >( std::round(length) ) );
    const float piece = length / static_cast< float >( count );
    for ( int i = 0; i < count; ++i ) {
        const float start = piece * static_cast< float >( i );
        if ( start >= visible ) { break; }
        const float size = ( std::min )( piece, visible - start );
        Push(kBoardModel, MatrixMath::Multiply(Place({ 1.0f, 1.0f, size }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, start }), frame), false);
    }
}

// 渡し板。ぱたん：駅の側（ちょうつがい）を軸に、起き上がった所（閉）から倒れた所（開）まで。倒れる時はぽよんと弾む。
//   ころころ：巻いた草のマットが転がりながら、後ろに草の板が伸びる
void TrainVisuals::AddBoard(const std::vector<SplineRail>& rails, BoardState& state, const Camera* camera, float deltaTime, int playerRail, bool countAll){
    const SplineRail& rail = rails[state.rail];
    if ( rail.Nodes().size() < 2 ) { return; }
    const float open = countAll ? 1.0f : std::clamp(rail.Board().boardOpen, 0.0f, 1.0f);
    const bool events = !countAll && deltaTime > 0.0f;

    if ( rail.IsRollBoard() ) {
        RailMotion::RollPose pose;
        if ( !RailMotion::GetRollPose(rail, pose) ) { return; }
        const float yaw = YawOf(pose.along);
        AddBoardPlanks(Place({ 1.0f, 1.0f, 1.0f }, { 0.0f, yaw, 0.0f }, pose.hinge), pose.length, countAll ? pose.length : pose.rolled);
        // 巻いたマット：転がった距離だけ回る（広がりきったら消える。数える時は出しておく）
        if ( countAll || open < 1.0f ) {
            const float spin = pose.rolled / 0.33f;
            Push(kRollModel, Place({ 1.0f, pose.radius, pose.radius }, { spin, yaw, 0.0f }, pose.center), false);
        }
        if ( events && state.lastOpen <= 0.0f && open > 0.0f ) { // 卵が当たった：ぽん
            AudioManager::GetInstance()->PlayWave(kRollSe, false, 0.6f);
            if ( smokeReady_ ) { smoke_.PuffRing(pose.center, 6, 1.0f, 0.4f, PaperFx::PuffColor::White); }
        }
        if ( events && state.lastOpen < 1.0f && open >= 1.0f ) { // 広がりきった：ぱたん
            AudioManager::GetInstance()->PlayWave(kBoardSe, false, 0.6f);
            const Vector3 tip { pose.hinge.x + pose.along.x * pose.length, pose.hinge.y, pose.hinge.z + pose.along.z * pose.length };
            if ( smokeReady_ ) { smoke_.PuffRing(tip, 6, 0.9f, 0.35f, PaperFx::PuffColor::Kraft); }
        }
        state.lastOpen = open;
        return;
    }

    const Vector3 hinge = rail.Board().boardHingeFront ? rail.Nodes().front() : rail.Nodes().back();
    const Vector3 tip = rail.Board().boardHingeFront ? rail.Nodes().back() : rail.Nodes().front();
    const Vector3 along { tip.x - hinge.x, 0.0f, tip.z - hinge.z };
    const float length = std::sqrt(along.x * along.x + along.z * along.z);
    if ( length < 0.05f ) { return; }
    const float lift = ( Math::kPi * 0.5f ) * ( 1.0f - Easing::EaseOutBounce(open) ); // 0＝倒れている / π/2＝起き上がっている
    const Matrix4x4 frame = Place({ 1.0f, 1.0f, 1.0f }, { -lift, YawOf(along), 0.0f }, hinge);
    AddBoardPlanks(frame, length, length);
    Push(ToyTrain::FeltModel("tube", kBoardHinge), MatrixMath::Multiply(Place({ 0.16f, kBoardWidth - 0.1f, 0.16f }, { 0.0f, 0.0f, Math::kPi * 0.5f }, { 0.0f, -0.08f, 0.0f }), frame), false); // ちょうつがい

    // 倒れて地面に着いた瞬間：ぱたん・土ぼこり
    if ( events && state.lastOpen < kBoardLand && open >= kBoardLand ) {
        AudioManager::GetInstance()->PlayWave(kBoardSe, false, 0.6f);
        if ( smokeReady_ ) { smoke_.PuffRing({ tip.x, tip.y, tip.z }, 6, 0.9f, 0.35f, PaperFx::PuffColor::Kraft); }
    }
    state.lastOpen = open;

    // 開いている間：板の先の上に、黄色い矢印（下向き）が弾む（その列車・板に乗っている間は出さない）
    const bool riding = playerRail == state.rail || playerRail == rail.Board().boardTrain;
    if ( !countAll && ( open < 0.99f || riding ) ) { state.time = 0.0f; return; }
    state.time += deltaTime;
    const float bob = 0.12f * std::abs(std::sin(state.time * 4.0f));
    const Vector3 at { hinge.x + along.x * 0.7f, hinge.y + 1.9f + bob, hinge.z + along.z * 0.7f };
    const Vector3 eye = camera ? camera->GetWorldPosition() : Vector3 { at.x, at.y, at.z - 1.0f };
    const Matrix4x4 arrow = Place({ 1.0f, 1.0f, 1.0f }, { 0.0f, YawOf({ eye.x - at.x, 0.0f, eye.z - at.z }), 0.0f }, at);
    auto arrowPart = [&](const Vector3& size, const Vector3& offset, float roll){
        Push(ToyTrain::FeltModel("box", kArrowColor), MatrixMath::Multiply(Place(size, { 0.0f, 0.0f, roll }, offset), arrow), false);
    };
    arrowPart({ 0.16f, 0.42f, 0.16f }, { 0.0f, 0.42f, 0.0f }, 0.0f);                    // 軸
    arrowPart({ 0.15f, 0.42f, 0.15f }, { -0.13f, 0.14f, 0.0f }, Math::kPi * 0.25f);     // 先（V の左）
    arrowPart({ 0.15f, 0.42f, 0.15f }, { 0.13f, 0.14f, 0.0f }, -Math::kPi * 0.25f);     // 先（V の右）
}

void TrainVisuals::Update(const std::vector<SplineRail>& rails, const Camera* camera, float deltaTime, int playerRail){
    if ( !camera ) { return; }
    for ( auto& [name, batch] : batches_ ) { batch.dynamics.clear(); }
    for ( BoardState& state : boards_ ) {
        if ( state.rail < 0 || state.rail >= ( int ) rails.size() || !rails[state.rail].IsBoard() ) { continue; }
        AddBoard(rails, state, camera, deltaTime, playerRail);
    }
    for ( TrainState& state : trains_ ) {
        if ( state.rail < 0 || state.rail >= ( int ) rails.size() || !rails[state.rail].IsTrain() ) { continue; }
        AddTrain(rails, state);
        // 煙突から綿のけむり（走っている間はぽこぽこ、止まっている間はゆっくり）
        state.smokeTimer -= deltaTime;
        if ( state.hasChimney && state.smokeTimer <= 0.0f ) {
            state.smokeTimer = rails[state.rail].IsTrainRunning() ? kSmokeRun : kSmokeIdle;
            smoke_.Puff(state.chimney, { 0.0f, kSmokeRise, 0.0f }, kSmokeSize, kSmokeLife, PaperFx::PuffColor::White);
        }
    }
    if ( smokeReady_ ) { smoke_.Update(deltaTime, camera); }
    const Matrix4x4& viewProjection = camera->GetViewProjectionMatrix();
    for ( auto& [name, batch] : batches_ ) {
        if ( !batch.group ) { continue; }
        const size_t count = ( std::min )( batch.statics.size() + batch.dynamics.size(), static_cast< size_t >( batch.capacity ) );
        batch.matrices.resize(count);
        for ( size_t k = 0; k < count; ++k ) {
            Obj3d::TransformationMatrix& matrix = batch.matrices[k];
            if ( k < batch.statics.size() ) {
                matrix = batch.statics[k];
            } else {
                matrix.World = batch.dynamics[k - batch.statics.size()];
                matrix.WorldInverseTranspose = MatrixMath::Transpose(MatrixMath::Inverse(matrix.World));
            }
            matrix.WVP = MatrixMath::Multiply(matrix.World, viewProjection);
        }
        batch.group->Update(batch.matrices);
    }
}

void TrainVisuals::Draw(const Camera* camera){
    for ( auto& [name, batch] : batches_ ) {
        if ( batch.group && !batch.matrices.empty() ) { batch.group->Draw(camera); }
    }
    if ( smokeReady_ ) { smoke_.Draw(camera); }
}
