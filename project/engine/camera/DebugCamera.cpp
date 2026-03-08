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

    // 1 = 右クリックが押されているか判定
    if (input->PushMouseButton(1)) {

        // --- マウスによる視点回転 ---
        // マウスの移動量を取得
        float dx = input->GetMouseMoveX();
        float dy = input->GetMouseMoveY();

        const float rotationSpeed = 0.003f; // 回転の感度
        rotation.y += dx * rotationSpeed;
        rotation.x += dy * rotationSpeed;

        // X軸（上下）の回転を制限
        const float limit = 1.5f; // 約85度
        rotation.x = std::clamp(rotation.x, -limit, limit);


        // --- キーボードによる移動操作 (右クリック中のみ) ---
        // 向いている角度から「前」「右」「上」の方向を計算
        Matrix4x4 rotateMatrix = Multiply(MakeRotateX(rotation.x), MakeRotateY(rotation.y));
        Vector3 forward = Transforms({ 0.0f, 0.0f, 1.0f }, rotateMatrix);
        Vector3 right = Transforms({ 1.0f, 0.0f, 0.0f }, rotateMatrix);
        Vector3 up = Transforms({ 0.0f, 1.0f, 0.0f }, rotateMatrix);

        float moveSpeed = 0.2f;

        // 左Shiftキーを押していると高速移動
        if (input->Pushkey(DIK_LSHIFT)) {
            moveSpeed *= 3.0f;
        }

        // WASD キーで移動
        if (input->Pushkey(DIK_W)) {
            translation = Add(translation, Multiply(moveSpeed, forward));

        }

        if (input->Pushkey(DIK_S)) {
            translation = Add(translation, Multiply(-moveSpeed, forward));
        }
        if (input->Pushkey(DIK_D)) {
            translation = Add(translation, Multiply(moveSpeed, right));
        }
        if (input->Pushkey(DIK_A)) {
            translation = Add(translation, Multiply(-moveSpeed, right));
        }

        // E, Q キーで上下移動
        if (input->Pushkey(DIK_E)) {
            translation = Add(translation, Multiply(moveSpeed, up));
        }
        if (input->Pushkey(DIK_Q)) {
            translation = Add(translation, Multiply(-moveSpeed, up));
        }
    }

    // 更新した新しい位置と角度を本物のカメラにセット！
    camera->SetTranslation(translation);
    camera->SetRotation(rotation);
}


void DebugCamera::DrawDebugUI(){
#ifdef USE_IMGUI
    if ( ImGui::Begin("インスペクター (詳細設定)") ) {
        if ( ImGui::CollapsingHeader("デバッグカメラ (Debug Camera)", ImGuiTreeNodeFlags_DefaultOpen) ) {

            bool isPreDebugCameraActive = isActive_;

            // チェックボックスでON/OFFを切り替え
            ImGui::Checkbox("デバッグカメラを有効化", &isActive_);

            // ターゲットとなるカメラが存在し、状態が切り替わったときの処理
            if ( targetCamera_ ) {
                // OFF -> ON になった瞬間（切り替え前の状態を保存）
                if ( isActive_ && !isPreDebugCameraActive ) {
                    preCameraPos_ = targetCamera_->GetTransform().translate;
                    preCameraRot_ = targetCamera_->GetTransform().rotate;
                }
                // ON -> OFF になった瞬間（切り替え前の状態を復元）
                if ( !isActive_ && isPreDebugCameraActive ) {
                    targetCamera_->SetTranslation(preCameraPos_);
                    targetCamera_->SetRotation(preCameraRot_);
                }
            }

            // 操作方法の説明を追加（ユーザーに親切な設計）
            ImGui::TextDisabled("操作方法:");
            ImGui::Text("  右ドラッグ : 視点回転");
            ImGui::Text("  ホイール   : 前後移動");
            ImGui::Text("  中ドラッグ : 平行移動");
            ImGui::Text("  WASD       : 水行・昇降移動");
        }
    }
    ImGui::End();
#endif
}