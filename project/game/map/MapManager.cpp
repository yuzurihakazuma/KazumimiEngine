#include "MapManager.h"

#include <cmath>
#include "../../engine/utils/Level/LevelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/utils/ImGuiManager.h"
#include "engine/3d/model/ModelManager.h"

// デフォルト生成
MapManager::MapManager() = default;

// デフォルト破棄
MapManager::~MapManager() = default;

// 初期化
void MapManager::Initialize() {

    // サイズ同期
    editWidth_ = levelData_.width;
    editHeight_ = levelData_.height;

    // マップ生成初期化
    mapGenerator_ = std::make_unique<MapGenerator>();
    mapGenerator_->Initialize();

    // 初期マップ設定
    currentMapFile_ = "resources/map/map01.json";
    mapType_ = 0;
    saveFileName_ = "map01.json";

    // 床インスタンス
    floorGroup_ = std::make_unique<InstancedGroup>();
    floorGroup_->Initialize("block", 10000);

    // 壁インスタンス
    wallGroup_ = std::make_unique<InstancedGroup>();
    wallGroup_->Initialize("block", 10000);

    // 階段インスタンス
    stairsGroup_ = std::make_unique<InstancedGroup>();
    stairsGroup_->Initialize("block", 10000);

    // マップ読み込み
    LoadAndCreateMap(currentMapFile_);
}

// マップ読み込み＋生成
void MapManager::LoadAndCreateMap(const std::string& fileName) {

    // 読み込み
    levelData_ = LevelManager::GetInstance()->Load(fileName);

    // サイズ同期
    editWidth_ = levelData_.width;
    editHeight_ = levelData_.height;

    // オブジェクト再構築
    RebuildMapObjects();

    // 変更フラグ
    mapChanged_ = true;

    // 現在ファイル更新
    currentMapFile_ = fileName;
}

// 配列サイズ調整
void MapManager::ResizeObjectGrids() {

    // 行数確保
    floorObjects_.resize(levelData_.height);
    wallObjects_.resize(levelData_.height);

    // 列数確保
    for (int z = 0; z < levelData_.height; ++z) {
        floorObjects_[z].resize(levelData_.width);
        wallObjects_[z].resize(levelData_.width);
    }
}

// マップ再構築
void MapManager::RebuildMapObjects() {

    // サイズ調整
    ResizeObjectGrids();

    // 全タイル更新
    for (int z = 0; z < levelData_.height; ++z) {
        for (int x = 0; x < levelData_.width; ++x) {
            UpdateTileObject(x, z);
        }
    }
}

// 更新処理
void MapManager::Update(const Vector3& playerPos) {

    // インスタンス初期化
    if (floorGroup_) floorGroup_->PreUpdate();
    if (wallGroup_) wallGroup_->PreUpdate();
    if (stairsGroup_) stairsGroup_->PreUpdate();

    const float tileSize = levelData_.tileSize;

    // プレイヤー中心タイル
    int centerX = static_cast<int>(std::round(playerPos.x / tileSize));
    int centerZ = static_cast<int>(std::round(playerPos.z / tileSize));

    // 描画範囲
    const int range = 16;

    int minX = std::max(0, centerX - range);
    int maxX = std::min(levelData_.width - 1, centerX + range);
    int minZ = std::max(0, centerZ - range);
    int maxZ = std::min(levelData_.height - 1, centerZ + range);

    // 範囲内だけ更新
    for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {

            // 床更新
            if (floorObjects_[z][x]) {
                floorObjects_[z][x]->Update();
                floorGroup_->AddObject(floorObjects_[z][x].get());
            }

            // 壁・階段更新
            if (wallObjects_[z][x]) {
                wallObjects_[z][x]->Update();

                int tile = levelData_.tiles[z][x];
                if (tile == 1) {
                    wallGroup_->AddObject(wallObjects_[z][x].get());
                } else if (tile == 3) {
                    stairsGroup_->AddObject(wallObjects_[z][x].get());
                }
            }
        }
    }
}

