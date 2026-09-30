#include "TitleScene.h"
// --- ゲーム固有のファイル ---
#include "GamePlayScene.h"
#include "game/title/TitleAssets.h"
#include "game/title/TitleLayout.h"

// --- エンジン側のファイル ---
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/audio/AudioManager.h"
#include "engine/base/Input.h"
#include "engine/base/WindowProc.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/TextureManager.h"
#include "engine/math/VectorMath.h"
#include "engine/postEffect/PostEffect.h"
#include "engine/rail/SplineRail.h"
#include "engine/scene/SceneManager.h"
#include "engine/sdf/SDFManager.h"
#include "engine/sdf/SDFText.h"
#include "engine/utils/Easing.h"
#include "engine/utils/EditorManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>
#include <string>

using namespace TitleLayout;

namespace {
	constexpr float kDeltaTime = 1.0f / 60.0f;
	constexpr float kPi = 3.14159265358979323846f;

	// --- 登場（自動操縦）---
	constexpr float kIntroMoveSpeed  = 6.0f;   // 登場中の走る速さ(m/s)
	constexpr float kMenuMoveSpeed   = 5.0f;   // 自分で歩く時の速さ（本編と同じ）
	constexpr float kArriveDistance  = 0.3f;   // 角に「着いた」とみなす距離(m)
	constexpr float kLogoThrowX      = -0.3f * kRailX; // 奥のレールのここを通る時にロゴを投げる
	constexpr float kIntroTimeLimit  = 14.0f;  // 何かで進めなくなっても、ここで登場を切り上げる(秒)

	// --- メニュー ---
	constexpr float kFocusRange     = 3.4f;    // この距離(m)まで近づいた的が選ばれる
	constexpr float kAimTimeLimit   = 0.5f;    // 的へ向き直るのを待つ上限(秒)
	constexpr float kEggFlightTime  = 0.45f;   // 卵が画面へ届くまで(秒)
	constexpr float kIrisCloseTime  = 0.35f;   // 画面を閉じる時間(秒)
	constexpr float kIrisHoldTime   = 0.15f;   // 閉じ切ってからシーンを切り替えるまで(秒)
	constexpr float kNoticeTime     = 1.4f;    // 「じゅんびちゅう」を出す時間(秒)

	const char* kGuideMessage  = "A/D : いどう   W/S : のりかえ   E : えらぶ";
	const char* kNoticeMessage = "じゅんびちゅう";
	const char* kFontAtlas = "jpdot";
	constexpr float kGuideFontSize  = 34.0f;
	constexpr float kNoticeFontSize = 52.0f;
	// jpdot は線が細く、ふちの色がそのまま文字の色に見えるので、中とふちを同じ色にして少し太らせる
	constexpr float   kTextThickness = 0.1f;
	constexpr Vector4 kGuideColor  { 1.0f, 0.97f, 0.88f, 1.0f };  // 手前の段ボールのふち（暗い茶色）の上に出すので明るい色
	constexpr Vector4 kNoticeColor { 0.24f, 0.15f, 0.09f, 1.0f }; // 空の上に出すのでこげ茶

	// jpdot は等幅（半角＝文字サイズの半分 / 全角＝文字サイズ）。中央寄せ用に幅を見積もる
	float EstimateTextWidth(const std::string& utf8Text, float fontSize){
		float width = 0.0f;
		for ( unsigned char character : utf8Text ) {
			if ( ( character & 0xC0 ) == 0x80 ) { continue; } // UTF-8 の2バイト目以降
			width += ( character < 0x80 ) ? fontSize * 0.5f : fontSize;
		}
		return width;
	}

	// 文字入力中（ImGui の入力欄）はキー操作を受け付けない
	bool IsTypingInEditor(){
#ifdef USE_IMGUI
		return ImGui::GetIO().WantTextInput;
#else
		return false;
#endif
	}

	// 決定キー（E＝本編のベロと同じ / Enter）
	bool DecidePressed(){
		if ( IsTypingInEditor() ) { return false; }
		Input* input = Input::GetInstance();
		return input->Triggerkey(DIK_E) || input->Triggerkey(DIK_RETURN);
	}

