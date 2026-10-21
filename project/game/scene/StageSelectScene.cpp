#include "StageSelectScene.h"
// --- ゲーム固有のファイル ---
#include "GamePlayAssets.h"
#include "game/rail/RailConnections.h"
#include "game/stage/StageIntroSequence.h"
#include "game/transition/KamishibaiTransition.h"
#include "game/worldmap/StageDiorama.h"

// --- エンジン側のファイル ---
#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/3d/obj/SkinnedObj3d.h"
#include "engine/audio/AudioManager.h"
#include "engine/base/Input.h"
#include "engine/base/WindowProc.h"
#include "engine/camera/Camera.h"
#include "engine/camera/DebugCamera.h"
#include "engine/graphics/TextureManager.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/VectorMath.h"
#include "engine/postEffect/PostEffect.h"
#include "engine/sdf/SDFManager.h"
#include "engine/sdf/SDFText.h"
#include "engine/utils/Easing.h"
#include "externals/nlohmann/json.hpp"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

namespace {
	constexpr float kDeltaTime = 1.0f / 60.0f;
	constexpr float kPi = 3.14159265358979323846f;
	constexpr const char* kMapFile = "resources/worldmap/worldmap.json";
	constexpr const char* kDiveFile = "resources/worldmap/dive.json";
	constexpr const char* kBackdropFile = "resources/worldmap/worldmap_backdrop.json";
	constexpr const char* kJumpSe = "resources/kamishibai/se_whoosh.wav";
	constexpr const char* kSplashSe = "resources/kamishibai/se_pon.wav";
	constexpr const char* kStepSe = "resources/kamishibai/se_patan.wav";

	constexpr float kWalkSpeed = 6.0f;      // マスからマスへ歩く速さ(m/秒)
	constexpr float kTurnRate = 10.0f;      // 向きを変える速さ
	constexpr float kOpenTime = 0.6f;       // 始まりに円を開く時間(秒)
	constexpr float kPreviewBlendTime = 0.8f; // マップ ⇔ プレビューのカメラの切り替え(秒)
	constexpr float kPreviewShift = 0.7f;   // プレビューで見る所を画面の右へ寄せる量（左にパネル）
	constexpr float kPreviewSway = 0.38f;   // プレビューのカメラが左右に振れる角度(rad)
	constexpr float kPreviewEdge = 0.9f;    // なぞる所を追う時、箱庭の端から内側へ寄せる(m)
	constexpr float kPedestalBorder = 0.35f; // 箱庭のまわりの台座のふち(m)
	constexpr float kLockedDepth = 1.4f;    // まだ遊べないマスの台座の奥行き(m)
	constexpr float kLabelSize = 0.5f;      // 台座の番号の紙(m)

	const char* kGuideMessage = "T : タイトルへ";
	const char* kFontAtlas = "jpdot";
	constexpr float kGuideFontSize = 30.0f;

	// 文字入力中（ImGui の入力欄）はキー操作を受け付けない
	bool IsTypingInEditor(){
#ifdef USE_IMGUI
		return ImGui::GetIO().WantTextInput;
#else
		return false;
#endif
	}
	Vector3 Lerp(const Vector3& a, const Vector3& b, float t){
		return { a.x + ( b.x - a.x ) * t, a.y + ( b.y - a.y ) * t, a.z + ( b.z - a.z ) * t };
	}
	float WrapAngle(float angle){
		while ( angle > kPi ) { angle -= 2.0f * kPi; }
		while ( angle < -kPi ) { angle += 2.0f * kPi; }
		return angle;
	}
	// カメラを position から target を見る向きへ（PlayCameraController と同じ規約）
	void LookAt(Camera* camera, const Vector3& position, const Vector3& target){
		camera->SetTranslation(position);
		const Vector3 look { target.x - position.x, target.y - position.y, target.z - position.z };
		const float horizontal = std::sqrt(look.x * look.x + look.z * look.z);
		if ( horizontal < 1e-4f && std::abs(look.y) < 1e-4f ) { return; }
		camera->SetRotation({ std::atan2(-look.y, horizontal), std::atan2(look.x, look.z), 0.0f });
	}
	// 板（平面モデル）を、画像を貼った別名で作る
	void CreateCardModel(const std::string& name, const std::string& texture, float size){
		ModelManager* modelManager = ModelManager::GetInstance();
		if ( !modelManager->FindModel(name) ) { modelManager->CreatePlaneModel(name, size, size); }
		if ( Model* model = modelManager->FindModel(name) ) {
			model->SetTexture(texture);
			if ( model->GetMaterial() ) { model->GetMaterial()->matte = 1.0f; }
		}
	}
}

