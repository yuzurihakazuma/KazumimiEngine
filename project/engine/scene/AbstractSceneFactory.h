#pragma once
// --- 標準ライブラリ ---
#include <string>
#include <memory>

// 前方宣言
class IScene;

// シーン生成のための抽象クラス
class AbstractSceneFactory {
public:
	// デストラクタ
	virtual ~AbstractSceneFactory() = default;

	// シーン生成
	virtual std::unique_ptr<IScene> CreateScene(const std::string& sceneName) = 0;

};