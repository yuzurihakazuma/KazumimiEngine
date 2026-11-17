#pragma once
// =====================================================================
//  GoalProps：「ゴール！」の小道具の出し入れ（クリア演出 企画書）。画面効果ではなく、糸・棒で支えられた工作として出す。
//   ・切り文字「ゴ」「ー」「ル」「！」：白い紙の札に色紙の立体の文字。門のアーチから糸で下がってくる
//     （糸が伸びて下がり、行き過ぎて戻り、糸で少し揺れる。出る時刻を 1 つずつずらす）
//   ・棒付きの紙の星：門の両わきの足元から、外へ扇状に飛び出す。行き過ぎて戻る
//   どれも立体のモデル（tools/clear/make_goal_set.py）で、表を演出カメラへ向ける。置き場所・秒数は clear.json（ClearSettings）
// =====================================================================
#include "engine/math/struct.h"
#include "game/clear/ClearSettings.h"

#include <memory>
#include <vector>

class Obj3d;

class GoalProps {
public:
    GoalProps();
    ~GoalProps();

    // 見た目を作る（モデルは読み込み済みであること）。settings は呼ぶ側が持ち続けること（置き場所・秒数を毎フレームここから読む）
    void Initialize(const ClearSettings& settings);

    // 舞台を決めて出し始める。
    //   origin＝門の真ん中の足元、right＝門の表から見て右（水平・長さ 1）、front＝門の表の向き、
    //   toCamera＝演出カメラの方（水平・長さ 1。小道具の表をこちらへ向ける）
    void Begin(const Vector3& origin, const Vector3& right, const Vector3& front, const Vector3& toCamera);
    void Hide();
    bool IsVisible() const{ return visible_; }

    // elapsed＝出し始めてからの秒数。出る瞬間の音もここで鳴らす
    void Update(float elapsed);
    void Draw();

private:
    // 小道具 1 つ（切り文字は糸も持つ）
    struct Piece {
        std::unique_ptr<Obj3d> object;
        std::unique_ptr<Obj3d> string; // 糸（切り文字だけ）
        bool soundPlayed = false;
    };

    // 舞台の点（x＝右・y＝上・z＝門の表の向き）→ 世界
    Vector3 ToWorld(const Vector3& stagePoint) const;
    // 表をカメラへ向けたうえで、画面の中で angle だけ回す（左回りが＋）向き
    Vector3 FacingRotation(float angle) const;
    // 切り文字の札の上のはしの、アーチ（糸をつなぐ所）の高さ
    float ArchHeightAt(float across) const;

    const ClearSettings* settings_ = nullptr; // 調整値（持たない。ClearFlow の物。インスペクターで直すとその場で効く）
    std::vector<Piece> letters_;
    std::vector<Piece> stars_;

    bool    visible_ = false;
    Vector3 origin_ {};
    Vector3 right_ { 0.0f, 0.0f, 1.0f };
    Vector3 front_ { 1.0f, 0.0f, 0.0f };
    float   faceYaw_ = 0.0f;              // モデルの表（+Z）をカメラへ向ける角度
};
