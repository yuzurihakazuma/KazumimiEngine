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
	constexpr float kFastForward = 6.0f;     // 早送りの速さ
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
	constexpr float kCurtainHemRatio = 1080.0f / 1028.0f; // 裾の房（画像の下 52px）を画面の下の外へ出す
	constexpr float kValanceHeight = 230.0f; // valance.png の高さ（1920 幅のとき）
	constexpr const char* kWhitePath = "resources/block/white1x1.png";
	const char* kSounds[] = {
		"resources/kamishibai/se_hyoshigi.wav",
		"resources/kamishibai/se_door.wav",
		"resources/kamishibai/se_whoosh.wav",
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

	snapshotWidth_ = ( std::max )( 1u, ( uint32_t ) WindowProc::GetInstance()->GetClientWidth() );
	snapshotHeight_ = ( std::max )( 1u, ( uint32_t ) WindowProc::GetInstance()->GetClientHeight() );
	snapshot_ = std::make_unique<RenderTexture>();
	snapshot_->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), snapshotWidth_, snapshotHeight_);

	for ( const char* sound : kSounds ) { AudioManager::GetInstance()->LoadWave(sound); }
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
	snapshot_.reset();
	loaded_ = false;
}

bool KamishibaiTransition::LoadDefinition(const std::string& definitionName){
	std::ifstream file(kDefinitionDir + definitionName + ".json");
	if ( !file.is_open() ) { return false; }
	json definition;
	try { file >> definition; } catch ( ... ) { return false; }
	timeline_.Load(definition.value("parts", json::array()));
	toScene_ = definition.value("to", std::string("STAGESELECT"));
	definitionName_ = definitionName;
	// 定義に書いた音は先に読んでおく
	for ( const TransitionTimeline::Part& part : timeline_.Parts() ) {
		if ( part.type == "sound" ) { AudioManager::GetInstance()->LoadWave(part.params.value("sound", std::string())); }
	}
	return !timeline_.Parts().empty();
}

bool KamishibaiTransition::SaveDefinition() const{
	if ( definitionName_.empty() ) { return false; }
	json definition = json::object();
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
	speed_ = 1.0f;
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
	Input::GetInstance()->SetLocked(true); // 遷移中はゲーム側のキー操作を止める
	timeline_.Start();
	return true;
}

void KamishibaiTransition::ChangeSceneWithCurtain(const std::string& sceneName){
	KamishibaiTransition* transition = GetInstance();
	if ( transition->IsActive() ) { return; } // 遷移の最中：新しい切り替えは受け付けない
	if ( !transition->Play("curtain", sceneName) ) {
		SceneManager::GetInstance()->ChangeScene(sceneName); // 幕が使えない：そのまま切り替える
	}
}

void KamishibaiTransition::Update(float deltaTime){
	if ( !active_ ) { return; }

	// 早送り（押した瞬間から、残りを速く再生する。始まった直後の押しっぱなしは拾わない）
	Input* input = Input::GetInstance();
	if ( timeline_.GetTime() > 0.3f &&
	     ( input->TriggerkeyRaw(DIK_SPACE) || input->TriggerkeyRaw(DIK_RETURN) || input->TriggerkeyRaw(DIK_ESCAPE) ) ) {
		speed_ = kFastForward;
	}
	// 前のフレームで頼んだシーンの切り替えは、シーンマネージャが済ませている
	if ( sceneRequested_ && !loadedFired_ ) {
		loadedFired_ = true;
		timeline_.Fire("loaded");
	}

	const float step = ( std::min )( deltaTime, kMaxStep ) * speed_;
	timeline_.Update(step);
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
			//   開く時は左右へ引かれ、最後に上の飾り幕も上がっていく
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
		} else if ( part.type == "frameZoom" ) {
			// 枠をくぐる：枠が大きくなって画面の外へ出ていく
			frameZoom_ = 1.0f + ( params.value("scale", 2.6f) - 1.0f ) * Easing::EaseInCubic(p);
			frameAlpha_ = 1.0f - Easing::Clamp01(( p - 0.65f ) / 0.35f);
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
	if ( !sceneRequested_ ) { // 早送りなどで読み込みより先に終わった
		SceneManager::GetInstance()->ChangeScene(playTo_);
		sceneRequested_ = true;
	}
	active_ = false;
	speed_ = 1.0f;
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
	// 赤い幕：幕の幅を伸び縮みさせる（ひだが寄ったり広がったりして、布を引いているように見える）
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

	Transition(commandList, resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
	if ( active_ ) { ImGui::Text("再生中 %.2f 秒%s", timeline_.GetTime(), speed_ > 1.0f ? "（早送り）" : ""); }
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
