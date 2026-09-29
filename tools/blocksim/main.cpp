// ブロック引っかかり調査用のシミュレーション。
//   ゲーム本体の Player / BlockSystem / SplineRail をそのまま使い、まっすぐなレールにブロックを置いて
//   キー入力を台本どおりに与え、毎フレームの位置・高さ・接地を記録する。
//   「右を押し続けているのに進まない」「高さが上下に振動する」フレームを引っかかりとして検出する
#define private public
#include "engine/base/TimeManager.h"
#undef private
#include "game/player/Player.h"
#include "game/stage/BlockSystem.h"
#include "engine/rail/SplineRail.h"
#include <dinput.h>
#include <cstdio>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

extern unsigned char g_keyNow[256];
extern unsigned char g_keyPrev[256];

struct Frame { int frame; float dist, footY; bool grounded; bool right, left, jump; };

struct Scenario {
    std::string name;
    std::vector<BlockData> blocks;
    float spawnDist = 2.0f;
    int   frames = 240;
    // 入力の台本：frame とその時の状態から、押すキーを決める
    std::function<void(int frame, const Frame& state, bool& right, bool& left, bool& jump)> script;
    std::vector<int> holes; // レールの穴（ノードごとのフラグ。ノードは5m間隔＝x=0,5,10...）
};

static BlockData B(float dist, int level, int type = 0){
    BlockData b; b.rail = 0; b.dist = dist; b.level = level; b.side = 0.0f; b.type = type; return b;
}

static std::vector<Frame> Run(const Scenario& sc, bool verbose){
    std::vector<SplineRail> rails(1);
    for ( int i = 0; i <= 8; ++i ) { rails[0].nodes.push_back({ ( float ) i * 5.0f, 0.0f, 0.0f }); }
    rails[0].type = SplineRail::RailType::Horizontal;
    rails[0].nodeHole = sc.holes;
    rails[0].BuildDistanceTable();

    BlockSystem blocks;
    blocks.Sync(sc.blocks, &rails);

    Player player;
    player.SetBlocks(&blocks);
    player.SetSpawn(0, sc.spawnDist);
    player.SetCameraYaw(0.0f);
    player.Initialize();
    Time::GetInstance()->deltaTime_ = 1.0f / 60.0f;

    std::vector<Frame> log;
    Frame state { 0, sc.spawnDist, 0.0f, true, false, false, false };
    for ( int f = 0; f < sc.frames; ++f ) {
        bool right = false, left = false, jump = false;
        sc.script(f, state, right, left, jump);
        std::memcpy(g_keyPrev, g_keyNow, 256);
        std::memset(g_keyNow, 0, 256);
        if ( right ) g_keyNow[DIK_D] = 1;
        if ( left )  g_keyNow[DIK_A] = 1;
        if ( jump )  g_keyNow[DIK_SPACE] = 1;
        player.Update(rails);
        state = { f, player.GetPosition().x, player.GetPosition().y, player.IsGrounded(), right, left, jump };
        log.push_back(state);
        if ( verbose ) {
            std::printf("  f%03d x=%7.3f y=%6.3f %s %s%s%s\n", f, state.dist, state.footY,
                state.grounded ? "G" : "-", right ? "R" : " ", left ? "L" : " ", jump ? "J" : " ");
        }
    }
    return log;
}

// 引っかかりの検出：左右キーを押しているのに6フレーム以上ほぼ進まない区間 / 高さが上下に振動する区間
static void Analyze(const Scenario& sc, const std::vector<Frame>& log){
    int stuckStart = -1;
    int reports = 0;
    for ( size_t i = 1; i < log.size(); ++i ) {
        const bool pressing = log[i].right || log[i].left;
        const bool noProgress = std::abs(log[i].dist - log[i - 1].dist) < 0.002f;
        if ( pressing && noProgress ) {
            if ( stuckStart < 0 ) stuckStart = ( int ) i;
        } else {
            if ( stuckStart >= 0 && ( int ) i - stuckStart >= 6 && reports < 6 ) {
                std::printf("  [止まった] f%03d-f%03d x=%.3f y=%.3f %s\n", stuckStart, ( int ) i - 1,
                    log[stuckStart].dist, log[stuckStart].footY, log[stuckStart].grounded ? "接地" : "空中");
                ++reports;
            }
            stuckStart = -1;
        }
    }
    if ( stuckStart >= 0 && ( int ) log.size() - stuckStart >= 6 ) {
        std::printf("  [止まった] f%03d-最後 x=%.3f y=%.3f %s\n", stuckStart, log[stuckStart].dist,
            log[stuckStart].footY, log[stuckStart].grounded ? "接地" : "空中");
    }
    // 高さの振動（上がって下がってを繰り返す）
    int bounces = 0;
    for ( size_t i = 2; i < log.size(); ++i ) {
        float a = log[i - 1].footY - log[i - 2].footY, b = log[i].footY - log[i - 1].footY;
        if ( a > 0.05f && b < -0.001f && !log[i].jump ) ++bounces;
    }
    if ( bounces >= 3 ) std::printf("  [高さが跳ね上がる] %d回（押し出し→落下の繰り返しの疑い）\n", bounces);
    std::printf("  最終: x=%.3f y=%.3f %s\n", log.back().dist, log.back().footY, log.back().grounded ? "接地" : "空中");
}

