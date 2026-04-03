#include "GamePlayScene.h"
// --- ゲーム固有のファイル ---
#include "TitleScene.h"

// --- エンジン側のファイル ---
#include "Engine/Math/Matrix4x4.h"
#include "Engine/Utils/ImGuiManager.h"
#include "Engine/Utils/Color.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/3D/Model/ModelManager.h"
#include "Engine/Particle/ParticleManager.h"
#include "Engine/Graphics/TextureManager.h"
#include "Engine/Graphics/PipelineManager.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Camera/DebugCamera.h"
#include "Engine/2D/Sprite.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "Engine/Base/Input.h"
#include "Engine/2D/SpriteCommon.h"
#include "Engine/3D/Obj/Obj3dCommon.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Base/WindowProc.h"
#include "engine/math/VectorMath.h"
#include "engine/collision/Collision.h"
#include "engine/graphics/RenderTexture.h"
#include "engine/graphics/SrvManager.h"
#include "engine/postEffect/PostEffect.h"
#include "game/player/Player.h"
#include"engine/utils/Level/LevelEditor.h"
#include "engine/utils/TextManager.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/Boss.h"
#include "game/enemy/EnemyManager.h"
#include "game/enemy/BossManager.h"
#include "game/card/CardUseSystem.h"
#include "game/map/Minimap.h"
#include "game/map/MapManager.h"
#include "Bloom.h"
#include "engine/3d/model/Model.h"
#include "engine/utils/EditorManager.h"
using namespace VectorMath;
using namespace MatrixMath;

// 初期化
void GamePlayScene::Initialize() {
	DirectXCommon *dxCommon = DirectXCommon::GetInstance();
	WindowProc *windowProc = WindowProc::GetInstance();

	// コマンドリスト取得
	auto commandList = dxCommon->GetCommandList();

	// BGMロード (シングルトン)
	AudioManager::GetInstance()->LoadWave(bgmFile_);
	// モデル読み込み (シングルトン)
	ModelManager::GetInstance()->LoadModel("fence", "resources", "fence.obj");

	ModelManager::GetInstance()->LoadModel("grass", "resources", "terrain.obj");
	ModelManager::GetInstance()->LoadModel("block", "resources/block", "block.obj");

	// プレイヤーモデル読み込み
	ModelManager::GetInstance()->LoadModel("player", "resources/player", "player.obj");

	// 敵モデル読み込み
	ModelManager::GetInstance()->LoadModel("enemy", "resources/enemy", "enemy.obj");

	// ボスモデル読み込み
	ModelManager::GetInstance()->LoadModel("boss", "resources/boss", "boss.obj");

	// 球モデル作成 (シングルトン)
	ModelManager::GetInstance()->CreateSphereModel("sphere", 16);
	// パーティクルグループ作成 (シングルトン)
	//ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");

	// テクスチャ読み込み
	textures_["uvChecker"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);
	textures_["monsterBall"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/monsterBall.png", commandList);
	textures_["fence"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/fence.png", commandList);
	textures_["circle"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/circle.png", commandList);
	textures_["noise0"] = { TextureManager::GetInstance()->LoadTextureAndCreateSRV("Resources/noise0.png", commandList) };
	textures_["noise1"] = { TextureManager::GetInstance()->LoadTextureAndCreateSRV("Resources/noise1.png", commandList) };

	// モデル読み込み (シングルトン)
	// アニメーション
	ModelManager::GetInstance()->LoadModel("animatedCube", "resources/AnimatedCube", "AnimatedCube.gltf");
	testAnimation_ = LoadAnimationFromFile("resources/AnimatedCube", "AnimatedCube.gltf");

	// カメラ生成
	camera_ = std::make_unique<Camera>(windowProc->GetClientWidth(), windowProc->GetClientHeight(), dxCommon);
	camera_->SetTranslation({ 0.0f, 2.0f, -15.0f });

	//UI専用カメラの初期化
	uiCamera_ = std::make_unique<Camera>(1280, 720, dxCommon);

	// デバッグカメラ生成
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();

	// 敵
	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize();

	playerObj_ = Obj3d::Create("player");
	if (playerObj_) {
		playerObj_->SetCamera(camera_.get());
		playerObj_->SetTranslation(playerPos_);
		playerObj_->SetScale(playerScale_);
	}

	player_ = std::make_unique<Player>();
	player_->Initialize();
	player_->SetPosition(playerPos_);
	player_->SetScale(playerScale_);

	bossManager_ = std::make_unique<BossManager>();
	bossManager_->Initialize(camera_.get());

	// カード使用システム初期化
	playerCardSystem_ = std::make_unique<CardUseSystem>();
	playerCardSystem_->Initialize(camera_.get());

	// ファイル名を指定するだけで、読み込み・生成・配置
	// 引数: (ファイルパス, 座標)
	sprite_ = Sprite::Create(textures_["uvChecker"].srvIndex, spritePos_);
	// プレイヤーオブジェクト生成
	testObj_ = Obj3d::Create("block");
	if (testObj_) {

		testObj_->SetCamera(camera_.get());
		testObj_->SetTranslation({ 0.0f, 0.0f, 5.0f });

		// ノイズ画像と初期の閾値(0.0)をセット
		testObj_->SetNoiseTexture(textures_["noise0"].srvIndex);
		testObj_->SetDissolveThreshold(0.0f);

		//testObj_->PlayAnimation(&testAnimation_);

		Bloom::GetInstance()->SetTargetEmissivePower(&testObj_->GetModel()->GetMaterial()->emissive);
	}


	// デプスステンシル作成 (TextureManagerシングルトン)
	depthStencilResource_ = TextureManager::GetInstance()->CreateDepthStencilTextureResource(
		windowProc->GetClientWidth(), windowProc->GetClientHeight()
	);

	currentFloor_ = 1;
	// マップエディタ生成・初期化
	// マップエディタ生成・初期化
	mapManager_ = std::make_unique<MapManager>();
	mapManager_->SetCamera(camera_.get());
	mapManager_->Initialize();
	mapManager_->SetNoiseTexture(textures_["noise0"].srvIndex);



	// カード用の3Dモデルを読み込んでおく（※パスやファイル名はご自身の環境に合わせてください）
	ModelManager::GetInstance()->LoadModel("plane", "resources/plane", "plane.obj");
	ModelManager::GetInstance()->LoadModel("cardR", "resources/card", "CardR.obj");
	ModelManager::GetInstance()->LoadModel("cardF", "resources/card", "cardF.obj");
	ModelManager::GetInstance()->LoadModel("cardFire", "resources/card", "CardFire.obj");
	ModelManager::GetInstance()->LoadModel("cardPotion", "resources/card", "CardPotion.obj");
	ModelManager::GetInstance()->LoadModel("cardSpeedUp", "resources/card", "CardSpeedUp.obj");
	ModelManager::GetInstance()->LoadModel("CardShield", "resources/card", "CardShield.obj");
	ModelManager::GetInstance()->LoadModel("CardIce", "resources/card", "CardIce.obj");
	ModelManager::GetInstance()->LoadModel("CardFang", "resources/card", "CardFang.obj");
	ModelManager::GetInstance()->LoadModel("CardDecoy", "resources/card", "CardDecoy.obj");
	ModelManager::GetInstance()->LoadModel("CardAtkDown", "resources/card", "CardAtkDown.obj");
	ModelManager::GetInstance()->LoadModel("CardClaw", "resources/card", "CardClaw.obj");

	// CSVからカードデータベースを初期化
	CardDatabase::Initialize("resources/card/CardData.csv");

	CardDatabase::LoadAdditionalCards("resources/card/BossCardData.csv");

	// 手札マネージャーの初期化
	handManager_.Initialize(uiCamera_.get(), textures_["noise0"].srvIndex);

	//最初から手札にID１を追加する
	handManager_.AddCard(CardDatabase::GetCardData(1));

	// これだけでOK
	RegenerateDungeonAndRespawnPlayer(8);

	minimap_ = std::make_unique<Minimap>();
	minimap_->Initialize();
	minimap_->SetLevelData(&mapManager_->GetLevelData());
	EditorManager::GetInstance()->SetCamera(camera_.get());

	// 初期ロード時のマップ変更通知を消す
	if (mapManager_) {
		mapManager_->ConsumeMapChanged();
	}

	TextManager::GetInstance()->Initialize();

	TextManager::GetInstance()->SetPosition("PlayerHP", 40, 560);
	TextManager::GetInstance()->SetPosition("PlayerCost", 40, 600);
	TextManager::GetInstance()->SetPosition("PlayerLevel", 40, 640);
	TextManager::GetInstance()->SetPosition("PlayerEXP", 40, 680);

	// 画面サイズ取得
	float screenW = static_cast<float>(WindowProc::GetInstance()->GetClientWidth());
	float screenH = static_cast<float>(WindowProc::GetInstance()->GetClientHeight());

	// 中央に配置（少し上に出すなら -50 くらい）
	TextManager::GetInstance()->SetPosition("CostLack", screenW * 0.5f - 100.0f, screenH * 0.5f - 50.0f);

	// 左下のステータス背景
	playerStatusBgSprite_ = Sprite::Create("resources/white1x1.png", { 170.0f, 625.0f });
	playerStatusBgSprite_->SetSize({ 340.0f, 190.0f });
	playerStatusBgSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.65f });

	// スプライト作成（座標 X:100, Y:500）
	descBgSprite_ = Sprite::Create("resources/white1x1.png", { 100.0f, 500.0f });

	// 大きさを幅600, 高さ100の長方形にする
	descBgSprite_->SetSize({ 600.0f, 100.0f });

	// 色を半透明の黒にする（Vector4 で R, G, B, A）
	descBgSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.75f });// 画面全体を覆うフェード用スプライトの作成 (座標0,0)

	fadeSprite_ = Sprite::Create("resources/white1x1.png", { 0.0f, 0.0f });
	// 画面サイズに合わせる (ウィンドウサイズに合わせて変更してください)
	fadeSprite_->SetSize({ 4000.0f, 4000.0f });
	// 初期状態は透明の黒
	fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });

	// ポーズ画面用の文字位置
	TextManager::GetInstance()->SetPosition("PauseTitle", 560, 220);
	TextManager::GetInstance()->SetPosition("PauseResume", 540, 320);
	TextManager::GetInstance()->SetPosition("PauseToTitle", 540, 360);

	// ポーズ中の半透明背景
	pauseBgSprite_ = Sprite::Create("resources/white1x1.png", { 0.0f, 0.0f });
	pauseBgSprite_->SetSize({ 4000.0f, 4000.0f });
	pauseBgSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.5f });
}

