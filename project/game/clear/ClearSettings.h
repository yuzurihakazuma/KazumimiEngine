#pragma once
// =====================================================================
//  ClearSettings：クリア演出の調整値（クリア演出 企画書）。コードに数字を埋め込まず、ファイルから読む。
//   resources/clear/clear.json     … 場面の秒数（フル版・短縮版）・小道具の置き場所・リザルトの置き場所（どのステージでも同じ）
//   resources/clear/<stage>.json   … ステージごと：門の場所・着地する所・演出カメラの終点・目標連鎖数
//   どちらも tools/clear/make_clear_assets.py が最初の値を作る（門の寸法 "gate" は、モデルと一緒に tools/clear/make_goal_set.py が書く）。
//   ステージごとの値は、インスペクターの「クリア演出」で直して保存できる
//   ファイルが無い・項目が無い時は、ここに書いた値のまま動く
// =====================================================================
#include "engine/math/struct.h"

#include <string>
#include <vector>

// 場面の長さ（秒）
struct ClearTiming {
    float gate = 2.0f;      // 門をくぐる：卵が割れ、恐竜が高く跳び、カメラが横から正面へ回り込む
    float goal = 10.0f;     // 「ゴール！」：切り文字・星・紙吹雪。恐竜は決めポーズから喜びの踊り
    float toResult = 1.0f;  // 景色が暗い緑に沈み、リザルトのマットが出る
    float leave = 0.6f;     // 所持数へ数え終わってから、幕が出るまで
};

// 恐竜の大ジャンプ（GoalJump クリップの中の時刻。tools/stage_intro/make_intro_clips.py と合わせる）
struct ClearJump {
    float takeOff = 0.2f;   // 跳び上がる
    float land = 1.3f;      // 着地する
    float pose = 1.0f;      // 決めポーズ（GoalPose）の長さ。この後、喜びの踊り
};

// たまごの門（モデルの寸法。tools/clear/make_goal_set.py がモデルと一緒に書く）。長さは m、門の真ん中の足元から
struct ClearGate {
    float innerHalfWidth = 1.28f;   // くぐる所の半幅（柱の内側）
    float innerHeight = 3.93f;      // くぐる所の高さ
    float archCenterHeight = 2.7f;  // アーチの円の中心の高さ（切り文字の糸をつなぐ所を求める）
    float archOuterRadius = 2.17f;  // アーチの外の半径
    float archFront = 0.37f;        // アーチの表の面（門の真ん中から、恐竜が抜けていく向きへ）
    Vector3 eggHinge { 0.0f, 5.84f, 0.5f };  // 卵のふたのちょうつがい（横・高さ・入口の側へ）
    Vector2 tapePivot { 1.33f, 1.05f };      // ゴールテープの根元（横・高さ）
    float height = 6.33f;           // いちばん上
    // くぐった時の動き（clear.json の "gateOpen"）
    float openTime = 0.8f;          // 卵のふたが開く秒数
    float eggAngle = 1.25f;         // ふたが後ろへ倒れる角度(rad)
    float hop = 0.45f;              // 開く時にぽんと跳ねる高さ(m)
    float tapeAngle = 1.45f;        // テープが根元を軸に垂れる角度(rad)
};

// 「ゴール！」の切り文字（白い紙の札に色紙の文字）。上から糸で吊られて下りてきて、アーチの虹の上に弧を描いて並ぶ
//   （門の柱・花や、札どうしに重ならないよう、虹の帯の上に置く）
struct ClearLetters {
    int   count = 4;
    float height = 1.05f;        // 札の高さ(m)（モデルと同じ）
    float scale = 0.7f;          // 札の大きさ（モデルに対する倍率）
    float arcRadius = 1.65f;     // 札の真ん中を並べる円の半径（アーチの円の中心から。虹の帯のまん中あたり）
    std::vector<float> arcAngles { -62.0f, -22.0f, 22.0f, 62.0f }; // 札の位置（てっぺんからの角度・度。左が－）
    float stringLength = 3.5f;   // 札の上の糸の長さ(m)。糸は画面の上の外まで伸びる
    float dropHeight = 3.0f;     // 下りてくる高さ(m)
    float front = 0.05f;         // アーチの表の面から、さらに手前へ(m)
    float start = 0.15f;         // 場面に入ってから、最初の文字が下り始めるまで(秒)
    float interval = 0.14f;      // 文字ごとの遅れ(秒)
    float dropTime = 0.75f;      // 下りる時間(秒)
    float swing = 0.28f;         // 下りた時の揺れ(rad)
    float sway = 0.035f;         // その後の、ゆっくりした揺れ(rad)
};

// 棒付きの紙の星（門の両わきの足元から、外へ扇状に飛び出す）
struct ClearStars {
    int   perSide = 3;
    float pivotSide = 2.55f;     // 棒の根元：門の真ん中から横へ(m)
    float pivotHeight = 0.0f;    // 棒の根元の高さ(m)
    float front = 0.7f;          // 門の面から手前へ(m)
    std::vector<float> angles { 0.3f, 0.68f, 1.05f }; // 扇の角度（真上から外へ rad）
    float start = 0.75f;         // 場面に入ってから(秒)
    float interval = 0.09f;
    float popTime = 0.5f;
};

