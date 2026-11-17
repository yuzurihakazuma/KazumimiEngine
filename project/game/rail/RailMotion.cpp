#include "game/rail/RailMotion.h"
#include "engine/math/MathUtil.h"
#include "engine/rail/SplineRail.h"

#include <algorithm>
#include <cmath>

namespace {
    const float kPi    = Math::kPi;
    const float kTwoPi = 2.0f * kPi;

    Vector3 Scaled(const Vector3& amp, float w){ return { amp.x * w, amp.y * w, amp.z * w }; }

    // 後から出現する道：発動レールに乗ったら出現（以後は下からせり上がるアニメで現れる）
    void UpdateAppear(SplineRail& rail, float dt, int ridingRail){
        if ( rail.appearTrigger < 0 ) return;
        if ( !rail.appeared && ( ridingRail == rail.appearTrigger || ridingRail == RailMotion::kStartAll ) ) {
            rail.appeared = true;
        }
        if ( rail.appeared && rail.appearAnim < 1.0f ) {
            rail.appearAnim = ( std::min )( 1.0f, rail.appearAnim + dt / 0.6f );
        }
    }

    // 1: 端で一時停止つき往復（各端で周期の15%停止。移動はコサインで滑らか）
    float DwellSwing(float u){
        const float dwell = 0.15f;
        const float move  = 0.5f - dwell;
        if ( u < move )        { return -std::cos(( u / move ) * kPi); }           // -1 → +1
        if ( u < 0.5f )        { return 1.0f; }                                     // +端で停止
        if ( u < 0.5f + move ) { return  std::cos(( ( u - 0.5f ) / move ) * kPi); } // +1 → -1
        return -1.0f;                                                               // -端で停止
    }

    // 3: ガイドレール追従：別レールの経路に沿って動く（ヨッシー1-1の列車式）。
    //   周期=1周期の秒数（一周ループ=1周 / 往復=行って帰る1往復）/ 位相=スタート位置 /
    //   区間=ガイドのここからここまで。
    //   guideMode 0=一周ループ（終点→始点へ戻る） / 1=往復（コサイン緩急で滑らかに折り返す） / 2=片道
    void FollowGuide(SplineRail& rail, const std::vector<SplineRail>& rails, float period, float u){
        int g = rail.guideRail;
        if ( g < 0 || g >= ( int ) rails.size() || &rails[g] == &rail || rails[g].GetLength() <= 0.0f ) {
            rail.animOffset = { 0.0f, 0.0f, 0.0f };
            return;
        }
        const SplineRail& guide = rails[g];
        const float guideLen = guide.GetLength();
        float s0 = std::clamp(rail.guideStart, 0.0f, guideLen);
        float s1 = ( rail.guideEnd < 0.0f ) ? guideLen : std::clamp(rail.guideEnd, 0.0f, guideLen);
        if ( s1 < s0 ) { std::swap(s0, s1); }
        if ( s1 - s0 < 0.01f ) { s0 = 0.0f; s1 = guideLen; } // 区間が潰れていたら全区間

        const float dwell = ( std::max )( rail.guideDwell, 0.0f );
        float w;
        if ( rail.guideMode == 1 ) {
            // 往復：行き→(停車)→帰り→(停車)。移動はコサイン緩急＝両端で速度0。
            //   周期=1往復の移動秒数（停車時間は別枠で足される）
            float half = period * 0.5f;
            float cycle = period + dwell * 2.0f;
            float t = std::fmod(rail.motionTime + rail.motionPhase * cycle, cycle);
            if      ( t < half )           { w = 0.5f - 0.5f * std::cos(( t / half ) * kPi); }                  // 行き 0→1
            else if ( t < half + dwell )   { w = 1.0f; }                                                        // 終点で停車
            else if ( t < period + dwell ) { w = 0.5f + 0.5f * std::cos(( ( t - half - dwell ) / half ) * kPi); } // 帰り 1→0
            else                           { w = 0.0f; }                                                        // 始点で停車
        } else if ( rail.guideMode == 2 ) {
            // 片道：出発まで dwell 秒待ち→走行→到着したら止まる（出発・到着ともコサイン緩急）。
            //   周期=片道の秒数。u は周回でラップしているため経過時間から直接進行度を求める
            float progress = std::clamp(( rail.motionTime - dwell ) / period + rail.motionPhase, 0.0f, 1.0f);
            w = 0.5f - 0.5f * std::cos(progress * kPi);
        } else {
            // 一周ループ：区間の端まで行くと始点へ戻って回り続ける（閉じたガイド向け）
            w = u;
        }
        const float d = s0 + ( s1 - s0 ) * w;
        Vector3 p  = guide.GetPositionByDistance(d);
        Vector3 p0 = guide.GetPositionByDistance(s0);
        rail.animOffset = { p.x - p0.x, p.y - p0.y, p.z - p0.z };

        // 列車式：ガイドの進行方向に足場の向きを合わせて転回（縦向き⇔横向きも滑らか）。
        //   ほぼ真上/真下の区間では向きを保つ（水平成分が無いとヨーが定まらないため）
        if ( rail.guideAlign != 1 ) return;
        Vector3 guideTan = guide.GetTangentByDistance(d);
        float horizLen = std::sqrt(guideTan.x * guideTan.x + guideTan.z * guideTan.z);
        if ( horizLen <= 0.15f ) return;
        float targetYaw = std::atan2(guideTan.x, guideTan.z) - rail.restYaw;
        // 最短角で連続に追従（±180°の境目でクルッと一回転しない）
        rail.animYaw += Math::AngleDelta(rail.animYaw, targetYaw);
    }
}

