#include "game/player/PlayerBlockContact.h"
#include "game/stage/BlockSystem.h"

#include <algorithm>
#include <cmath>

float PlayerBlockContact::ResolveWalk(int rail, float prevDist, float newDist, float footY, bool grounded) const{
    if ( !blocks_ ) return newDist;
    // 地面を歩いている時は、進んだ先で実際に立つ高さ（斜面の表面・小段差の上面）で壁を調べる。
    //   前フレームの高さのままだと、45°の斜面では1歩ぶん低い足で調べることになり、
    //   登り切る直前で「隣のブロックの側面」に当たって坂の途中で止まっていた
    if ( grounded ) { footY = ( std::max )( footY, blocks_->GroundHeightAt(rail, newDist, footY) ); }

    // 体の高さ帯に重なるブロックへは進入できない（壁になる）。
    //   乗っている時（足がブロック上面）は帯が重ならないので普通に上を歩ける
    float blockMin = 0.0f, blockMax = 0.0f;
    if ( !blocks_->BlockedAt(rail, newDist, footY + kBodyBottom, footY + kBodyTop, &blockMin, &blockMax) ) {
        return newDist;
    }
    const float kPushGap = 0.01f; // 面から少し離す（毎フレーム再衝突しない）
    if ( prevDist <= blockMin ) { return blockMin - kPushGap; }
    if ( prevDist >= blockMax ) { return blockMax + kPushGap; }
    // すでに壁の範囲の内側にいる（ブロックの端から落ちた直後・ノックバック等）：
    //   ブロックから離れる向きの動きだけ許す。以前は一切動けず、端の外側で身動きが取れなくなっていた
    const float center = ( blockMin + blockMax ) * 0.5f;
    return ( std::abs(newDist - center) >= std::abs(prevDist - center) ) ? newDist : prevDist;
}

bool PlayerBlockContact::IsBodyBlocked(int rail, float dist, float footY) const{
    return blocks_ && blocks_->BlockedAt(rail, dist, footY + kBodyBottom, footY + kBodyTop, nullptr, nullptr);
}

bool PlayerBlockContact::ClampToCeiling(int rail, float dist, float& footY) const{
    if ( !blocks_ ) return false;
    float ceiling = blocks_->CeilingHeightAt(rail, dist, footY);
    if ( footY + kHeadHeight <= ceiling ) return false;
    footY = ceiling - kHeadHeight;
    blocks_->NotifyHeadBump(rail, dist, ceiling); // ？ブロックならコインが飛び出す（BlockSystem が判定）
    return true;
}

bool PlayerBlockContact::PushOutOfBlocks(int rail, float dist, float& footY) const{
    if ( !blocks_ ) return false;
    bool pushed = false;
    float embedTop = 0.0f;
    // 「埋まっている」とみなすのは、押し出した先の上面に実際に立てる範囲（ブロックの実寸＋縁）だけ。
    //   壁の範囲（体の半径ぶん外まで）で調べると、端の外側0.1〜0.3mの帯では
    //   上面へ押し出す→そこには立てないので落ちる→また押し出す、を永遠に繰り返していた。
    //   帯の中で落ちてきた時は、ブロックの側面に沿ってそのまま下まで落ちる
    // 積み重なったブロックの中に居たら1段ずつ上へ（念のため回数に上限）
    for ( int guard = 0; guard < 10; ++guard ) {
        if ( !blocks_->BlockedAt(rail, dist, footY + kBodyBottom, footY + kBodyTop,
                                 nullptr, nullptr, &embedTop, kEmbedRadius) ) break;
        footY = embedTop;
        pushed = true;
    }
    return pushed;
}

float PlayerBlockContact::GroundHeight(int rail, float dist, float footY) const{
    return blocks_ ? blocks_->GroundHeightAt(rail, dist, footY) : 0.0f;
}

bool PlayerBlockContact::IsOnSpring(int rail, float dist, float footY) const{
    return blocks_ && blocks_->SupportTypeAt(rail, dist, footY) == BlockSystem::kTypeSpring;
}
