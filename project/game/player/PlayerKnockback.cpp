#include "game/player/PlayerKnockback.h"

#include <cmath>

void PlayerKnockback::Start(const Vector3& awayDir, float facingYaw){
    float len = std::sqrt(awayDir.x * awayDir.x + awayDir.z * awayDir.z);
    if ( len >= 1e-4f ) {
        knockDir_ = { awayDir.x / len, 0.0f, awayDir.z / len };
    } else {
        // 真上/真下で重なっていた時は、向いている方向の逆へ弾く
        knockDir_ = { -std::sin(facingYaw), 0.0f, -std::cos(facingYaw) };
    }
    knockTimer_      = kKnockTime;
    invincibleTimer_ = kInvincibleTime;
}