namespace {
    // 列車の区間（ガイドの guideStart〜guideEnd）。足りない・つぶれていたら全区間
    void TrainSection(const SplineRail& rail, const SplineRail& guide, float& s0, float& s1){
        const float guideLen = guide.GetLength();
        s0 = std::clamp(rail.guideStart, 0.0f, guideLen);
        s1 = ( rail.guideEnd < 0.0f ) ? guideLen : std::clamp(rail.guideEnd, 0.0f, guideLen);
        if ( s1 < s0 ) { std::swap(s0, s1); }
        if ( s1 - s0 < 0.01f ) { s0 = 0.0f; s1 = guideLen; }
    }

    const SplineRail* GuideOf(const SplineRail& rail, const std::vector<SplineRail>& rails){
        const int g = rail.guideRail;
        if ( g < 0 || g >= ( int ) rails.size() || &rails[g] == &rail || rails[g].GetLength() <= 0.0f ) { return nullptr; }
        return &rails[g];
    }

    // 足場の真ん中をガイドの距離 d に置き、向きをガイドに合わせる（ガイドの上に真ん中が乗る。高さはガイドの高さ＝足場の上の面）
    void PlaceTrain(SplineRail& rail, const SplineRail& guide, float d){
        rail.trainDist = d;
        const Vector3 p = guide.GetPositionByDistance(d);
        rail.animOffset = { p.x - rail.animPivot.x, p.y - rail.animPivot.y, p.z - rail.animPivot.z };
        const Vector3 tangent = guide.GetTangentByDistance(d);
        if ( std::sqrt(tangent.x * tangent.x + tangent.z * tangent.z) > 0.15f ) {
            rail.animYaw = Math::WrapAngle(std::atan2(tangent.x, tangent.z) + rail.trainFlip - rail.restYaw);
        }
    }

    // ガイドの距離 d での速さ（点ごとの倍率を、点と点の間でなめらかにつなぐ）
    float TrainSpeedAt(const SplineRail& rail, const SplineRail& guide, float d){
        const std::vector<float>& muls = rail.trainSpeedMuls;
        const int count = ( int ) guide.nodes.size();
        if ( muls.empty() || count < 2 ) { return rail.trainSpeed; }
        auto mulAt = [&](int k){ return ( k < ( int ) muls.size() && muls[k] > 0.01f ) ? muls[k] : 1.0f; };
        float prevD = guide.GetDistanceFromT(0.0f);
        if ( d <= prevD ) { return rail.trainSpeed * mulAt(0); }
        for ( int k = 1; k < count; ++k ) {
            const float nextD = guide.GetDistanceFromT(static_cast< float >( k ));
            if ( d <= nextD ) {
                const float t = nextD > prevD ? ( d - prevD ) / ( nextD - prevD ) : 0.0f;
                const float w = t * t * ( 3.0f - 2.0f * t ); // なめらかに
                return rail.trainSpeed * ( mulAt(k - 1) + ( mulAt(k) - mulAt(k - 1) ) * w );
            }
            prevD = nextD;
        }
        return rail.trainSpeed * mulAt(count - 1);
    }

