#pragma once

// --- エンジン側のファイル ---
#include "engine/utils/Level/LevelData.h"
#include "engine/base/Singleton.h"
#include <string>



// レベルマネージャー（シングルトン）
class LevelManager : public Singleton<LevelManager> {
	friend class Singleton<LevelManager>; // 土台（Singleton）だけが作れる
public:

    // セーブ機能（現在のデータをJSONファイルに書き出す）
    void Save(const std::string& fileName, const LevelData& levelData);

    // ロード機能（JSONファイルからデータを読み込む）
    LevelData Load(const std::string& fileName);

private:
	LevelManager() = default;// コンストラクタはprivateにしてシングルトンを実現
	~LevelManager() = default; // デストラクタもデフォルトでOK
};