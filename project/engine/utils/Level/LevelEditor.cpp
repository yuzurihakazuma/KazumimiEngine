#include "LevelEditor.h"

/// --- 標準ライブラリ ---
#include <cmath>

// --- エンジン側のファイル ---
#include "LevelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/utils/ImGuiManager.h"
#include "engine/3d/model/ModelManager.h"


LevelEditor::LevelEditor() = default;
LevelEditor::~LevelEditor() = default;


void LevelEditor::Initialize() {
	// 最初は空の状態でスタートするか、デフォルトのマップを読み込む
	LoadAndCreateMap("resources/map/map01.json");
}
// マップの読み込みと生成
void LevelEditor::LoadAndCreateMap(const std::string& fileName) {
	object3ds_.clear();
	levelData_ = LevelManager::GetInstance()->Load(fileName);

	for (auto& objData : levelData_.objects) {
		// 実際のモデルデータを検索して持ってくる
		Model* model = ModelManager::GetInstance()->FindModel(objData.type);

		std::unique_ptr<Obj3d> newObj = std::make_unique<Obj3d>();
		// 検索したモデルを渡す！
		newObj->Initialize(model);
		newObj->SetCamera(camera_);
		newObj->SetTranslation(objData.translation);
		newObj->SetRotation(objData.rotation);
		newObj->SetScale(objData.scale);
		newObj->Update();
		object3ds_.push_back(std::move(newObj));
	}
	selectedObjectIndex_ = -1;
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


void LevelEditor::DrawDebugUI(){
#ifdef USE_IMGUI

	if ( isEditorActive ) {

		// =========================================================
		//  1. Main Menu ウィンドウ（ファイル操作・追加）
		// =========================================================
		ImGui::Begin("メインメニュー");

		char buffer[256];
		strcpy_s(buffer, saveFileName_.c_str());
		if ( ImGui::InputText("保存ファイル名", buffer, sizeof(buffer)) ) {
			saveFileName_ = buffer;
		}

		std::string fullPath = "resources/map/" + saveFileName_;
		if ( ImGui::Button("マップを保存") ) { LevelManager::GetInstance()->Save(fullPath, levelData_); }
		ImGui::SameLine();
		if ( ImGui::Button("マップを読み込む") ) { LoadAndCreateMap(fullPath); }
		ImGui::SameLine();
		if ( ImGui::Button("マップをクリア") ) {
			object3ds_.clear();
			levelData_.objects.clear();
			selectedObjectIndex_ = -1;
		}

		ImGui::Separator();

		const char* modelNames[] = { "block", "fence", "plane", "sphere", "terrain", "animatedCube" };
		static int currentModelIndex = 0;
		ImGui::Combo("モデルの種類", &currentModelIndex, modelNames, IM_ARRAYSIZE(modelNames));

		if ( ImGui::Button("選択したモデルを追加") ) {
			LevelObjectData newObj;
			newObj.type = modelNames[currentModelIndex];
			newObj.translation = { 0.0f, 0.0f, 0.0f };
			newObj.rotation = { 0.0f, 0.0f, 0.0f };
			newObj.scale = { 1.0f, 1.0f, 1.0f };
			levelData_.objects.push_back(newObj);

			Model* model = ModelManager::GetInstance()->FindModel(newObj.type);
			if ( model != nullptr ) {
				std::unique_ptr<Obj3d> obj = std::make_unique<Obj3d>();
				obj->Initialize(model);
				obj->SetCamera(camera_);
				object3ds_.push_back(std::move(obj));
				selectedObjectIndex_ = ( int ) levelData_.objects.size() - 1;
			}
		}
		ImGui::End();

		// =========================================================
		//  2. Hierarchy ウィンドウ（オブジェクト一覧とドロップ先）
		// =========================================================
		ImGui::Begin("ヒエラルキー (配置リスト)");

		// リストを広げる（一番下にドロップ用の余白を少し残すため -40.0f にする）
		if ( ImGui::BeginListBox("##ObjectList", ImVec2(-FLT_MIN, -40.0f)) ) {
			for ( int i = 0; i < levelData_.objects.size(); ++i ) {
				std::string label = std::to_string(i) + ": " + levelData_.objects[i].type;
				if ( ImGui::Selectable(label.c_str(), selectedObjectIndex_ == i) ) {
					selectedObjectIndex_ = i;
				}
			}
			ImGui::EndListBox();
		}

		//  ドロップ先の的（まと） 🌟🌟
		// ウィンドウの残りのスペースを「見えない的」にする
		ImGui::Dummy(ImGui::GetContentRegionAvail());

		// もし、この「ヒエラルキー」ウィンドウの余白に何かがドロップされたら…
		if ( ImGui::BeginDragDropTarget() ) {
			// "DND_MODEL" というラベルの荷物を受け取る
			if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_MODEL") ) {
				// 荷物（文字列）を取り出す
				const char* droppedModelName = ( const char* ) payload->Data;

				// ドロップされたモデルを新しく追加！
				LevelObjectData newObj;
				newObj.type = droppedModelName;
				newObj.translation = { 0.0f, 0.0f, 0.0f }; // 原点に配置
				newObj.rotation = { 0.0f, 0.0f, 0.0f };
				newObj.scale = { 1.0f, 1.0f, 1.0f };
				levelData_.objects.push_back(newObj);

				Model* model = ModelManager::GetInstance()->FindModel(newObj.type);
				if ( model != nullptr ) {
					std::unique_ptr<Obj3d> obj = std::make_unique<Obj3d>();
					obj->Initialize(model);
					obj->SetCamera(camera_);
					object3ds_.push_back(std::move(obj));
					selectedObjectIndex_ = ( int ) levelData_.objects.size() - 1; // 今追加したものを選択状態にする
				}
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::End();

		// =========================================================
		// ⚙ 3. Inspector ウィンドウ（選択中のオブジェクト編集）
		// =========================================================
		ImGui::Begin("インスペクター (詳細設定)");
		if ( selectedObjectIndex_ >= 0 && selectedObjectIndex_ < levelData_.objects.size() ) {
			auto& objData = levelData_.objects[selectedObjectIndex_];

			ImGui::Text("選択中: [%d] %s", selectedObjectIndex_, objData.type.c_str());
			ImGui::Separator();

			if ( ImGui::Button("オブジェクトを削除") ) {
				levelData_.objects.erase(levelData_.objects.begin() + selectedObjectIndex_);
				object3ds_.erase(object3ds_.begin() + selectedObjectIndex_);
				selectedObjectIndex_ = -1;
			}

			if ( selectedObjectIndex_ != -1 ) {
				ImGui::SameLine();
				if ( ImGui::Button("複製") ) {
					LevelObjectData dupObj = objData;
					levelData_.objects.push_back(dupObj);

					Model* model = ModelManager::GetInstance()->FindModel(dupObj.type);
					if ( model != nullptr ) {
						std::unique_ptr<Obj3d> obj = std::make_unique<Obj3d>();
						obj->Initialize(model);
						obj->SetCamera(camera_);
						obj->SetTranslation(dupObj.translation);
						obj->SetRotation(dupObj.rotation);
						obj->SetScale(dupObj.scale);
						object3ds_.push_back(std::move(obj));
						selectedObjectIndex_ = ( int ) levelData_.objects.size() - 1;
					}
				}

				ImGui::Checkbox("グリッドにスナップ (1.0刻み)", &snapToGrid_);
				ImGui::Separator();

				bool isChanged = false;
				float moveStep = snapToGrid_ ? 1.0f : 0.1f;
				isChanged |= ImGui::DragFloat3("座標", &objData.translation.x, moveStep);
				isChanged |= ImGui::DragFloat3("回転", &objData.rotation.x, 0.05f);
				isChanged |= ImGui::DragFloat3("スケール", &objData.scale.x, 0.1f);

				if ( isChanged ) {
					if ( snapToGrid_ ) {
						objData.translation.x = std::round(objData.translation.x);
						objData.translation.y = std::round(objData.translation.y);
						objData.translation.z = std::round(objData.translation.z);
					}
					object3ds_[selectedObjectIndex_]->SetTranslation(objData.translation);
					object3ds_[selectedObjectIndex_]->SetRotation(objData.rotation);
					object3ds_[selectedObjectIndex_]->SetScale(objData.scale);
				}
			}
		} else {
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "オブジェクトが選択されていません");
		}
		ImGui::End();

		// =========================================================
		//  4. アセットブラウザ ウィンドウ（ドラッグ元）
		// =========================================================
		ImGui::Begin("アセットブラウザ (Assets)");

		ImGui::Text("【 3Dモデル 】");
		ImGui::Separator();

		// ドラッグ＆ドロップで使えるモデルのリスト
		const char* assetModels[] = { "block", "fence", "plane", "sphere", "terrain", "animatedCube" };

		for ( int i = 0; i < IM_ARRAYSIZE(assetModels); ++i ) {
			// リスト表示
			ImGui::Selectable(assetModels[i]);

			// 
			if ( ImGui::BeginDragDropSource(ImGuiDragDropFlags_None) ) {
				// "DND_MODEL" というラベルで、モデルの名前を荷物として送る
				ImGui::SetDragDropPayload("DND_MODEL", assetModels[i], strlen(assetModels[i]) + 1);

				// ドラッグ中にマウスカーソルにくっついて表示される文字
				ImGui::Text("モデルを配置: %s", assetModels[i]);

				ImGui::EndDragDropSource();
			}
		}

		ImGui::End();

	}
#endif
}