void GamePlayScene::Update() {

	// デバッグカメラ更新
	if (debugCamera_) {
		debugCamera_->Update(camera_.get());
	}

	Input *input = Input::GetInstance();

	// BossManagerから必要なものだけ取る
	Boss* boss = bossManager_ ? bossManager_->GetBoss() : nullptr;
	Sprite* bossHpBackSprite = bossManager_ ? bossManager_->GetBossHpBackSprite() : nullptr;
	Sprite* bossHpFillSprite = bossManager_ ? bossManager_->GetBossHpFillSprite() : nullptr;

	// ポーズ切り替え
	if (input->Triggerkey(DIK_ESCAPE)) {
		isPaused_ = !isPaused_;
		pauseSelection_ = 0; // 開くたびに先頭へ戻す
	}

	// ポーズ中は専用更新だけして止める
	if (isPaused_) {
		UpdatePause(input);
		return;
	}

	// ==========================================
// FadeOut中だけゲーム更新を止める
// 真っ黒になったら階層切り替えして、FadeInから更新再開
// ==========================================
	if (transitionState_ == TransitionState::FadeOut) {
		fadeAlpha_ += kFadeSpeed;

		if (fadeAlpha_ >= 1.0f) {
			fadeAlpha_ = 1.0f;

			// 真っ黒になった瞬間に階層切り替え
			AdvanceFloor();

			// ここからは新しい階層を更新しながら見せる
			transitionState_ = TransitionState::FadeIn;
		}

		if (fadeSprite_) {
			fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, fadeAlpha_ });
			fadeSprite_->Update();
		}

		// FadeOut中はここで止める
		return;
	}

	// マップ切り替えがあったら戦闘ごとリセット
	if (mapManager_ && mapManager_->ConsumeMapChanged()) {

		// GUI切り替えでも、ボス部屋になったら登場演出を開始する
		if (mapManager_->IsBossMap()) {
			if (bossManager_) {
				bossManager_->SetBossIntroPlaying(true);
				bossManager_->SetBossIntroCameraState(BossManager::IntroCameraState::PlayerFocus);
				bossManager_->SetBossIntroTimer(60);
			}
		} else {
			if (bossManager_) {
				bossManager_->SetBossIntroPlaying(false);
				bossManager_->SetBossIntroCameraState(BossManager::IntroCameraState::None);
				bossManager_->SetBossIntroTimer(0);
			}
		}

		ResetBattleDebug();
		return;
	}

	if (isCardSwapMode_) {
		UpdateCardSwapMode(input);
		return;
	}

	// デバッグ用リセット
	if (input->Triggerkey(DIK_R)) {
		ResetBattleDebug();
	}

	// BGM再生
	if (input->Triggerkey(DIK_SPACE)) {
		AudioManager::GetInstance()->PlayWave(bgmFile_);
	}

	// タイトルシーンへ移動
	if (input->Triggerkey(DIK_T)) {
		SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
	}

	//// パーティクル発生
	//if (input->Triggerkey(DIK_P)) {
	//	ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 0.0f, 0.0f }, 10);
	//}

	//// パーティクル更新
	//ParticleManager::GetInstance()->Update(camera_.get());

	// ==========================================
	// プレイヤーの更新処理
	// ==========================================

	if (player_) {

		if (input->Triggerkey(DIK_F1)) {
			isInfiniteMode_ = !isInfiniteMode_;
		}

		if (isInfiniteMode_) {
			player_->SetMaxCost(999);
			player_->SetCost(player_->GetMaxCost());
			player_->SetHP(player_->GetMaxHP());
		}


		// デバッグカメラ中、またはボス部屋突入演出中は操作を止める
		bool isBossIntroPlaying = bossManager_ ? bossManager_->IsBossIntroPlaying() : false;

		if ((debugCamera_ && debugCamera_->IsActive()) || isBossIntroPlaying) {
			player_->SetInputEnable(false);
		} else {
			player_->SetInputEnable(true);
		}

		Vector3 oldPos = player_->GetPosition();

		player_->Update();
		playerPos_ = player_->GetPosition();

		AABB playerAABB;
		playerAABB.min = { playerPos_.x - 0.5f, playerPos_.y - 0.5f, playerPos_.z - 0.5f };
		playerAABB.max = { playerPos_.x + 0.5f, playerPos_.y + 0.5f, playerPos_.z + 0.5f };

		const LevelData &level = mapManager_->GetLevelData();

		int centerGridX = static_cast<int>(std::round(playerPos_.x / level.tileSize));
		int centerGridZ = static_cast<int>(std::round(playerPos_.z / level.tileSize));

		int startX = std::max(0, centerGridX - 1);
		int endX = std::min(level.width - 1, centerGridX + 1);
		int startZ = std::max(0, centerGridZ - 1);
		int endZ = std::min(level.height - 1, centerGridZ + 1);

		bool isPlayerHit = false;

		for (int z = startZ; z <= endZ && !isPlayerHit; z++) {
			for (int x = startX; x <= endX; x++) {

				if (level.tiles[z][x] != 1) continue;

				float worldX = x * level.tileSize;
				float worldZ = z * level.tileSize;

				AABB blockAABB;
				blockAABB.min = { worldX - 1.0f, level.baseY, worldZ - 1.0f };
				blockAABB.max = { worldX + 1.0f, level.baseY + 2.0f, worldZ + 1.0f };

				if (Collision::IsCollision(playerAABB, blockAABB)) {
					player_->SetPosition(oldPos);
					playerPos_ = oldPos;
					isPlayerHit = true;
					break;
				}
			}
		}

		playerScale_ = player_->GetScale();
	}

	if (playerObj_) {
		playerObj_->SetTranslation(playerPos_);
		playerObj_->SetRotation(player_->GetRotation());
		playerObj_->SetScale(playerScale_);
		playerObj_->Update();
	}

	if (player_ && player_->IsDead()) {
		SceneManager::GetInstance()->ChangeScene("GAMEOVER");
		return;
	}


	// ==========================================
	// 階段タイル(3)との判定
	// ==========================================
	if (player_ && mapManager_) {
		const LevelData &level = mapManager_->GetLevelData();
		int gridX = static_cast<int>(std::round(playerPos_.x / level.tileSize));
		int gridZ = static_cast<int>(std::round(playerPos_.z / level.tileSize));

		if (gridX >= 0 && gridX < level.width && gridZ >= 0 && gridZ < level.height) {
			if (level.tiles[gridZ][gridX] == 3) {
				if (transitionState_ == TransitionState::None) {
					transitionState_ = TransitionState::FadeOut;
					fadeAlpha_ = 0.0f;
				}
			}
		}
	}

	// ==========================================
	// 雑魚敵の更新処理
	// ==========================================

	// ターゲットを決める！
	Vector3 targetPos = playerPos_; // 基本はプレイヤーの位置を狙う
	if (playerCardSystem_ && playerCardSystem_->IsDecoyActive()) {
		targetPos = playerCardSystem_->GetDecoyPosition(); // 身代わりがいたら身代わりを狙う！
	}

	// EnemyManager に更新をお願いする
	if (enemyManager_) {
		enemyManager_->Update(player_.get(), &cardPickupManager_, mapManager_.get(), boss);
	}
	
	// ==========================================
	// ボスの更新処理
	// ==========================================
	
	// ボス関連の更新をまとめてBossManagerに任せる
	if (bossManager_) {
		bossManager_->Update(
			player_.get(),
			enemyManager_.get(),
			&cardPickupManager_,
			mapManager_.get(),
			camera_.get(),
			playerPos_,
			targetPos
		);
	}

	// ボスを倒していたらゲームクリアへ遷移
	if (bossManager_ && bossManager_->ShouldTriggerGameClear(mapManager_.get())) {
		SceneManager::GetInstance()->ChangeScene("GAMECLEAR");
		return;
	}

	// 敵→ プレイヤーの攻撃
	enemyManager_->CheckCollisions(player_.get());
	
	// ==========================================
	// カメラ・各種オブジェクトの更新
	// ==========================================
	// メインカメラの更新（プレイヤーに追従）
	if (camera_) {
		if (debugCamera_ && !debugCamera_->IsActive()) {

			// 今のカメラ位置と回転を取得
			Vector3 currentPos = camera_->GetTranslation();
			Vector3 currentRot = camera_->GetRotation();

			bool isBossIntroPlaying = bossManager_ ? bossManager_->IsBossIntroPlaying() : false;
			BossManager::IntroCameraState bossIntroState =
				bossManager_ ? bossManager_->GetBossIntroCameraState() : BossManager::IntroCameraState::None;
			int bossIntroTimer = bossManager_ ? bossManager_->GetBossIntroTimer() : 0;

			// 目標位置と目標回転
			Vector3 targetPos = currentPos;
			Vector3 targetRot = currentRot;

			// ボス部屋突入時の演出カメラ
			if (isBossIntroPlaying && mapManager_ && mapManager_->IsBossMap() && boss) {

				Vector3 bossPos = boss->GetPosition();

				// 最初はプレイヤー側を見る
				if (bossIntroState == BossManager::IntroCameraState::PlayerFocus) {

					targetPos = {
						playerPos_.x,
						playerPos_.y + 10.0f,
						playerPos_.z - 8.0f
					};

					targetRot = {
						0.75f, 0.0f, 0.0f
					};

					bossIntroTimer--;
					if (bossManager_) {
						bossManager_->SetBossIntroTimer(bossIntroTimer);
					}
					if (bossIntroTimer <= 0) {
						if (bossManager_) {
							bossManager_->SetBossIntroCameraState(BossManager::IntroCameraState::BossFocus);
							bossManager_->SetBossIntroTimer(75);
						}
					}
				}

				// 次にボス側を見る
				else if (bossIntroState == BossManager::IntroCameraState::BossFocus) {

					targetPos = {
						bossPos.x,
						bossPos.y + 8.0f,
						bossPos.z - 10.0f
					};

					targetRot = {
						0.55f, 0.0f, 0.0f
					};

					bossIntroTimer--;
					if (bossManager_) {
						bossManager_->SetBossIntroTimer(bossIntroTimer);
					}
					if (bossIntroTimer <= 0) {
						if (bossManager_) {
							bossManager_->SetBossIntroCameraState(BossManager::IntroCameraState::ToBattle);
							bossManager_->SetBossIntroTimer(30);
						}
					}
				}

				// 最後に通常のボス戦カメラへ戻す
				else if (bossIntroState == BossManager::IntroCameraState::ToBattle) {

					Vector3 center = {
						(playerPos_.x + bossPos.x) * 0.5f,
						(playerPos_.y + bossPos.y) * 0.5f,
						(playerPos_.z + bossPos.z) * 0.5f
					};

					Vector3 diff = {
						bossPos.x - playerPos_.x,
						0.0f,
						bossPos.z - playerPos_.z
					};

					float distance = Length(diff);

					float t = distance / 20.0f;
					if (t > 1.0f) t = 1.0f;
					t = t * t;

					float height = 8.0f + t * 14.0f;
					float back = 6.0f + t * 22.0f;

					if (height < 8.0f) height = 8.0f;
					if (back < 6.0f) back = 6.0f;
					if (height > 22.0f) height = 22.0f;
					if (back > 28.0f) back = 28.0f;

					targetPos = {
						center.x,
						center.y + height,
						center.z - back
					};

					targetRot = {
						0.85f, 0.0f, 0.0f
					};

					bossIntroTimer--;
					if (bossManager_) {
						bossManager_->SetBossIntroTimer(bossIntroTimer);
					}
					if (bossIntroTimer <= 0) {
						if (bossManager_) {
							bossManager_->SetBossIntroPlaying(false);
							bossManager_->SetBossIntroCameraState(BossManager::IntroCameraState::None);
						}
						if (boss && !boss->IsDead()) {
							boss->SetState(Boss::State::Chase);
						}
					}
				}
			}

			// 通常のボス戦カメラ
			else if (mapManager_ && mapManager_->IsBossMap() && boss && !boss->IsDead()) {

				Vector3 bossPos = boss->GetPosition();

				Vector3 center = {
					(playerPos_.x + bossPos.x) * 0.5f,
					(playerPos_.y + bossPos.y) * 0.5f,
					(playerPos_.z + bossPos.z) * 0.5f
				};

				Vector3 diff = {
					bossPos.x - playerPos_.x,
					0.0f,
					bossPos.z - playerPos_.z
				};

				float distance = Length(diff);

				float t = distance / 20.0f;
				if (t > 1.0f) t = 1.0f;
				t = t * t;

				float height = 8.0f + t * 14.0f;
				float back = 6.0f + t * 22.0f;

				if (height < 8.0f) height = 8.0f;
				if (back < 6.0f) back = 6.0f;
				if (height > 22.0f) height = 22.0f;
				if (back > 28.0f) back = 28.0f;

				targetPos = {
					center.x,
					center.y + height,
					center.z - back
				};

				targetRot = {
					0.85f, 0.0f, 0.0f
				};
			}

			// 通常時
			else {
				targetPos = {
					playerPos_.x,
					playerPos_.y + 15.0f,
					playerPos_.z - 15.0f
				};

				targetRot = {
					0.9f, 0.0f, 0.0f
				};
			}

			// 補間率
			float followRate = 0.08f;

			// 演出中はちょっと速めに動かす
			if (isBossIntroPlaying) {
				followRate = 0.12f;
			}

			// 位置をなめらかに移動
			currentPos.x += (targetPos.x - currentPos.x) * followRate;
			currentPos.y += (targetPos.y - currentPos.y) * followRate;
			currentPos.z += (targetPos.z - currentPos.z) * followRate;

			// 回転もなめらかに移動
			currentRot.x += (targetRot.x - currentRot.x) * followRate;
			currentRot.y += (targetRot.y - currentRot.y) * followRate;
			currentRot.z += (targetRot.z - currentRot.z) * followRate;

			camera_->SetTranslation(currentPos);
			camera_->SetRotation(currentRot);
		}

		camera_->Update();
	}


	// ==========================================
	// ドロップアイテム(カード)の取得判定
	// ==========================================

	cardPickupManager_.Update();

	for (auto &pickup : cardPickupManager_.GetPickups()) {
		if (!pickup.isActive) {
			continue;
		}

		// プレイヤーとの距離計算
		Vector3 playerDiff = {
			playerPos_.x - pickup.position.x,
			playerPos_.y - pickup.position.y,
			playerPos_.z - pickup.position.z
		};

		float playerDist = Length(playerDiff);

		// プレイヤーが拾う処理
		if (player_ && !player_->IsDead() && playerDist < 2.0f) {
			bool success = handManager_.AddCard(pickup.card);
			if (success) {
				pickup.isActive = false;
				continue;
			} else {
				// 手札が一杯ならカード交換モードへ移行
				/*isCardSwapMode_ = true;
				pendingCard_ = pickup.card;*/
				pickup.isActive = false;
				continue;
			}
		}


	}

	// ボス頭上HPバー更新
	if (boss && !boss->IsDead() && mapManager_ && mapManager_->IsBossMap() &&
		bossHpBackSprite && bossHpFillSprite && camera_) {

		Vector3 bossHeadPos = boss->GetPosition();
		bossHeadPos.y += 2.8f; // 高すぎたので少し下げる

		Vector2 screenPos = WorldToScreen(bossHeadPos);

		// バーサイズ
		const float backWidth = 160.0f;
		const float backHeight = 16.0f;
		const float fillMaxWidth = 152.0f;
		const float fillHeight = 10.0f;

		// HP割合
		float hpRate = 0.0f;
		if (boss->GetMaxHP() > 0) {
			hpRate = static_cast<float>(boss->GetHP()) / static_cast<float>(boss->GetMaxHP());
		}
		if (hpRate < 0.0f) hpRate = 0.0f;
		if (hpRate > 1.0f) hpRate = 1.0f;

		// 背景バーは中央基準でそのまま配置
		bossHpBackSprite->SetPosition(screenPos);
		bossHpBackSprite->SetSize({ backWidth, backHeight });
		bossHpBackSprite->Update();

		// HP割合で色変更
		Vector4 hpColor{};
		if (hpRate > 0.6f) {
			hpColor = { 0.2f, 1.0f, 0.2f, 1.0f }; // 緑
		} else if (hpRate > 0.3f) {
			hpColor = { 1.0f, 0.9f, 0.2f, 1.0f }; // 黄
		} else {
			hpColor = { 1.0f, 0.2f, 0.2f, 1.0f }; // 赤
		}

		// 本体バーの現在幅
		float fillWidth = fillMaxWidth * hpRate;

		// 背景バーの左端を基準に、本体バーの中心を計算
		float backLeft = screenPos.x - backWidth * 0.5f;
		float fillLeft = backLeft + 4.0f;
		float fillCenterX = fillLeft + fillWidth * 0.5f;

		// 背景の中央から少し下に本体バーを置く
		Vector2 fillPos = {
			fillCenterX,
			screenPos.y + 1.0f
		};

		bossHpFillSprite->SetPosition(fillPos);
		bossHpFillSprite->SetSize({ fillWidth, fillHeight });
		bossHpFillSprite->SetColor(hpColor);
		bossHpFillSprite->Update();
	}
	// その他3Dオブジェクトの更新
	for (auto &obj : object3ds_) {
		obj->Update();
	}

	// スプライトの更新
	if (sprite_) {
		sprite_->SetPosition(spritePos_);
		sprite_->Update();
	}

	// ステータス背景更新
	if (playerStatusBgSprite_) {
		playerStatusBgSprite_->Update();
	}

	// テストオブジェクトの更新
	if (testObj_) {
		testObj_->Update();
	}

	// レベル(マップ)・ポストエフェクトの更新
	mapManager_->Update(playerPos_);
	PostEffect::GetInstance()->Update();

	// ミニマップ更新
	if (minimap_) {
		// プレイヤー
		minimap_->SetPlayerPosition(playerPos_);

		

		// カード
		std::vector<Vector3> cardPositions;
		for (const auto& pickup : cardPickupManager_.GetPickups()) {
			if (!pickup.isActive) {
				continue;
			}
			cardPositions.push_back(pickup.position);
		}
		minimap_->SetCardPositions(cardPositions);

		minimap_->Update();
	}

	// 手札(UIなど)の更新
	if (player_ && !player_->IsDead()) {
		handManager_.Update();
	}

	// コスト不足メッセージ更新 ←ここ追加
	if (costLackMessageTimer_ > 0) {
		costLackMessageTimer_--;

		TextManager::GetInstance()->SetText("CostLack", "コスト不足です");
	} else {
		TextManager::GetInstance()->SetText("CostLack", "");
	}

	// プレイヤーステータス表示更新
	if (player_) {
		std::string hpText =
			"HP : " + std::to_string(player_->GetHP()) + " / " + std::to_string(player_->GetMaxHP());

		std::string costText =
			"COST : " + std::to_string(player_->GetCost()) + " / " + std::to_string(player_->GetMaxCost());

		std::string levelText =
			"LV : " + std::to_string(player_->GetLevel());

		std::string expText =
			"EXP : " + std::to_string(player_->GetExp()) + " / " + std::to_string(player_->GetNextLevelExp());

		TextManager::GetInstance()->SetText("PlayerHP", hpText);
		TextManager::GetInstance()->SetText("PlayerCost", costText);
		TextManager::GetInstance()->SetText("PlayerLevel", levelText);
		TextManager::GetInstance()->SetText("PlayerEXP", expText);
	}

	// ==========================================
	// カードシステム用のターゲット検索と更新
	// ==========================================

	

	Boss *targetBoss = nullptr;
	Vector3 bossPos{};

	if (boss && !boss->IsDead()) {
		targetBoss = boss;
		bossPos = boss->GetPosition();
	}

	UpdateCardUse(input);

	// ==========================================
	// 攻撃カードの「撃ち放題」タイマーと発動処理
	// ==========================================
	if (isCardReady_) {
		cardReadyTimer_--; // 毎フレーム時間を減らす

		// 画面に表示する文字を作る
		std::string displayText = "Eキーで発動：" + readiedCard_.name;
		
		if (cardReadyTimer_ > 120) {
			// 【残り2秒(120フレーム)より多いとき】
			// まだ余裕があるので、ずっと常時表示しておく
			TextManager::GetInstance()->SetText("ReadyCardT", displayText);

		} else {
			// 【残り2秒以下になったとき】
			// 時間切れが近いので、チカチカ点滅させてプレイヤーを焦らす！
			// （% 20 >= 10 にすることで、少し早めのスピードで点滅します）
			if (cardReadyTimer_ % 20 >= 10) {
				TextManager::GetInstance()->SetText("ReadyCardT", displayText);
			} else {
				TextManager::GetInstance()->SetText("ReadyCardT", "");
			}
		}


		// 別キー（例として Eキー）を押した瞬間に魔法を発動！（連打で何回でも撃てる）
		if (input->Triggerkey(DIK_E)) {
			if (playerCardSystem_) {
				playerCardSystem_->UseCard(
					readiedCard_,
					playerPos_,
					player_->GetRotation().y,
					true,
					player_.get()
				);
			}
		}

		// 時間切れになったら構え状態（撃ち放題）を終了する
		if (cardReadyTimer_ <= 0) {
			isCardReady_ = false;

			// 時間切れになったら文字を空（非表示）にする
			TextManager::GetInstance()->SetText("ReadyCardT", "");
		}
	}

	// プレイヤー用カードシステム更新
	if (playerCardSystem_) {
		playerCardSystem_->Update(
			player_.get(),
			enemyManager_.get(),
			targetBoss,
			playerPos_,
			{ 0.0f, 0.0f, 0.0f },
			bossPos,
			mapManager_->GetLevelData()
		);
	
	for (auto& block : blocks_) {
		block->Update();
	}

	// 背景枠の更新
	if (descBgSprite_) {
		descBgSprite_->Update();
	}

	// 手札がある時だけ処理
	if (handManager_.GetHandSize() > 0) {
		// 1. 今選んでいるカードの番号を取得
		int selectedIdx = handManager_.GetSelectedCardIndex();

		// 2. その番号のカード情報（CSVのデータ）をごっそり取得
		Card selectedCard = handManager_.GetCard(selectedIdx);

		// 3. 説明文だけを変数に入れる
		std::string displayText = selectedCard.description;

		size_t pos = displayText.find("\\n");
		while (pos != std::string::npos) {
			displayText.replace(pos, 2, "\n");
			pos = displayText.find("\\n", pos + 1);
		}

		// 4. テキストオブジェクトに文字を流し込む！
		// ※ textObj_ の部分は、チームメンバーさんが作ったテキスト管理の変数名に直してください
		TextManager::GetInstance()->SetText("CardT", displayText);


	} else {
		// 手札がない時は文字を消す
		TextManager::GetInstance()->SetText("CardT", "");
	}

	// ==========================================
	// ★最優先：フェード演出 ＆ マップ切り替え処理
	// ==========================================
	if (transitionState_ == TransitionState::FadeOut) {
		fadeAlpha_ += kFadeSpeed; // 画面を暗くしていく

		if (fadeAlpha_ >= 1.0f) {
			fadeAlpha_ = 1.0f;

			// 画面が完全に真っ黒になった瞬間に、裏でマップを切り替える
			AdvanceFloor();

			// 切り替えが終わったらフェードインへ移行
			transitionState_ = TransitionState::FadeIn;
		}
	} else if (transitionState_ == TransitionState::FadeIn) {
		fadeAlpha_ -= kFadeSpeed; // 画面を明るくしていく

		if (fadeAlpha_ <= 0.0f) {
			fadeAlpha_ = 0.0f;
			transitionState_ = TransitionState::None; // 演出終了
		}
	}

	// フェード用スプライトの色と透明度を更新
	if (fadeSprite_) {
		fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, fadeAlpha_ });
		fadeSprite_->Update();
	}
	// ==========================================
