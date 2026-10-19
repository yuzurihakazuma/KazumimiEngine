#include "game/transition/KamishibaiTransition.h"

#include "engine/2d/Sprite.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/audio/AudioManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Input.h"
#include "engine/base/WindowProc.h"
#include "engine/graphics/RenderTexture.h"
#include "engine/graphics/SrvManager.h"
#include "engine/graphics/TextureManager.h"
#include "engine/scene/IScene.h"
#include "engine/scene/SceneManager.h"
#include "engine/utils/Easing.h"
#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>

using json = nlohmann::json;

namespace {
	constexpr float kPi = 3.14159265358979323846f;

	// 枠の画像（1920x1080）の窓の位置。tools/kamishibai/make_kamishibai_assets.py の FRAME_WINDOW と同じ
	constexpr float kImageWidth = 1920.0f;
	constexpr float kImageHeight = 1080.0f;
	constexpr float kWindowLeft = 250.0f, kWindowTop = 175.0f, kWindowRight = 1670.0f, kWindowBottom = 935.0f;

	constexpr float kBannerWidth = 1000.0f, kBannerHeight = 210.0f; // hajimari.png
	constexpr float kEggWidth = 150.0f, kEggHeight = 185.0f;        // egg.png を少し小さく
	constexpr float kDoorMaxAngle = 1.40f;  // 開き切った扉の角度(rad)。行き過ぎても 90° 手前で止める
	constexpr float kMaxStep = 1.0f / 30.0f; // シーンの切り替えで重いフレームがあっても、演出は飛ばさない
	constexpr float kSafetyTime = 20.0f;     // 何かで「おわり」に届かなくても、ここで終える(秒)

	constexpr const char* kDefinitionDir = "resources/transition/";
	constexpr const char* kFramePath = "resources/kamishibai/frame.png";
	constexpr const char* kDoorLeftPath = "resources/kamishibai/door_left.png";
	constexpr const char* kDoorRightPath = "resources/kamishibai/door_right.png";
	constexpr const char* kBannerPath = "resources/kamishibai/hajimari.png";
	constexpr const char* kEggPath = "resources/kamishibai/egg.png";
	constexpr const char* kCurtainPath = "resources/kamishibai/curtain.png";
	constexpr const char* kValancePath = "resources/kamishibai/valance.png";
	constexpr float kCurtainHalf = 0.53f;    // 閉じた時の幕1枚の幅（画面幅=1。真ん中で少し重ねる）
	constexpr float kCurtainHemRatio = 1080.0f / 1032.0f; // 裾のギザギザ（画像の下 48px）を画面の下の外へ出す（閉じた時にすき間を作らない）
	constexpr float kValanceHeight = 200.0f; // valance.png の高さ（1920 幅のとき）
	constexpr const char* kWhitePath = "resources/block/white1x1.png";
	// 読み込み中のカード（tools/kamishibai/make_kamishibai_assets.py の make_cards で作る）
	constexpr const char* kCardDir = "resources/kamishibai/";
	constexpr const char* kCardLayoutPath = "resources/kamishibai/card_layout.json";
	const char* kCardScenes[] = { "STAGESELECT", "GAMEPLAY", "TITLE" }; // 絵を用意している行き先
	constexpr float kCardTilt = 0.12f;       // 入る・引き抜く時のカードの傾き(rad)
	const char* kSounds[] = {
		"resources/kamishibai/se_hyoshigi.wav",
		"resources/kamishibai/se_door.wav",
		"resources/kamishibai/se_whoosh.wav",
		"resources/kamishibai/se_snip.wav",
	};
	// 遷移先に選べるシーン（SceneFactory の名前）
	const char* kSceneNames[] = { "STAGESELECT", "GAMEPLAY", "TITLE" };

