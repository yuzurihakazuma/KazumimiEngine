#pragma once
// =====================================================================
//  PlayerBlockContact：プレイヤーの体とブロックの当たり（レール空間）。
//   プレイヤーは「足元から高さ1mの筒」。ブロックの問い合わせ（BlockSystem）を
//   プレイヤーの体の寸法で呼ぶ窓口をここへまとめた。
//     ・横：進んだ先で体の高さ帯がブロックに重なったら面の手前で止める（壁）
//     ・上：ジャンプ中に頭が天井に届いたら止める（？ブロックならコインが出る）
//     ・下：足元の支持面（ブロックの上面 or レール面0）と、埋まった時の押し出し
//   高さはどれも「レール面からの相対高さ」（ワールドYではない）。
//   ブロックが無い（BlockSystem 未設定）時は、どれも「何も無い」を返す
// =====================================================================
class BlockSystem;

class PlayerBlockContact {
public:
    // 体の寸法（足元からの高さ）
    static constexpr float kBodyBottom = 0.05f; // 足元のすき間（地面と重なって壁扱いにならない）
    static constexpr float kBodyTop    = 0.95f; // 体の上端（横当たりの帯）
    static constexpr float kHeadHeight = 1.0f;  // 頭の高さ（天井の判定）

    void SetBlocks(BlockSystem* blocks){ blocks_ = blocks; }

    // 横：prevDist から newDist へ進んだ時、壁に当たれば面の手前へ戻した距離を返す（当たらなければ newDist）
    float ResolveWalk(int rail, float prevDist, float newDist, float footY) const;
    // その位置で体がブロックに重なっているか（ノックバックで押し戻す時の判定）
    bool  IsBodyBlocked(int rail, float dist, float footY) const;

    // 上：上昇中に頭が天井へ届いたら footY を天井の下へ戻して true（？ブロックへの頭突きも通知する）
    bool  ClampToCeiling(int rail, float dist, float& footY) const;

    // 下：体がブロックに埋まっていたら上面へ押し出して true（中で身動きが取れなくなるのを防ぐ）
    bool  PushOutOfBlocks(int rail, float dist, float& footY) const;
    // 足元の支持面の高さ（ブロックの上面 or レール面0）
    float GroundHeight(int rail, float dist, float footY) const;
    // ブロックの上に立っているか（穴の上でも落ちない＝ブロックで橋を渡せる）
    bool  IsOnBlock(int rail, float dist, float footY) const { return GroundHeight(rail, dist, footY) > 0.0f; }
    // 足元がジャンプ台ブロックか
    bool  IsOnSpring(int rail, float dist, float footY) const;

private:
    BlockSystem* blocks_ = nullptr; // シーンの BlockSystem（所有しない。？ブロックの頭突き通知で書き換えるため非const）
};
