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

	editWidth_ = levelData_.width;
	editHeight_ = levelData_.height;

	dungeonGenerator_ = std::make_unique<DungeonGenerator>();

	currentMapFile_ = "resources/map/map01.json";
	mapType_ = 0;
	saveFileName_ = "map01.json";

	floorGroup_ = std::make_unique<InstancedGroup>();
	floorGroup_->Initialize("block", 10000);

	wallGroup_ = std::make_unique<InstancedGroup>();
	wallGroup_->Initialize("block", 10000);

	stairsGroup_ = std::make_unique<InstancedGroup>();
	stairsGroup_->Initialize("block", 10000);

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
	floorObjects_.resize(levelData_.height);
	wallObjects_.resize(levelData_.height);

	for (int z = 0; z < levelData_.height; ++z) {
		floorObjects_[z].resize(levelData_.width);
		wallObjects_[z].resize(levelData_.width);
	}
}
void LevelEditor::RebuildMapObjects() {
	ResizeObjectGrids();

	for (int z = 0; z < levelData_.height; ++z) {
		for (int x = 0; x < levelData_.width; ++x) {
			UpdateTileObject(x, z);
		}
	}
}

void LevelEditor::Update(const Vector3& playerPos) {
	if (floorGroup_) floorGroup_->PreUpdate();
	if (wallGroup_) wallGroup_->PreUpdate();
	if (stairsGroup_) stairsGroup_->PreUpdate();

	const float tileSize = levelData_.tileSize;

	int centerX = static_cast<int>(std::round(playerPos.x / tileSize));
	int centerZ = static_cast<int>(std::round(playerPos.z / tileSize));

	// 表示範囲
	const int range = 16;

	int minX = std::max(0, centerX - range);
	int maxX = std::min(levelData_.width - 1, centerX + range);
	int minZ = std::max(0, centerZ - range);
	int maxZ = std::min(levelData_.height - 1, centerZ + range);

	for (int z = minZ; z <= maxZ; ++z) {
		for (int x = minX; x <= maxX; ++x) {

			// 床
			if (floorObjects_[z][x]) {
				floorObjects_[z][x]->Update();
				floorGroup_->AddObject(floorObjects_[z][x].get());
			}

			// 壁・階段
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
void LevelEditor::Draw(const Vector3& playerPos) {
	if (floorGroup_) floorGroup_->Draw(camera_);
	if (wallGroup_) wallGroup_->Draw(camera_);
	if (stairsGroup_) stairsGroup_->Draw(camera_);
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
		ImGui::SameLine();
		ImGui::RadioButton("階段(3)", &selectedTile_, 3);

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

				if (tile == 0) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 1.0f, 1.0f)); // 青
				} else if (tile == 1) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.3f, 0.3f, 1.0f)); // 赤
				} else if (tile == 3) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 1.0f, 1.0f, 1.0f)); // 水色
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

Vector3 LevelEditor::GetTileWorldPosition(int x, int z, float y) const {
	Vector3 pos;
	pos.x = x * levelData_.tileSize;
	pos.y = levelData_.baseY + y;
	pos.z = z * levelData_.tileSize;
	return pos;
}