    // 列車の1フレーム：駅で待つ → 乗ると発車 → 次の駅で止まる。だれも乗らないまま待つと元の駅へ戻る
    void AdvanceTrain(SplineRail& rail, const std::vector<SplineRail>& rails, float dt, bool riding, bool aboard, bool preview){
        const SplineRail* guide = GuideOf(rail, rails);
        const RailMotion::TrainLayout layout = RailMotion::MakeTrainLayout(rail, rails);
        if ( !guide || !layout.valid ) { rail.animOffset = { 0.0f, 0.0f, 0.0f }; return; }
        using State = SplineRail::TrainState;
        switch ( rail.trainState ) {
        case State::Wait:
            if ( rail.trainNeedsLeave && !aboard ) { rail.trainNeedsLeave = false; }
            if ( preview || ( riding && !rail.trainNeedsLeave ) ) {
                rail.trainState = State::Depart; // 乗った：少し待って発車
                rail.trainTimer = 0.0f;
            } else if ( !aboard && rail.trainAt != rail.trainHome && rail.trainReturn >= 0.0f ) {
                rail.trainTimer += dt; // だれも乗っていない：しばらく待ったら元の駅へ戻る（戻らないと、ミスしてやり直す時に乗れない）
                if ( rail.trainTimer >= rail.trainReturn ) {
                    rail.trainFrom = rail.trainAt;
                    rail.trainAt = rail.trainHome;
                    rail.trainProgress = 0.0f;
                    rail.trainSpeedNow = 0.0f;
                    rail.trainState = State::Run;
                }
            } else {
                rail.trainTimer = 0.0f;
            }
            break;
        case State::Depart:
            if ( !preview && !aboard ) { rail.trainState = State::Wait; rail.trainTimer = 0.0f; break; } // 発車の前に降りた
            rail.trainTimer += dt;
            if ( rail.trainTimer >= ( std::max )( rail.trainDepart, 0.0f ) ) {
                rail.trainFrom = rail.trainAt;
                rail.trainAt = 1 - rail.trainAt;
                rail.trainProgress = 0.0f;
                rail.trainSpeedNow = 0.0f;
                rail.trainState = State::Run;
            }
            break;
        case State::Run: {
            // 速さ（m/秒。場所ごとの倍率つき）で進む。出発はなめらかに加速し、駅の手前ではなめらかに減速して止まる
            constexpr float kAccel = 1.5f;     // 加速・減速(m/秒²)
            constexpr float kMinSpeed = 0.3f;  // 駅の手前で止まりきらないように
            const float from = layout.station[rail.trainFrom];
            const float to = layout.station[rail.trainAt];
            const float dir = ( to >= from ) ? 1.0f : -1.0f;
            const float remaining = ( to - rail.trainDist ) * dir;
            const float brake = std::sqrt(2.0f * kAccel * ( std::max )( remaining, 0.0f ));
            const float wanted = ( std::min )( TrainSpeedAt(rail, *guide, rail.trainDist), brake );
            if ( wanted > rail.trainSpeedNow ) { rail.trainSpeedNow = ( std::min )( wanted, rail.trainSpeedNow + kAccel * dt ); }
            else                               { rail.trainSpeedNow = ( std::max )( wanted, rail.trainSpeedNow - kAccel * 2.0f * dt ); }
            if ( brake < rail.trainSpeedNow ) { rail.trainSpeedNow = brake; } // 駅に止まるのは必ず守る
            const float step = ( std::max )( rail.trainSpeedNow, kMinSpeed ) * dt;
            const float total = std::abs(to - from);
            if ( remaining <= step || total < 1e-4f ) {
                rail.trainDist = to;
                rail.trainProgress = 1.0f;
                rail.trainSpeedNow = 0.0f;
                rail.trainState = State::Wait;
                rail.trainTimer = 0.0f;
                rail.trainNeedsLeave = aboard && !preview; // 乗ったまま着いた：降りて乗り直すまで発車しない
            } else {
                rail.trainDist += dir * step;
                rail.trainProgress = 1.0f - ( remaining - step ) / total;
            }
            break;
        }
        }
        const float d = ( rail.trainState == State::Run ) ? rail.trainDist : layout.station[rail.trainAt];
        PlaceTrain(rail, *guide, d);
    }
}