// FadeIn中はゲームを動かしたまま明るく戻す
// ==========================================
	if (transitionState_ == TransitionState::FadeIn) {
		fadeAlpha_ -= kFadeSpeed;

		if (fadeAlpha_ <= 0.0f) {
			fadeAlpha_ = 0.0f;
			transitionState_ = TransitionState::None;
		}
	}

	if (fadeSprite_) {
		fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, fadeAlpha_ });
		fadeSprite_->Update();
	}
}

void GamePlayScene::Draw() {


void GamePlayScene::Draw(){
	auto dxCommon = DirectXCommon::GetInstance();
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();

	Boss* boss = bossManager_ ? bossManager_->GetBoss() : nullptr;
	Obj3d* bossObj = bossManager_ ? bossManager_->GetBossObj() : nullptr;
	CardUseSystem* bossCardSystem = bossManager_ ? bossManager_->GetBossCardSystem() : nullptr;
	Sprite* bossHpBackSprite = bossManager_ ? bossManager_->GetBossHpBackSprite() : nullptr;
	Sprite* bossHpFillSprite = bossManager_ ? bossManager_->GetBossHpFillSprite() : nullptr;

	// 画用紙への切り替え
	PostEffect::GetInstance()->PreDrawScene(commandList);
	auto commandList = dxCommon->GetCommandList();

	// 1. 【MRT開始】キャンバスを2枚(色用とマスク用)セットする！
	PostEffect::GetInstance()->PreDrawSceneMRT(commandList);

	// --- 3D描画の前準備 ---
	Obj3dCommon::GetInstance()->PreDraw(commandList);

	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D_CullNone);

	// プレイヤー描画
	if (playerObj_ && player_ && !player_->IsDead() && player_->IsVisible()) {
		playerObj_->Draw(); // 被弾中は点滅表示
	}

	// ボス描画を先に行う
	Obj3dCommon::GetInstance()->PreDraw(commandList);

	// --- カリングありの3D描画 ---
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D);
	if ( playerObj_ ) { playerObj_->Draw(); }
	for ( auto& obj : object3ds_ ) { obj->Draw(); }
	
	// --- カリングなしの3D描画 ---
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D_CullNone);

	if (bossManager_) {
		bossManager_->Draw(mapManager_.get());
	}

	// そのあと敵描画
	if (enemyManager_) {
		enemyManager_->Draw(camera_.get(), minimap_.get());
	}
	
	// カード使用演出描画
	if (playerCardSystem_) {
		playerCardSystem_->Draw();
	}

	/*for (auto &system : enemyCardSystems_) {
		if (system) {
			system->Draw();
		}
	}*/


	cardPickupManager_.Draw();


	// 3Dオブジェクト描画
	for (auto &obj : object3ds_) {
		obj->Draw();
	}

	if (blockGroup_) {
		blockGroup_->Draw(camera_.get());
	}

	//手札カード
	handManager_.Draw();
	if ( testObj_ ){ testObj_->Draw(); }

	// --- インスタンシングの3D描画 ---
	if ( blockGroup_ ) { blockGroup_->Draw(camera_.get()); }

	// --- パーティクル描画 ---
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);


	if (handManager_.GetHandSize() > 0 && descBgSprite_) {
		descBgSprite_->Draw();
	}
	// 2. 【MRT終了】3Dの描画が終わったので、2枚のキャンバスを読み込みモードに戻す
	PostEffect::GetInstance()->PostDrawSceneMRT(commandList);

	// パーティクル描画 (パイプライン切り替え)
	/*PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);*/


	SpriteCommon::GetInstance()->PreDraw(commandList);
	/*if (sprite_) {
		sprite_->Draw();
	}*/

	// ボスHPバーはSprite描画のタイミングで描く
	if (bossManager_) {
		bossManager_->DrawHpBar(mapManager_.get());
	}

	// ステータス背景描画
	if (playerStatusBgSprite_) {
		playerStatusBgSprite_->Draw();
	}
	if (minimap_) {
		minimap_->Draw();
	}

	// 3. いつものPostEffect（色用のキャンバスだけに処理がかかります）
	 PostEffect::GetInstance()->Draw(commandList); // ※もしこの関数を作っていれば

	// 4. エフェクト後の「色画像」と「マスク画像」の番号(SRV)をもらう
	uint32_t colorSrv = PostEffect::GetInstance()->GetSrvIndex();
	uint32_t maskSrv = PostEffect::GetInstance()->GetMaskSrvIndex();

	DrawPauseUI();

	TextManager::GetInstance()->Draw();
	// 5. Bloomに「色」と「マスク」を両方渡す！
	Bloom::GetInstance()->Render(commandList, colorSrv, maskSrv);
	
	// 5-2. Bloomの最終結果のSRV番号をもらう
	uint32_t finalSrv = Bloom::GetInstance()->GetResultSrvIndex();

	PostEffect::GetInstance()->PostDrawScene(commandList);
	PostEffect::GetInstance()->Draw(commandList);
	// ★一番最後にフェード用スプライトを描画（UIよりも手前に表示するため）
	if (fadeSprite_ && transitionState_ != TransitionState::None) {
		SpriteCommon::GetInstance()->PreDraw(commandList); // 必要に応じて
		fadeSprite_->Draw();
	}

	// 6. メイン画面（バックバッファ）への直接描画！
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon->GetBackBufferRtvHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon->GetDsvHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// 7. 最終的な結果を画面にドーン！と描く
	PipelineManager::GetInstance()->SetPostEffectPipeline(commandList, PostEffectType::None); // シェーダーはエフェクトなしのやつを使う
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, finalSrv); // Bloomの結果をSRVとしてセット
	commandList->DrawInstanced(3, 1, 0, 0); // 巨大な三角形を描いて全画面にテクスチャを貼る方式なので、頂点数は3でOK！


	// --- スプライト・UI描画 ---
	SpriteCommon::GetInstance()->PreDraw(commandList);
	if ( sprite_ ) { sprite_->Draw(); }
	TextManager::GetInstance()->Draw();
}