// 紙吹雪
struct ClearConfetti {
    int   burst = 46;            // 輪をくぐった瞬間の枚数
    int   rain = 5;              // 降らせる 1 回の枚数
    float rainInterval = 0.22f;  // 降らせる間(秒)
    float height = 5.2f;         // 降らせ始める高さ(m)
    float spread = 3.6f;         // 横の広がり(m)
};

// リザルト画面の置き場所（1920×1080 の画素。x・y は真ん中）と、出し方の秒数
struct ClearResultLayout {
    Vector4 dim { 0.03f, 0.13f, 0.08f, 0.74f };  // 景色を沈める色
    Vector2 mat { 960.0f, 566.0f };
    Vector2 title { 470.0f, 230.0f };
    Vector2 name { 1010.0f, 230.0f };
    float   nameWidth = 600.0f;
    Vector2 hanamaru { 1532.0f, 246.0f };
    float   hanamaruSize = 236.0f;
    float   rowX = 960.0f;
    std::vector<float> rowYs { 404.0f, 540.0f, 676.0f, 812.0f }; // 帯・1 行目・2 行目・3 行目
    float iconX = 366.0f, iconSize = 92.0f, labelX = 430.0f;
    float valueRight = 1330.0f, valueHeight = 78.0f;
    float stampX = 1516.0f, stampSize = 112.0f;
    float flowerX = 800.0f, flowerGap = 124.0f, flowerSize = 100.0f;
    Vector2 prompt { 1560.0f, 1034.0f };
    Vector2 owned { 200.0f, 64.0f };
    float ownedNumberX = 150.0f, ownedNumberHeight = 62.0f;
    // 秒数
    float matIn = 0.5f;          // マットがすべり込む
    float itemIn = 0.3f;         // 行・文字がぽんと出る
    float flowerInterval = 0.4f; // 帯の花が 1 つずつ咲く間
    float countTime = 0.9f;      // 数え上げ
    float stampTime = 0.35f;     // スタンプを押す
    float rowGap = 0.25f;        // 行と行の間
    float hanamaruTime = 1.25f;  // 花丸を描く
    float addTime = 1.4f;        // 所持数へ数え込む
};

struct ClearSounds {
    std::string gate = "resources/clear/se_gate.wav";
    std::string letter = "resources/clear/se_letter.wav";
    std::string star = "resources/clear/se_star.wav";
    std::string count = "resources/clear/se_count.wav";
    std::string stamp = "resources/clear/se_stamp.wav";
    std::string hanamaru = "resources/clear/se_hanamaru.wav";
    std::string add = "resources/clear/se_add.wav";
    std::string road = "resources/clear/se_road.wav";
};

// クリアの書き割り 1 枚（ClearBackdrop）。門の足元から、カメラから見て奥へ back・右へ side・上へ up(m)。表をカメラへ向ける
struct ClearBackdropItem {
    std::string model;            // クラフトのキットのモデル（resources/craft/models/<model>.gltf）
    float back = 20.0f;
    float side = 0.0f;
    float up = 0.0f;
    Vector3 scale { 1.0f, 1.0f, 1.0f };
    float turn = 0.0f;            // カメラへ向けた後に、さらに回す(rad)
};

// どのステージでも同じ値（clear.json）
struct ClearSettings {
    ClearTiming full;          // 初めてのクリア
    ClearTiming shortVersion;  // 同じステージの 2 回目から（「ゴール！」を縮める）
    ClearJump jump;
    ClearGate gate;
    ClearLetters letters;
    ClearStars stars;
    ClearConfetti confetti;
    std::vector<ClearBackdropItem> backdrop; // クリアの間だけ、門の後ろに立てる書き割り
    ClearResultLayout result;
    ClearSounds sounds;

    bool Load();
    // 切り文字と星の置き場所だけを clear.json へ書き戻す（インスペクターで直した分。ほかの項目はファイルのまま）
    bool SaveProps() const;
    const ClearTiming& TimingFor(bool isShort) const{ return isShort ? shortVersion : full; }
};

// ステージごとの値（<stage>.json）
struct ClearStageSettings {
    float gateOffset = -3.2f;       // ゴールの点から、レールに沿って門を置く所(m)。マイナス＝手前
    float gateTurn = 15.0f;         // 門の入口の面を、遊んでいる時のカメラの方へ回す角度(度)。横から近づく時に、門の表が見える
    float landAhead = 1.4f;         // 門をくぐってから着地する所（門から先へ m）
    // 演出カメラの終点（着地する所から見て）。くぐった向きから、遊んでいる時のカメラの側へ cameraYaw 度回した方へ置く
    //   （真正面だと、来た道をまっすぐ見通してしまう。少し横からにすると、門と恐竜が重ならずに並ぶ）
    float cameraYaw = 20.0f;
    float cameraDistance = 8.6f;
    float cameraHeight = 2.0f;
    float lookHeight = 2.4f;        // 見る点の高さ
    float lookBack = 0.7f;          // 見る点を、恐竜から門の方へずらす(m)（恐竜と門のまん中あたりを見る）
    float fovY = 0.82f;
    int   comboTarget = 5;          // 1 行目：奥投げコンボの目標
    std::string band = "comboFlowers"; // 帯に出す集める物：comboFlowers＝コンボ花 / flowers＝かくれ花 / none＝帯なし
    std::string healthRule = "full";   // 2 行目：full＝満タンでゴール / noDamage＝1 回もダメージを受けない

    bool Load(const std::string& stage);
    bool Save(const std::string& stage) const;
};