RailMotion::TrainLayout RailMotion::MakeTrainLayout(const SplineRail& train, const std::vector<SplineRail>& rails){
    TrainLayout layout;
    const SplineRail* guide = GuideOf(train, rails);
    if ( !guide ) { return layout; }
    float s0 = 0.0f, s1 = 0.0f;
    TrainSection(train, *guide, s0, s1);
    // 列車の長さ：足場の真ん中から、機関車の無い側は足場の半分・ある側は機関車のはしまで
    const float half = train.GetLength() * 0.5f;
    const float front = half + ( train.trainEngine ? kTrainCoupling + kTrainEngineLength : 0.0f );
    layout.engineSign = ( train.trainHome == 0 ) ? 1.0f : -1.0f; // 機関車は、元の駅から向こうの駅の側
    const float back0 = layout.engineSign > 0.0f ? half : front;   // はじめ側の駅で、はじめ側へはみ出す長さ
    const float back1 = layout.engineSign > 0.0f ? front : half;   // おわり側の駅で、おわり側へはみ出す長さ
    layout.station[0] = s0 + back0;
    layout.station[1] = s1 - back1;
    if ( layout.station[1] < layout.station[0] ) { // 区間が列車より短い：真ん中に止まったまま
        layout.station[0] = layout.station[1] = ( s0 + back0 + s1 - back1 ) * 0.5f;
    }
    layout.valid = true;
    return layout;
}

void RailMotion::SetupTrain(SplineRail& train, const std::vector<SplineRail>& rails){
    const SplineRail* guide = GuideOf(train, rails);
    if ( !train.IsTrain() || !guide ) { return; }
    // 元の駅：足場を置いた所に近い方の区間のはし
    float s0 = 0.0f, s1 = 0.0f;
    TrainSection(train, *guide, s0, s1);
    const float placed = guide->GetClosestDistance(train.animPivot);
    train.trainHome = ( std::abs(placed - s0) <= std::abs(placed - s1) ) ? 0 : 1;
    // 向き：足場の向きとガイドの向きが逆なら、足場を逆向きのまま走らせる
    const Vector3 tangent = guide->GetTangentByDistance(placed);
    const float rest[2] = { std::sin(train.restYaw), std::cos(train.restYaw) };
    train.trainFlip = ( rest[0] * tangent.x + rest[1] * tangent.z < 0.0f ) ? kPi : 0.0f;
}

void RailMotion::SetupBoard(SplineRail& board, const std::vector<SplineRail>& rails){
    if ( board.boardKind != 1 ) { return; } // ころころは SetupRollBoard（つながりが決まってから）
    if ( board.boardTrain < 0 || board.boardTrain >= ( int ) rails.size() || board.nodes.size() < 2 ) { board.boardKind = 0; return; }
    const SplineRail& train = rails[board.boardTrain];
    const SplineRail* guide = GuideOf(train, rails);
    if ( !train.IsTrain() || !guide ) { board.boardKind = 0; return; }
    // 線路に遠い方のはしが、ちょうつがい（駅の側）。近い方のはしの所の駅の板
    auto offTrack = [&](const Vector3& p, float& outDist){
        outDist = guide->GetClosestDistance(p);
        const Vector3 q = guide->GetPositionByDistance(outDist);
        return std::sqrt(( p.x - q.x ) * ( p.x - q.x ) + ( p.z - q.z ) * ( p.z - q.z ));
    };
    float frontDist = 0.0f, backDist = 0.0f;
    const float front = offTrack(board.nodes.front(), frontDist);
    const float back = offTrack(board.nodes.back(), backDist);
    board.boardHingeFront = ( front >= back );
    const float trackDist = board.boardHingeFront ? backDist : frontDist;
    const TrainLayout layout = MakeTrainLayout(train, rails);
    board.boardStation = ( std::abs(trackDist - layout.station[0]) <= std::abs(trackDist - layout.station[1]) ) ? 0 : 1;
}