void GamePlayScene::DrawDebugUI() {

#ifdef USE_IMGUI
	Boss* boss = bossManager_ ? bossManager_->GetBoss() : nullptr;
	// 3Dオブジェクト、カメラ、パーティクルのUI
	Obj3dCommon::GetInstance()->DrawDebugUI();
	if (camera_) { camera_->DrawDebugUI(); }
	if (debugCamera_) { debugCamera_->DrawDebugUI(); }
	//ParticleManager::GetInstance()->DrawDebugUI();


	TextManager::GetInstance()->DrawDebugUI();

	ImGui::Begin("Block Dissolve Test");

	// スライダーで 0.0(通常) 〜 1.0(消滅) を操作
	if (ImGui::SliderFloat("ブロックの消滅度", &dissolveThreshold_, 0.0f, 1.0f)) {
		if (testObj_) {
			// スライダーを動かすと、このブロックの閾値だけが書き換わる
			testObj_->SetDissolveThreshold(dissolveThreshold_);
		}
	}

	// 便利なリセットボタン
	if (ImGui::Button("元に戻す")) {
		dissolveThreshold_ = 0.0f;
		if (testObj_) {

			testObj_->SetDissolveThreshold(0.0f);
		}

	}
	ImGui::SameLine();
	if (ImGui::Button("完全に消す")) {
		dissolveThreshold_ = 1.0f;
		if (testObj_) {
			testObj_->SetDissolveThreshold(1.0f);
		}
	}

	ImGui::End();

	ImGui::Begin("Card System Test");

	ImGui::Separator();
	ImGui::Text("[Card Pickups]");
	for (size_t i = 0; i < cardPickupManager_.GetPickups().size(); ++i) {
		const auto &pickup = cardPickupManager_.GetPickups()[i];
		ImGui::Text("%s : pos(%.1f, %.1f, %.1f) active=%s",
			pickup.card.name.c_str(),
			pickup.position.x,
			pickup.position.y,
			pickup.position.z,
			pickup.isActive ? "true" : "false");
	}

	// 仮のプレイヤー状況を表示
	if (player_) {
		ImGui::Text("Player Level: %d", player_->GetLevel());       // プレイヤーレベル表示
		ImGui::Text("Player EXP: %d / %d", player_->GetExp(), player_->GetNextLevelExp()); // 経験値表示
		ImGui::Text("Player Cost: %d / %d", player_->GetCost(), player_->GetMaxCost());     // コスト表示
		ImGui::Text("Player HP: %d / %d", player_->GetHP(), player_->GetMaxHP());           // HP表示
		ImGui::Text("Player Dead: %s", player_->IsDead() ? "true" : "false");               // 死亡状態表示
		ImGui::Text("Player Hit: %s", player_->IsHit() ? "true" : "false");                 // 被弾状態表示
		ImGui::Text("Player Invincible: %s", player_->IsInvincible() ? "true" : "false");   // 無敵状態表示
	}

	if (boss) {
		ImGui::Separator();
		ImGui::Text("[Boss]");
		ImGui::Text("Boss HP: %d / %d", boss->GetHP(), boss->GetMaxHP());
		ImGui::Text("Boss Dead: %s", boss->IsDead() ? "true" : "false");
	}

	// デバッグ用
	if (ImGui::Button("Add EXP +1")) {
		if (player_) {
			player_->AddExp(1); // デバッグ用経験値追加
		}
	}

	ImGui::Separator();
	ImGui::Text("[Dungeon Floor]");

	ImGui::Text("Current Floor: %d F", currentFloor_); // 現在の階層を表示
	if (ImGui::Button("Go to Next Floor (Stairs)")) {
		AdvanceFloor(); // ボタンを押したら次の階層へ
	}

	// 図鑑（CardDatabase）からIDを指定して正しいデータを拾う！
	ImGui::SameLine();
	if (ImGui::Button("Pick Up (ID: 2)")) {
		handManager_.AddCard(CardDatabase::GetCardData(2));
	}
	if (ImGui::Button("Pick Up (ID: 3)")) {
		handManager_.AddCard(CardDatabase::GetCardData(3));
	}
	if (ImGui::Button("Pick Up (ID: 4)")) {
		handManager_.AddCard(CardDatabase::GetCardData(4));
	}

	if (ImGui::Button("Pick Up (ID: 6)")) {
		handManager_.AddCard(CardDatabase::GetCardData(6));
	}

	if (ImGui::Button("Pick Up (ID: 7)")) {
		handManager_.AddCard(CardDatabase::GetCardData(7));
	}

	if (ImGui::Button("Pick Up (ID: 8)")) {
		handManager_.AddCard(CardDatabase::GetCardData(8));
	}

	if (ImGui::Button("Pick Up (ID: 10)")) {
		handManager_.AddCard(CardDatabase::GetCardData(10));
	}

	ImGui::Separator();
	ImGui::Text("[Player Hand] : %d/10", handManager_.GetHandSize());

	//手札の数だけループしてボタンを作る
	for (int i = 0; i < handManager_.GetHandSize(); ++i) {
		Card card = handManager_.GetCard(i);

		//ボタンの名前
		std::string btnName = card.name + "(Cost:" + std::to_string(card.cost) + ")##" + std::to_string(i);

		// 使う処理を入れる場合はこのif文の中に書く
		if (ImGui::Button(btnName.c_str())) {
			// 例：手札を使用する処理
		}
	}

	ImGui::End();

	
#endif

}