// 描画
void MapManager::Draw(const Vector3& playerPos) {

    // 床描画
    if (floorGroup_) floorGroup_->Draw(camera_);

    // 壁描画
    if (wallGroup_) wallGroup_->Draw(camera_);

    // 階段描画
    if (stairsGroup_) stairsGroup_->Draw(camera_);
}

// デバッグUI
void MapManager::DrawDebugUI() {
#ifdef USE_IMGUI
    // エディタ有効時
    if (isEditorActive) {

        // 部屋作成用
        static int roomX = 1;
        static int roomZ = 1;
        static int roomWidth = 5;
        static int roomHeight = 5;

        ImGui::Begin("マップエディタ");
        ImGui::Text("マップ切り替え");

        // 通常マップ切り替え
        if (ImGui::RadioButton("通常マップ(map01.json)", mapType_ == 0)) {
            mapType_ = 0;
            saveFileName_ = "map01.json";
            LoadAndCreateMap("resources/map/map01.json");
        }
        ImGui::SameLine();

        // ボスマップ切り替え
        if (ImGui::RadioButton("ボス部屋(boss.json)", mapType_ == 1)) {
            mapType_ = 1;
            saveFileName_ = "boss.json";
            LoadAndCreateMap("resources/map/boss.json");
        }

        ImGui::Separator();

        // 保存名入力
        char buffer[256];
        strcpy_s(buffer, saveFileName_.c_str());
        if (ImGui::InputText("保存ファイル名", buffer, sizeof(buffer))) {
            saveFileName_ = buffer;
        }

        // 保存パス
        std::string fullPath = "resources/map/" + saveFileName_;

        // 保存
        if (ImGui::Button("マップを保存")) {
            LevelManager::GetInstance()->Save(fullPath, levelData_);
        }
        ImGui::SameLine();

        // 読み込み
        if (ImGui::Button("マップを読み込む")) {
            LoadAndCreateMap(fullPath);
        }
        ImGui::SameLine();

        // クリア
        if (ImGui::Button("マップをクリア")) {
            FillAllTiles(0);
        }
        ImGui::SameLine();

        // 全部壁
        if (ImGui::Button("全部壁(1)で埋める")) {
            FillAllTiles(1);
        }

        // 全部床
        if (ImGui::Button("全部壁(0)で埋める")) {
            FillAllTiles(0);
        }

        ImGui::Separator();

        // サイズ入力
        ImGui::InputInt("幅", &editWidth_);
        ImGui::InputInt("高さ", &editHeight_);

        // 最小補正
        if (editWidth_ < 1) { editWidth_ = 1; }
        if (editHeight_ < 1) { editHeight_ = 1; }

        // サイズ反映
        if (ImGui::Button("サイズを反映")) {
            levelData_.width = editWidth_;
            levelData_.height = editHeight_;

            levelData_.tiles.resize(levelData_.height);
            for (auto& row : levelData_.tiles) {
                row.resize(levelData_.width, 0);
            }

            RebuildMapObjects();
        }

        ImGui::Separator();

        ImGui::Text("部屋作成");

        // 部屋入力
        ImGui::InputInt("部屋開始X", &roomX);
        ImGui::InputInt("部屋開始Z", &roomZ);
        ImGui::InputInt("部屋幅", &roomWidth);
        ImGui::InputInt("部屋高さ", &roomHeight);

        // 最小補正
        if (roomWidth < 1) { roomWidth = 1; }
        if (roomHeight < 1) { roomHeight = 1; }

        // 部屋作成
        if (ImGui::Button("部屋を作る")) {
            CreateRoom(roomX, roomZ, roomWidth, roomHeight);
        }
        ImGui::SameLine();

        // 中央部屋作成
        if (ImGui::Button("中央に部屋を作る")) {
            int autoRoomWidth = 6;
            int autoRoomHeight = 6;

            int startX = (levelData_.width - autoRoomWidth) / 2;
            int startZ = (levelData_.height - autoRoomHeight) / 2;

            CreateRoom(startX, startZ, autoRoomWidth, autoRoomHeight);
        }

        // ランダム部屋数
        static int randomRoomCount = 8;

        ImGui::Separator();
        ImGui::Text("ランダムダンジョン生成");

        // 部屋数入力
        ImGui::InputInt("部屋数", &randomRoomCount);
        if (randomRoomCount < 1) { randomRoomCount = 1; }

        // 部屋だけ生成
        if (ImGui::Button("部屋だけ生成")) {
            GenerateRandomDungeon(randomRoomCount);
        }

        ImGui::SameLine();

        // 部屋＋通路生成
        if (ImGui::Button("部屋+通路生成")) {
            GenerateRandomDungeon(randomRoomCount);
        }

        ImGui::Separator();

        ImGui::Text("配置タイル");

        // 配置タイル選択
        ImGui::RadioButton("床(0)", &selectedTile_, 0);
        ImGui::SameLine();
        ImGui::RadioButton("壁(1)", &selectedTile_, 1);
        ImGui::SameLine();
        ImGui::RadioButton("階段(3)", &selectedTile_, 3);

        ImGui::Separator();

        // 行数補正
        if ((int)levelData_.tiles.size() != levelData_.height) {
            levelData_.tiles.resize(levelData_.height);
        }

        // 列数補正
        for (auto& row : levelData_.tiles) {
            if ((int)row.size() != levelData_.width) {
                row.resize(levelData_.width, 0);
            }
        }

        ImGui::Text("グリッド編集");

        // グリッド表示
        for (int viewZ = 0; viewZ < levelData_.height; ++viewZ) {
            int dataZ = levelData_.height - 1 - viewZ;

            for (int x = 0; x < levelData_.width; ++x) {

                int tile = levelData_.tiles[dataZ][x];

                // 床色
                if (tile == 0) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 1.0f, 1.0f));
                }
                // 壁色
                else if (tile == 1) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                }
                // 階段色
                else if (tile == 3) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 1.0f, 1.0f, 1.0f));
                }

                // ボタン名
                std::string label =
                    std::to_string(tile) +
                    "##" + std::to_string(dataZ) + "_" + std::to_string(x);

                // タイル変更
                if (ImGui::Button(label.c_str(), ImVec2(24, 24))) {
                    if (levelData_.tiles[dataZ][x] != selectedTile_) {
                        levelData_.tiles[dataZ][x] = selectedTile_;
                        UpdateTileObject(x, dataZ);
                    }
                }

                ImGui::PopStyleColor();

                // 横並び
                if (x < levelData_.width - 1) {
                    ImGui::SameLine();
                }
            }
        }

        ImGui::End();
    }
