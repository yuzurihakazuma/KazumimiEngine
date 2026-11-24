#pragma once
#include "game/scene/BaseScene.h"
#include "engine/3d/obj/Obj3dCommon.h"
#include "engine/rail/SplineRail.h"
#include "game/rail/RoadMesh.h"
#include "game/stage/StageBackdrop.h"
#include "game/worldmap/DiveSequence.h"
#include "game/effect/PaperFx.h"
#include "game/worldmap/WorldMapData.h"
#include "game/worldmap/WorldMapPanel.h"

#include <memory>
#include <vector>

namespace KazumimiEngine {
class Obj3d;
class SDFText;
class SkinnedObj3d;
}
class StageDiorama;

// =====================================================================
//  StageSelectScene：ステージ選択（ワールドマップ）。ステージ選択・飛び込み演出 企画書。
//   ・段ボールの台座の上に、ステージの「箱庭」（本物のステージを縮めて描いた物）が並ぶ
//   ・たまごザウルスが布の道の上をマスからマスへ歩く（A / D）
//   ・マスに立つと、箱庭が跳ねて持ち上がり、カメラが寄ってゆっくり回る。左から情報パネル
//     （プレビュー中は敵が動き、布の道がスタートから順に敷かれて道筋をなぞる）
//   ・SPACE で飛び込み（構え → ジャンプ → 前転 → 頭から → 吸い込み。時刻は dive.json）。
//     円を箱庭の真ん中へ閉じ、ステージ開始カードの裏で読み込んで、上から落ちてきて着地
//   ・まだ遊べないマスは蓋がしてある（パネルは「？？？」）
//   ・布の道は、行けるマスまでしか敷かれていない。ステージをクリアして戻って来ると、次のマスへ布の道が転がって伸びる
//     （クリア演出 企画書の「マップ」。行けるマス＝最初のマスと、クリアしたステージの次のマス）
//   ・T でタイトルへ
//   置き場所は resources/worldmap/（tools/worldmap/make_worldmap_assets.py が作る）
// =====================================================================
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
	bool GetSdfViewerPosition(KazumimiEngine::Vector3& outPos) const override;

	// RoadOpen＝クリアして戻って来た：次のマスへ布の道が転がって伸びる間（操作は待つ）
	enum class Phase { Opening, Map, RoadOpen, Dive, Leaving };

	// マスの物（台座・番号・蓋・箱庭）
	struct Node {
		std::unique_ptr<KazumimiEngine::Obj3d> pedestal;
		std::unique_ptr<KazumimiEngine::Obj3d> label;
		std::unique_ptr<KazumimiEngine::Obj3d> lid;      // まだ遊べないマスの蓋
		std::unique_ptr<KazumimiEngine::Obj3d> lidLabel;
		std::unique_ptr<StageDiorama> diorama;
		float roadDistance = 0.0f;       // 道の上のどこに立つか(m)
		KazumimiEngine::Vector3 center {};               // 箱庭の真ん中（無ければ台座の上）
		KazumimiEngine::Vector3 top {};                  // 台座の上面の中心
		float   smokeTimer = 0.0f;       // 汽車のけむりを出すまで
		KazumimiEngine::Vector3 size {};                 // 台座の大きさ
	};

	void BuildMap();
	void ApplyLook();
	void RestoreLook();
	// 進行の段階ごとの状態クラス（中身は cpp。段階を足す時は、派生を 1 つ足して Phases::Of の表に 1 行足す）
	class PhaseState;
	struct Phases;
	void ChangePhase(Phase next){ phase_ = next; } // 次の段階へ（状態クラスが呼ぶ）
	void WalkToTarget(float deltaTime);      // 行き先のマスへ歩く
	void UpdateDive(float deltaTime);
	void OnDiveJumped();                     // 飛び込み：跳んだ瞬間の音とけむり
	void OnDiveLanded();                     // 飛び込み：箱庭に着いた瞬間の音・弾み・紙吹雪
	StageDiorama* PreviewDiorama();          // プレビューしているマスの箱庭（無ければ nullptr）
	void SetupProgress();                    // 行けるマスを決める。クリアして戻って来たなら、道が伸びる演出を用意する
	int  CountReachable(const std::string& ignoreClear) const; // 行けるマスの数（ignoreClear のステージは、まだクリアしていない扱い）
	float RoadFrontFor(int reachable) const; // そのマスの数まで行ける時に、布の道を敷く長さ(m)
	bool UpdateRoadOpen(float deltaTime);    // 布の道が転がって伸びる（伸び切ったフレームに true）
	void UpdateRoadCloth();                  // 布の道を、行ける所までに切る
	void UpdateDino(float deltaTime);
	void UpdateTexts();
	void StartDive();
	bool IsPreviewing() const; // マスに立って止まっている（プレビュー中）
	int  StandingNode() const; // 立っているマス（歩いている途中は -1）
	void MapCamera(KazumimiEngine::Vector3& outPosition, KazumimiEngine::Vector3& outLook) const;
	void PreviewCamera(KazumimiEngine::Vector3& outPosition, KazumimiEngine::Vector3& outLook) const;

	WorldMapData data_;
	DiveTiming   timing_;
	std::vector<Node> nodes_;
	std::vector<KazumimiEngine::SplineRail> road_;  // 道（1本。布ロード）
	RoadMesh     roadMesh_;
	StageBackdrop backdrop_;        // まわりの地面・木・丘・空
	WorldMapPanel panel_;
	DiveSequence  dive_;
	PaperFx       fx_;          // 紙吹雪・綿のけむり

	std::unique_ptr<KazumimiEngine::SkinnedObj3d> dino_;
	std::string dinoClip_;
	float dinoDistance_ = 0.0f;     // 道の上の位置(m)
	float dinoYaw_ = 0.0f;
	int   targetNode_ = 0;          // 向かっているマス
	static inline int lastNode_ = 0; // 前に立っていたマス（戻って来た時に同じ所から）

	// 行けるマスと、布の道が伸びる演出
	int   reachable_ = 1;           // 行けるマスの数（左から）
	float roadFront_ = 0.0f;        // 布の道を敷いてある長さ（道の始めから m）
	bool  roadOpenPending_ = false; // 円が開いたら、道が伸びる演出を始める
	int   roadOpenTo_ = 1;          // 伸び終わった時の、行けるマスの数
	float roadOpenFrom_ = 0.0f, roadOpenTarget_ = 0.0f; // 伸びる前・後の長さ
	float roadOpenTime_ = 0.0f;
	float roadPan_ = 0.0f;          // 伸びる先を見せるための、カメラの寄せ具合（0〜1）
	std::unique_ptr<KazumimiEngine::Obj3d> roadRoll_; // 転がる布のロール（ステージ開始の演技と同じ物）
	float roadRollAngle_ = 0.0f;

	Phase phase_ = Phase::Opening;
	float phaseTime_ = 0.0f;
	float previewBlend_ = 0.0f;     // 0＝マップのカメラ 1＝プレビューのカメラ
	float orbitAngle_ = 0.0f;       // プレビューのカメラが振れる位相
	int   previewNode_ = -1;
	KazumimiEngine::Vector3 cameraPosition_ {}, cameraLook_ {};

	std::unique_ptr<KazumimiEngine::SDFText> guideText_;

	// 見た目（タイトルと同じ絵本風）。離れる時に元へ戻す
	KazumimiEngine::Obj3dCommon::DirectionalLightData savedLight_ {};
	float savedPointIntensity_ = 0.0f;
	float savedSpotIntensity_ = 0.0f;
	bool  lightSaved_ = false;
	float tiltStrength_ = 5.0f;
	float tiltCenterY_ = 0.52f;
	float tiltHalfWidth_ = 0.3f;
};
