#include "LevelEditor.h"

// --- 標準ライブラリ ---
#include <cmath>

// --- エンジン側のファイル ---
#include "LevelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/utils/ImGuiManager.h"
#include "engine/3d/model/ModelManager.h"

LevelEditor::LevelEditor() = default;
LevelEditor::~LevelEditor() = default;

void LevelEditor::Initialize() {
	LoadAndCreateMap("resources/map/map01.json");

	editWidth_ = levelData_.width;
	editHeight_ = levelData_.height;

	dungeonGenerator_ = std::make_unique<DungeonGenerator>();

	currentMapFile_ = "resources/map/map01.json";
	mapType_ = 0;
	saveFileName_ = "map01.json";
	LoadAndCreateMap(currentMapFile_);
}

void LevelEditor::LoadAndCreateMap(const std::string& fileName) {
	levelData_ = LevelManager::GetInstance()->Load(fileName);

	editWidth_ = levelData_.width;
	editHeight_ = levelData_.height;

	RebuildMapObjects();

	mapChanged_ = true;

	currentMapFile_ = fileName;
}

void LevelEditor::ResizeObjectGrids() {
	wallObjects_.resize(levelData_.height);

	for (int z = 0; z < levelData_.height; ++z) {
		wallObjects_[z].resize(levelData_.width);
	}
}

void LevelEditor::RebuildMapObjects() {
	ResizeObjectGrids();
	CreateFloorObject();

	for (int z = 0; z < levelData_.height; ++z) {
		for (int x = 0; x < levelData_.width; ++x) {
			UpdateTileObject(x, z);
		}
	}
}

