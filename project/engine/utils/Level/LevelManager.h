#pragma once

// --- エンジン側のファイル ---
#include "engine/utils/Level/LevelData.h"
#include <string>



// レベルマネージャー（シングルトン）
class LevelManager{
public:
    static LevelManager* GetInstance(){
        static LevelManager instance;
        return &instance;
    }

    // セーブ機能（現在のデータをJSONファイルに書き出す）
    void Save(const std::string& fileName, const LevelData& levelData);

    // ロード機能（JSONファイルからデータを読み込む）
    LevelData Load(const std::string& fileName);

private:
	LevelManager() = default;// コンストラクタはprivateにしてシングルトンを実現
	~LevelManager() = default; // デストラクタもデフォルトでOK
	LevelManager(const LevelManager&) = delete; // コピーコンストラクタを削除
	LevelManager& operator=(const LevelManager&) = delete; // コピー代入演算子を削除
};