	// -π〜π に収めた角度の差
	float AngleDelta(float from, float to){
		float delta = std::fmod(to - from + kPi, kPi * 2.0f);
		if ( delta < 0.0f ) { delta += kPi * 2.0f; }
		return delta - kPi;
	}

	float Sign(float value){ return ( value > 0.0f ) ? 1.0f : -1.0f; }

	// position にいちばん近いレールと、その上の距離を探す（無ければ false）
	bool FindRailNear(const std::vector<SplineRail>& rails, const Vector3& position, int& outRail, float& outDistance){
		float best = 1e9f;
		outRail = -1;
		for ( int i = 0; i < ( int ) rails.size(); ++i ) {
			if ( rails[i].nodes.size() < 2 ) { continue; }
			const float distance = rails[i].GetClosestDistance(position);
			const float gap = VectorMath::Length(rails[i].GetPositionByDistance(distance) - position);
			if ( gap < best ) { best = gap; outRail = i; outDistance = distance; }
		}
		return outRail >= 0;
	}
}

TitleScene::TitleScene(){
	features_.overlay2D = true;                        // 操作の案内を最終画像へ重ねる
	features_.sceneMap = "resources/map/title.json";   // 背景とレール（ステージのマップとは別ファイル）
	// レールは使うが、エディタの線とノードは既定では出さない（インスペクターの「レールを編集する」で出す）
	features_.railEditing = false;
}
TitleScene::~TitleScene(){
	OnFinalize();
}

// モデル・画像・音（実行中の読み込みはデバッグレイヤーが嫌うので、使う物は先に読む）
void TitleScene::OnLoadResources(){
	TitleAssets::Load();
	// マップにあるのにモデルが見つからない物の代わりの表示
	ModelManager::GetInstance()->CreateSphereModel("sphere", 16);
}

void TitleScene::OnInitialize(){
	SetupCamera();
	ApplyLook();

	dino_.Initialize(EnvironmentMapSrv());
	logo_.Initialize();
	menu_.Initialize();
	paperBits_.Initialize();
	ambience_.Initialize(GetCamera());
	egg_ = Obj3d::Create("egg");

	// 文字（アトラスは SDFManager が resources/sdf/ から自動ロードするので、ここではアイテムを作るだけ）
	guideText_ = std::make_unique<SDFText>();
	guideText_->Initialize();
	guideText_->SetText(kGuideMessage);
	guideText_->SetFontSize(kGuideFontSize);
	guideText_->SetOutlineWidth(0.2f);
	guideText_->SetThickness(kTextThickness);

	noticeText_ = std::make_unique<SDFText>();
	noticeText_->Initialize();
	noticeText_->SetText(kNoticeMessage);
	noticeText_->SetFontSize(kNoticeFontSize);
	noticeText_->SetOutlineWidth(0.2f);
	noticeText_->SetThickness(kTextThickness);

	// レールと道はマップ（title.json）のレールから作る。本編と同じ RailField / RoadMesh
	SyncRails();
	StartIntro();
}

void TitleScene::OnFinalize(){
	RestoreLook();

	// タイトル専用オブジェクトの GPU リソースを明示的に解放
	dino_.Finalize();
	logo_.Finalize();
	menu_.Finalize();
	paperBits_.Finalize();
	ambience_.Finalize();
	egg_.reset();
	guideText_.reset();
	noticeText_.reset();
	roadMesh_.Clear();
}

// SDF看板（操作説明など）はステージの中に立てた物なので、タイトルには出さない。
//   看板は「近づいた時だけ表示」なので、基準位置をはるか遠くにしておけば現れない
bool TitleScene::GetSdfViewerPosition(Vector3& outPos) const{
	outPos = { 0.0f, -100000.0f, 0.0f };
	return true;
}

