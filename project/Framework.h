#pragma once
#include <Windows.h>
#include <memory>

// フレームワーククラス
class Framework{
public:
	// 仮想デストラクタ (継承する場合の必須マナー)
	virtual ~Framework() = default;

	// 初期化
	virtual void Initialize();

	// 終了
	virtual void Finalize();

	// 毎フレーム更新
	virtual void Update();

	// 描画 (純粋仮想関数：継承先で必ず実装させる)
	virtual void Draw() = 0;

	// 終了チェック
	virtual bool IsEndRequest();

	// 実行
	void Run();

protected: // 継承先のGameクラスでも使えるようにする

	bool endRequest_ = false;


};