void GamePlayScene::ResetBattleDebug() {

	// プレイヤーの状態だけ初期化
	if (player_) {
		//player_->Initialize();
		playerScale_ = { 1.0f, 1.0f, 1.0f };
	}

	if (bossManager_) {
		bossManager_->Reset();
	}

	// カード使用システムの状態をリセット
	if (playerCardSystem_) {
		playerCardSystem_->Reset();
	}

	/*for (auto &system : enemyCardSystems_) {
		if (system) {
			system->Reset();
		}
	}*/

	// 手札を初期化
	/*handManager_.Initialize(uiCamera_.get(), textures_["noise0"].srvIndex);
	handManager_.AddCard(CardDatabase::GetCardData(1));*/

	// ダンジョン生成 + プレイヤー再配置 + 敵/カード再生成 + ボス再配置
	RegenerateDungeonAndRespawnPlayer(5);

	if (minimap_ && mapManager_) {
		minimap_->SetLevelData(&mapManager_->GetLevelData());
	}

	// 交換モードも戻しておく
	isCardSwapMode_ = false;
	pendingCard_ = Card{};

}


void GamePlayScene::UpdateCardSwapMode(Input *input) {

	// 手札の選択と見た目の更新
	handManager_.Update();
	uiCamera_->Update(); // UIカメラの更新もここで行う

	if (input->Triggerkey(DIK_SPACE)) {
		// 現在選んでいるカードを取得
		Card selectedCard = handManager_.GetSelectedCard();

		// 選んでいるカードがID: 1（初期カード)なら交換をしない
		if (selectedCard.id == 1) {
			return;
		}

		handManager_.SwapSelectedCard(pendingCard_);
		// ★追加：交換成功したら、地面に落ちていたアイテムを消す！
		if (pendingPickup_) {
			pendingPickup_->isActive = false;
			pendingPickup_ = nullptr;
		}

		isCardSwapMode_ = false;
	} else if (input->Triggerkey(DIK_C)) {
		// キャンセルした場合はアイテムは消さずにポインタだけリセット
		pendingPickup_ = nullptr;
		isCardSwapMode_ = false;
	}
}