void TitleScene::SetupCamera(){
	Camera* camera = GetCamera();
	const Vector3 toTarget = kCameraTarget - kCameraPosition;
	const float horizontal = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
	camera->SetTranslation(kCameraPosition);
	// 回転は X=見下ろす向きが正、Y=右回りが正
	camera->SetRotation({ std::atan2(-toTarget.y, horizontal), std::atan2(toTarget.x, toTarget.z), 0.0f });
	camera->SetFovY(kCameraFovY);
}

// タイトルの見た目：ジオラマ風のぼかし＋紙の質感＋絵本風。光は手前上から当てて、切り絵の正面を明るくする
void TitleScene::ApplyLook(){
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->SetEffectActive(PostEffectType::TiltShift, true);
	postEffect->SetEffectActive(PostEffectType::PaperGrain, true);
	postEffect->SetEffectActive(PostEffectType::PictureBook, true);
	postEffect->SetEffectActive(PostEffectType::IrisWipe, false);
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
		// 点光源・スポットライトは場所でムラが出るので、タイトルの間は消す
		obj3dCommon->GetPointLightData()->intensity = 0.0f;
		obj3dCommon->GetSpotLightData()->intensity = 0.0f;
	}
}

// 次のシーンへ見た目を持ち越さない
void TitleScene::RestoreLook(){
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->SetEffectActive(PostEffectType::TiltShift, false);
	postEffect->SetEffectActive(PostEffectType::PaperGrain, false);
	postEffect->SetEffectActive(PostEffectType::PictureBook, false);
	postEffect->SetEffectActive(PostEffectType::IrisWipe, false);
	postEffect->SetIrisParams(2.0f, 0.5f, 0.5f); // 全開（＝効いていない状態）

	if ( lightSaved_ ) {
		Obj3dCommon* obj3dCommon = Obj3dCommon::GetInstance();
		*obj3dCommon->GetLightData() = savedLight_;
		obj3dCommon->GetPointLightData()->intensity = savedPointIntensity_;
		obj3dCommon->GetSpotLightData()->intensity = savedSpotIntensity_;
		lightSaved_ = false;
	}
}

// =====================================================================
//  レール・道（本編と同じ仕組み）
// =====================================================================
// マップのレールから、実行用のレール・道・スタート地点を作り直す。
//   simple=true はエディタでドラッグ中の軽い作り直し（道は簡易表示）
void TitleScene::SyncRails(bool simple){
	const uint32_t whiteTexture = TextureManager::GetInstance()->Load("resources/block/white1x1.png").srvIndex;
	railField_.Sync(GetCamera(), whiteTexture);
	railField_.SetShowMarkers(editRails_);
	roadMesh_.Build(railField_.GetRails(), GetCamera(), simple);
	player_.SetSpawn(railField_.GetStartRail(), railField_.GetStartDistance());
}

// エディタでレールを編集したら追従する（ドラッグ中は10回/秒の軽い作り直し、離したら本生成）
void TitleScene::OnPreUpdate(){
	EditorManager* editorManager = EditorManager::GetInstance();
	const bool dragging = editorManager->IsRailDragging();
	const bool changed = ( editorManager->GetRailEditVersion() != railField_.Version() );
	railSyncTimer_ += kDeltaTime;
	if ( changed && dragging ) {
		railFullSyncPending_ = true;
		if ( railSyncTimer_ >= 0.1f ) {
			railSyncTimer_ = 0.0f;
			SyncRails(true);
		}
	} else if ( !dragging && ( changed || railFullSyncPending_ ) ) {
		railFullSyncPending_ = false;
		SyncRails(false);
	}
}