	void Transition(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
	                D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after){
		D3D12_RESOURCE_BARRIER barrier {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = before;
		barrier.Transition.StateAfter = after;
		commandList->ResourceBarrier(1, &barrier);
	}

	void DrawSprite(Sprite* sprite, const Vector2& position, const Vector2& size, const Vector4& color, float rotation = 0.0f){
		if ( !sprite ) { return; }
		sprite->SetPosition(position);
		sprite->SetSize(size);
		sprite->SetColor(color);
		sprite->SetRotation(rotation);
		sprite->Update();
		sprite->Draw();
	}
}

KamishibaiTransition* KamishibaiTransition::GetInstance(){
	static KamishibaiTransition instance;
	return &instance;
}

void KamishibaiTransition::Load(){
	if ( loaded_ ) { return; }
	TextureManager* textures = TextureManager::GetInstance();
	frame_     = Sprite::Create(textures->Load(kFramePath).srvIndex, {}, Colors::WHITE, { 0.5f, 0.5f });
	doorLeft_  = Sprite::Create(textures->Load(kDoorLeftPath).srvIndex, {}, Colors::WHITE, { 0.0f, 0.5f });  // 蝶番＝左の辺
	doorRight_ = Sprite::Create(textures->Load(kDoorRightPath).srvIndex, {}, Colors::WHITE, { 1.0f, 0.5f }); // 蝶番＝右の辺
	banner_    = Sprite::Create(textures->Load(kBannerPath).srvIndex, {}, Colors::WHITE, { 0.5f, 0.5f });
	egg_       = Sprite::Create(textures->Load(kEggPath).srvIndex, {}, Colors::WHITE, { 0.5f, 0.5f });
	// 写し取った画面を貼る板。1x1 の画像で作ってから、写し取った絵に差し替える（UV が 0〜1 になる）
	picture_   = Sprite::Create(textures->Load(kWhitePath).srvIndex, {}, Colors::WHITE, { 0.5f, 0.5f });
	const uint32_t curtainTexture = textures->Load(kCurtainPath).srvIndex;
	curtainLeft_  = Sprite::Create(curtainTexture, {}, Colors::WHITE, { 0.0f, 0.0f }); // 左の辺から伸びる
	curtainRight_ = Sprite::Create(curtainTexture, {}, Colors::WHITE, { 1.0f, 0.0f }); // 右の辺から伸びる
	curtainRight_->SetIsFlipX(true);
	valance_      = Sprite::Create(textures->Load(kValancePath).srvIndex, {}, Colors::WHITE, { 0.0f, 0.0f });
	// 読み込み中のカード：行き先ごとの地と、上で動く部品を先に全部読む（実行中の読み込みはデバッグレイヤーが嫌う）
	auto loadCard = [&](const std::string& name){
		const std::string base = kCardDir + std::string("card_bg_") + name;
		return CardTextures { textures->Load(base + "_top.png").srvIndex, textures->Load(base + "_bottom.png").srvIndex };
	};
	defaultCardTextures_ = loadCard("default");
	for ( const char* scene : kCardScenes ) {
		std::string lower = scene;
		for ( char& c : lower ) { c = ( char ) std::tolower(( unsigned char ) c); }
		cardTextures_[scene] = loadCard(lower);
	}
	cardTop_ = Sprite::Create(defaultCardTextures_.top, {}, Colors::WHITE, { 0.5f, 0.5f });
	cardBottom_ = Sprite::Create(defaultCardTextures_.bottom, {}, Colors::WHITE, { 0.5f, 0.5f });
	cardSlit_ = Sprite::Create(textures->Load(std::string(kCardDir) + "card_cut.png").srvIndex, {}, Colors::WHITE, { 0.0f, 0.5f }); // 左の端から伸ばす
	slitWidth_ = cardSlit_->GetSize().x;
	LoadCardLayout();

	snapshotWidth_ = ( std::max )( 1u, ( uint32_t ) WindowProc::GetInstance()->GetClientWidth() );
	snapshotHeight_ = ( std::max )( 1u, ( uint32_t ) WindowProc::GetInstance()->GetClientHeight() );
	snapshot_ = std::make_unique<RenderTexture>();
	snapshot_->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), snapshotWidth_, snapshotHeight_);

	for ( const char* sound : kSounds ) { AudioManager::GetInstance()->LoadWave(sound); }
	stageIntro_.Load();
	LoadDefinition("stage_intro"); // ステージ開始カードの部品を先に作っておく（効果音も先に読む）
	LoadDefinition(editDefinitionName_);
	loaded_ = true;
}

void KamishibaiTransition::Finalize(){
	if ( active_ ) { Input::GetInstance()->SetLocked(false); }
	active_ = false;
	frame_.reset();
	doorLeft_.reset();
	doorRight_.reset();
	banner_.reset();
	egg_.reset();
	picture_.reset();
	curtainLeft_.reset();
	curtainRight_.reset();
	valance_.reset();
	cardTop_.reset();
	cardBottom_.reset();
	cardSlit_.reset();
	scissors_.reset();
	cardPieces_.clear();
	cardTextures_.clear();
	snapshot_.reset();
	stageIntro_.Finalize();
	loaded_ = false;
}

bool KamishibaiTransition::LoadDefinition(const std::string& definitionName){
	std::ifstream file(kDefinitionDir + definitionName + ".json");
	if ( !file.is_open() ) { return false; }
	json definition;
	try { file >> definition; } catch ( ... ) { return false; }
	timeline_.Load(definition.value("parts", json::array()));
	toScene_ = definition.value("to", std::string("STAGESELECT"));
	if ( definition.contains("stageIntro") ) { stageIntro_.Configure(definition["stageIntro"]); } // ステージ開始カードの置き場所
	definitionName_ = definitionName;
	// 定義に書いた音は先に読んでおく
	for ( const TransitionTimeline::Part& part : timeline_.Parts() ) {
		if ( part.type == "sound" ) { AudioManager::GetInstance()->LoadWave(part.params.value("sound", std::string())); }
	}
	return !timeline_.Parts().empty();
}

bool KamishibaiTransition::SaveDefinition() const{
	if ( definitionName_.empty() ) { return false; }
	// 部品の並び以外（ステージ開始カードの置き場所など）は、今のファイルの中身をそのまま残す
	json definition = json::object();
	{
		std::ifstream current(kDefinitionDir + definitionName_ + ".json");
		if ( current.is_open() ) {
			try { current >> definition; } catch ( ... ) { definition = json::object(); }
		}
		if ( !definition.is_object() ) { definition = json::object(); }
	}
	definition["to"] = toScene_;
	definition["parts"] = timeline_.Save();
	std::ofstream file(kDefinitionDir + definitionName_ + ".json");
	if ( !file.is_open() ) { return false; }
	file << definition.dump(2) << "\n";
	return true;
}