// ---------------------------------------------------------------------
//  埋まりの検出：フレームの終わりに体（足元〜高さ0.95m・半径0.3m）がブロックの中に入っているか。
//   ・中心が実寸の中     … |x-中心| < 半幅 かつ 縦に重なる（足が上面より2cm以上下）
//   ・壁の範囲まで深く … |x-中心| < 半幅+0.28 かつ 足が上面より0.31m以上下（段差の許容＝体の下端0.05＋0.25を超えた食い込み）
//   斜面・すり抜け床・横へずらした飾りは対象外
// ---------------------------------------------------------------------
struct Embed { int frame = -1; float x = 0, y = 0; int block = -1; };
static Embed FindEmbed(const std::vector<BlockData>& blocks, const Frame& s){
    for ( int i = 0; i < ( int ) blocks.size(); ++i ) {
        const BlockData& b = blocks[i];
        if ( BlockShape::IsSlope(b.type) || b.type == BlockShape::kTypeCloud || std::abs(b.side) > 0.01f ) continue;
        const float half = BlockShape::FootprintHalf(b.type);
        const float bottom = ( float ) b.level, top = bottom + 1.0f;
        const float dx = std::abs(s.dist - b.dist);
        const bool vertical = ( s.footY + 0.95f > bottom + 0.05f );
        const bool centerInside = dx < half && s.footY < top - 0.02f && vertical;
        const bool deepInWall   = dx < half + 0.28f && s.footY < top - 0.31f && vertical;
        if ( centerInside || deepInWall ) return { s.frame, s.dist, s.footY, i };
    }
    return {};
}

// 階段の総当たり：ジャンプする位置を少しずつずらし、押し方（タップ/長押し/連打）も変えて、埋まる組み合わせを探す
static int SweepEmbeds(const char* name, const std::vector<BlockData>& blocks, float spawn, float fromX, float toX,
                       const std::vector<int>& holes = {}){
    int found = 0;
    const char* styles[3] = { "タップ", "長押し(ふんばり)", "着地ごとに連打" };
    for ( int style = 0; style < 3; ++style ) {
        for ( float jumpX = fromX; jumpX <= toX; jumpX += 0.05f ) {
            Scenario sc { name, blocks, spawn, 360,
                [=](int, const Frame& s, bool& r, bool&, bool& j){
                    r = true;
                    if ( style == 0 ) { j = ( s.dist > jumpX && s.dist < jumpX + 0.1f ); }
                    else if ( style == 1 ) { j = ( s.dist > jumpX ) && !( s.grounded && s.dist > jumpX + 1.5f ); }
                    else { static int wait = 0; if ( s.grounded ) ++wait; else wait = 0; j = ( s.dist > jumpX && s.grounded && wait > 2 ); }
                }, holes };
            auto log = Run(sc, false);
            for ( const Frame& s : log ) {
                Embed e = FindEmbed(blocks, s);
                if ( e.frame < 0 ) continue;
                if ( found < 12 ) {
                    std::printf("  [埋まり] %s ジャンプ位置x=%.2f → f%03d x=%.3f y=%.3f ブロック#%d(dist %.1f 段%d)\n",
                        styles[style], jumpX, e.frame, e.x, e.y, e.block, blocks[e.block].dist, blocks[e.block].level);
                }
                ++found;
                break;
            }
        }
    }
    std::printf("  → 埋まった組み合わせ: %d\n", found);
    return found;
}