// =====================================================================
//  進行
// =====================================================================
// 登場：恐竜を奥のレールの左端に出し、自動操縦で走らせる
void TitleScene::StartIntro(){
	phase_ = Phase::Intro;
	logoThrown_ = false;
	eggVisible_ = false;
	noticeTime_ = 0.0f;
	introTime_ = 0.0f;
	waypointIndex_ = 0;
	pilotRail_ = -1;
	pilotReleaseFrames_ = 0;
	pilotSwitchWait_ = 0;
	pilotHopFrames_ = 0;
	logo_.Reset();
	menu_.Reset();
	menu_.SetSelected(-1);
	PostEffect::GetInstance()->SetEffectActive(PostEffectType::IrisWipe, false);

	// 奥のレールの左端から始める。見つからなければ登場を飛ばす
	const auto& rails = railField_.GetRails();
	int rail = -1;
	float distance = 0.0f;
	const Vector3 entry { -kRailX + 0.5f, kGroundTop, kRailBack };
	if ( !FindRailNear(rails, entry, rail, distance) ) { SkipIntro(); return; }

	ReleasePlayer();
	player_.SetSpawn(rail, distance);
	player_.Initialize();
	// 落ちた時に戻る場所は、マップのスタート地点（手前の中央）にしておく
	player_.SetSpawn(railField_.GetStartRail(), railField_.GetStartDistance());
	*player_.MoveSpeedPtr() = kIntroMoveSpeed;
	player_.SetAutoPilot(true);

	dino_.Appear();
	Vector3 puff = rails[rail].GetPositionByDistance(distance);
	puff.y += 0.4f;
	paperBits_.Burst(puff, 10, 2.4f, 3.4f); // ぽんっと現れる
}

void TitleScene::SkipIntro(){
	ReleasePlayer();
	player_.SetSpawn(railField_.GetStartRail(), railField_.GetStartDistance());
	player_.Initialize();
	dino_.ShowImmediately();
	logo_.PlaceLanded();
	menu_.ShowImmediately();
	logoThrown_ = true;
	EnterMenu();
}

void TitleScene::EnterMenu(){
	player_.SetAutoPilot(false);
	*player_.MoveSpeedPtr() = kMenuMoveSpeed;
	phase_ = Phase::Menu;
}

void TitleScene::ReleasePlayer(){
	player_.SetMovementLocked(false);
	player_.SetFaceOverride(false, 0.0f);
	dino_.CancelAction();
}

void TitleScene::OnUpdate(){
	const auto& rails = railField_.GetRails();
	player_.SetCameraYaw(GetCamera()->GetRotation().y);

	switch ( phase_ ) {
	case Phase::Intro:  UpdateIntro();  break;
	case Phase::Menu:   UpdateMenu();   break;
	case Phase::Aim:    UpdateAim();    break;
	case Phase::Tongue: UpdateTongue(); break;
	case Phase::TurnToCamera:
		// カメラのほうへ向き直ったら、卵を構えて投げる
		if ( std::abs(AngleDelta(player_.GetRotation().y, aimYaw_)) < 0.06f ) {
			dino_.StartThrow();
			eggVisible_ = true;
			phase_ = Phase::Throw;
		}
		break;
	default:
		break;
	}

	// 本編と同じ移動処理（レール上の移動・乗り換え・ジャンプ）
	player_.Update(rails);
	roadMesh_.Update(rails);
	railField_.UpdateMarkers();

	dino_.Update(player_, kDeltaTime);
	logo_.Update(kDeltaTime);
	menu_.Update(kDeltaTime);
	paperBits_.Update(kDeltaTime);
	ambience_.Update(kDeltaTime, player_.GetPosition()); // 草花は恐竜が通ると押される
	UpdateEgg(kDeltaTime);       // 恐竜の手の位置を使うので、恐竜の後
	UpdateIrisOut(kDeltaTime);
	UpdateTexts(kDeltaTime);
}