StageSelectScene::StageSelectScene(){
	features_.overlay2D = true; // 操作の案内（SDF 文字）
}
StageSelectScene::~StageSelectScene(){
	OnFinalize();
}

// =====================================================================
//  用意
// =====================================================================
void StageSelectScene::OnLoadResources(){
	GamePlayAssets::LoadForDiorama(); // 箱庭に使う物（ブロック・敵・背景のキット・布の道）と恐竜
	AudioManager* audio = AudioManager::GetInstance();
	audio->LoadWave(kJumpSe);
	audio->LoadWave(kSplashSe);
	audio->LoadWave(kStepSe);
}

void StageSelectScene::OnInitialize(){
	data_.Load(kMapFile);
	timing_.Load(kDiveFile);
	BuildMap();
	panel_.Initialize();

	guideText_ = std::make_unique<SDFText>();
	guideText_->Initialize();
	guideText_->SetText(kGuideMessage);
	guideText_->SetFontSize(kGuideFontSize);
	guideText_->SetOutlineWidth(0.2f);
	guideText_->SetThickness(0.1f);
	guideText_->SetColor({ 1.0f, 0.97f, 0.88f, 0.9f });
	guideText_->SetOutlineColor({ 0.27f, 0.17f, 0.08f, 0.9f });

	// 恐竜：前に立っていたマスから（初めては1番）
	dino_ = SkinnedObj3d::Create("player", "resources/player", "player.gltf");
	if ( dino_ ) {
		dino_->SetEnvironmentMap(EnvironmentMapSrv());
		dino_->LoadClips("resources/player", "player.gltf");
		dino_->SetClip("Idle", true);
		dinoClip_ = "Idle";
	}
	targetNode_ = std::clamp(lastNode_, 0, ( std::max )( ( int ) nodes_.size() - 1, 0 ));
	dinoDistance_ = nodes_.empty() ? 0.0f : nodes_[targetNode_].roadDistance;
	dinoYaw_ = 0.0f;
	previewNode_ = -1;
	previewBlend_ = 0.0f;
	orbitAngle_ = 0.0f;

	ApplyLook();
	MapCamera(cameraPosition_, cameraLook_);
	// タイトルは円を閉じて来るので、恐竜を中心に円を開いて始める
	phase_ = Phase::Opening;
	phaseTime_ = 0.0f;
	PostEffect::GetInstance()->SetEffectActive(PostEffectType::IrisWipe, true);
	PostEffect::GetInstance()->SetIrisParams(0.0f, 0.5f, 0.5f);
}

