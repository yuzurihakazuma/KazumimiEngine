#pragma once
// =====================================================================
//  EditorCameraUtil：エディタでメインカメラを置き直す操作（共通）。
//   敵エディタ・配置ビューの「カメラをここへ」、インスペクターの視点プリセット、
//   ミニマップの空クリックが使う。どれも今のカメラの向きを基準にする
// =====================================================================
#include <vector>

class Camera;
class SplineRail;

namespace EditorCameraUtil {
    // レール上の場所（レール番号・距離・レール面からの高さ）が画面の中央に来るように置く。
    //   向きは変えずに、見ている方向の逆へ引いた位置へ動かす
    void FocusOnRail(Camera& camera, const std::vector<SplineRail>& rails,
                     int railIndex, float distance, float height);

    // 真上から見下ろして、レール全体が収まる高さに置く
    void TopView(Camera& camera, const std::vector<SplineRail>& rails);

    // 既定の斜め視点に戻す
    void DefaultAngle(Camera& camera);

    // ワールドの (x, z) が画面の中央に来るように水平に動かす（向きと高さは今のまま）
    void CenterOnXZ(Camera& camera, float worldX, float worldZ);
}