// 登場の自動操縦：目指す角へ向かう入力を作る。
//   レールの向きに沿って進めるなら「移動キー」を押し続け、角で曲がる時は「乗り換えキー」を押す
//   （人がキーを押すのと同じ入力を Player へ渡すので、移動・乗り換えの処理は本編そのもの）
PlayerInput::AutoPilot TitleScene::SteerAutoPilot(){
	PlayerInput::AutoPilot pilot;
	const auto& rails = railField_.GetRails();
	const int currentRail = player_.GetCurrentRail();
	if ( currentRail < 0 || currentRail >= ( int ) rails.size() ) { return pilot; }

	// 奥の右の角 → 手前の右の角 → スタート地点（手前の中央）
	const Vector3 start = rails[railField_.GetStartRail()].GetPositionByDistance(railField_.GetStartDistance());
	const Vector3 waypoints[] = {
		{ kRailX, 0.0f, kRailBack },
		{ kRailX, 0.0f, kRailFront },
		{ start.x, 0.0f, start.z },
	};
	const int waypointCount = ( int ) ( sizeof(waypoints) / sizeof(waypoints[0]) );
	if ( waypointIndex_ >= waypointCount ) { return pilot; } // 着いた：何も押さない

	const Vector3& position = player_.GetPosition();
	float toX = waypoints[waypointIndex_].x - position.x;
	float toZ = waypoints[waypointIndex_].z - position.z;
	if ( std::sqrt(toX * toX + toZ * toZ) < kArriveDistance ) {
		++waypointIndex_;
		pilotSwitchWait_ = 0;
		if ( waypointIndex_ >= waypointCount ) { return pilot; }
		toX = waypoints[waypointIndex_].x - position.x;
		toZ = waypoints[waypointIndex_].z - position.z;
	}

	// レールを乗り換えた直後は、いったんキーを離す（本編でも、乗り換えた後は押し直さないと進まない）
	if ( currentRail != pilotRail_ ) {
		pilotRail_ = currentRail;
		pilotReleaseFrames_ = 2;
	}
	if ( pilotReleaseFrames_ > 0 ) { --pilotReleaseFrames_; return pilot; }

	const bool horizontal = ( rails[currentRail].type == SplineRail::RailType::Horizontal );
	const float along  = horizontal ? toX : toZ; // レールの向きに沿った残り
	const float across = horizontal ? toZ : toX; // レールを横切る向きの残り
	if ( std::abs(along) > kArriveDistance * 0.6f ) {
		( horizontal ? pilot.moveX : pilot.moveZ ) = Sign(along);           // そのまま進む
	} else if ( std::abs(across) > kArriveDistance * 0.6f ) {
		if ( --pilotSwitchWait_ <= 0 ) {
			( horizontal ? pilot.switchZ : pilot.switchX ) = ( int ) Sign(across); // 角で乗り換える
			pilotSwitchWait_ = 15;
		}
	}
	return pilot;
}

// 登場：走る → 奥のレールの途中でロゴを投げる → 刺さったらメニューが出る → 手前の中央に着いたら操作開始
void TitleScene::UpdateIntro(){
	Input* input = Input::GetInstance();
	if ( DecidePressed() || ( !IsTypingInEditor() && input->Triggerkey(DIK_SPACE) ) ) {
		SkipIntro(); // 待たずに始めたい人向け
		return;
	}
	introTime_ += kDeltaTime;
	if ( introTime_ > kIntroTimeLimit ) { SkipIntro(); return; }

	PlayerInput::AutoPilot pilot = SteerAutoPilot();

	// 奥のレールの途中で、小さく跳ねながらロゴを投げる
	//   （最初の数フレームは恐竜の位置がまだ決まっていないので見ない）
	if ( !logoThrown_ && introTime_ > 0.1f && waypointIndex_ == 0 && player_.GetPosition().x >= kLogoThrowX ) {
		logoThrown_ = true;
		Vector3 from = player_.GetPosition();
		from.y += 1.6f; // 頭の上から
		logo_.Launch(from);
		pilot.jumpPressed = true;
		pilotHopFrames_ = 8;
		AudioManager::GetInstance()->PlayWave(TitleAssets::kThrowSe);
	}
	if ( pilotHopFrames_ > 0 ) { --pilotHopFrames_; pilot.jumpHeld = true; }
	player_.SetAutoPilot(true, pilot);

	if ( logo_.ConsumeLanded() ) {
		AudioManager::GetInstance()->PlayWave(TitleAssets::kHitSe);
		menu_.Show();
		// 刺さった根元から紙くずが舞う
		paperBits_.Burst({ kLogoLand.x, kLogoLand.y + 0.3f, kLogoLand.z - 0.2f }, 14, 3.0f, 3.6f);
	}
	const bool arrived = ( waypointIndex_ >= 3 );
	if ( arrived && logo_.HasLanded() && menu_.IsReady() ) { EnterMenu(); }
}

