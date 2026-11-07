#include "DebugCamera.h"

// --- 標準・外部ライブラリ ---
#include <algorithm>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

// --- エンジン側のファイル ---
#include "Camera.h" 
#include "engine/math/VectorMath.h"
#include "engine/math/Matrix4x4.h"
#include "engine/base/Input.h" // ★作成したInputクラスを使う！
#include "engine/base/TimeManager.h"

#include <cmath>


using namespace VectorMath;
using namespace MatrixMath;

void DebugCamera::Initialize() {
	// DirectInputのおかげで初期化時に記憶するものがなくなりました！
}

void DebugCamera::Update(Camera* camera) {
    if (!camera){
        return;
    }

    // UI等で使うために現在のターゲットカメラを保持
    targetCamera_ = camera;

    // アクティブじゃない時は何もしない
    if (!isActive_){
        return;
    }

    // 現在のカメラの位置と回転を取得
    Vector3 translation = camera->GetWorldPosition();
    Vector3 rotation = camera->GetRotation();

    // Inputクラスのインスタンスを取得
    Input* input = Input::GetInstance();

    // ドラッグを押し始めた瞬間に、Game View の上だったかを覚える（パネル上で始めたドラッグは無視する）
    const bool rightDown  = input->PushMouseButton(1);
    const bool middleDown = input->PushMouseButton(2);
    const bool dragStarted = ( rightDown && !rightWasDown_ ) || ( middleDown && !middleWasDown_ );
    if (rightDown  && !rightWasDown_)  { rotateDragValid_ = gameViewDragHovered_; }
    if (middleDown && !middleWasDown_) { panDragValid_    = gameViewDragHovered_; }
    if (!rightDown)  { rotateDragValid_ = false; }
    if (!middleDown) { panDragValid_    = false; }
    rightWasDown_  = rightDown;
    middleWasDown_ = middleDown;
    const bool rotating = rightDown && rotateDragValid_;
    const bool panning  = middleDown && panDragValid_;

    // マウスの動いた量（画面のピクセル）とホイールの量（ノッチ）。
    //   DirectInput の量はマウスの読み取りの細かさそのまま（感度の高いマウスでは画面の何倍にもなる）で、
    //   窓に戻った直後にはたまった分が一度に来るため、たまにカメラが大きく飛んでいた。
    //   エディタのある構成では、OS のカーソルの動き（ImGui）を使う。押し始めたフレームは動かさない
    float mouseDx = 0.0f, mouseDy = 0.0f, wheelNotches = 0.0f;
#ifdef USE_IMGUI
    {
        const ImGuiIO& io = ImGui::GetIO();
        if ( std::isfinite(io.MouseDelta.x) && std::isfinite(io.MouseDelta.y) && std::abs(io.MouseDelta.x) < 1e4f && std::abs(io.MouseDelta.y) < 1e4f ) {
            mouseDx = io.MouseDelta.x;
            mouseDy = io.MouseDelta.y;
        }
        wheelNotches = io.MouseWheel;
    }
#else
    mouseDx = input->GetMouseMoveX();
    mouseDy = input->GetMouseMoveY();
    wheelNotches = input->GetMouseWheel() / 120.0f;
#endif
    if ( dragStarted ) { mouseDx = mouseDy = 0.0f; }
    mouseDx = std::clamp(mouseDx, -120.0f, 120.0f);
    mouseDy = std::clamp(mouseDy, -120.0f, 120.0f);
    wheelNotches = std::clamp(wheelNotches, -3.0f, 3.0f);
    // 1フレームの長さ（止めた時間に関係なく、実時間。引っかかったフレームで一気に進まないよう上限付き）
    const float frameSeconds = std::clamp(Time::GetInstance()->GetUnscaledDeltaTime(), 0.0f, 1.0f / 20.0f);

    // 右ドラッグ中：視点回転（先に回転を確定させてから移動方向を計算する）
    if (rotating) {
        float dx = mouseDx;
        float dy = mouseDy;
        const float rotationSpeed = 0.003f; // 回転の感度
        rotation.y += dx * rotationSpeed;
        rotation.x += dy * rotationSpeed;
        const float limit = 1.5f; // 約85度で頭打ち
        rotation.x = std::clamp(rotation.x, -limit, limit);
    }

    // 向いている角度から「前」「右」「上」の方向を求める（移動系で共通利用）
    Matrix4x4 rotateMatrix = Multiply(MakeRotateX(rotation.x), MakeRotateY(rotation.y));
    Vector3 forward = Transforms({ 0.0f, 0.0f, 1.0f }, rotateMatrix);
    Vector3 right   = Transforms({ 1.0f, 0.0f, 0.0f }, rotateMatrix);
    Vector3 up      = Transforms({ 0.0f, 1.0f, 0.0f }, rotateMatrix);

    float moveSpeed = 12.0f * frameSeconds; // 毎秒 12m（以前の 60fps で 0.2m/フレームと同じ）
    if (input->Pushkey(DIK_LSHIFT)) { moveSpeed *= 3.0f; } // Shiftで高速

    // WASD / Q,E で移動（右ドラッグ中のみ。視点を回しながら飛び回る用）
    if (rotating) {
        if (input->Pushkey(DIK_W)) translation = Add(translation, Multiply( moveSpeed, forward));
        if (input->Pushkey(DIK_S)) translation = Add(translation, Multiply(-moveSpeed, forward));
        if (input->Pushkey(DIK_D)) translation = Add(translation, Multiply( moveSpeed, right));
        if (input->Pushkey(DIK_A)) translation = Add(translation, Multiply(-moveSpeed, right));
        if (input->Pushkey(DIK_E)) translation = Add(translation, Multiply( moveSpeed, up));
        if (input->Pushkey(DIK_Q)) translation = Add(translation, Multiply(-moveSpeed, up));
    }

    // ★ホイール：前後ズーム（Game View にマウスが乗っている時だけ効く）。
    //   ImGuiパネル（レールエディタ等）の上でスクロールしてもカメラは動かさない。
    //   1フレームの適用量は3ノッチ分まで（差分スパイクで一気に吹っ飛ばない保険）
    if (wheelNotches != 0.0f && gameViewHovered_) {
        const float zoomPerNotch = 1.2f; // 1ノッチで進む距離(m)
        translation = Add(translation, Multiply(wheelNotches * zoomPerNotch, forward));
    }

    // ★中ドラッグ：平行移動（パン）。画面の右/上方向にスライド
    if (panning) {
        float dx = mouseDx;
        float dy = mouseDy;
        const float panSpeed = 0.01f;
        translation = Add(translation, Multiply(-dx * panSpeed, right)); // マウス右→視界左へ流れる
        translation = Add(translation, Multiply( dy * panSpeed, up));    // マウス下→視界上へ流れる
    }

    // 更新した新しい位置と角度を本物のカメラにセット！
    camera->SetTranslation(translation);
    camera->SetRotation(rotation);
}

// 外部（メニュー等）からの ON/OFF。切替時に元カメラ姿勢を保存/復元する
void DebugCamera::SetActive(bool active){
    if (active == isActive_) return;
    if (targetCamera_) {
        if (active) {
            // OFF→ON：元の見た目を覚えておく
            preCameraPos_ = targetCamera_->GetTransform().translate;
            preCameraRot_ = targetCamera_->GetTransform().rotate;
        } else {
            // ON→OFF：元の見た目に戻す
            targetCamera_->SetTranslation(preCameraPos_);
            targetCamera_->SetRotation(preCameraRot_);
        }
    }
    isActive_ = active;
}