void StageSelectScene::BuildMap(){
	// 道：1本の布ロード
	road_.clear();
	SplineRail rail;
	rail.nodes = data_.road;
	rail.BuildDistanceTable();
	rail.AutoDetectType();
	rail.skin.style = RailSkin::Cloth;
	rail.skin.width = data_.roadWidth;
	rail.skin.color = data_.roadColor;
	road_.push_back(std::move(rail));
	RailConnections::Build(road_);
	roadMesh_.SetJointVisible(2);
	roadMesh_.Build(road_, GetCamera());

	// まわりの地面・木・丘・空
	const uint32_t noise = TextureManager::GetInstance()->Load("resources/uvChecker.png").srvIndex;
	std::ifstream file(kBackdropFile);
	nlohmann::json backdrop = nlohmann::json::object();
	if ( file.is_open() ) {
		try { file >> backdrop; } catch ( ... ) { backdrop = nlohmann::json::object(); }
	}
	backdrop_.Build(backdrop.value("objects", nlohmann::json::array()), { 0.0f, 0.0f, 0.0f }, noise);

	// マス：台座・番号の紙・（まだ遊べなければ）蓋・箱庭
	CreateCardModel("wm_label_lock", "resources/worldmap/label_lock.png", kLabelSize);
	nodes_.clear();
	for ( const WorldStage& stage : data_.stages ) {
		Node node;
		node.roadDistance = road_[0].GetClosestDistance(stage.node);
		const Vector3 top = data_.PedestalTop(stage);
		Vector3 size = data_.pedestalSize;
		node.center = { top.x, top.y + 0.3f, top.z };
		size.z = kLockedDepth;
		if ( stage.unlocked ) {
			node.diorama = std::make_unique<StageDiorama>();
			if ( node.diorama->Build(stage.stage, top, size.x * stage.dioramaMargin, data_.pedestalSize.z * stage.dioramaMargin, GetCamera(), noise, EnvironmentMapSrv()) ) {
				// 台座の奥行きは箱庭の形に合わせる（細長いステージは細長い台座）
				node.center = node.diorama->Center();
				const float depth = node.diorama->BoundsMax().z - node.diorama->BoundsMin().z;
				size.z = std::clamp(depth + kPedestalBorder, 0.6f, data_.pedestalSize.z);
			} else {
				node.diorama.reset();
			}
		}
		// 台座（2×2×1m の台座ブロックを伸ばす。原点は底面の中心）
		node.pedestal = Obj3d::Create("craftPedestal", { top.x, top.y - size.y, top.z }, { 0, 0, 0 }, { size.x * 0.5f, size.y, size.z * 0.5f });
		// 番号の紙：台座の前の面
		const std::string labelModel = "wm_label_" + stage.label;
		CreateCardModel(labelModel, "resources/worldmap/label_" + stage.label + ".png", kLabelSize);
		node.label = Obj3d::Create(labelModel, { top.x, top.y - size.y * 0.5f, top.z - size.z * 0.5f - 0.02f });
		if ( !stage.unlocked ) {
			// 蓋：段ボールの板をかぶせて「？」
			node.lid = Obj3d::Create("craftLayer", top, { 0, 0, 0 }, { size.x * 0.96f, 0.45f, size.z * 0.96f });
			node.lidLabel = Obj3d::Create("wm_label_lock", { top.x, top.y + 0.225f, top.z - size.z * 0.48f - 0.02f }, { 0, 0, 0 }, { 0.8f, 0.8f, 0.8f });
		}
		nodes_.push_back(std::move(node));
	}
}

void StageSelectScene::OnFinalize(){
	RestoreLook();
	lastNode_ = targetNode_;
	nodes_.clear();
	roadMesh_.Clear();
	road_.clear();
	backdrop_.Clear();
	panel_.Finalize();
	dino_.reset();
	guideText_.reset();
}

// 見た目：タイトルと同じジオラマ風のぼかし＋紙の質感＋絵本風。光は手前上から
void StageSelectScene::ApplyLook(){
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->SetEffectActive(PostEffectType::TiltShift, true);
	postEffect->SetEffectActive(PostEffectType::PaperGrain, true);
	postEffect->SetEffectActive(PostEffectType::PictureBook, true);
	postEffect->SetTiltShiftParams(tiltStrength_, tiltCenterY_, tiltHalfWidth_);
	postEffect->SetPaperGrainStrength(0.17f);
	postEffect->SetPictureBookParams(14.0f, 0.8f);

	Obj3dCommon* obj3dCommon = Obj3dCommon::GetInstance();
	if ( Obj3dCommon::DirectionalLightData* light = obj3dCommon->GetLightData() ) {
		if ( !lightSaved_ ) {
			savedLight_ = *light;
			savedPointIntensity_ = obj3dCommon->GetPointLightData()->intensity;
			savedSpotIntensity_ = obj3dCommon->GetSpotLightData()->intensity;
			lightSaved_ = true;
		}
		light->activeCount = 1;
		light->lights[0].color = { 1.0f, 0.97f, 0.9f, 1.0f };
		light->lights[0].direction = VectorMath::Normalize({ 0.35f, -0.7f, 0.6f });
		light->lights[0].intensity = 1.25f;
		obj3dCommon->GetPointLightData()->intensity = 0.0f;
		obj3dCommon->GetSpotLightData()->intensity = 0.0f;
	}
}