#endif
}

// 全タイル埋める
void MapManager::FillAllTiles(int tileType) {
    if (!mapGenerator_) {
        return;
    }

    mapGenerator_->FillAllTiles(levelData_, tileType);
    RebuildMapObjects();
}

// 部屋生成
void MapManager::CreateRoom(int startX, int startZ, int roomWidth, int roomHeight) {
    if (!mapGenerator_) {
        return;
    }

    mapGenerator_->CreateRoom(levelData_, startX, startZ, roomWidth, roomHeight);
    RebuildMapObjects();
}

// ランダムダンジョン生成
void MapManager::GenerateRandomDungeon(int roomCount) {
    if (!mapGenerator_) {
        return;
    }

    mapGenerator_->GenerateRandomDungeon(levelData_, roomCount);
    RebuildMapObjects();
}

// プレイヤー初期位置取得
Vector3 MapManager::GetRandomPlayerSpawnPosition(float y) {
    if (!mapGenerator_) {
        return { 0.0f, levelData_.baseY + y, 0.0f };
    }

    return mapGenerator_->GetRandomPlayerSpawnPosition(levelData_, y);
}

// マップ中心座標取得
Vector3 MapManager::GetMapCenterPosition(float y) const {
    const float tileSize = levelData_.tileSize;

    Vector3 pos;
    pos.x = ((float)levelData_.width - 1.0f) * tileSize * 0.5f;
    pos.y = levelData_.baseY + y;
    pos.z = ((float)levelData_.height - 1.0f) * tileSize * 0.5f;

    return pos;
}

// マップ変更消費
bool MapManager::ConsumeMapChanged() {
    if (mapChanged_) {
        mapChanged_ = false;
        return true;
    }
    return false;
}

