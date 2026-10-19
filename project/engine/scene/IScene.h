#pragma once

#include <string>

// シーンのインターフェース
class IScene{
public:
	// 仮想デストラクタ
	virtual ~IScene() = default;

	// シーンの基本操作
	
	// 初期化
	virtual void Initialize() = 0;
	// 終了
	virtual void Finalize() = 0;
	// 更新
	virtual void Update() = 0;
	// 描画
	virtual void Draw() = 0;

	virtual void Reload(){}
	// デバッグ用UIの描画
	virtual void DrawDebugUI() {}

	// 遷移の退場演出（紙芝居の「起き倒し」など）。遷移の演出が始めて、終わるのを待つ。
	//   退場の動きが無いシーンは何もしない（すぐ終わった扱い）
	virtual void BeginExitEffect() {}
	virtual bool IsExitEffectDone() const { return true; }

	// 遷移の演出からの合図（ステージ開始の「振り向け」「アクション」「カメラを戻せ」など）。
	//   合図の名前は遷移の定義（resources/transition/*.json の sceneCue）に書く。知らない合図は無視する
	virtual void OnTransitionCue(const std::string& /*cue*/) {}

};