bool KamishibaiTransition::Play(const std::string& definitionName, const std::string& toScene){
	if ( !loaded_ || active_ ) { return false; }
	// エディタで直した中身を試せるように、同じ定義なら読み直さない
	if ( definitionName != definitionName_ || timeline_.Parts().empty() ) {
		if ( !LoadDefinition(definitionName) ) { return false; }
	}
	playTo_ = toScene.empty() ? toScene_ : toScene;
	active_ = true;
	sceneRequested_ = false;
	loadedFired_ = false;
	captureRequested_ = false; // 絵の引き抜きが始まる時に、その時の画面を写し取る
	snapshotValid_ = false;
	stageVisible_ = false;
	exitFired_ = false;
	pictureVisible_ = false;
	pictureOffset_ = pictureLift_ = pictureAngle_ = 0.0f;
	doorOpen_ = 0.0f;
	frameZoom_ = 1.0f;
	frameAlpha_ = 1.0f;
	bannerScale_ = bannerAlpha_ = 0.0f;
	eggVisible_ = false;
	eggFallTime_ = -1.0f;
	curtainClose_ = 0.0f;
	valanceDrop_ = 0.0f;
	cardVisible_ = false;
	cardOffset_ = cardAngle_ = 0.0f;
	cutProgress_ = snipTime_ = 0.0f;
	scissorsX_ = -1.0f;
	topLift_ = topTurn_ = bottomDrop_ = bottomTurn_ = 0.0f;
	// カードの絵は行き先で選ぶ（用意していない行き先は、名前の帯が無いカード）
	const auto cardTexture = cardTextures_.find(playTo_);
	const CardTextures& textures = ( cardTexture != cardTextures_.end() ) ? cardTexture->second : defaultCardTextures_;
	cardTop_->SetTexture(textures.top);
	cardBottom_->SetTexture(textures.bottom);
	stageIntro_.Reset();
	if ( !stageIntroContent_.empty() ) { stageIntro_.SetContent(stageIntroContent_); }
	Input::GetInstance()->SetLocked(true); // 遷移中はゲーム側のキー操作を止める
	timeline_.Start();
	return true;
}

void KamishibaiTransition::ChangeSceneWith(const std::string& definitionName, const std::string& sceneName){
	KamishibaiTransition* transition = GetInstance();
	if ( transition->IsActive() ) { return; } // 遷移の最中：新しい切り替えは受け付けない
	if ( !transition->Play(definitionName, sceneName) ) {
		SceneManager::GetInstance()->ChangeScene(sceneName); // 演出が使えない：そのまま切り替える
	}
}

void KamishibaiTransition::ChangeSceneWithCurtain(const std::string& sceneName){ ChangeSceneWith("curtain", sceneName); }
void KamishibaiTransition::ChangeSceneWithCard(const std::string& sceneName){ ChangeSceneWith("card", sceneName); }

void KamishibaiTransition::ChangeSceneWithStageIntro(const std::string& sceneName, const std::string& stage){
	if ( GetInstance()->IsActive() ) { return; }
	GetInstance()->stageIntroContent_ = stage;
	ChangeSceneWith("stage_intro", sceneName);
}

void KamishibaiTransition::Update(float deltaTime){
	if ( !active_ ) { return; }

	// 前のフレームで頼んだシーンの切り替えは、シーンマネージャが済ませている
	if ( sceneRequested_ && !loadedFired_ ) {
		loadedFired_ = true;
		timeline_.Fire("loaded");
	}

	const float step = ( std::min )( deltaTime, kMaxStep );
	timeline_.Update(step);
	stageIntro_.Update(timeline_.GetTime());
	ApplyParts(step);
	if ( active_ && timeline_.GetTime() > kSafetyTime ) { Finish(); }
}

