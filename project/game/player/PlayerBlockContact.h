#pragma once
class BlockSystem;

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
class PlayerBlockContact {
public:
    // 体の寸法（足元からの高さ）
    static constexpr float kBodyBottom = 0.05f; // 足元のすき間（地面と重なって壁扱いにならない）
    static constexpr float kBodyTop    = 0.95f; // 体の上端（横当たりの帯）
    static constexpr float kHeadHeight = 1.0f;  // 頭の高さ（天井の判定）
    // 「ブロックに埋まった」とみなす横の広がり。BlockSystem の支持面の縁（ブロックの端＋0.1m）と同じにする
    //   （押し出した先の上面に必ず立てる範囲だけを「埋まった」とする）
    static constexpr float kEmbedRadius = 0.10f;

    void SetBlocks(BlockSystem* blocks){ blocks_ = blocks; }

    // 横：prevDist から newDist へ進んだ時、壁に当たれば面の手前へ戻した距離を返す（当たらなければ newDist）。
    //   grounded=true なら、進んだ先で立つ高さ（斜面・小段差）で壁を調べる
    float ResolveWalk(int rail, float prevDist, float newDist, float footY, bool grounded) const;
    // その位置で体がブロックに重なっているか（ノックバックで押し戻す時の判定）
    bool  IsBodyBlocked(int rail, float dist, float footY) const;

    // 横：体が壁の範囲の内側にある時（ブロックの端から落ちた直後・とどかずに落ちてきた時など）、
    //   ブロックから離れる向きへ maxStep までずらした距離を返す（体がブロックに重なったままにならない）。
    //   出た先が別のブロックの壁の中になる（すき間がせまい）時は動かさない
    float SlideOutOfWall(int rail, float dist, float footY, float maxStep, bool grounded) const;

    // 上：上昇中に頭が天井へ届いたら footY を天井の下へ戻して true（？ブロックへの頭突きも通知する）
    bool  ClampToCeiling(int rail, float dist, float& footY) const;

    // 下：体がブロックに埋まっていたら上面へ押し出して true（中で身動きが取れなくなるのを防ぐ）
    bool  PushOutOfBlocks(int rail, float dist, float& footY) const;
    // 足元の支持面の高さ（ブロックの上面 or レール面0）
    float GroundHeight(int rail, float dist, float footY) const;
    // その距離で、体がブロックに重ならずに居られる一番低い高さ。支持面の高さで体が壁の範囲に入る所
    //   （ブロックの面のすぐ外）では、そのブロックの上面の高さを返す（後ろの卵をブロックの中に置かないために使う）
    float ClearHeight(int rail, float dist, float footY) const;
    // ブロックの上に立っているか（穴の上でも落ちない＝ブロックで橋を渡せる）
    bool  IsOnBlock(int rail, float dist, float footY) const { return GroundHeight(rail, dist, footY) > 0.0f; }
    // 足元がジャンプ台ブロックか
    bool  IsOnSpring(int rail, float dist, float footY) const;

private:
    // 壁の範囲の中から、ブロックの面まで歩くと上面の高さまで登れるか（手前に坂・小さな段がある）
    bool CanWalkUpTo(int rail, float dist, float footY, float blockMin, float blockMax) const;
    BlockSystem* blocks_ = nullptr; // シーンの BlockSystem（所有しない。？ブロックの頭突き通知で書き換えるため非const）
};