void GamePlayScene::UpdateCardUse(Input *input) {

	if (!player_ || player_->IsDead() || !input->Triggerkey(DIK_SPACE)) {
		return;
	}

	if (player_->IsDodging()) {
		return;
	}

	if (player_->IsActionLocked()) {
		return;
	}

	if (handManager_.IsSelectedCardDissolving()) {
		return;
	}

	Card selectedCard = handManager_.GetSelectedCard();
	if (selectedCard.id == -1) {
		return;
	}

	// すでに攻撃カードを構えている（撃ち放題中）なら、他の攻撃カードは使えないようにする！
	if (isCardReady_ && selectedCard.id != 1 && static_cast<int>(selectedCard.effectType) == 0) {
		// 構えが終わるまでは弾く（ここで return するので、コストも消費されず手札からも消えません）
		return;
	}

	if (!player_->CanUseCost(selectedCard.cost)) {
		// コスト不足メッセージを出す
		costLackMessageTimer_ = 60; // 約1秒表示

		return;
	}

	player_->UseCost(selectedCard.cost);

	// カードの種類によって処理を分ける！

	if (selectedCard.id != 1 && static_cast<int>(selectedCard.effectType) == 0) {
		// 攻撃カード(effectType == 0)なら、数秒間「撃ち放題モード（構え状態）」にする
		isCardReady_ = true;
		readiedCard_ = selectedCard;
		cardReadyTimer_ = 60 * 5; // 5秒間維持 (60FPS想定)

	} else {
		if (playerCardSystem_) {
			playerCardSystem_->UseCard(
				selectedCard,
				playerPos_,
				player_->GetRotation().y,
				true,
				player_.get()
			);
		}
	}

	if (selectedCard.id != 1) {
		handManager_.StartDissolveSelectedCard();
	}
}