int main(int argc, char** argv){
    if ( argc > 1 && std::string(argv[1]) == "sweep" ) {
        // 1段ずつの階段（塗りの「階段」で置いた形＝下は空いている）
        std::printf("=== 階段（1段ずつ・下は空き）\n");
        SweepEmbeds("stair", { B(6, 0), B(7, 1), B(8, 2), B(9, 3), B(10, 4) }, 2.0f, 3.0f, 10.0f);
        // 下まで埋まった階段（柱で置いた形）
        std::printf("=== 階段（下まで埋まった形）\n");
        SweepEmbeds("stairFilled", { B(6, 0), B(7, 0), B(7, 1), B(8, 0), B(8, 1), B(8, 2), B(9, 0), B(9, 1), B(9, 2), B(9, 3) }, 2.0f, 3.0f, 9.5f);
        // 2段ずつ上がる階段（ジャンプ1回では届かない段を含む）
        std::printf("=== 高い段（2段差）\n");
        SweepEmbeds("tall", { B(6, 0), B(7, 0), B(7, 1), B(7, 2) }, 2.0f, 3.0f, 7.5f);
        // 天井の低い所：頭上に1段浮いたブロック
        std::printf("=== 頭上に浮いたブロックの下でジャンプ\n");
        SweepEmbeds("ceiling", { B(8, 1), B(9, 1), B(10, 2) }, 2.0f, 5.0f, 11.0f);
        // 斜面を混ぜた階段
        std::printf("=== 斜面つき階段\n");
        SweepEmbeds("slopeStair", { B(6, 0, 2), B(7, 0), B(8, 1, 2), B(8, 0), B(9, 1), B(9, 0) }, 2.0f, 3.0f, 9.5f);
        // 横長・台座
        std::printf("=== 横長・台座の階段\n");
        SweepEmbeds("wide", { B(6.5f, 0, 7), B(8.5f, 1, 7), B(10.5f, 2, 8) }, 2.0f, 3.0f, 11.0f);
        // 穴（x=10付近）を飛び越えた先・穴の縁にブロック（空中状態＝レールから離れた自由落下からの着地）
        std::printf("=== 穴を飛び越えた先にブロック\n");
        SweepEmbeds("holeBlock", { B(12, 0), B(13, 0), B(14, 1) }, 2.0f, 6.0f, 11.0f, { 0, 0, 1, 0, 0, 0, 0, 0, 0 });
        std::printf("=== 穴の縁ぎりぎりにブロック\n");
        SweepEmbeds("holeEdge", { B(11, 0), B(8, 0) }, 2.0f, 6.0f, 11.0f, { 0, 0, 1, 0, 0, 0, 0, 0, 0 });
        return 0;
    }
    const bool verbose = ( argc > 1 );
    std::vector<Scenario> scenarios;
    auto holdRight = [](int, const Frame&, bool& r, bool&, bool&){ r = true; };

    scenarios.push_back({ "A: 1段ブロックへ歩いて当たる（壁で止まる＝正常）", { B(10, 0) }, 2.0f, 150, holdRight });
    scenarios.push_back({ "B: 壁の手前でジャンプして1段ブロックに乗る", { B(10, 0) }, 2.0f, 200,
        [](int f, const Frame& s, bool& r, bool&, bool& j){ r = true; j = ( s.dist > 8.9f && f < 200 ); } });
    scenarios.push_back({ "C: 斜面45°→1段ブロックへ歩いて登る", { B(9, 0, 2), B(10, 0), B(11, 0) }, 2.0f, 200, holdRight });
    scenarios.push_back({ "D: 斜面26°→1段ブロックへ歩いて登る", { B(8, 0, 3), B(10, 0), B(11, 0) }, 2.0f, 200, holdRight });
    scenarios.push_back({ "E: ブロックの上を歩いて端から降りる", { B(4, 0), B(5, 0), B(6, 0) }, 3.9f, 120,
        [](int f, const Frame&, bool& r, bool&, bool& j){ r = true; j = ( f == 0 ); } });
    scenarios.push_back({ "F: ブロックの端で止まって、ゆっくり降りる（歩き出しを刻む）", { B(4, 0) }, 3.9f, 240,
        [](int f, const Frame&, bool& r, bool&, bool& j){ j = ( f == 0 ); r = ( f > 60 ) && ( f % 6 == 0 ); } });
    scenarios.push_back({ "G: 小ジャンプで1段ブロックの角に飛びつく（押しっぱなし）", { B(10, 0) }, 2.0f, 200,
        [](int, const Frame& s, bool& r, bool&, bool& j){ r = true; j = ( s.dist > 9.0f && s.footY < 0.3f ); } });
    scenarios.push_back({ "H: 階段（1段→2段）を歩きジャンプで登る", { B(8, 0), B(9, 0), B(9, 1) }, 2.0f, 240,
        [](int, const Frame& s, bool& r, bool&, bool& j){ r = true; j = ( s.grounded && ( s.dist > 6.9f ) ); } });
    scenarios.push_back({ "I: 頭上の2段目ブロックの下をくぐる（床は道）", { B(10, 1) }, 2.0f, 200, holdRight });
    scenarios.push_back({ "J: 横長ブロック（2m）へ斜面45°から登る", { B(9, 0, 2), B(10.5f, 0, 7) }, 2.0f, 200, holdRight });
    scenarios.push_back({ "K: 1段ブロックの上から2マス先の1段ブロックへ飛び移る", { B(6, 0), B(9, 0) }, 5.9f, 200,
        [](int f, const Frame& s, bool& r, bool&, bool& j){ r = true; j = ( f == 0 ) || ( s.grounded && s.dist > 6.3f && s.dist < 7.0f ); } });
    scenarios.push_back({ "L: 1段ブロックの上で左右に振り向く", { B(5, 0), B(6, 0) }, 4.9f, 200,
        [](int f, const Frame&, bool& r, bool& l, bool& j){ j = ( f == 0 ); r = ( f > 30 && ( f / 20 ) % 2 == 0 ); l = ( f > 30 && ( f / 20 ) % 2 == 1 ); } });

    // 支える範囲（端+0.1m）と壁の範囲（端+0.3m）の間＝0.2mの帯で止まった時
    scenarios.push_back({ "M: ブロックの上から端の外側0.1〜0.3mの帯まで歩いて止まる", { B(4, 0) }, 3.9f, 180,
        [](int f, const Frame& s, bool& r, bool&, bool& j){ static bool stop = false; if ( f == 0 ) stop = false; j = ( f == 0 );
            if ( s.dist > 4.62f ) stop = true; r = !stop && f > 30; } });
    scenarios.push_back({ "N: 帯で止まった後、外側へ歩き出す", { B(4, 0) }, 3.9f, 240,
        [](int f, const Frame& s, bool& r, bool&, bool& j){ static bool stop = false; if ( f == 0 ) stop = false; j = ( f == 0 );
            if ( s.dist > 4.62f ) stop = true; r = ( !stop && f > 30 ) || f > 150; } });
    scenarios.push_back({ "O: 帯で止まった後、ブロック側へ戻る", { B(4, 0) }, 3.9f, 240,
        [](int f, const Frame& s, bool& r, bool& l, bool& j){ static bool stop = false; if ( f == 0 ) stop = false; j = ( f == 0 );
            if ( s.dist > 4.62f ) stop = true; r = !stop && f > 30; l = f > 150; } });
    scenarios.push_back({ "P: 1段ブロックの上から斜面45°を歩いて下る", { B(4, 0), B(5, 0), B(6, 0, 2) }, 3.9f, 150,
        [](int f, const Frame&, bool& r, bool&, bool& j){ j = ( f == 0 ); r = f > 30; } });
    scenarios.push_back({ "Q: 帯で止まって下まで落ちた後、外へ歩く", { B(4, 0) }, 3.9f, 240,
        [](int f, const Frame& s, bool& r, bool&, bool& j){ static bool stop = false; if ( f == 0 ) stop = false; j = ( f == 0 );
            if ( s.dist > 4.62f ) stop = true; r = ( !stop && f > 30 ) || f > 120; } });
    scenarios.push_back({ "R: 斜面45°の低い側から逆向きに登る", { B(10, 0, 2), B(9, 0) }, 14.0f, 200,
        [](int, const Frame&, bool&, bool& l, bool&){ l = true; } });
    scenarios.push_back({ "T: 階段で長押しジャンプ（埋まりの再現）", { B(6, 0), B(7, 1), B(8, 2), B(9, 3), B(10, 4) }, 2.0f, 200,
        [](int f, const Frame& s, bool& r, bool&, bool& j){ r = true; j = ( s.dist > 3.0f ) && !( s.grounded && s.dist > 4.5f ); ( void ) f; } });
    scenarios.push_back({ "S: 2段の壁に歩いて当たる（壁で止まる＝正常）", { B(10, 0), B(10, 1) }, 2.0f, 150, holdRight });

    const char* only =( argc > 2 ) ? argv[2] : nullptr;
    for ( const auto& sc : scenarios ) {
        if ( only && sc.name.rfind(only, 0) != 0 ) continue;
        std::printf("=== %s\n", sc.name.c_str());
        auto log = Run(sc, verbose && only);
        Analyze(sc, log);
    }
    return 0;
}