void KamishibaiTransition::ApplyParts(float step){
	for ( TransitionTimeline::Part& part : timeline_.Parts() ) {
		if ( !part.started ) { continue; }
		const float p = part.progress;
		const json& params = part.params;

		if ( part.type == "sceneExit" ) {
			// シーンの退場：今のシーンに退場の動き（段ボールが倒れる）を頼み、終わったら出来事を知らせる。
			//   長さは待つ上限（シーンが終わったと言わなくても、ここで次へ進む）
			IScene* scene = SceneManager::GetInstance()->GetCurrentScene();
			if ( part.justStarted && scene ) { scene->BeginExitEffect(); }
			if ( !exitFired_ && ( part.finished || !scene || scene->IsExitEffectDone() ) ) {
				exitFired_ = true;
				timeline_.Fire(params.value("emit", std::string("exited")));
			}
		} else if ( part.type == "picturePull" ) {
			// 絵の引き抜き：始まった瞬間の画面を写し取って絵にし、紙芝居の枠と扉をその後ろに出す
			if ( part.justStarted ) {
				captureRequested_ = true;
				pictureVisible_ = true;
				stageVisible_ = true;
			}
			// 一瞬こらえて（EaseInBack で逆へ少し戻る）から、横へ吹き飛ぶ
			const float direction = params.value("direction", -1.0f);
			const float rotate = params.value("rotate", 14.0f) * kPi / 180.0f;
			pictureOffset_ = direction * 1.35f * Easing::EaseInBack(p);
			pictureAngle_ = direction * rotate * Easing::EaseInQuad(p);
			pictureLift_ = -0.06f * Easing::EaseInQuad(p);
			if ( part.finished ) { pictureVisible_ = false; }
		} else if ( part.type == "sound" ) {
			if ( part.justStarted ) {
				const std::string sound = params.value("sound", std::string());
				if ( !sound.empty() ) { AudioManager::GetInstance()->PlayWave(sound, false, params.value("volume", 1.0f)); }
			}
		} else if ( part.type == "loadScene" ) {
			// 扉が閉じて画面が隠れている間に、次のシーンへ切り替える
			if ( part.justStarted && !sceneRequested_ ) {
				SceneManager::GetInstance()->ChangeScene(playTo_);
				sceneRequested_ = true;
			}
		} else if ( part.type == "banner" ) {
			// 「はじまり、はじまり」：ポンッと出て、最後に消える
			if ( part.finished ) { bannerAlpha_ = 0.0f; continue; }
			const float pop = params.value("pop", 0.3f);
			const float fade = params.value("fade", 0.3f);
			const float t = part.elapsed;
			bannerScale_ = Easing::EaseOutBack(t / ( std::max )( pop, 0.01f ));
			const float fadeOut = Easing::Clamp01(( t - ( part.duration - fade ) ) / ( std::max )( fade, 0.01f ));
			bannerAlpha_ = Easing::Clamp01(t / 0.08f) * ( 1.0f - fadeOut );
		} else if ( part.type == "eggThrow" ) {
			// 扉の真ん中へ、下から山なりに卵が飛んでくる。当たったら出来事を知らせる
			if ( !part.finished ) {
				eggVisible_ = true;
				eggAlpha_ = 1.0f;
				const float fromX = 0.5f + params.value("fromX", -0.12f);
				eggX_ = Easing::Lerp(fromX, 0.5f, p);
				eggY_ = Easing::Lerp(1.15f, 0.5f, p) - params.value("arc", 0.3f) * 4.0f * p * ( 1.0f - p );
				eggAngle_ = p * 3.0f * kPi;
			}
			if ( part.justFinished ) {
				timeline_.Fire(params.value("emit", std::string("doorHit")));
				eggFallTime_ = 0.0f;
				eggX_ = 0.5f;
				eggY_ = 0.5f;
			}
		} else if ( part.type == "doors" ) {
			// 扉：開く時はバンッと勢いよく（行き過ぎて戻る）、閉じる時は加速して閉じる
			const bool open = params.value("open", true);
			doorOpen_ = open ? Easing::EaseOutBack(p) : 1.0f - Easing::EaseInCubic(p);
		} else if ( part.type == "curtain" ) {
			// 赤い幕：閉じる時は左右からすべって来て、真ん中で少し揺れて合わさる。
			//   開く時は左右へ引かれ、最後に上の幕板も上がっていく
			const bool open = params.value("open", false);
			if ( open ) {
				curtainClose_ = 1.0f - Easing::EaseInOutCubic(p);
				valanceDrop_ = 1.0f - Easing::EaseInCubic(Easing::Clamp01(( p - 0.55f ) / 0.45f));
			} else {
				curtainClose_ = Easing::EaseOutBack(p);
				valanceDrop_ = Easing::EaseOutCubic(Easing::Clamp01(p * 2.0f));
			}
			// 閉じ切った（開き切った）ことを知らせる。幕で隠れてからシーンを切り替えるのに使う
			if ( part.justFinished && params.contains("emit") ) { timeline_.Fire(params.value("emit", std::string())); }
		} else if ( part.type == "card" ) {
			// 読み込み中のカード：紙芝居の絵を差し込むように右から入って少し行き過ぎて止まり、
			//   読み終えたら左へ引き抜かれる（一瞬こらえてから一気に抜ける）
			const bool in = params.value("in", true);
			cardVisible_ = true;
			if ( in ) {
				cardOffset_ = 1.15f * ( 1.0f - Easing::EaseOutBack(p) );
				cardAngle_ = ( 1.0f - Easing::EaseOutCubic(p) ) * kCardTilt;
			} else {
				cardOffset_ = -1.25f * Easing::EaseInBack(p);
				cardAngle_ = -Easing::EaseInQuad(p) * kCardTilt;
				if ( part.finished ) { cardVisible_ = false; }
			}
			// 入り切った（画面を覆った）ことを知らせる。隠れてからシーンを切り替えるのに使う
			if ( part.justFinished && params.contains("emit") ) { timeline_.Fire(params.value("emit", std::string())); }
		} else if ( part.type == "cardCut" ) {
			// 段ボールの切れ目：はさみが下の折り目の線を左から右へ切り進み、切り終えたら
			//   上の部分は上へ、下の細い部分は下へ分かれて飛んでいく（その間から次のシーンが始まる）
			const float cutTime = ( std::max )( params.value("cutTime", 0.4f), 0.01f );
			const float flyTime = ( std::max )( part.duration - cutTime, 0.01f );
			const float elapsed = part.elapsed;
			cardVisible_ = !part.finished;
			snipTime_ = elapsed;
			cutProgress_ = Easing::Clamp01(elapsed / cutTime);
			// はさみは切り終えても止まらず、そのまま右へ抜けていく
			const float travel = elapsed / cutTime;
			scissorsX_ = ( travel < 1.8f ) ? travel : -1.0f;
			const float fly = Easing::Clamp01(( elapsed - cutTime ) / flyTime);
			topLift_ = params.value("lift", 1.25f) * Easing::EaseInCubic(fly);
			topTurn_ = -0.10f * Easing::EaseInQuad(fly);
			bottomDrop_ = params.value("drop", 0.5f) * Easing::EaseInQuad(fly);
			bottomTurn_ = 0.12f * Easing::EaseInQuad(fly);
		} else if ( part.type == "frameZoom" ) {
			// 枠をくぐる：枠が大きくなって画面の外へ出ていく
			frameZoom_ = 1.0f + ( params.value("scale", 2.6f) - 1.0f ) * Easing::EaseInCubic(p);
			frameAlpha_ = 1.0f - Easing::Clamp01(( p - 0.65f ) / 0.35f);
		} else if ( part.type == "introElement" ) {
			// ステージ開始カードの部品の動き（台紙・星・名前の帯・ステージ名・キャラ）
			stageIntro_.Animate(params.value("id", std::string()), params.value("motion", std::string()), p, part.elapsed);
		} else if ( part.type == "introFall" ) {
			stageIntro_.SetFall(p); // 台紙ごと後ろへ倒れる（その奥に次のシーン）
		} else if ( part.type == "introLift" ) {
			stageIntro_.SetLift(p); // 台紙ごと画面の上へ引き抜く（その下に次のシーン）
		} else if ( part.type == "letterbox" ) {
			stageIntro_.SetBars(params.value("in", true) ? p : 1.0f - p);
		} else if ( part.type == "sceneCue" ) {
			// シーンへ合図を送る（プレイヤーの振り向き・アクション・カメラの戻りなど）
			if ( part.justStarted ) {
				if ( IScene* scene = SceneManager::GetInstance()->GetCurrentScene() ) { scene->OnTransitionCue(params.value("cue", std::string())); }
			}
		} else if ( part.type == "waitAll" ) {
			// 「読み込み完了」と「最短表示時間の経過」がそろったら、次へ進む合図を出す
			bool ready = timeline_.GetTime() >= params.value("minTime", 0.0f);
			for ( const json& event : params.value("events", json::array()) ) {
				if ( event.is_string() && !timeline_.HasFired(event.get<std::string>()) ) { ready = false; }
			}
			if ( ready ) { timeline_.Fire(params.value("emit", std::string("ready"))); }
		} else if ( part.type == "end" ) {
			if ( part.justStarted ) { Finish(); return; }
		}
	}

	// 扉に当たった卵は、はね返って下へ落ちていく
	if ( eggFallTime_ >= 0.0f ) {
		eggFallTime_ += step;
		const float t = eggFallTime_;
		eggX_ = 0.5f + 0.22f * t;
		eggY_ = 0.5f - 0.45f * t + 2.2f * t * t;
		eggAngle_ += step * 9.0f;
		eggAlpha_ = 1.0f - Easing::Clamp01(( t - 0.35f ) / 0.3f);
		if ( t > 0.7f ) { eggVisible_ = false; eggFallTime_ = -1.0f; }
	}
}