void GamePlayScene::UpdatePause(Input* input) {

	// 上下で選択
	if (input->Triggerkey(DIK_W)) {
		pauseSelection_--;
		if (pauseSelection_ < 0) {
			pauseSelection_ = 1;
		}
	}

	if (input->Triggerkey(DIK_S)) {
		pauseSelection_++;
		if (pauseSelection_ > 1) {
			pauseSelection_ = 0;
		}
	}

	// 決定
	if (input->Triggerkey(DIK_SPACE) || input->Triggerkey(DIK_RETURN)) {
		if (pauseSelection_ == 0) {
			isPaused_ = false; // ゲームに戻る
		} else if (pauseSelection_ == 1) {
			SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
			return;
		}
	}

	// ポーズ中の文字表示
	TextManager::GetInstance()->SetText("PauseTitle", "PAUSE");

	if (pauseSelection_ == 0) {
		TextManager::GetInstance()->SetText("PauseResume", "> Resume");
		TextManager::GetInstance()->SetText("PauseToTitle", "  Title");
	} else {
		TextManager::GetInstance()->SetText("PauseResume", "  Resume");
		TextManager::GetInstance()->SetText("PauseToTitle", "> Title");
	}

	// 背景更新
	if (pauseBgSprite_) {
		pauseBgSprite_->Update();
	}
}

void GamePlayScene::DrawPauseUI() {

	if (!isPaused_) {
		TextManager::GetInstance()->SetText("PauseTitle", "");
		TextManager::GetInstance()->SetText("PauseResume", "");
		TextManager::GetInstance()->SetText("PauseToTitle", "");
		return;
	}

	if (pauseBgSprite_) {
		pauseBgSprite_->Draw();
	}
}

GamePlayScene::GamePlayScene() {}

GamePlayScene::~GamePlayScene() {}
// 終了
void GamePlayScene::Finalize() {

	object3ds_.clear();

	playerCardSystem_.reset();
	//enemyCardSystems_.clear();
	if (bossManager_) {
		bossManager_->Finalize();
		bossManager_.reset();
	}
	pauseBgSprite_.reset();

	TextManager::GetInstance()->Finalize();

	textures_.clear();
	depthStencilResource_.Reset();
}

Vector2 GamePlayScene::WorldToScreen(const Vector3 &worldPos) const {
	if (!camera_) {
		return { -10000.0f, -10000.0f };
	}

	Matrix4x4 viewProjection = camera_->GetViewProjectionMatrix();

	Vector4 clip{};
	clip.x = worldPos.x * viewProjection.m[0][0] + worldPos.y * viewProjection.m[1][0] + worldPos.z * viewProjection.m[2][0] + 1.0f * viewProjection.m[3][0];
	clip.y = worldPos.x * viewProjection.m[0][1] + worldPos.y * viewProjection.m[1][1] + worldPos.z * viewProjection.m[2][1] + 1.0f * viewProjection.m[3][1];
	clip.z = worldPos.x * viewProjection.m[0][2] + worldPos.y * viewProjection.m[1][2] + worldPos.z * viewProjection.m[2][2] + 1.0f * viewProjection.m[3][2];
	clip.w = worldPos.x * viewProjection.m[0][3] + worldPos.y * viewProjection.m[1][3] + worldPos.z * viewProjection.m[2][3] + 1.0f * viewProjection.m[3][3];

	if (clip.w == 0.0f) {
		return { -10000.0f, -10000.0f };
	}

	float invW = 1.0f / clip.w;
	float ndcX = clip.x * invW;
	float ndcY = clip.y * invW;

	Vector2 screen{};
	screen.x = (ndcX * 0.5f + 0.5f) * static_cast<float>(WindowProc::GetInstance()->GetClientWidth());
	screen.y = (-ndcY * 0.5f + 0.5f) * static_cast<float>(WindowProc::GetInstance()->GetClientHeight());
	return screen;
}


