#pragma once
// =====================================================================
//  StageSelectScene：ステージ選択（ワールドマップ）。ステージ選択・飛び込み演出 企画書。
//   ・段ボールの台座の上に、ステージの「箱庭」（本物のステージを縮めて描いた物）が並ぶ
//   ・たまごザウルスが布の道の上をマスからマスへ歩く（A / D）
//   ・マスに立つと、箱庭が跳ねて持ち上がり、カメラが寄ってゆっくり回る。左から情報パネル
//     （プレビュー中は敵が動き、布の道がスタートから順に敷かれて道筋をなぞる）
//   ・SPACE で飛び込み（構え → ジャンプ → 前転 → 頭から → 吸い込み。時刻は dive.json）。
//     円を箱庭の真ん中へ閉じ、ステージ開始カードの裏で読み込んで、上から落ちてきて着地
//   ・まだ遊べないマスは蓋がしてある（パネルは「？？？」）
//   ・T でタイトルへ
//   置き場所は resources/worldmap/（tools/worldmap/make_worldmap_assets.py が作る）
// =====================================================================
#include "game/scene/BaseScene.h"
#include "engine/3d/obj/Obj3dCommon.h"
#include "engine/rail/SplineRail.h"
#include "game/rail/RoadMesh.h"
#include "game/stage/StageBackdrop.h"
#include "game/worldmap/DiveSequence.h"
#include "game/worldmap/PaperFx.h"
#include "game/worldmap/WorldMapData.h"
#include "game/worldmap/WorldMapPanel.h"

#include <memory>
#include <vector>

class Obj3d;
class SDFText;
class SkinnedObj3d;
class StageDiorama;

class StageSelectScene : public BaseScene {
public:
	StageSelectScene();
	~StageSelectScene() override;

	void Reload() override {}

private:
	// --- BaseScene の差し込み口 ---
	void OnLoadResources() override;
	void OnInitialize() override;
	void OnFinalize() override;
	void UpdateCamera() override;
	void OnUpdate() override;
	void OnDrawOpaque(ID3D12GraphicsCommandList* commandList) override;
	void OnDrawInstanced() override;
	void OnDrawOverlay2D(ID3D12GraphicsCommandList* commandList) override;
	void OnDrawUI(ID3D12GraphicsCommandList* commandList) override;
	void OnDrawInspector() override;
	bool GetSdfViewerPosition(Vector3& outPos) const override;

	enum class Phase { Opening, Map, Dive, Leaving };

	// マスの物（台座・番号・蓋・箱庭）
	struct Node {
		std::unique_ptr<Obj3d> pedestal;
		std::unique_ptr<Obj3d> label;
		std::unique_ptr<Obj3d> lid;      // まだ遊べないマスの蓋
		std::unique_ptr<Obj3d> lidLabel;
		std::unique_ptr<StageDiorama> diorama;
		float roadDistance = 0.0f;       // 道の上のどこに立つか(m)
		Vector3 center {};               // 箱庭の真ん中（無ければ台座の上）
		Vector3 top {};                  // 台座の上面の中心
		float   smokeTimer = 0.0f;       // 汽車のけむりを出すまで
		Vector3 size {};                 // 台座の大きさ
	};

	void BuildMap();
	void ApplyLook();
	void RestoreLook();
	void UpdateWalk(float deltaTime);
	void UpdateDive(float deltaTime);
	void UpdateDino(float deltaTime);
	void UpdateTexts();
	void StartDive();
	bool IsPreviewing() const; // マスに立って止まっている（プレビュー中）
	int  StandingNode() const; // 立っているマス（歩いている途中は -1）
	void MapCamera(Vector3& outPosition, Vector3& outLook) const;
	void PreviewCamera(Vector3& outPosition, Vector3& outLook) const;

	WorldMapData data_;
	DiveTiming   timing_;
	std::vector<Node> nodes_;
	std::vector<SplineRail> road_;  // 道（1本。布ロード）
	RoadMesh     roadMesh_;
	StageBackdrop backdrop_;        // まわりの地面・木・丘・空
	WorldMapPanel panel_;
	DiveSequence  dive_;
	PaperFx       fx_;          // 紙吹雪・綿のけむり

	std::unique_ptr<SkinnedObj3d> dino_;
	std::string dinoClip_;
	float dinoDistance_ = 0.0f;     // 道の上の位置(m)
	float dinoYaw_ = 0.0f;
	int   targetNode_ = 0;          // 向かっているマス
	static inline int lastNode_ = 0; // 前に立っていたマス（戻って来た時に同じ所から）

	Phase phase_ = Phase::Opening;
	float phaseTime_ = 0.0f;
	float previewBlend_ = 0.0f;     // 0＝マップのカメラ 1＝プレビューのカメラ
	float orbitAngle_ = 0.0f;       // プレビューのカメラが振れる位相
	int   previewNode_ = -1;
	Vector3 cameraPosition_ {}, cameraLook_ {};

	std::unique_ptr<SDFText> guideText_;

	// 見た目（タイトルと同じ絵本風）。離れる時に元へ戻す
	Obj3dCommon::DirectionalLightData savedLight_ {};
	float savedPointIntensity_ = 0.0f;
	float savedSpotIntensity_ = 0.0f;
	bool  lightSaved_ = false;
	float tiltStrength_ = 5.0f;
	float tiltCenterY_ = 0.52f;
	float tiltHalfWidth_ = 0.3f;
};