// 次のシーンへ見た目を持ち越さない
void StageSelectScene::RestoreLook(){
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->SetEffectActive(PostEffectType::TiltShift, false);
	postEffect->SetEffectActive(PostEffectType::PaperGrain, false);
	postEffect->SetEffectActive(PostEffectType::PictureBook, false);
	postEffect->SetEffectActive(PostEffectType::IrisWipe, false);
	postEffect->SetIrisParams(2.0f, 0.5f, 0.5f);
	if ( lightSaved_ ) {
		Obj3dCommon* obj3dCommon = Obj3dCommon::GetInstance();
		*obj3dCommon->GetLightData() = savedLight_;
		obj3dCommon->GetPointLightData()->intensity = savedPointIntensity_;
		obj3dCommon->GetSpotLightData()->intensity = savedSpotIntensity_;
		lightSaved_ = false;
	}
}

// SDF看板（ステージの中に立てた物）はここには出さない
bool StageSelectScene::GetSdfViewerPosition(Vector3& outPos) const{
	outPos = { 0.0f, -100000.0f, 0.0f };
	return true;
}

// =====================================================================
//  進行
// =====================================================================
int StageSelectScene::StandingNode() const{
	if ( targetNode_ < 0 || targetNode_ >= ( int ) nodes_.size() ) { return -1; }
	return std::abs(dinoDistance_ - nodes_[targetNode_].roadDistance) < 0.01f ? targetNode_ : -1;
}

bool StageSelectScene::IsPreviewing() const{
	return phase_ != Phase::Opening && StandingNode() >= 0;
}

void StageSelectScene::OnUpdate(){
	const float dt = kDeltaTime;
	phaseTime_ += dt;
	switch ( phase_ ) {
	case Phase::Opening:
		if ( phaseTime_ >= kOpenTime ) {
			phase_ = Phase::Map;
			PostEffect::GetInstance()->SetEffectActive(PostEffectType::IrisWipe, false);
		}
		UpdateWalk(dt);
		break;
	case Phase::Map:
		UpdateWalk(dt);
		break;
	case Phase::Dive:
	case Phase::Leaving:
		UpdateDive(dt);
		break;
	}

	// 立っているマスが変わった：箱庭が跳ねる・パネルの中身・カメラが回り始める位置
	const int standing = ( phase_ == Phase::Map || phase_ == Phase::Dive ) ? StandingNode() : -1;
	if ( standing != previewNode_ && phase_ != Phase::Dive ) {
		if ( previewNode_ >= 0 && nodes_[previewNode_].diorama ) { nodes_[previewNode_].diorama->SetSelected(false); }
		previewNode_ = standing;
		if ( standing >= 0 ) {
			const WorldStage& stage = data_.stages[standing];
			StageDiorama* diorama = nodes_[standing].diorama.get();
			if ( diorama ) { diorama->SetSelected(true); }
			panel_.SetContent(stage.stage, stage.unlocked && diorama, diorama ? diorama->CoinCount() : 0, 0);
			orbitAngle_ = 0.0f;
			panProgress_ = 0.0f;
			AudioManager::GetInstance()->PlayWave(kSplashSe, false, 0.5f);
		}
	}
	const bool previewing = previewNode_ >= 0;
	const float wanted = previewing ? 1.0f : 0.0f;
	const float step = dt / kPreviewBlendTime;
	previewBlend_ = previewBlend_ < wanted ? ( std::min )( previewBlend_ + step, wanted ) : ( std::max )( previewBlend_ - step * 1.5f, wanted );
	if ( previewing && phase_ == Phase::Map ) {
		orbitAngle_ += dt * 2.0f * kPi / ( std::max )( data_.stages[previewNode_].orbitSeconds, 1.0f );
		// 布の道をなぞっている所を、なめらかに追う（最初へ戻る時はすっと戻る）
		if ( const StageDiorama* diorama = nodes_[previewNode_].diorama.get() ) {
			panProgress_ += ( diorama->TraceProgress() - panProgress_ ) * ( std::min )( dt * 2.0f, 1.0f );
		}
	}

	UpdateDino(dt);
	for ( int i = 0; i < ( int ) nodes_.size(); ++i ) {
		Node& node = nodes_[i];
		if ( node.diorama ) { node.diorama->Update(dt, i == previewNode_, GetCamera()); }
		for ( Obj3d* obj : { node.pedestal.get(), node.label.get(), node.lid.get(), node.lidLabel.get() } ) {
			if ( obj ) { obj->Update(); }
		}
	}
	roadMesh_.Update(road_);
	backdrop_.Update(GetCamera());
	panel_.Update(dt, previewing && phase_ == Phase::Map && previewBlend_ > 0.25f);
	UpdateTexts();

	// 始まりの円：恐竜を中心に開く
	if ( phase_ == Phase::Opening ) {
		const float t = Easing::Clamp01(phaseTime_ / kOpenTime);
		float u = 0.5f, v = 0.5f;
		Vector2 ndc;
		const Vector3 dino = road_.empty() ? Vector3 {} : road_[0].GetPositionByDistance(dinoDistance_);
		if ( MatrixMath::WorldToNdc({ dino.x, dino.y + 0.6f, dino.z }, GetCamera()->GetViewProjectionMatrix(), ndc) ) {
			u = ndc.x * 0.5f + 0.5f;
			v = 1.0f - ( ndc.y * 0.5f + 0.5f );
		}
		PostEffect::GetInstance()->SetIrisParams(1.4f * t * t, u, v);
	}
}

