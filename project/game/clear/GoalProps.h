#pragma once
// =====================================================================
//  GoalProps：「ゴール！」の小道具の出し入れ（クリア演出 企画書）。画面効果ではなく、糸・棒で支えられた工作として出す。
//   ・切り文字「ゴ」「ー」「ル」「！」：上から糸で下がってくる。行き過ぎて戻り、糸で少し揺れる。出る時刻を 1 つずつずらす
//   ・棒付きの紙の星：左右の下から扇状に飛び出す。行き過ぎて戻る
//   どれも、絵を貼った板（平面モデル）を、演出カメラの正面に向けて置く。置き場所・秒数は clear.json（ClearSettings）
//   透明な所のある板なので、不透明な物を描き終えた後（BaseScene::OnDrawTransparent）に描く
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

    // 板のモデルと見た目を作る。settings は呼ぶ側が持ち続けること（置き場所・秒数を毎フレームここから読む）
    void Initialize(const ClearSettings& settings);

    // 舞台を決めて出し始める。origin＝恐竜が着地する所（地面）、toCamera＝そこから演出カメラの方（水平・長さ 1）
    void Begin(const Vector3& origin, const Vector3& toCamera);
    void Hide();
    bool IsVisible() const{ return visible_; }

    // elapsed＝出し始めてからの秒数。出る瞬間の音もここで鳴らす
    void Update(float elapsed);
    void Draw();

private:
    // 板 1 枚：支点（糸の上のはし・棒の根元）と、そこからの回り方
    struct Board {
        std::unique_ptr<Obj3d> object;
        float width = 1.0f, height = 1.0f; // 板の大きさ(m)
        bool  soundPlayed = false;
    };

    // 板を、舞台の上の支点 pivot（右・上・手前の量）から、角度 angle だけ傾けて置く。
    //   hangs＝true なら支点は板の上のはし（文字）、false なら下のはし（星）
    void PlaceBoard(Board& board, const Vector3& pivot, float angle, bool hangs, float scale);
    Vector3 ToWorld(const Vector3& stagePoint) const;

    const ClearSettings* settings_ = nullptr; // 調整値（持たない。ClearFlow の物。インスペクターで直すとその場で効く）
    std::vector<Board> letterBoards_;
    std::vector<Board> starBoards_;

    bool    visible_ = false;
    Vector3 origin_ {};
    Vector3 right_ { 0.0f, 0.0f, 1.0f };     // 画面の右
    Vector3 toCamera_ { 1.0f, 0.0f, 0.0f };  // 手前（カメラの方）
    float   faceYaw_ = 0.0f;                 // 板の表をカメラへ向ける角度
};
