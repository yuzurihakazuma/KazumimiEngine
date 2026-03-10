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
}

void LevelEditor::LoadAndCreateMap(const std::string& fileName) {
	levelData_ = LevelManager::GetInstance()->Load(fileName);

	editWidth_ = levelData_.width;
	editHeight_ = levelData_.height;

	RebuildMapObjects();
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

	Model* model = ModelManager::GetInstance()->FindModel("block");
	if (model == nullptr) {
		return;
	}

	for (int z = 0; z < levelData_.height; ++z) {
		for (int x = 0; x < levelData_.width; ++x) {
			UpdateTileObject(x, z);
		}
	}
}
void LevelEditor::Update(const Vector3& playerPos) {
	for (int z = 0; z < levelData_.height; ++z) {
		for (int x = 0; x < levelData_.width; ++x) {

			if (floorObjects_[z][x]) {
				Vector3 objPos = floorObjects_[z][x]->GetTranslation();
				float diffX = objPos.x - playerPos.x;
				float diffZ = objPos.z - playerPos.z;
				float distSq = (diffX * diffX) + (diffZ * diffZ);

				if (distSq < 400.0f) {
					floorObjects_[z][x]->Update();
				}
			}

			if (wallObjects_[z][x]) {
				Vector3 objPos = wallObjects_[z][x]->GetTranslation();
				float diffX = objPos.x - playerPos.x;
				float diffZ = objPos.z - playerPos.z;
				float distSq = (diffX * diffX) + (diffZ * diffZ);

				if (distSq < 400.0f) {
					wallObjects_[z][x]->Update();
				}
			}
		}
	}
}
// LevelEditor.cpp
void LevelEditor::Draw(const Vector3& playerPos) {
	for (int z = 0; z < levelData_.height; ++z) {
		for (int x = 0; x < levelData_.width; ++x) {

			if (floorObjects_[z][x]) {
				Vector3 objPos = floorObjects_[z][x]->GetTranslation();
				float diffX = objPos.x - playerPos.x;
				float diffZ = objPos.z - playerPos.z;
				float distSq = diffX * diffX + diffZ * diffZ;

				if (distSq < 400.0f) {
					floorObjects_[z][x]->Draw();
				}
			}

			if (wallObjects_[z][x]) {
				Vector3 objPos = wallObjects_[z][x]->GetTranslation();
				float diffX = objPos.x - playerPos.x;
				float diffZ = objPos.z - playerPos.z;
				float distSq = diffX * diffX + diffZ * diffZ;

				if (distSq < 400.0f) {
					wallObjects_[z][x]->Draw();
				}
			}
		}
	}
}
void LevelEditor::DrawDebugUI() {
#ifdef USE_IMGUI
	if (isEditorActive) {

		ImGui::Begin("マップエディタ");

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
			for (int z = 0; z < levelData_.height; ++z) {
				for (int x = 0; x < levelData_.width; ++x) {
					levelData_.tiles[z][x] = 0;
				}
			}
			RebuildMapObjects();
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

		for (int z = 0; z < levelData_.height; ++z) {
			for (int x = 0; x < levelData_.width; ++x) {
				std::string label = std::to_string(levelData_.tiles[z][x]) + "##" + std::to_string(z) + "_" + std::to_string(x);
				if (ImGui::Button(label.c_str(), ImVec2(24, 24))) {
					if (levelData_.tiles[z][x] != selectedTile_) {
						levelData_.tiles[z][x] = selectedTile_;
						UpdateTileObject(x, z);
					}
				}
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
	floorObjects_[z][x].reset();
	wallObjects_[z][x].reset();

	const float tileSize = levelData_.tileSize;
	const int tile = levelData_.tiles[z][x];

	// --------------------
	// 床は常に置く
	// --------------------
	{
		std::unique_ptr<Obj3d> floorObj = std::make_unique<Obj3d>();
		floorObj->Initialize(model);
		floorObj->SetCamera(camera_);

		Vector3 pos;
		pos.x = x * tileSize;
		pos.y = levelData_.baseY;
		pos.z = z * tileSize;

		floorObj->SetTranslation(pos);
		floorObj->SetRotation({ 0.0f, 0.0f, 0.0f });
		floorObj->SetScale({ 1.0f, 1.0f, 1.0f });
		floorObj->Update();

		floorObjects_[z][x] = std::move(floorObj);
	}

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