void RailMotion::SetupRollBoard(SplineRail& board){
    if ( !board.IsRollBoard() ) { return; }
    if ( board.nodes.size() < 2 ) { board.boardKind = 0; return; }
    // ほかの道とつながっているはしから転がり出す（両方・どちらもつながっていなければ始点から）
    board.boardHingeFront = !( board.frontConnIndex < 0 && board.backConnIndex >= 0 );
}

bool RailMotion::GetRollPose(const SplineRail& board, RollPose& out){
    if ( !board.IsRollBoard() || board.nodes.size() < 2 ) { return false; }
    const Vector3 hinge = board.boardHingeFront ? board.nodes.front() : board.nodes.back();
    const Vector3 tip = board.boardHingeFront ? board.nodes.back() : board.nodes.front();
    const float dx = tip.x - hinge.x, dz = tip.z - hinge.z;
    out.length = std::sqrt(dx * dx + dz * dz);
    if ( out.length < 0.05f ) { return false; }
    out.hinge = hinge;
    out.along = { dx / out.length, 0.0f, dz / out.length };
    const float open = std::clamp(board.boardOpen, 0.0f, 1.0f);
    // 巻いたマットは広がるほど細くなる（半径 0.45 → 0.22）。転がりながら、後ろにマットが残る
    constexpr float kRollRadius = 0.45f, kRollRadiusEnd = 0.22f, kBoardThick = 0.12f;
    out.radius = kRollRadius + ( kRollRadiusEnd - kRollRadius ) * open;
    out.rolled = out.length * open;
    out.center = { hinge.x + out.along.x * out.rolled, hinge.y - kBoardThick + out.radius, hinge.z + out.along.z * out.rolled };
    return true;
}

bool RailMotion::BoardShouldOpen(const SplineRail& board, const std::vector<SplineRail>& rails){
    if ( board.boardKind != 1 || board.boardTrain < 0 || board.boardTrain >= ( int ) rails.size() ) { return false; }
    const SplineRail& train = rails[board.boardTrain];
    return train.IsTrain() && train.trainState == SplineRail::TrainState::Wait && train.trainAt == board.boardStation;
}