// 自分で歩く：近づいた的が選ばれ、E（Enter）でベロを当てて決定
void TitleScene::UpdateMenu(){
	const int focus = menu_.FindNearest(player_.GetPosition(), kFocusRange);
	menu_.SetSelected(focus);

	if ( focus >= 0 && player_.IsGrounded() && DecidePressed() ) {
		const Vector3 target = menu_.GetTargetCenter(focus);
		const float toX = target.x - player_.GetPosition().x;
		const float toZ = target.z - player_.GetPosition().z;
		tongueDistance_ = std::sqrt(toX * toX + toZ * toZ);
		aimYaw_ = std::atan2(toX, toZ);
		aimTime_ = 0.0f;
		noticeTime_ = 0.0f;
		// 決定の動きが終わるまで、その場で的のほうを向かせる
		player_.SetMovementLocked(true);
		player_.SetFaceOverride(true, aimYaw_);
		phase_ = Phase::Aim;
	}
}

// 的のほうへ向き直ったら、ベロを伸ばす
void TitleScene::UpdateAim(){
	aimTime_ += kDeltaTime;
	const bool facing = std::abs(AngleDelta(player_.GetRotation().y, aimYaw_)) < 0.06f;
	if ( facing || aimTime_ > kAimTimeLimit ) {
		dino_.ShootTongue(tongueDistance_);
		AudioManager::GetInstance()->PlayWave(TitleAssets::kThrowSe);
		phase_ = Phase::Tongue;
	}
}

// ベロが当たったら的を揺らす。戻り切ったら、項目ごとの動きへ
void TitleScene::UpdateTongue(){
	const int selected = menu_.GetSelected();
	if ( selected < 0 ) { ReleasePlayer(); phase_ = Phase::Menu; return; }

	if ( dino_.ConsumeTongueHit() ) {
		menu_.Hit(selected);
		AudioManager::GetInstance()->PlayWave(TitleAssets::kHitSe);
		Vector3 hit = menu_.GetTargetCenter(selected);
		hit.z -= 0.2f; // 的の手前の面から
		paperBits_.Burst(hit, 8, 2.4f, 2.6f, 0.18f);
	}
	if ( dino_.IsBusy() ) { return; } // まだベロを出している

	if ( selected == TitleMenu::Item_Start ) {
		// カメラのほうへ向き直る
		const Vector3 toCamera = kCameraPosition - player_.GetPosition();
		aimYaw_ = std::atan2(toCamera.x, toCamera.z);
		player_.SetFaceOverride(true, aimYaw_);
		phase_ = Phase::TurnToCamera;
	} else {
		// つづき／せってい はまだ中身が無い：的を震わせて知らせ、歩ける状態へ戻る
		menu_.Refuse(selected);
		noticeTime_ = kNoticeTime;
		ReleasePlayer();
		phase_ = Phase::Menu;
	}
}