// マスからマスへ歩く（A / D）。SPACE で飛び込む・T でタイトルへ
void StageSelectScene::UpdateWalk(float deltaTime){
	if ( phase_ == Phase::Map && !IsTypingInEditor() && !nodes_.empty() ) {
		Input* input = Input::GetInstance();
		if ( input->Triggerkey(DIK_A) || input->Triggerkey(DIK_LEFT) ) { targetNode_ = ( std::max )( targetNode_ - 1, 0 ); }
		if ( input->Triggerkey(DIK_D) || input->Triggerkey(DIK_RIGHT) ) { targetNode_ = ( std::min )( targetNode_ + 1, ( int ) nodes_.size() - 1 ); }
		if ( input->Triggerkey(DIK_SPACE) && StandingNode() >= 0 && data_.stages[targetNode_].unlocked && nodes_[targetNode_].diorama ) {
			StartDive();
			return;
		}
		if ( input->Triggerkey(DIK_T) ) {
			phase_ = Phase::Leaving;
			phaseTime_ = 0.0f;
			KamishibaiTransition::ChangeSceneWithCard("TITLE");
			return;
		}
	}
	if ( nodes_.empty() ) { return; }
	const float goal = nodes_[targetNode_].roadDistance;
	const float diff = goal - dinoDistance_;
	const float move = kWalkSpeed * deltaTime;
	dinoDistance_ = std::abs(diff) <= move ? goal : dinoDistance_ + ( diff > 0.0f ? move : -move );
}

void StageSelectScene::StartDive(){
	Node& node = nodes_[targetNode_];
	const Vector3 from = road_[0].GetPositionByDistance(dinoDistance_);
	const Vector3 center = node.diorama->Center();
	const float yaw = std::atan2(center.x - from.x, center.z - from.z);
	dinoYaw_ = yaw;
	dive_.Start(timing_, from, yaw, center, cameraPosition_, cameraLook_);
	phase_ = Phase::Dive;
	phaseTime_ = 0.0f;
	if ( dino_ ) {
		dino_->SetClip("Idle", true);
		dinoClip_ = "Idle";
	}
}

// 飛び込み：5段の形・箱庭の弾み・円を閉じて、ステージ開始カードへ
void StageSelectScene::UpdateDive(float deltaTime){
	if ( !dive_.IsActive() ) { return; }
	dive_.Update(deltaTime);
	if ( dive_.ConsumeJumped() ) { AudioManager::GetInstance()->PlayWave(kJumpSe, false, 0.8f); }
	if ( dive_.ConsumeAbsorbed() ) {
		AudioManager::GetInstance()->PlayWave(kSplashSe);
		if ( previewNode_ >= 0 && nodes_[previewNode_].diorama ) { nodes_[previewNode_].diorama->Splash(); }
		PostEffect::GetInstance()->SetEffectActive(PostEffectType::IrisWipe, true);
	}
	// 円は箱庭の真ん中へ閉じる
	if ( PostEffect::GetInstance() && dive_.IrisRadius() < 1.99f && previewNode_ >= 0 ) {
		float u = 0.5f, v = 0.5f;
		Vector2 ndc;
		if ( MatrixMath::WorldToNdc(nodes_[previewNode_].center, GetCamera()->GetViewProjectionMatrix(), ndc) ) {
			u = ndc.x * 0.5f + 0.5f;
			v = 1.0f - ( ndc.y * 0.5f + 0.5f );
		}
		PostEffect::GetInstance()->SetIrisParams(( std::max )( dive_.IrisRadius(), 0.0f ), u, v);
	}
	if ( phase_ == Phase::Dive && dive_.IsFinished() ) {
		phase_ = Phase::Leaving;
		lastNode_ = targetNode_;
		StageIntroSequence::RequestDropIn(); // 着いたら上から落ちてくる
		KamishibaiTransition::ChangeSceneWithStageIntro("GAMEPLAY", data_.stages[targetNode_].stage);
	}
}

