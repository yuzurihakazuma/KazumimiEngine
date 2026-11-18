#pragma once
#include "engine/math/struct.h"
#include "game/clear/ClearSettings.h"

#include <memory>
#include <vector>

namespace KazumimiEngine { class Obj3d; }

// =====================================================================
//  ClearBackdrop：クリア演出の間だけ、門の後ろの空に出す飾り（紙の太陽・雲）。
//   ・遊んできたステージが後ろに見えるのが大事なので、地面・丘・木・空の板のような、ステージを隠す物は置かない
//     （一度置いてみたら、ステージが全部隠れて別の場所のようになった）
//   ・演出のカメラの反対側（門の後ろ）の空に、下からせり上がって出る（舞台の書き割りのように）
//   ・クリア演出の間しか描かないので、遊んでいる時の景色・カメラ反転の裏の世界には何もしない
//   何をどこに置くかは clear.json の "backdrop"（門の後ろへの距離・横・上・大きさ）
// =====================================================================
class ClearBackdrop {
public:
    ClearBackdrop();
    ~ClearBackdrop();

    // 書き割りの物を作る（モデルが読まれていなければ、クラフトのキットから読む）
    void Initialize(const std::vector<ClearBackdropItem>& items);

    // 出し始める。origin＝門の足元、back＝カメラから見て奥の向き（水平・長さ 1）
    void Begin(const KazumimiEngine::Vector3& origin, const KazumimiEngine::Vector3& back);
    void Hide();
    bool IsVisible() const{ return visible_; }

    void Update(float deltaTime);
    void Draw();

private:
    // 書き割りの物 1 つ（見た目の物と、置き場所の設定）
    struct Piece {
        std::unique_ptr<KazumimiEngine::Obj3d> object;
        ClearBackdropItem item;
    };
    std::vector<Piece> pieces_;
    bool    visible_ = false;
    float   time_ = 0.0f;       // 出し始めてからの秒数
    KazumimiEngine::Vector3 origin_ {};
    KazumimiEngine::Vector3 back_ { 1.0f, 0.0f, 0.0f };
    KazumimiEngine::Vector3 right_ { 0.0f, 0.0f, -1.0f };
    float   faceYaw_ = 0.0f;    // 板の表（+Z）をカメラへ向ける角度
};
