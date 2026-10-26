#pragma once
// =====================================================================
//  WorldMapData：ワールドマップ（ステージ選択）の置き場所と、飛び込みの時刻（ステージ選択・飛び込み演出 企画書）。
//   resources/worldmap/worldmap.json … マス（ステージ）・道・台座・カメラ
//   resources/worldmap/dive.json     … 飛び込みの5段の長さ・高さ・カメラの位置（後から調整できる）
//   どちらも tools/worldmap/make_worldmap_assets.py が作る（手で直してもよい）
// =====================================================================
#include "engine/math/struct.h"

#include <string>
#include <vector>

// マス1つ＝ステージ1つ
struct WorldStage {
    std::string id;
    std::string stage;       // ステージの名前（resources/map/<stage>.json）。空＝まだ無い
    std::string label;       // 台座に貼る番号
    bool    unlocked = false;
    Vector3 node {};         // 恐竜が立つ所（道の上）
    float   dioramaMargin = 0.92f;  // 台座の幅に対して、箱庭が占める割合
    // 箱庭に切り抜く四角（ステージの中の m）。center が無ければ、ブロック・敵・道がいちばん多い所を自動で選ぶ
    float   windowDepth = 30.0f;
    bool    hasWindowCenter = false;
    Vector3 windowCenter {};        // x と z を使う
    std::string feature;            // 箱庭の目印（"train"＝汽車がぐるぐる走る）
    float   previewDistance = 2.9f; // プレビュー：箱庭の中心からカメラまで
    float   previewHeight = 1.35f;
    float   orbitSeconds = 10.0f;   // ゆっくり1周する秒数
};

struct WorldMapData {
    std::vector<Vector3> road;        // 道の点（SplineRail。布ロード）
    float   roadWidth = 1.6f;
    Vector3 roadColor { 0.95f, 0.55f, 0.28f };
    float   pedestalBack = 3.4f;      // 立つ所から台座の中心までの奥行き
    Vector3 pedestalSize { 4.6f, 0.7f, 1.7f };
    Vector3 cameraOffset { 0.0f, 5.2f, -8.5f }; // マップを歩く時のカメラ（恐竜から）
    Vector3 cameraLook { 0.0f, 0.8f, 2.6f };    // 見る所（恐竜から）
    float   fovY = 0.62f;
    std::vector<WorldStage> stages;

    bool Load(const std::string& path);
    // 台座の中心（上面の高さ）
    Vector3 PedestalTop(const WorldStage& stage) const{
        return { stage.node.x, stage.node.y + pedestalSize.y, stage.node.z + pedestalBack };
    }
};

// 飛び込みの時刻（企画書の5段：構え → ジャンプ → 頂点 → 飛び込み → 吸い込み）
struct DiveTiming {
    float crouch = 0.3f, jump = 0.5f, apex = 0.2f, dive = 0.4f, absorb = 0.3f;
    float apexHeight = 3.2f;   // 箱庭の上、どれだけ高く跳ぶか(m)
    float endScale = 0.06f;    // 飛び込み終わりの恐竜の大きさ
    float irisTime = 0.35f;    // 円を閉じる時間（吸い込みの始めから）
    float irisHold = 0.2f;     // 閉じ切ってから次へ移るまで
    Vector3 cameraApex { 0.0f, 4.2f, -2.4f }; // 頂点：箱庭の中心からのカメラの位置
    Vector3 cameraDive { 0.0f, 1.5f, -1.1f }; // 飛び込み：箱庭へ急に寄る

    bool Load(const std::string& path);
    float Total() const{ return crouch + jump + apex + dive + absorb; }
};