void GamePlayScene::SpawnCardsRandom(int cardCount, int margin) {

	if (!spawnManager_.HasLevelData()) {
		return;
	}

	cardPickupManager_.Initialize(camera_.get());

	const LevelData &level = mapManager_->GetLevelData();

	std::vector<std::pair<int, int>> candidates =
		spawnManager_.FindCardSpawnCandidates(margin);

	if (candidates.empty()) {
		return;
	}

	// まず敵のいるマスを記録
	std::vector<std::pair<int, int>> enemyTiles;
	const auto &enemies = enemyManager_->GetEnemies();
	for (const auto &enemy : enemies) {
		if (!enemy || enemy->IsDead()) {
			continue;
		}
		Vector3 pos = enemy->GetPosition();
		int tileX = static_cast<int>(std::round(pos.x / level.tileSize));
		int tileZ = static_cast<int>(std::round(pos.z / level.tileSize));
		enemyTiles.push_back({ tileX, tileZ });
	}

	std::vector<std::pair<int, int>> filtered;
	for (const auto &c : candidates) {
		int x = c.first;
		int z = c.second;

		// 階段周囲3x3禁止
		if (IsNearStairsTile(x, z)) {
			continue;
		}

		// 階段タイルそのもの禁止
		if (x >= 0 && x < level.width && z >= 0 && z < level.height) {
			if (level.tiles[z][x] == 3) {
				continue;
			}
		}

		// 敵と同じマス禁止
		bool overlapsEnemy = false;
		for (const auto &e : enemyTiles) {
			if (e.first == x && e.second == z) {
				overlapsEnemy = true;
				break;
			}
		}
		if (overlapsEnemy) {
			continue;
		}

		filtered.push_back(c);
	}

	if (filtered.empty()) {
		return;
	}

	std::random_device rd;
	std::mt19937 mt(rd());
	std::shuffle(filtered.begin(), filtered.end(), mt);

	int spawnCount = std::min(cardCount, static_cast<int>(filtered.size()));

	

	for (int i = 0; i < spawnCount; ++i) {
		int tileX = filtered[i].first;
		int tileZ = filtered[i].second;

		Vector3 worldPos = spawnManager_.TileToWorldPosition(tileX, tileZ, 0.0f);
		worldPos.y = -0.99f;

		// IDを数字でランダムに選ぶのではなく、プレイヤー用リストから取得する
		Card dropCard = CardDatabase::GetRandomPlayerCard();

		cardPickupManager_.AddPickup(worldPos, dropCard);
	}
}

void GamePlayScene::RespawnPlayerInRoom() {
	if (!mapManager_ || !player_) {
		return;
	}

	if (mapManager_->IsBossMap()) {
		Vector3 center = mapManager_->GetMapCenterPosition(1.5f);
		playerPos_ = center;
		playerPos_.z -= 6.0f; // ボスより少し手前
	} else {
		playerPos_ = mapManager_->GetRandomPlayerSpawnPosition(1.5f);
	}

	playerScale_ = { 1.0f, 1.0f, 1.0f };

	player_->SetPosition(playerPos_);
	player_->SetScale(playerScale_);

	if (playerObj_) {
		playerObj_->SetTranslation(playerPos_);
		playerObj_->SetRotation({ 0.0f, 0.0f, 0.0f });
		playerObj_->SetScale(playerScale_);
		playerObj_->Update();
	}

	if (camera_) {
		camera_->SetTranslation({
			playerPos_.x,
			playerPos_.y + 15.0f,
			playerPos_.z - 15.0f
			});
		camera_->SetRotation({
			0.9f, 0.0f, 0.0f
			});
		camera_->Update();
	}
}
void GamePlayScene::RegenerateDungeonAndRespawnPlayer(int roomCount) {
	if (!mapManager_) {
		return;
	}

	stairsTile_ = { -1, -1 };

	if (mapManager_) {
		if (!mapManager_->IsBossMap()) {
			mapManager_->GenerateRandomDungeon(roomCount);
		}
	}

	// プレイヤー再配置
	RespawnPlayerInRoom();

	// 通常マップなら階段を1個置く
	if (mapManager_ && !mapManager_->IsBossMap()) {
		stairsTile_ = mapManager_->PlaceStairsTileRandomAndGetTile(playerPos_, 6.0f);
	}

	// スポーンマネージャに新しいマップを渡し直す
	spawnManager_.SetLevelData(&mapManager_->GetLevelData());

	
		ClearEnemiesAndCards();
	if (!mapManager_->IsBossMap()) {
		enemyManager_->SpawnEnemiesRandom(enemySpawnCount_, enemySpawnMargin_, &spawnManager_, mapManager_.get(), playerPos_, camera_.get());
		SpawnCardsRandom(cardSpawnCount_, cardSpawnMargin_);
		
	}

	RespawnBossInRoom();
}

void GamePlayScene::RespawnBossInRoom() {
	if (bossManager_) {
		bossManager_->RespawnInRoom(mapManager_.get());
	}
}

// 敵とカードを完全にクリアしてスポーンマネージャもリセット
void GamePlayScene::ClearEnemiesAndCards() {

	if (enemyManager_) {
		enemyManager_->Clear();
	}

	cardPickupManager_.Initialize(camera_.get());
}

void GamePlayScene::AdvanceFloor() {
	
	// 階層が進む瞬間に、絶対に前の階の敵を全消去する！
	if (enemyManager_) {
		enemyManager_->Clear();
	}
	currentFloor_++; // 階層を1つ進める

	if (mapManager_) {
		// 5の倍数階ならボス部屋、それ以外は通常部屋
		if (currentFloor_ % 5 == 0) {
			mapManager_->ChangeToBossMap();

			// ボス部屋突入時のカメラ演出開始
			if (bossManager_) {
				bossManager_->SetBossIntroPlaying(true);
				bossManager_->SetBossIntroCameraState(BossManager::IntroCameraState::PlayerFocus);
				bossManager_->SetBossIntroTimer(60);
				bossManager_->SetBossCardRainTimer(bossManager_->GetBossCardRainInterval());
			}
		} else {
			mapManager_->ChangeToNormalMap();

			// 通常マップでは演出を切る
			if (bossManager_) {
				bossManager_->SetBossIntroPlaying(false);
				bossManager_->SetBossIntroCameraState(BossManager::IntroCameraState::None);
				bossManager_->SetBossIntroTimer(0);
				bossManager_->SetBossCardRainTimer(0);
			}
		}
	}

	ResetBattleDebug();

	if (minimap_ && mapManager_) {
		minimap_->SetLevelData(&mapManager_->GetLevelData());
	}
}

bool GamePlayScene::IsNearStairsTile(int x, int z) const {
	if (stairsTile_.first < 0 || stairsTile_.second < 0) {
		return false;
	}

	int dx = x - stairsTile_.first;
	int dz = z - stairsTile_.second;

	// 周囲1マス以内 = 3x3禁止
	return (std::abs(dx) <= 1 && std::abs(dz) <= 1);
}