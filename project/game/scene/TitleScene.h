#pragma once
// =====================================================================
//  TitleScene：タイトルシーン。
//   スタート案内を出し、T でゲームプレイシーンへ進む。
//   レールは使わない（エディタもタイトルではレールの線・ノードを出さない）。
//   カメラ・ポストエフェクト/Bloom・デバッグ描画・エディタで置いた物・展示物などは
//   BaseScene が持つので、ゲームプレイシーンと同じようにエディタで編集・確認できる
// =====================================================================
#include "game/scene/BaseScene.h"
#include "engine/math/struct.h"

#include <memory>
#include <string>

class Sprite;
class SDFText;

class TitleScene : public BaseScene {
public:
	TitleScene();
	~TitleScene() override;

	void Reload() override {}

private:
	// --- BaseScene の差し込み口 ---
	void OnLoadResources() override;
	void OnInitialize() override;
	void OnUpdate() override;
	void OnDrawOverlay2D(ID3D12GraphicsCommandList* commandList) override; // スタート案内（Game View にも映る）
	void OnDrawUI(ID3D12GraphicsCommandList* commandList) override;        // スプライト
	void OnDebugUI() override;

	// スタート案内の点滅
	void UpdateTitleAnimation();

	
	std::string bgmFile_ = "resources/BGMDon.mp3";

	// スタート案内
	std::unique_ptr<SDFText> startText_;
	float titleAnimTime_ = 0.0f;
};
