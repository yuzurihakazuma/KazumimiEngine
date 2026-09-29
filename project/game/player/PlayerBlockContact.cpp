#include "game/player/PlayerBlockContact.h"
#include "game/stage/BlockSystem.h"

float PlayerBlockContact::ResolveWalk(int rail, float prevDist, float newDist, float footY) const{
    if ( !blocks_ ) return newDist;
    // 体の高さ帯に重なるブロックへは進入できない（壁になる）。
    //   乗っている時（足がブロック上面）は帯が重ならないので普通に上を歩ける
    float blockMin = 0.0f, blockMax = 0.0f;
    if ( !blocks_->BlockedAt(rail, newDist, footY + kBodyBottom, footY + kBodyTop, &blockMin, &blockMax) ) {
        return newDist;
    }
    const float kPushGap = 0.01f; // 面から少し離す（毎フレーム再衝突しない）
    if ( prevDist <= blockMin ) { return blockMin - kPushGap; }
    if ( prevDist >= blockMax ) { return blockMax + kPushGap; }
    return prevDist; // 万一内部に居たら動かさない
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
    // 積み重なったブロックの中に居たら1段ずつ上へ（念のため回数に上限）
    for ( int guard = 0; guard < 10; ++guard ) {
        if ( !blocks_->BlockedAt(rail, dist, footY + kBodyBottom, footY + kBodyTop, nullptr, nullptr, &embedTop) ) break;
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