// 恐竜：歩く向き・クリップ。飛び込み中は飛び込みの形
void StageSelectScene::UpdateDino(float deltaTime){
	if ( !dino_ || road_.empty() ) { return; }
	if ( dive_.IsActive() ) {
		const DiveSequence::Pose pose = dive_.DinoPose();
		dino_->SetTranslation(pose.position);
		dino_->SetRotation(pose.rotation);
		dino_->SetScale(pose.visible ? pose.scale : Vector3 { 0.0f, 0.0f, 0.0f });
		dino_->Update();
		return;
	}
	const Vector3 position = road_[0].GetPositionByDistance(dinoDistance_);
	const float diff = nodes_.empty() ? 0.0f : nodes_[targetNode_].roadDistance - dinoDistance_;
	const bool walking = std::abs(diff) > 0.01f;
	float wantedYaw = dinoYaw_;
	if ( walking ) {
		const Vector3 tangent = road_[0].GetTangentByDistance(dinoDistance_);
		wantedYaw = std::atan2(tangent.x, tangent.z) + ( diff < 0.0f ? kPi : 0.0f );
	} else if ( previewNode_ >= 0 ) {
		// マスに立ったら台座（箱庭）の方を向く
		const Vector3 center = nodes_[previewNode_].center;
		wantedYaw = std::atan2(center.x - position.x, center.z - position.z);
	}
	dinoYaw_ += WrapAngle(wantedYaw - dinoYaw_) * ( std::min )( deltaTime * kTurnRate, 1.0f );
	const std::string clip = walking ? "Walk" : "Idle";
	if ( clip != dinoClip_ ) {
		dino_->SetClip(clip, true);
		dinoClip_ = clip;
	}
	dino_->SetPlaybackSpeed(walking ? kWalkSpeed / 5.0f : 1.0f);
	dino_->SetTranslation(position);
	dino_->SetRotation({ 0.0f, dinoYaw_, 0.0f });
	dino_->SetScale({ 1.0f, 1.0f, 1.0f });
	dino_->Update();
}

void StageSelectScene::UpdateTexts(){
	if ( !guideText_ ) { return; }
	const float width = ( float ) WindowProc::GetInstance()->GetClientWidth();
	const float height = ( float ) WindowProc::GetInstance()->GetClientHeight();
	const float alpha = phase_ == Phase::Map ? 0.9f : 0.0f;
	guideText_->SetColor({ 1.0f, 0.97f, 0.88f, alpha });
	guideText_->SetOutlineColor({ 0.27f, 0.17f, 0.08f, alpha });
	guideText_->SetPosition(width - kGuideFontSize * 8.5f, height - kGuideFontSize * 2.0f);
}

// =====================================================================
//  カメラ
// =====================================================================
// マップを歩く時：恐竜の後ろ上から
void StageSelectScene::MapCamera(Vector3& outPosition, Vector3& outLook) const{
	const Vector3 dino = road_.empty() ? Vector3 {} : road_[0].GetPositionByDistance(dinoDistance_);
	outPosition = { dino.x + data_.cameraOffset.x, dino.y + data_.cameraOffset.y, dino.z + data_.cameraOffset.z };
	outLook = { dino.x + data_.cameraLook.x, dino.y + data_.cameraLook.y, dino.z + data_.cameraLook.z };
}