// 卵：構えている間は手に持ち、手を離れたら山なりに画面（カメラの目の前）へ飛んでくる
void TitleScene::UpdateEgg(float deltaTime){
	if ( !egg_ || !eggVisible_ ) { return; }
	Vector3 position {};
	float size = 0.29f; // 手のサイズに合う大きさ

	if ( phase_ == Phase::Throw ) {
		position = dino_.GetHandPosition();
		if ( dino_.ConsumeThrowRelease() ) {
			eggFrom_ = position;
			// カメラのすぐ前を狙う
			const Vector3 forward = VectorMath::Normalize(kCameraTarget - kCameraPosition);
			eggTo_ = kCameraPosition + forward * 1.3f;
			eggTime_ = 0.0f;
			phase_ = Phase::EggFlight;
			AudioManager::GetInstance()->PlayWave(TitleAssets::kThrowSe);
		}
	} else {
		eggTime_ += deltaTime;
		const float t = Easing::Clamp01(eggTime_ / kEggFlightTime);
		position = VectorMath::Lerp(eggFrom_, eggTo_, t);
		position.y += 1.0f * 4.0f * t * ( 1.0f - t );
		size = Easing::Lerp(size, 0.55f, t);
		if ( phase_ == Phase::EggFlight && t >= 1.0f ) {
			// 画面に当たった：閉じ始める
			AudioManager::GetInstance()->PlayWave(TitleAssets::kHitSe);
			irisTime_ = 0.0f;
			PostEffect::GetInstance()->SetEffectActive(PostEffectType::IrisWipe, true);
			phase_ = Phase::IrisOut;
		}
	}

	egg_->SetTranslation(position);
	egg_->SetRotation({ eggTime_ * 9.0f, 0.0f, eggTime_ * 5.0f }); // 回りながら飛ぶ
	egg_->SetScale({ size, size, size });
	egg_->Update();
}

// 画面の中央へ向けて丸く閉じ、閉じ切ったらゲームプレイへ
void TitleScene::UpdateIrisOut(float deltaTime){
	if ( phase_ != Phase::IrisOut ) { return; }
	irisTime_ += deltaTime;
	const float t = Easing::Clamp01(irisTime_ / kIrisCloseTime);
	PostEffect::GetInstance()->SetIrisParams(1.4f * ( 1.0f - Easing::EaseInQuad(t) ), 0.5f, 0.5f);
	if ( irisTime_ >= kIrisCloseTime + kIrisHoldTime ) {
		phase_ = Phase::Done;
		SceneManager::GetInstance()->ChangeScene(std::make_unique<GamePlayScene>());
	}
}

// 操作の案内は歩ける間だけ出す（ゆっくり点滅）。「じゅんびちゅう」は少しの間だけ出して消える
void TitleScene::UpdateTexts(float deltaTime){
	textTime_ += deltaTime;
	const float width  = ( float ) WindowProc::GetInstance()->GetClientWidth();
	const float height = ( float ) WindowProc::GetInstance()->GetClientHeight();

	const float wanted = ( phase_ == Phase::Menu ) ? 1.0f : 0.0f;
	guideAlpha_ += ( wanted - guideAlpha_ ) * ( std::min )( deltaTime * 8.0f, 1.0f );
	if ( guideText_ ) {
		const float blink = 0.8f + 0.2f * std::sin(textTime_ * 2.5f);
		const Vector4 color { kGuideColor.x, kGuideColor.y, kGuideColor.z, guideAlpha_ * blink };
		guideText_->SetColor(color);
		guideText_->SetOutlineColor(color);
		// 画面の一番下（ジオラマの手前のふち）に出す。恐竜やメニューに重ならない
		guideText_->SetPosition(( width - EstimateTextWidth(kGuideMessage, kGuideFontSize) ) * 0.5f, height * 0.93f);
	}

	noticeTime_ = ( std::max )( noticeTime_ - deltaTime, 0.0f );
	if ( noticeText_ ) {
		const float alpha = Easing::Clamp01(noticeTime_ / 0.3f); // 最後の0.3秒で消える
		const Vector4 color { kNoticeColor.x, kNoticeColor.y, kNoticeColor.z, alpha };
		noticeText_->SetColor(color);
		noticeText_->SetOutlineColor(color);
		// 看板の上の空に出す（背景がすっきりしていて読みやすい）
		noticeText_->SetPosition(( width - EstimateTextWidth(kNoticeMessage, kNoticeFontSize) ) * 0.5f, height * 0.12f);
	}
}

// =====================================================================
//  描画
// =====================================================================
void TitleScene::OnDrawOpaque(ID3D12GraphicsCommandList* /*commandList*/){
	ambience_.Draw();                             // 紙の波（草原と丘の間）
	roadMesh_.Draw();                             // レールの下の道（本編と同じ自動生成）
	if ( editRails_ ) { railField_.DrawMarkers(); } // レールの線（編集する時だけ）
	logo_.Draw();
	menu_.Draw();
	if ( eggVisible_ && egg_ ) { egg_->Draw(); }
	paperBits_.Draw();
	dino_.Draw();
}

