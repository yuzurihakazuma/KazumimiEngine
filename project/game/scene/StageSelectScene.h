#pragma once
// =====================================================================
//  StageSelectScene：ステージ選択シーン（仮）。
//   タイトルで「スタート」を決定した後に来るシーン。
//   今はまだ中身が無く、キーでシーンを切り替えるだけ：
//     SPACE … ゲームプレイシーンへ
//     T     … タイトルシーンへ戻る
//   カメラ・ポストエフェクト・デバッグ描画などの土台は BaseScene
// =====================================================================
#include "game/scene/BaseScene.h"

#include <memory>

class SDFText;

class StageSelectScene : public BaseScene {
public:
	StageSelectScene();
	~StageSelectScene() override;

	void Reload() override {}

private:
	// --- BaseScene の差し込み口 ---
	void OnInitialize() override;
	void OnFinalize() override;
	void OnUpdate() override;
	void OnDrawOverlay2D(ID3D12GraphicsCommandList* commandList) override; // 仮の案内文字

	// 仮の案内文字（シーン名と操作）
	std::unique_ptr<SDFText> titleText_;
	std::unique_ptr<SDFText> guideText_;

	bool sceneChanging_ = false; // 切り替えを要求済み（同じフレームで二重に要求しない）
};