// 通常マップ切り替え
void MapManager::ChangeToNormalMap() {
    mapType_ = 0;
    currentMapFile_ = "resources/map/map01.json";
    saveFileName_ = "map01.json";
    LoadAndCreateMap(currentMapFile_);
}

// ボスマップ切り替え
void MapManager::ChangeToBossMap() {
    mapType_ = 1;
    currentMapFile_ = "resources/map/boss.json";
    saveFileName_ = "boss.json";
    LoadAndCreateMap(currentMapFile_);
}

// タイル→ワールド座標変換
Vector3 MapManager::GetTileWorldPosition(int x, int z, float y) const {
    if (!mapGenerator_) {
        Vector3 pos;
        pos.x = x * levelData_.tileSize;
        pos.y = levelData_.baseY + y;
        pos.z = z * levelData_.tileSize;
        return pos;
    }

    return mapGenerator_->GetTileWorldPosition(levelData_, x, z, y);
}

// 階段ランダム配置
void MapManager::PlaceStairsTileRandom(const Vector3& avoidWorldPos, float avoidDistance) {
    if (!mapGenerator_) {
        return;
    }

    mapGenerator_->PlaceStairsTileRandom(levelData_, avoidWorldPos, avoidDistance);
    RebuildMapObjects();
}

// タイルオブジェクト更新
void MapManager::UpdateTileObject(int x, int z) {
    if (z < 0 || z >= levelData_.height || x < 0 || x >= levelData_.width) return;

    // モデル取得
    Model* model = ModelManager::GetInstance()->FindModel("block");
    if (model == nullptr) return;

    const float tileSize = levelData_.tileSize;
    const int tile = levelData_.tiles[z][x];

    // 床
    if (tile == 0) {
        if (!floorObjects_[z][x]) {
            floorObjects_[z][x] = std::make_unique<Obj3d>();
            floorObjects_[z][x]->Initialize(model);
            floorObjects_[z][x]->SetCamera(camera_);
        }

        Vector3 pos = { x * tileSize, levelData_.baseY, z * tileSize };
        floorObjects_[z][x]->SetTranslation(pos);
        floorObjects_[z][x]->SetRotation({ 0.0f, 0.0f, 0.0f });
        floorObjects_[z][x]->SetScale({ 1.0f, 1.0f, 1.0f });

        floorObjects_[z][x]->Update();
    }

    // 壁 or 階段
    if (tile == 1 || tile == 3) {
        if (!wallObjects_[z][x]) {
            wallObjects_[z][x] = std::make_unique<Obj3d>();
            wallObjects_[z][x]->Initialize(model);
            wallObjects_[z][x]->SetCamera(camera_);
        }

        // 壁
        if (tile == 1) {
            Vector3 pos = { x * tileSize, levelData_.baseY + tileSize, z * tileSize };
            wallObjects_[z][x]->SetTranslation(pos);
            wallObjects_[z][x]->SetScale({ 1.0f, 1.0f, 1.0f });
        }
        // 階段
        else {
            Vector3 pos = { x * tileSize, levelData_.baseY + 2.5f, z * tileSize };
            wallObjects_[z][x]->SetTranslation(pos);
            wallObjects_[z][x]->SetScale({ 1.0f, 2.0f, 1.0f });
        }

        wallObjects_[z][x]->Update();
    }
}

// 階段配置＋座標取得
std::pair<int, int> MapManager::PlaceStairsTileRandomAndGetTile(const Vector3& avoidWorldPos, float avoidDistance) {
    if (!mapGenerator_) {
        return { -1, -1 };
    }

    auto result = mapGenerator_->PlaceStairsTileRandomAndGetTile(levelData_, avoidWorldPos, avoidDistance);
    RebuildMapObjects();
    return result;
}

// ノイズテクスチャ設定
void MapManager::SetNoiseTexture(uint32_t index) {
    noiseTextureIndex_ = index;
    if (floorGroup_) floorGroup_->SetNoiseTexture(index);
    if (wallGroup_) wallGroup_->SetNoiseTexture(index);
    if (stairsGroup_) stairsGroup_->SetNoiseTexture(index);
}