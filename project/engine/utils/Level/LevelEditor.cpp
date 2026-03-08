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
}

void LevelEditor::LoadAndCreateMap(const std::string& fileName) {
	levelData_ = LevelManager::GetInstance()->Load(fileName);
	RebuildMapObjects();
}

void LevelEditor::RebuildMapObjects() {
	object3ds_.clear();

	for (int z = 0; z < levelData_.height; ++z) {
		for (int x = 0; x < levelData_.width; ++x) {
			int tile = levelData_.tiles[z][x];

			// 0=床, 1=壁
			// どちらも block を使う
			Model* model = ModelManager::GetInstance()->FindModel("block");
			if (model == nullptr) {
				continue;
			}

			// まず床を置く
			{
				std::unique_ptr<Obj3d> floorObj = std::make_unique<Obj3d>();
				floorObj->Initialize(model);
				floorObj->SetCamera(camera_);

				Vector3 pos;
				pos.x = x * levelData_.tileSize;
				pos.y = levelData_.baseY;
				pos.z = z * levelData_.tileSize;

				floorObj->SetTranslation(pos);
				floorObj->SetRotation({ 0.0f, 0.0f, 0.0f });
				floorObj->SetScale({ 1.0f, 1.0f, 1.0f });
				floorObj->Update();

				object3ds_.push_back(std::move(floorObj));
			}

			// 壁なら床の上にもう1個置く
			if (tile == 1) {
				std::unique_ptr<Obj3d> wallObj = std::make_unique<Obj3d>();
				wallObj->Initialize(model);
				wallObj->SetCamera(camera_);

				Vector3 pos;
				pos.x = x * levelData_.tileSize;
				pos.y = levelData_.baseY + levelData_.tileSize;
				pos.z = z * levelData_.tileSize;

				wallObj->SetTranslation(pos);
				wallObj->SetRotation({ 0.0f, 0.0f, 0.0f });
				wallObj->SetScale({ 1.0f, 1.0f, 1.0f });
				wallObj->Update();

				object3ds_.push_back(std::move(wallObj));
			}
		}
	}
}

void LevelEditor::Update() {
	for (auto& obj : object3ds_) {
		obj->Update();
	}
}

void LevelEditor::Draw() {
	for (auto& obj : object3ds_) {
		obj->Draw();
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

		ImGui::InputInt("幅", &levelData_.width);
		ImGui::InputInt("高さ", &levelData_.height);
		ImGui::DragFloat("タイルサイズ", &levelData_.tileSize, 0.1f, 0.5f, 10.0f);
		ImGui::DragFloat("床の高さ", &levelData_.baseY, 0.1f);

		if (levelData_.width < 1) { levelData_.width = 1; }
		if (levelData_.height < 1) { levelData_.height = 1; }

		if (ImGui::Button("サイズを反映")) {
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
					levelData_.tiles[z][x] = selectedTile_;
					RebuildMapObjects();
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