// 文字（最終画像へ重ねる＝Game View にもフルスクリーンにも映る）
void TitleScene::OnDrawOverlay2D(ID3D12GraphicsCommandList* commandList){
	SDFManager* sdfManager = SDFManager::GetInstance();
	if ( guideText_ && guideAlpha_ > 0.01f )   { sdfManager->DrawTextItem(commandList, *guideText_, kFontAtlas); }
	if ( noticeText_ && noticeTime_ > 0.0f )   { sdfManager->DrawTextItem(commandList, *noticeText_, kFontAtlas); }
}

// 共有の「インスペクター (詳細設定)」に出す調整項目
void TitleScene::OnDrawInspector(){
#ifdef USE_IMGUI
	if ( !ImGui::CollapsingHeader("タイトル") ) { return; }

	if ( ImGui::Button("登場からやり直す") ) { StartIntro(); }
	ImGui::SameLine();
	if ( ImGui::Button("登場を飛ばす") ) { SkipIntro(); }

	ImGui::SeparatorText("レール");
	if ( ImGui::Checkbox("レールを編集する（線とノードを表示）", &editRails_) ) {
		EditorManager::GetInstance()->SetRailEditingEnabled(editRails_);
		railField_.SetShowMarkers(editRails_);
	}
	ImGui::TextDisabled("レールは title.json に保存されます。道は編集に合わせて作り直されます");
	ImGui::TextDisabled("登場の自動操縦は「奥右の角→手前右の角→スタート地点」を目指します");

	ImGui::SeparatorText("背景の動き");
	ImGui::Checkbox("風（草・花・木・雲がゆれる）", &ambience_.RefWindEnabled());
	ImGui::SliderFloat("風の強さ", &ambience_.RefWindStrength(), 0.0f, 2.5f);
	ImGui::Checkbox("紙の波", &ambience_.RefWavesEnabled());
	ImGui::Checkbox("エディタを出している間も動かす", &ambience_.RefMoveWhileEditing());
	ImGui::TextDisabled("置いた位置は変えず、表示だけを動かしています（保存しても揺れは書き込まれません）");

	ImGui::SeparatorText("紙ふぶき");
	bool ambient = paperBits_.IsAmbient();
	if ( ImGui::Checkbox("ただよう紙を出す", &ambient) ) { paperBits_.SetAmbient(ambient); }

	ImGui::SeparatorText("看板とメニュー");
	ImGui::DragFloat3("ロゴが刺さる位置", &logo_.RefLandPosition().x, 0.05f);
	ImGui::DragFloat3("的：つづき", &menu_.RefPosition(TitleMenu::Item_Continue).x, 0.05f);
	ImGui::DragFloat3("的：スタート", &menu_.RefPosition(TitleMenu::Item_Start).x, 0.05f);
	ImGui::DragFloat3("的：せってい", &menu_.RefPosition(TitleMenu::Item_Options).x, 0.05f);
	ImGui::TextDisabled("ここでの変更は保存されません（決まった値は TitleLayout.h へ書く）");

	ImGui::SeparatorText("ぼかし（ジオラマ風）");
	bool tiltChanged = false;
	tiltChanged |= ImGui::SliderFloat("強さ (px)", &tiltStrength_, 0.0f, 16.0f);
	tiltChanged |= ImGui::SliderFloat("くっきり見える帯の中心", &tiltCenterY_, 0.0f, 1.0f);
	tiltChanged |= ImGui::SliderFloat("帯の半幅", &tiltHalfWidth_, 0.02f, 0.6f);
	if ( tiltChanged ) {
		PostEffect::GetInstance()->SetTiltShiftParams(tiltStrength_, tiltCenterY_, tiltHalfWidth_);
	}
	if ( ImGui::Button("カメラを初期位置へ") ) { SetupCamera(); }
#endif
}