void LevelEditor::Update(const Vector3& playerPos) {
	if (floorObject_) {
		floorObject_->Update();
	}

	for (int z = 0; z < levelData_.height; ++z) {
		for (int x = 0; x < levelData_.width; ++x) {
			if (wallObjects_[z][x]) {
				Vector3 objPos = wallObjects_[z][x]->GetTranslation();
				float diffX = objPos.x - playerPos.x;
				float diffZ = objPos.z - playerPos.z;
				float distSq = (diffX * diffX) + (diffZ * diffZ);

				if (distSq < 800.0f) {
					wallObjects_[z][x]->Update();
				}
			}
		}
	}
}
void LevelEditor::Draw(const Vector3& playerPos) {
	if (floorObject_) {
		floorObject_->Draw();
	}

	for (int z = 0; z < levelData_.height; ++z) {
		for (int x = 0; x < levelData_.width; ++x) {
			if (wallObjects_[z][x]) {
				Vector3 objPos = wallObjects_[z][x]->GetTranslation();
				float diffX = objPos.x - playerPos.x;
				float diffZ = objPos.z - playerPos.z;
				float distSq = diffX * diffX + diffZ * diffZ;

				if (distSq < 800.0f) {
					wallObjects_[z][x]->Draw();
				}
			}
		}
	}
}
void LevelEditor::DrawDebugUI() {
#ifdef USE_IMGUI
	if (isEditorActive) {

		static int roomX = 1;
		static int roomZ = 1;
		static int roomWidth = 5;
		static int roomHeight = 5;

		ImGui::Begin("マップエディタ");
		ImGui::Text("マップ切り替え");

		if (ImGui::RadioButton("通常マップ(map01.json)", mapType_ == 0)) {
			mapType_ = 0;
			saveFileName_ = "map01.json";
			LoadAndCreateMap("resources/map/map01.json");
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("ボス部屋(boss.json)", mapType_ == 1)) {
			mapType_ = 1;
			saveFileName_ = "boss.json";
			LoadAndCreateMap("resources/map/boss.json");

		}

		ImGui::Separator();


		char buffer[256];
		strcpy_s(buffer, saveFileName_.c_str());
		if (ImGui::InputText("保存ファイル名", buffer, sizeof(buffer))) {
			saveFileName_ = buffer;
		}

		std::string fullPath = "resources/map/" + saveFileName_;

		if (ImGui::Button("マップを保存")) {
			LevelManager::GetInstance()->Save(fullPath, levelData_);
		}
		ImGui::SameLine();
		if (ImGui::Button("マップを読み込む")) {
			LoadAndCreateMap(fullPath);
		}
		ImGui::SameLine();
		if (ImGui::Button("マップをクリア")) {
			FillAllTiles(0);
		}
		ImGui::SameLine();
		if (ImGui::Button("全部壁(1)で埋める")) {
			FillAllTiles(1);
		}
		if (ImGui::Button("全部壁(0)で埋める")) {
			FillAllTiles(0);
		}

		ImGui::Separator();

		ImGui::InputInt("幅", &editWidth_);
		ImGui::InputInt("高さ", &editHeight_);

		if (editWidth_ < 1) { editWidth_ = 1; }
		if (editHeight_ < 1) { editHeight_ = 1; }

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

		ImGui::InputInt("部屋開始X", &roomX);
		ImGui::InputInt("部屋開始Z", &roomZ);
		ImGui::InputInt("部屋幅", &roomWidth);
		ImGui::InputInt("部屋高さ", &roomHeight);

		if (roomWidth < 1) { roomWidth = 1; }
		if (roomHeight < 1) { roomHeight = 1; }

		if (ImGui::Button("部屋を作る")) {
			CreateRoom(roomX, roomZ, roomWidth, roomHeight);
		}
		ImGui::SameLine();
		if (ImGui::Button("中央に部屋を作る")) {
			int autoRoomWidth = 6;
			int autoRoomHeight = 6;

			int startX = (levelData_.width - autoRoomWidth) / 2;
			int startZ = (levelData_.height - autoRoomHeight) / 2;

			CreateRoom(startX, startZ, autoRoomWidth, autoRoomHeight);
		}

		static int randomRoomCount = 8;

		ImGui::Separator();
		ImGui::Text("ランダムダンジョン生成");
		ImGui::InputInt("部屋数", &randomRoomCount);
		if (randomRoomCount < 1) { randomRoomCount = 1; }

		if (ImGui::Button("部屋だけ生成")) {
			if (dungeonGenerator_) {
				dungeonGenerator_->GenerateRooms(levelData_, randomRoomCount);
				RebuildMapObjects();
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("部屋+通路生成")) {
			GenerateRandomDungeon(randomRoomCount);
		}

		ImGui::Separator();

		ImGui::Text("配置タイル");
		ImGui::RadioButton("床(0)", &selectedTile_, 0);
		ImGui::SameLine();
		ImGui::RadioButton("壁(1)", &selectedTile_, 1);

		ImGui::Separator();

		if ((int)levelData_.tiles.size() != levelData_.height) {
			levelData_.tiles.resize(levelData_.height);
		}
		for (auto& row : levelData_.tiles) {
			if ((int)row.size() != levelData_.width) {
				row.resize(levelData_.width, 0);
			}
		}

		ImGui::Text("グリッド編集");

		for (int viewZ = 0; viewZ < levelData_.height; ++viewZ) {
			int dataZ = levelData_.height - 1 - viewZ;

			for (int x = 0; x < levelData_.width; ++x) {

				int tile = levelData_.tiles[dataZ][x];

				// タイルの色を変更
				if (tile == 0) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 1.0f, 1.0f)); // 青
				} else if (tile == 1) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.3f, 0.3f, 1.0f)); // 赤
				}

				std::string label =
					std::to_string(tile) +
					"##" + std::to_string(dataZ) + "_" + std::to_string(x);

				if (ImGui::Button(label.c_str(), ImVec2(24, 24))) {
					if (levelData_.tiles[dataZ][x] != selectedTile_) {
						levelData_.tiles[dataZ][x] = selectedTile_;
						UpdateTileObject(x, dataZ);
					}
				}

				ImGui::PopStyleColor(); // 色戻す

				if (x < levelData_.width - 1) {
					ImGui::SameLine();
				}
			}
		}

		ImGui::End();
	}
#endif
}