void LevelEditor::PlaceStairsTileRandom(const Vector3& avoidWorldPos, float avoidDistance) {
	std::vector<std::pair<int, int>> candidates;

	for (int z = 0; z < levelData_.height; ++z) {
		for (int x = 0; x < levelData_.width; ++x) {
			if (levelData_.tiles[z][x] != 0) {
				continue;
			}

			Vector3 worldPos = GetTileWorldPosition(x, z, 0.0f);

			float dx = worldPos.x - avoidWorldPos.x;
			float dz = worldPos.z - avoidWorldPos.z;
			float dist = std::sqrt(dx * dx + dz * dz);

			if (dist < avoidDistance) {
				continue;
			}

			candidates.push_back({ x, z });
		}
	}

	// 候補がなければ距離条件なしで 0 タイルから選ぶ
	if (candidates.empty()) {
		for (int z = 0; z < levelData_.height; ++z) {
			for (int x = 0; x < levelData_.width; ++x) {
				if (levelData_.tiles[z][x] == 0) {
					candidates.push_back({ x, z });
				}
			}
		}
	}

	if (candidates.empty()) {
		return;
	}

	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);

	auto [tileX, tileZ] = candidates[dist(mt)];

	levelData_.tiles[tileZ][tileX] = 3;
	UpdateTileObject(tileX, tileZ);
}
void LevelEditor::UpdateTileObject(int x, int z) {
	if (z < 0 || z >= levelData_.height || x < 0 || x >= levelData_.width) return;

	Model* model = ModelManager::GetInstance()->FindModel("block");
	if (model == nullptr) return;

	const float tileSize = levelData_.tileSize;
	const int tile = levelData_.tiles[z][x];

	// ==========================================
	// 床の処理 (タイルが0の時)
	// ==========================================
	if (tile == 0) {
		if (!floorObjects_[z][x]) { // 無ければ作る（使い回し）
			floorObjects_[z][x] = std::make_unique<Obj3d>();
			floorObjects_[z][x]->Initialize(model);
			floorObjects_[z][x]->SetCamera(camera_);
		}
		Vector3 pos = { x * tileSize, levelData_.baseY, z * tileSize };
		floorObjects_[z][x]->SetTranslation(pos);
		floorObjects_[z][x]->SetRotation({ 0.0f, 0.0f, 0.0f });
		floorObjects_[z][x]->SetScale({ 1.0f, 1.0f, 1.0f });
		// floorObjects_[z][x]->Update(); // ※Updateは全体のUpdateで呼ばれるのでここでは不要
	}

	// ==========================================
	// 壁・階段の処理 (タイルが1 or 3の時)
	// ==========================================
	if (tile == 1 || tile == 3) {
		if (!wallObjects_[z][x]) { // 無ければ作る
			wallObjects_[z][x] = std::make_unique<Obj3d>();
			wallObjects_[z][x]->Initialize(model);
			wallObjects_[z][x]->SetCamera(camera_);
		}

		if (tile == 1) { // 壁
			Vector3 pos = { x * tileSize, levelData_.baseY + tileSize, z * tileSize };
			wallObjects_[z][x]->SetTranslation(pos);
			wallObjects_[z][x]->SetScale({ 1.0f, 1.0f, 1.0f });
		} else { // 階段
			Vector3 pos = { x * tileSize, levelData_.baseY + 2.5f, z * tileSize };
			wallObjects_[z][x]->SetTranslation(pos);
			wallObjects_[z][x]->SetScale({ 1.0f, 2.0f, 1.0f });
		}
	}

	// ※使わなくなったObj3dを消すと重いので、そのまま放置しておいてOKです。
	// Update() の描画登録の時に、tiles[z][x] の値を見て無視する仕組みになっているので悪さはしません。
}
std::pair<int, int> LevelEditor::PlaceStairsTileRandomAndGetTile(const Vector3& avoidWorldPos, float avoidDistance) {
	std::vector<std::pair<int, int>> candidates;

	for (int z = 1; z < levelData_.height - 1; ++z) {
		for (int x = 1; x < levelData_.width - 1; ++x) {
			if (levelData_.tiles[z][x] != 0) {
				continue;
			}

			// 周囲1マス(3x3)が全部床(0)の場所だけ候補にする
			bool allFloor = true;
			for (int oz = -1; oz <= 1; ++oz) {
				for (int ox = -1; ox <= 1; ++ox) {
					if (levelData_.tiles[z + oz][x + ox] != 0) {
						allFloor = false;
						break;
					}
				}
				if (!allFloor) {
					break;
				}
			}

			if (!allFloor) {
				continue;
			}

			Vector3 worldPos = GetTileWorldPosition(x, z, 0.0f);

			float dx = worldPos.x - avoidWorldPos.x;
			float dz = worldPos.z - avoidWorldPos.z;
			float dist = std::sqrt(dx * dx + dz * dz);

			if (dist < avoidDistance) {
				continue;
			}

			candidates.push_back({ x, z });
		}
	}

	// 条件が厳しすぎて候補がない時の保険
	if (candidates.empty()) {
		for (int z = 0; z < levelData_.height; ++z) {
			for (int x = 0; x < levelData_.width; ++x) {
				if (levelData_.tiles[z][x] == 0) {
					candidates.push_back({ x, z });
				}
			}
		}
	}

	if (candidates.empty()) {
		return { -1, -1 };
	}

	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);

	auto [tileX, tileZ] = candidates[dist(mt)];

	levelData_.tiles[tileZ][tileX] = 3;
	UpdateTileObject(tileX, tileZ);

	return { tileX, tileZ };
}

void LevelEditor::SetNoiseTexture(uint32_t index) {
	noiseTextureIndex_ = index;
	if (floorGroup_) floorGroup_->SetNoiseTexture(index);
	if (wallGroup_) wallGroup_->SetNoiseTexture(index);
	if (stairsGroup_) stairsGroup_->SetNoiseTexture(index);
}