void KamishibaiTransition::Finish(){
	if ( !sceneRequested_ ) { // 定義に読み込みの部品が無い・読み込みより先に「おわり」が来た
		SceneManager::GetInstance()->ChangeScene(playTo_);
		sceneRequested_ = true;
	}
	active_ = false;
	pictureVisible_ = false;
	eggVisible_ = false;
	Input::GetInstance()->SetLocked(false);
}

// 今のシーンの最終画像を、そのまま写し取る（絵の引き抜きの「絵」になる）
void KamishibaiTransition::Capture(ID3D12GraphicsCommandList* commandList, RenderTexture* target){
	ID3D12Resource* source = target->GetResource().Get();
	const D3D12_RESOURCE_DESC desc = source->GetDesc();
	const uint32_t width = ( uint32_t ) desc.Width;
	const uint32_t height = desc.Height;
	if ( width != snapshotWidth_ || height != snapshotHeight_ ) { // 画面の大きさが変わっていたら作り直す
		snapshot_->Resize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), width, height);
		snapshotWidth_ = width;
		snapshotHeight_ = height;
	}
	ID3D12Resource* destination = snapshot_->GetResource().Get();
	Transition(commandList, source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
	Transition(commandList, destination, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->CopyResource(destination, source);
	Transition(commandList, destination, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Transition(commandList, source, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	snapshotValid_ = true;
}

void KamishibaiTransition::DrawInto(ID3D12GraphicsCommandList* commandList, RenderTexture* target){
	if ( !active_ || !loaded_ || !target ) { return; }
	if ( captureRequested_ ) {
		Capture(commandList, target);
		captureRequested_ = false;
	}

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	ID3D12Resource* resource = target->GetResource().Get();
	const D3D12_RESOURCE_DESC desc = resource->GetDesc();
	const float width = ( float ) desc.Width;
	const float height = ( float ) desc.Height;

	// 最終画像の上に重ねて描く（クリアしない）
	Transition(commandList, resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = target->GetRtvHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon->GetDsvHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, width, height, 0.0f, 1.0f };
	const D3D12_RECT scissor = { 0, 0, ( LONG ) width, ( LONG ) height };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
	DrawLayers(commandList, width, height);

	Transition(commandList, resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

// 遷移の絵を、今の描き先へ描く（カード・幕・枠と扉・帯・卵・写し取った画面）
void KamishibaiTransition::DrawLayers(ID3D12GraphicsCommandList* commandList, float width, float height){
	SpriteCommon::GetInstance()->PreDraw(commandList);

	// 枠の画像の座標 → 画面の座標（枠をくぐる時は画面の中心から大きくする）
	const float scaleX = width / kImageWidth;
	const float scaleY = height / kImageHeight;
	const Vector2 center { width * 0.5f, height * 0.5f };
	auto onFrame = [&](float x, float y){
		return Vector2 { center.x + ( x * scaleX - center.x ) * frameZoom_, center.y + ( y * scaleY - center.y ) * frameZoom_ };
	};

	if ( stageVisible_ && frameAlpha_ > 0.001f ) {
		// 観音開きの扉：蝶番の辺を軸に手前へ開く（横幅を cos で縮め、開くほど暗くする）
		const float angle = std::clamp(doorOpen_, 0.0f, 1.12f) * kDoorMaxAngle;
		const float doorWidth = ( kWindowRight - kWindowLeft ) * 0.5f * scaleX * frameZoom_ * ( std::max )( std::cos(angle), 0.02f );
		const float doorHeight = ( kWindowBottom - kWindowTop ) * scaleY * frameZoom_;
		const float shade = 1.0f - 0.5f * std::sin(angle);
		const Vector4 doorColor { shade, shade, shade, frameAlpha_ };
		const float middleY = ( kWindowTop + kWindowBottom ) * 0.5f;
		DrawSprite(doorLeft_.get(), onFrame(kWindowLeft, middleY), { doorWidth, doorHeight }, doorColor);
		DrawSprite(doorRight_.get(), onFrame(kWindowRight, middleY), { doorWidth, doorHeight }, doorColor);
		DrawSprite(frame_.get(), center, { width * frameZoom_, height * frameZoom_ }, { 1.0f, 1.0f, 1.0f, frameAlpha_ });
	}
	// 赤い幕（色画用紙の蛇腹）：幅を伸び縮みさせる（蛇腹が伸びて閉じ、縮んで開くように見える）
	if ( curtainClose_ > 0.001f ) {
		const float curtainWidth = width * kCurtainHalf * curtainClose_;
		const float curtainHeight = height * kCurtainHemRatio;
		DrawSprite(curtainLeft_.get(), { 0.0f, 0.0f }, { curtainWidth, curtainHeight }, Colors::WHITE);
		DrawSprite(curtainRight_.get(), { width, 0.0f }, { curtainWidth, curtainHeight }, Colors::WHITE);
	}
	if ( valanceDrop_ > 0.001f ) {
		const float valanceHeight = kValanceHeight * scaleX;
		DrawSprite(valance_.get(), { 0.0f, -valanceHeight * ( 1.0f - valanceDrop_ ) }, { width, valanceHeight }, Colors::WHITE);
	}
	if ( cardVisible_ ) { DrawCard(width, height, scaleX, scaleY); }
	stageIntro_.Draw(width, height); // ステージ開始カードと上下の帯（使っていなければ何も描かない）
	if ( bannerAlpha_ > 0.001f ) {
		DrawSprite(banner_.get(), { center.x, height * 0.47f },
		           { kBannerWidth * scaleX * bannerScale_, kBannerHeight * scaleX * bannerScale_ }, { 1.0f, 1.0f, 1.0f, bannerAlpha_ });
	}
	if ( eggVisible_ ) {
		DrawSprite(egg_.get(), { eggX_ * width, eggY_ * height }, { kEggWidth * scaleX, kEggHeight * scaleX },
		           { 1.0f, 1.0f, 1.0f, eggAlpha_ }, eggAngle_);
	}
	if ( pictureVisible_ && snapshotValid_ ) {
		// 写し取った画面（絵）。吹き飛ぶまでは画面にぴったり重なっているので、切り替わりの継ぎ目は見えない
		picture_->SetTextureHandle(SrvManager::GetInstance()->GetGPUDescriptorHandle(snapshot_->GetSrvIndex()));
		DrawSprite(picture_.get(), { center.x + pictureOffset_ * width, center.y + pictureLift_ * height },
		           { width, height }, Colors::WHITE, pictureAngle_);
	}
}

// 画面へ直接、シーンの UI（HUD など）の上に重ねる。最終画像へ重ねただけだと、
//   その後に描かれる UI が遷移の上に出てしまう（カードの上にコインの数が見えるなど）
void KamishibaiTransition::DrawOverlay(ID3D12GraphicsCommandList* commandList){
	if ( !active_ || !loaded_ ) { return; }
	DrawLayers(commandList, ( float ) WindowProc::GetInstance()->GetClientWidth(), ( float ) WindowProc::GetInstance()->GetClientHeight());
}

// カードの上の部品の置き場所と動き方を読む（tools/kamishibai/make_kamishibai_assets.py が書き出す）
void KamishibaiTransition::LoadCardLayout(){
	cardPieces_.clear();
	std::ifstream file(kCardLayoutPath);
	if ( !file.is_open() ) { return; }
	json layout;
	try { file >> layout; } catch ( ... ) { return; }
	cardMargin_ = layout.value("margin", 0.0f);
	const json cut = layout.value("cut", json::object());
	cutY_ = cut.value("y", kImageHeight);
	cardTopHeight_ = cut.value("topHeight", kImageHeight);
	cardBottomTop_ = cut.value("bottomTop", kImageHeight);
	cardBottomHeight_ = cut.value("bottomHeight", 0.0f);
	slitTop_ = cut.value("slitTop", cutY_);
	slitHeight_ = cut.value("slitHeight", 0.0f);
	const json scissors = layout.value("scissors", json::object());
	if ( scissors.contains("file") ) {
		scissorsWidth_ = scissors.value("width", 1.0f);
		scissorsHeight_ = scissors.value("height", 1.0f);
		const Vector2 tip { scissors.value("tipX", 0.0f) / scissorsWidth_, scissors.value("tipY", 0.0f) / scissorsHeight_ };
		scissors_ = Sprite::Create(TextureManager::GetInstance()->Load(kCardDir + scissors.value("file", std::string())).srvIndex, {}, Colors::WHITE, tip);
	}
	const std::pair<const char*, CardMotion> motionNames[] = {
		{ "walk", CardMotion::Walk }, { "rock", CardMotion::Rock }, { "twinkle", CardMotion::Twinkle },
		{ "drift", CardMotion::Drift }, { "sway", CardMotion::Sway }, { "hop", CardMotion::Hop }, { "dot", CardMotion::Dot },
		{ "stamp", CardMotion::Stamp },
	};
	TextureManager* textures = TextureManager::GetInstance();
	for ( const json& entry : layout.value("pieces", json::array()) ) {
		CardPiece piece;
		piece.sprite = Sprite::Create(textures->Load(kCardDir + entry.value("file", std::string())).srvIndex, {}, Colors::WHITE, { 0.5f, 0.5f });
		piece.width = piece.sprite->GetSize().x;
		piece.height = piece.sprite->GetSize().y;
		piece.x = entry.value("x", 0.0f);
		piece.y = entry.value("y", 0.0f);
		piece.angle = -entry.value("angle", 0.0f) * kPi / 180.0f; // 画像を作る側は左回りが正、画面のスプライトは右回りが正
		piece.phase = entry.value("phase", 0.0f);
		piece.index = entry.value("index", 0);
		piece.bottom = ( entry.value("part", std::string()) == "bottom" );
		const std::string motion = entry.value("motion", std::string());
		for ( const auto& name : motionNames ) { if ( motion == name.first ) { piece.motion = name.second; } }
		cardPieces_.push_back(std::move(piece));
	}
}

// 読み込み中のカード：地（左右は段ボールの切れ目）と、その上で動く部品。部品はカードと一緒にすべって傾く。
//   下の折り目の線で上と下に分かれている（切る前はぴったり重なって1枚に見える）
void KamishibaiTransition::DrawCard(float width, float height, float scaleX, float scaleY){
	const Vector2 cardCenter { width * ( 0.5f + cardOffset_ ), height * 0.5f };
	const float c = std::cos(cardAngle_), s = std::sin(cardAngle_);
	// 上・下の部分の中心（カードの中心からの、画像の px）
	const float cardWidth = kImageWidth + cardMargin_ * 2.0f;
	const float topCenterY = cardTopHeight_ * 0.5f - kImageHeight * 0.5f;
	const float bottomCenterY = cardBottomTop_ + cardBottomHeight_ * 0.5f - kImageHeight * 0.5f;
	// カードの中の点（中心からの画像の px）→ 画面の位置。分かれた後は、その部分の中心のまわりに回って上・下へずれる
	auto place = [&](float x, float y, bool bottom){
		const float pivotY = bottom ? bottomCenterY : topCenterY;
		const float turn = bottom ? bottomTurn_ : topTurn_;
		const float ry = y - pivotY;
		const float px = x * std::cos(turn) - ry * std::sin(turn);
		const float py = x * std::sin(turn) + ry * std::cos(turn) + pivotY;
		const float lx = px * scaleX, ly = py * scaleY;
		const float shift = ( bottom ? bottomDrop_ : -topLift_ ) * height;
		return Vector2 { cardCenter.x + lx * c - ly * s, cardCenter.y + lx * s + ly * c + shift };
	};
	DrawSprite(cardBottom_.get(), place(0.0f, bottomCenterY, true), { cardWidth * scaleX, cardBottomHeight_ * scaleY },
	           Colors::WHITE, cardAngle_ + bottomTurn_);
	DrawSprite(cardTop_.get(), place(0.0f, topCenterY, false), { cardWidth * scaleX, cardTopHeight_ * scaleY },
	           Colors::WHITE, cardAngle_ + topTurn_);

	// 切れ目：はさみが進んだところまで開いた線を見せる（分かれ始めたら、その間は次のシーン）。
	//   部品より先に描く（切れ目にかかったシールは、上の部分に貼ってあって切れ目をまたいで見える）
	const bool splitting = ( topLift_ > 0.0f || bottomDrop_ > 0.0f );
	if ( cutProgress_ > 0.0f && !splitting && cardSlit_ ) {
		const float slitWidth = ( std::min )( cardMargin_ + cutProgress_ * kImageWidth, slitWidth_ );
		cardSlit_->SetTextureRect(0.0f, 0.0f, slitWidth, slitHeight_);
		DrawSprite(cardSlit_.get(), place(-cardWidth * 0.5f, slitTop_ + slitHeight_ * 0.5f - kImageHeight * 0.5f, false),
		           { slitWidth * scaleX, slitHeight_ * scaleY }, Colors::WHITE, cardAngle_);
	}

	const float time = timeline_.GetTime();
	for ( const CardPiece& piece : cardPieces_ ) {
		// 部品ごとの動き（画像の px で、ずれ・回り・大きさ）
		float moveX = 0.0f, moveY = 0.0f, turn = 0.0f, size = 1.0f;
		const float t = time + piece.phase;
		switch ( piece.motion ) {
		case CardMotion::Walk:    moveY = -std::abs(std::sin(t * 7.0f)) * 16.0f; turn = std::sin(t * 7.0f) * 0.05f; break; // とことこ歩く
		case CardMotion::Rock:    turn = std::sin(t * 2.6f) * 0.2f; break;                                               // ゆらゆら
		case CardMotion::Twinkle: turn = std::sin(t * 1.8f) * 0.18f; size = 1.0f + 0.07f * std::sin(t * 3.2f); break;   // きらきら
		case CardMotion::Drift:   moveX = std::sin(t * 0.9f) * 26.0f; moveY = std::sin(t * 1.3f) * 6.0f; break;          // ふわふわ流れる
		case CardMotion::Sway:    turn = std::sin(t * 2.2f) * 0.22f; break;                                              // 風にゆれる
		case CardMotion::Hop:     moveY = -std::abs(std::sin(t * 5.0f)) * 10.0f; turn = std::sin(t * 5.0f) * 0.2f; break; // ぴょこぴょこ
		case CardMotion::Stamp:   moveY = -std::abs(std::sin(t * 9.0f)) * 7.0f; turn = std::sin(t * 4.5f) * 0.04f; break;  // 足踏み
		case CardMotion::Dot:     if ( ( int ) ( time * 3.0f ) % 4 <= piece.index ) { continue; } break;                 // 「…」が1つずつ増える
		default: break;
		}
		DrawSprite(piece.sprite.get(), place(piece.x + moveX - kImageWidth * 0.5f, piece.y + moveY - kImageHeight * 0.5f, piece.bottom),
		           { piece.width * scaleX * size, piece.height * scaleX * size }, Colors::WHITE,
		           cardAngle_ + piece.angle + turn + ( piece.bottom ? bottomTurn_ : topTurn_ ));
	}

	// はさみ：刃先が切れ目の先。チョキチョキ（刃の開け閉め）に合わせて小さく揺れる。
	//   カードの部品は切れ目のまわりを空けて置いてあるので、はさみがシールの上を通らない（make_kamishibai_assets.py の CARD_CLEAR_Y）
	if ( scissorsX_ >= 0.0f && scissors_ ) {
		const float snip = std::sin(snipTime_ * 40.0f);
		const Vector2 tip = place(scissorsX_ * kImageWidth - kImageWidth * 0.5f, cutY_ - kImageHeight * 0.5f + snip * 3.0f, false);
		DrawSprite(scissors_.get(), tip, { scissorsWidth_ * scaleX, scissorsHeight_ * scaleX }, Colors::WHITE,
		           cardAngle_ + snip * 0.12f);
	}
}

// インスペクターの「紙芝居遷移」：部品の きっかけ・遅れ・長さ・設定 をその場で直して保存する
void KamishibaiTransition::DrawDebugUI(){
#ifdef USE_IMGUI
	if ( !ImGui::CollapsingHeader("紙芝居遷移 (Kamishibai)") ) { return; }
	ImGui::PushID("Kamishibai");

	ImGui::SetNextItemWidth(160.0f);
	ImGui::InputText("定義の名前", editDefinitionName_, sizeof(editDefinitionName_));
	ImGui::SameLine();
	if ( ImGui::Button("読み込む") ) { LoadDefinition(editDefinitionName_); }
	ImGui::SameLine();
	if ( ImGui::Button("保存") ) {
		definitionName_ = editDefinitionName_;
		SaveDefinition();
	}
	ImGui::TextDisabled("resources/transition/%s.json", definitionName_.c_str());

	// 遷移先
	int sceneIndex = 0;
	for ( int i = 0; i < IM_ARRAYSIZE(kSceneNames); ++i ) { if ( toScene_ == kSceneNames[i] ) { sceneIndex = i; } }
	ImGui::SetNextItemWidth(160.0f);
	if ( ImGui::Combo("行き先のシーン", &sceneIndex, kSceneNames, IM_ARRAYSIZE(kSceneNames)) ) { toScene_ = kSceneNames[sceneIndex]; }

	ImGui::BeginDisabled(active_);
	if ( ImGui::Button("今の画面から試す") ) { Play(definitionName_); }
	ImGui::EndDisabled();
	ImGui::SameLine();
	if ( active_ ) { ImGui::Text("再生中 %.2f 秒", timeline_.GetTime()); }
	else           { ImGui::TextDisabled("止まっている"); }

	ImGui::Separator();
	ImGui::TextDisabled("きっかけ：start / loaded（次のシーンを読み終えた）/ 卵の emit（doorHit）");
	int index = 0;
	for ( TransitionTimeline::Part& part : timeline_.Parts() ) {
		ImGui::PushID(index++);
		const char* state = !part.started ? "　" : ( part.finished ? "済" : "▶" );
		if ( ImGui::TreeNode("part", "%s %s  [%s]  %s +%.2f秒 / %.2f秒", state, part.name.c_str(), part.type.c_str(),
		                     part.event.c_str(), part.delay, part.duration) ) {
			char eventBuffer[64] {};
			strncpy_s(eventBuffer, part.event.c_str(), _TRUNCATE);
			ImGui::SetNextItemWidth(140.0f);
			if ( ImGui::InputText("きっかけ", eventBuffer, sizeof(eventBuffer)) ) { part.event = eventBuffer; }
			ImGui::SetNextItemWidth(140.0f);
			ImGui::DragFloat("遅れ(秒)", &part.delay, 0.01f, 0.0f, 10.0f, "%.2f");
			ImGui::SetNextItemWidth(140.0f);
			ImGui::DragFloat("長さ(秒)", &part.duration, 0.01f, 0.0f, 10.0f, "%.2f");
			// 部品ごとの設定（数・真偽・文字をそのまま直せる）
			for ( auto item = part.params.begin(); item != part.params.end(); ++item ) {
				ImGui::SetNextItemWidth(140.0f);
				if ( item->is_number() ) {
					float value = item->get<float>();
					if ( ImGui::DragFloat(item.key().c_str(), &value, 0.01f) ) { *item = value; }
				} else if ( item->is_boolean() ) {
					bool value = item->get<bool>();
					if ( ImGui::Checkbox(item.key().c_str(), &value) ) { *item = value; }
				} else if ( item->is_string() ) {
					char buffer[160] {};
					strncpy_s(buffer, item->get<std::string>().c_str(), _TRUNCATE);
					ImGui::SetNextItemWidth(280.0f);
					if ( ImGui::InputText(item.key().c_str(), buffer, sizeof(buffer)) ) { *item = std::string(buffer); }
				}
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	ImGui::PopID();
#endif
}