// プレビュー：箱庭に寄り、布の道をなぞる所を追って左から右へ流れながら、左右にゆっくり振れる
//   （箱庭の裏は木が立っているので、ぐるっと1周はしない）。左のパネルの分、見る所を画面の右へ寄せる
void StageSelectScene::PreviewCamera(Vector3& outPosition, Vector3& outLook) const{
	if ( previewNode_ < 0 ) { MapCamera(outPosition, outLook); return; }
	const WorldStage& stage = data_.stages[previewNode_];
	const Node& node = nodes_[previewNode_];
	Vector3 focus = node.center;
	if ( node.diorama ) {
		const float left = node.diorama->BoundsMin().x + kPreviewEdge;
		const float right = ( std::max )( node.diorama->BoundsMax().x - kPreviewEdge, left );
		focus.x = left + ( right - left ) * panProgress_;
	}
	const float a = kPreviewSway * std::sin(orbitAngle_);
	outPosition = { focus.x + std::sin(a) * stage.previewDistance, focus.y + stage.previewHeight, focus.z - std::cos(a) * stage.previewDistance };
	const Vector3 side { std::cos(a), 0.0f, std::sin(a) }; // カメラの右
	outLook = { focus.x - side.x * kPreviewShift, focus.y, focus.z - side.z * kPreviewShift };
}

void StageSelectScene::UpdateCamera(){
	Camera* camera = GetCamera();
	DebugCamera* debugCamera = GetDebugCamera();
	if ( debugCamera && debugCamera->IsActive() ) { BaseScene::UpdateCamera(); return; }
	// OnUpdate はカメラの後に来るので、ここで1フレーム前の状態から決める（ずれは見えない）
	if ( dive_.IsActive() ) {
		dive_.CameraPose(cameraPosition_, cameraLook_);
	} else {
		Vector3 mapPosition, mapLook, previewPosition, previewLook;
		MapCamera(mapPosition, mapLook);
		PreviewCamera(previewPosition, previewLook);
		const float t = Easing::EaseInOutSine(previewBlend_);
		cameraPosition_ = Lerp(mapPosition, previewPosition, t);
		cameraLook_ = Lerp(mapLook, previewLook, t);
	}
	camera->SetFovY(data_.fovY);
	LookAt(camera, cameraPosition_, cameraLook_);
	camera->Update();
}

// =====================================================================
//  描画
// =====================================================================
void StageSelectScene::OnDrawOpaque(ID3D12GraphicsCommandList* /*commandList*/){
	roadMesh_.Draw();
	for ( Node& node : nodes_ ) {
		for ( Obj3d* obj : { node.pedestal.get(), node.label.get(), node.lid.get(), node.lidLabel.get() } ) {
			if ( obj ) { obj->Draw(); }
		}
		if ( node.diorama ) { node.diorama->DrawOpaque(); }
	}
	if ( dino_ ) { dino_->Draw(); }
}

void StageSelectScene::OnDrawInstanced(){
	backdrop_.Draw(GetCamera());
	for ( Node& node : nodes_ ) {
		if ( node.diorama ) { node.diorama->DrawInstanced(GetCamera()); }
	}
}

void StageSelectScene::OnDrawOverlay2D(ID3D12GraphicsCommandList* commandList){
	if ( guideText_ && phase_ == Phase::Map ) { SDFManager::GetInstance()->DrawTextItem(commandList, *guideText_, kFontAtlas); }
}

void StageSelectScene::OnDrawUI(ID3D12GraphicsCommandList* /*commandList*/){
	panel_.Draw();
}

void StageSelectScene::OnDrawInspector(){
#ifdef USE_IMGUI
	if ( ImGui::CollapsingHeader("ワールドマップ") ) {
		bool changed = false;
		changed |= ImGui::SliderFloat("ぼかしの強さ", &tiltStrength_, 0.0f, 12.0f);
		changed |= ImGui::SliderFloat("ぼかしの中心", &tiltCenterY_, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("ぼかしの幅", &tiltHalfWidth_, 0.05f, 0.6f);
		if ( changed ) { PostEffect::GetInstance()->SetTiltShiftParams(tiltStrength_, tiltCenterY_, tiltHalfWidth_); }
		ImGui::Text("マス %d / %d  立っている %d  プレビュー %.2f", targetNode_ + 1, ( int ) nodes_.size(), previewNode_, previewBlend_);
		if ( ImGui::Button("飛び込みの時刻を読み直す (dive.json)") ) { timing_.Load(kDiveFile); }
	}
#endif
}
