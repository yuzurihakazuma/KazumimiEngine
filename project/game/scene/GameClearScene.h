#pragma once

#include "Engine/Scene/IScene.h"
#include "Engine/Math/Struct.h"

#include <memory>

// ゲームクリアシーン内で使うクラスを前方宣言する
class Camera;
class MapManager;
class Sprite;

class GameClearScene : public IScene {
public:
    GameClearScene();
    ~GameClearScene();

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
    void DrawDebugUI() override {}

private:
    // ブロック背景を映すためのカメラ
    std::unique_ptr<Camera> camera_ = nullptr;

    // ゲームオーバーと同じブロック背景を描画するためのマップ
    std::unique_ptr<MapManager> mapManager_ = nullptr;

    // GAME CLEARのPNGを描画するスプライト
    std::unique_ptr<Sprite> gameClearSprite_ = nullptr;

    // ゲームオーバーと同じ向きで背景を見るためのカメラ設定
    Vector3 gameClearBgCameraPos_ = { 49.0f, 16.0f, 49.1f };
    Vector3 gameClearBgCameraRot_ = { 1.57f, -1.57f, 0.0f };

    // MapManagerの描画基準にする中心位置
    Vector3 gameClearBgPlayerPos_ = { 0.0f, 0.0f, 0.0f };
};