bool RailMotion::Advance(std::vector<SplineRail>& rails, float dt, int ridingRail, int playerRail){
    bool anyMotion = false;
    if ( playerRail < 0 ) { playerRail = ridingRail; }
    // 渡し板：列車がその駅に止まっている間だけ開く（発車を待つ間に閉じる）。上に乗っている間は閉じない
    for ( size_t railIndex = 0; railIndex < rails.size(); ++railIndex ) {
        SplineRail& board = rails[railIndex];
        if ( !board.IsBoard() ) { continue; }
        if ( board.IsRollBoard() ) {
            // ころころ：卵が当たったら、転がって広がる（3.5m/秒・短くても 0.5 秒）。広がったらそのまま。
            //   エディタのプレビューでは当てなくても広がる（どう広がるかを見る）
            if ( ridingRail == kStartAll ) { board.boardTriggered = true; }
            if ( board.boardTriggered ) {
                RollPose pose;
                const float time = GetRollPose(board, pose) ? ( std::max )( 0.5f, pose.length / 3.5f ) : 0.5f;
                board.boardOpen = ( std::min )( 1.0f, board.boardOpen + dt / time );
            }
            continue;
        }
        const bool keep = ( playerRail == ( int ) railIndex ) && board.boardOpen > 0.0f;
        const bool open = keep || BoardShouldOpen(board, rails);
        constexpr float kOpenTime = 0.45f, kCloseTime = 0.3f;
        board.boardOpen = open ? ( std::min )( 1.0f, board.boardOpen + dt / kOpenTime )
                               : ( std::max )( 0.0f, board.boardOpen - dt / kCloseTime );
    }
    for ( size_t railIndex = 0; railIndex < rails.size(); ++railIndex ) {
        SplineRail& rail = rails[railIndex];
        UpdateAppear(rail, dt, ridingRail);
        if ( !rail.HasMotion() ) continue;
        anyMotion = true;
        if ( rail.IsTrain() ) {
            const bool preview = ( ridingRail == kStartAll ); // エディタのプレビュー：乗っている扱いで、駅と駅を行き来する
            AdvanceTrain(rail, rails, dt, ridingRail == ( int ) railIndex, playerRail == ( int ) railIndex, preview);
            continue;
        }
        // 乗ったら動き出す：発動するまで待機（animOffsetは基準位置のまま）。
        //   一度発動したら降りても動き続ける（ヨッシーの列車と同じ）。
        //   エディタのプレビュー（kStartAll）では全レール強制発動＝動きを確認できる
        if ( rail.motionTrigger == 1 && !rail.motionStarted ) {
            if ( ridingRail != ( int ) railIndex && ridingRail != kStartAll ) continue;
            rail.motionStarted = true;
        }
        rail.motionTime += dt; // 発動してからの経過時間（最初から動くレールはPlay開始からの時間）
        const float period = ( rail.motionPeriod > 0.1f ) ? rail.motionPeriod : 0.1f;
        float u = rail.motionTime / period + rail.motionPhase;
        u -= std::floor(u); // 0〜1 の周期位置

        switch ( rail.motionType ) {
        case 1:  rail.animOffset = Scaled(rail.motionAmp, DwellSwing(u)); break;
        case 2: { // 円運動（XZ楕円＋Y。半径は amp の各成分。amp.x と amp.z で円になる）
            const float th = u * kTwoPi;
            const Vector3& amp = rail.motionAmp;
            rail.animOffset = { amp.x * std::cos(th), amp.y * std::sin(th), amp.z * std::sin(th) };
            break;
        }
        case 3:  FollowGuide(rail, rails, period, u); break;
        default: rail.animOffset = Scaled(rail.motionAmp, std::sin(u * kTwoPi)); break; // 0: サイン往復
        }
    }
    return anyMotion;
}

void RailMotion::Reset(std::vector<SplineRail>& rails, bool keepRolls){
    for ( auto& rail : rails ) {
        rail.animOffset = { 0.0f, 0.0f, 0.0f };
        rail.motionTime = 0.0f;
        rail.motionStarted = false;
        rail.appeared   = ( rail.appearTrigger < 0 );
        rail.appearAnim = rail.appeared ? 1.0f : 0.0f;
        rail.animYaw = 0.0f; // 列車式の回転も基準向きへ戻す
        // 列車：元の駅で待つ
        rail.trainState = SplineRail::TrainState::Wait;
        rail.trainAt = rail.trainFrom = rail.trainHome;
        rail.trainTimer = rail.trainProgress = rail.trainSpeedNow = 0.0f;
        rail.trainNeedsLeave = false;
    }
    for ( auto& rail : rails ) {
        if ( !rail.IsTrain() ) { continue; }
        const SplineRail* guide = GuideOf(rail, rails);
        const TrainLayout layout = MakeTrainLayout(rail, rails);
        if ( guide && layout.valid ) { PlaceTrain(rail, *guide, layout.station[rail.trainHome]); }
    }
    // 渡し板：列車が止まっている駅の板は開いて、ほかは閉じて始める。ころころは巻き直す（落ちて戻った時はそのまま）
    for ( auto& rail : rails ) {
        if ( rail.IsRollBoard() ) {
            if ( !keepRolls ) { rail.boardOpen = 0.0f; rail.boardTriggered = false; }
        } else if ( rail.IsBoard() ) {
            rail.boardOpen = BoardShouldOpen(rail, rails) ? 1.0f : 0.0f;
        }
    }
}
