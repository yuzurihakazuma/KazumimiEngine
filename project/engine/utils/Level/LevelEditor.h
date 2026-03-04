#pragma once
#include "LevelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include <vector>
#include <memory>
#include <string>

// マップエディタ専用クラス
class LevelEditor{
public:
    // 初期化
    void Initialize();
    // 毎フレームの処理（ImGuiの表示とオブジェクトの更新）
    void Update();
    // 配置したオブジェクトの描画
    void Draw();

    // マップの読み込み＆生成
    void LoadAndCreateMap(const std::string& fileName);

private:
    LevelData levelData_; // 現在のマップデータ
    std::vector<std::unique_ptr<Obj3d>> object3ds_; // 配置された3Dオブジェクト
    int selectedObjectIndex_ = -1; // 選択中のオブジェクト番号
};