void LevelEditor::UpdateTileObject(int x, int z) {
	if (z < 0 || z >= levelData_.height || x < 0 || x >= levelData_.width) {
		return;
	}

	Model* model = ModelManager::GetInstance()->FindModel("block");
	if (model == nullptr) {
		return;
	}

	// いったんそのマスの床・壁を消す
	//floorObjects_[z][x].reset();
	wallObjects_[z][x].reset();

	const float tileSize = levelData_.tileSize;
	const int tile = levelData_.tiles[z][x];

	

	// --------------------
	// 壁タイルなら上に壁を置く
	// --------------------
	if (tile == 1) {
		std::unique_ptr<Obj3d> wallObj = std::make_unique<Obj3d>();
		wallObj->Initialize(model);
		wallObj->SetCamera(camera_);

		Vector3 pos;
		pos.x = x * tileSize;
		pos.y = levelData_.baseY + tileSize;
		pos.z = z * tileSize;

		wallObj->SetTranslation(pos);
		wallObj->SetRotation({ 0.0f, 0.0f, 0.0f });
		wallObj->SetScale({ 1.0f, 1.0f, 1.0f });
		wallObj->Update();

		wallObjects_[z][x] = std::move(wallObj);
	}
}

void LevelEditor::CreateFloorObject() {
	Model* model = ModelManager::GetInstance()->FindModel("block");
	if (model == nullptr) {
		return;
	}

	floorObject_ = std::make_unique<Obj3d>();
	floorObject_->Initialize(model);
	floorObject_->SetCamera(camera_);

	const float tileSize = levelData_.tileSize;

	Vector3 pos;
	pos.x = ((float)levelData_.width - 1.0f) * tileSize * 0.5f;
	pos.y = levelData_.baseY;
	pos.z = ((float)levelData_.height - 1.0f) * tileSize * 0.5f;

	floorObject_->SetTranslation(pos);
	floorObject_->SetRotation({ 0.0f, 0.0f, 0.0f });

	// blockモデル1個をマップ全体サイズまで拡大
	floorObject_->SetScale({
		(float)levelData_.width,
		1.0f,
		(float)levelData_.height
		});

	floorObject_->Update();
}

void LevelEditor::FillAllTiles(int tileType) {
	for (int z = 0; z < levelData_.height; ++z) {
		for (int x = 0; x < levelData_.width; ++x) {
			levelData_.tiles[z][x] = tileType;
		}
	}

	RebuildMapObjects();
}

void LevelEditor::CreateRoom(int startX, int startZ, int roomWidth, int roomHeight) {
	if (roomWidth <= 0 || roomHeight <= 0) {
		return;
	}

	int endX = startX + roomWidth;
	int endZ = startZ + roomHeight;

	for (int z = startZ; z < endZ; ++z) {
		for (int x = startX; x < endX; ++x) {
			if (x < 0 || x >= levelData_.width || z < 0 || z >= levelData_.height) {
				continue;
			}

			levelData_.tiles[z][x] = 0;
		}
	}

	RebuildMapObjects();
}

void LevelEditor::GenerateRandomDungeon(int roomCount) {
	if (!dungeonGenerator_) {
		dungeonGenerator_ = std::make_unique<DungeonGenerator>();
	}

	if (levelData_.width < 1 || levelData_.height < 1) {
		return;
	}

	dungeonGenerator_->Generate(levelData_, roomCount);
	RebuildMapObjects();
}

Vector3 LevelEditor::GetRandomPlayerSpawnPosition(float y) {
	if (!dungeonGenerator_) {
		return { 0.0f, levelData_.baseY + y, 0.0f };
	}

	return dungeonGenerator_->GetRandomRoomWorldPosition(levelData_, y);
}

Vector3 LevelEditor::GetMapCenterPosition(float y) const {
	const float tileSize = levelData_.tileSize;

	Vector3 pos;
	pos.x = ((float)levelData_.width - 1.0f) * tileSize * 0.5f;
	pos.y = levelData_.baseY + y;
	pos.z = ((float)levelData_.height - 1.0f) * tileSize * 0.5f;

	return pos;
}

bool LevelEditor::ConsumeMapChanged() {
	if (mapChanged_) {
		mapChanged_ = false;
		return true;
	}
	return false;
}

// LevelEditor.h の public: 内に追加

void LevelEditor::ChangeToNormalMap() {
	mapType_ = 0;
	currentMapFile_ = "resources/map/map01.json";
	saveFileName_ = "map01.json";
	LoadAndCreateMap(currentMapFile_);
}

void LevelEditor::ChangeToBossMap() {
	mapType_ = 1;
	currentMapFile_ = "resources/map/boss.json";
	saveFileName_ = "boss.json";
	LoadAndCreateMap(currentMapFile_);
}