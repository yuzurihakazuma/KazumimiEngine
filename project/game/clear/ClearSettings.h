#pragma once
// =====================================================================
//  ClearSettings：クリア演出の調整値（クリア演出 企画書）。コードに数字を埋め込まず、ファイルから読む。
//   resources/clear/clear.json     … 場面の秒数（フル版・短縮版）・小道具の置き場所・リザルトの置き場所（どのステージでも同じ）
//   resources/clear/<stage>.json   … ステージごと：輪の場所・着地する所・演出カメラの終点・目標連鎖数
//   どちらも tools/clear/make_clear_assets.py が最初の値を作る。ステージごとの値は、インスペクターの「クリア演出」で直して保存できる
//   ファイルが無い・項目が無い時は、ここに書いた値のまま動く
// =====================================================================
#include "engine/math/struct.h"

#include <string>
#include <vector>

// 場面の長さ（秒）
struct ClearTiming {
    float ring = 2.0f;      // 輪をくぐる：恐竜が高く跳び、カメラが横から正面へ回り込む
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

// 卵の殻の輪（モデルの寸法と、ふたの開き方）
struct ClearRing {
    Vector2 hinge { 1.0f, 2.0f };  // ふたのちょうつがい（輪の真ん中の足元から。x＝横・y＝高さ）
    float halfWidth = 1.3f;        // 輪の外側の半幅
    float height = 3.2f;           // 輪の高さ
    float innerHalfWidth = 0.96f;  // くぐる所の半幅
    float innerHeight = 2.86f;     // くぐる所の高さ
    float openTime = 0.7f;         // ふたが開く秒数
    float openAngle = 1.15f;       // 開く角度(rad)
    float openHop = 0.35f;         // 開く時にぽんと跳ねる高さ(m)
};

// 「ゴール！」の切り文字（上から糸で下がる）
struct ClearLetters {
    int   count = 4;
    Vector2 textureSize { 340.0f, 1224.0f };
    float height = 1.25f;        // 文字のマスの高さ(m)
    float string = 2.6f;         // 糸の長さ（文字のマスの高さに対して）
    float spacing = 1.12f;       // 文字の間(m)
    float centerHeight = 3.5f;   // 文字の真ん中の高さ（着地する所の地面から）
    float back = 0.9f;           // 着地する所から、輪の方へ下げる(m)
    float start = 0.15f;         // 場面に入ってから、最初の文字が下り始めるまで(秒)
    float interval = 0.14f;      // 文字ごとの遅れ(秒)
    float dropTime = 0.75f;      // 下りる時間(秒)
    float dropHeight = 4.6f;     // 画面の上の外からの高さ(m)
    float swing = 0.28f;         // 下りた時の揺れ(rad)
    float sway = 0.035f;         // その後の、ゆっくりした揺れ(rad)
};

// 棒付きの紙の星（左右から扇状に飛び出す）
struct ClearStars {
    Vector2 textureSize { 348.0f, 1128.0f };
    float height = 0.95f;        // 星の絵の高さ(m)
    float stick = 2.6f;          // 棒の長さ（星の絵の高さに対して）
    int   perSide = 3;
    float pivotSide = 4.3f;      // 棒の根元：真ん中から横へ(m)
    float pivotHeight = -0.6f;   // 棒の根元の高さ(m)
    float back = 1.3f;
    std::vector<float> angles { 0.42f, 0.78f, 1.14f }; // 扇の角度（真上から内側へ rad）
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
    std::string ring = "resources/clear/se_ring.wav";
    std::string letter = "resources/clear/se_letter.wav";
    std::string star = "resources/clear/se_star.wav";
    std::string count = "resources/clear/se_count.wav";
    std::string stamp = "resources/clear/se_stamp.wav";
    std::string hanamaru = "resources/clear/se_hanamaru.wav";
    std::string add = "resources/clear/se_add.wav";
    std::string road = "resources/clear/se_road.wav";
};

// どのステージでも同じ値（clear.json）
struct ClearSettings {
    ClearTiming full;          // 初めてのクリア
    ClearTiming shortVersion;  // 同じステージの 2 回目から（「ゴール！」を縮める）
    ClearJump jump;
    ClearRing ring;
    ClearLetters letters;
    ClearStars stars;
    ClearConfetti confetti;
    ClearResultLayout result;
    ClearSounds sounds;

    bool Load();
    // 切り文字と星の置き場所だけを clear.json へ書き戻す（インスペクターで直した分。ほかの項目はファイルのまま）
    bool SaveProps() const;
    const ClearTiming& TimingFor(bool isShort) const{ return isShort ? shortVersion : full; }
};

// ステージごとの値（<stage>.json）
struct ClearStageSettings {
    float ringOffset = -3.2f;       // ゴールの点から、レールに沿って輪を置く所(m)。マイナス＝手前
    float landAhead = 1.4f;         // 輪をくぐってから着地する所（輪から先へ m）
    // 演出カメラの終点（着地する所から見て）
    float cameraDistance = 7.2f;    // 進む向きの先へ（ここから恐竜を正面に見る）
    float cameraHeight = 1.7f;
    float cameraSide = 0.0f;        // 横へずらす（画面の右が＋）
    float lookHeight = 1.9f;        // 見る点の高さ
    float fovY = 0.72f;
    int   comboTarget = 5;          // 1 行目：奥投げコンボの目標
    std::string band = "comboFlowers"; // 帯に出す集める物：comboFlowers＝コンボ花 / flowers＝かくれ花 / none＝帯なし
    std::string healthRule = "full";   // 2 行目：full＝満タンでゴール / noDamage＝1 回もダメージを受けない

    bool Load(const std::string& stage);
    bool Save(const std::string& stage) const;
};
