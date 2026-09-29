#include "GamePlayScene.h"
// --- ゲーム固有のファイル ---
#include "TitleScene.h"
#include "game/player/Player.h"
#include "game/enemy/EnemyEditor.h"
#include "game/enemy/EnemyLevelConvert.h"
#include "game/enemy/EnemyRailPin.h"
#include "game/camera/EditorCameraUtil.h"

// --- エンジン側のファイル ---
#include "Engine/Audio/AudioManager.h"
#include "Engine/3D/Model/ModelManager.h"
#include "Engine/3D/Model/Model.h"
#include "Engine/Particle/ParticleManager.h"
#include "Engine/Graphics/PipelineManager.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Camera/DebugCamera.h"
#include "Engine/Base/Input.h"
#include "Engine/2D/SpriteCommon.h"
#include "Engine/3D/Obj/Obj3dCommon.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Base/TimeManager.h"
#include "engine/graphics/RenderTexture.h"
#include "engine/graphics/SrvManager.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/postEffect/PostEffect.h"
#include "engine/utils/TextManager.h"
#include "engine/sdf/SDFManager.h"
#include "engine/particle/GPUParticleManager.h"
#include "engine/utils/Level/BlenderImporter.h"
#include "engine/utils/Level/LevelEditor.h"
#include "engine/utils/Level/RailEditor.h"
#include "Bloom.h"

#include <cmath>

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;

// =====================================================================
//  初期化：読み込み → カメラ → 見た目 → ゲーム部品 の順
// =====================================================================
void GamePlayScene::Initialize(){
	LoadResources();
	SetupCameras();
	SetupVisuals();
	SetupGameplay();
}

// BGM・SE・モデル・テクスチャの読み込み
void GamePlayScene::LoadResources(){
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();

	// BGM と卵アクションのSE（投げ/命中/割れ。自前生成のプレースホルダ音源）
	AudioManager* audio = AudioManager::GetInstance();
	audio->LoadWave(bgmFile_);
	audio->LoadWave("resources/se/eggThrow.wav");
	audio->LoadWave("resources/se/eggHit.wav");
	audio->LoadWave("resources/se/eggBreak.wav");

	// モデル
	ModelManager* modelManager = ModelManager::GetInstance();
	modelManager->LoadModel("fence", "resources", "fence.obj");
	modelManager->LoadModel("grass", "resources", "terrain.obj");
	modelManager->LoadModel("block", "resources/block", "block.obj");
	modelManager->CreateSphereModel("sphere", 16);
	modelManager->CreatePlaneModel("plane");
	modelManager->LoadModel("animatedCube", "resources/AnimatedCube", "AnimatedCube.gltf");
	modelManager->LoadModel("human", "resources/human", "walk.gltf");
	modelManager->LoadModel("egg", "resources/egg", "egg.obj"); // ヨッシーの卵（専用モデル）
	modelManager->LoadModel("player", "resources/player", "player.gltf"); // プレイヤー（リグ付きマスコット。7色パレット焼き込み済み）
	// 敵キャラ3種（リグ+クリップ入りglb。プレイヤーと同じトイ風の公式デザイン）
	modelManager->LoadModel("enemyGround", "resources/enemy", "enemy_ground.glb"); // 地上「ドングリン」(Idle/Walk)
	modelManager->LoadModel("enemyAir",    "resources/enemy", "enemy_air.glb");    // 空中「フワリン」(Fly)
	modelManager->LoadModel("enemyPlant",  "resources/enemy", "enemy_plant.glb");  // 植物「カミバナ」(Idle/Bite)
	modelManager->LoadModel("roadStraight", "resources/road", "road_straight.obj"); // 道の直線ピース（グリッド組み用）
	modelManager->LoadModel("roadEnd",      "resources/road", "road_end.obj");      // 道の終端キャップ（自由端を閉じる）
	modelManager->LoadModel("roadCorner",   "resources/road", "road_corner.obj");   // 交差点ピース：直角コーナー
	modelManager->LoadModel("roadT",        "resources/road", "road_t.obj");        // 交差点ピース：T字路
	modelManager->LoadModel("roadCross",    "resources/road", "road_cross.obj");    // 交差点ピース：十字路
	modelManager->LoadModel("roadJoint",    "resources/road", "road_joint.obj");    // 接続ノードの凸ジョイント（プラレール風）
	modelManager->CreateEggShellModel("eggShell", 0.3f);        // 卵の殻の欠片（割れ演出用）

	// 汎用パーティクル用の粒（"sphere" は敵と共有＋モンスターボール柄がデフォルトなので、
	//   色を付けるだけの粒には専用の白い球を使う）
	modelManager->CreateSphereModel("fxSphere", 8);
	if ( auto* fxModel = modelManager->FindModel("fxSphere") ) {
		fxModel->SetTexture("resources/block/white1x1.png");
	}
	// 収集物（コイン）用の金色の球
	modelManager->CreateSphereModel("coin", 12);
	if ( auto* coinModel = modelManager->FindModel("coin") ) {
		coinModel->SetTexture("resources/block/white1x1.png");
		if ( coinModel->GetMaterial() ) { coinModel->GetMaterial()->color = { 1.0f, 0.85f, 0.2f, 1.0f }; }
	}

	// クラフトブロック一式（全モデルが block_atlas.png 1枚を共有。原点=底面中心・実寸1m角）。
	//   keepOrigin=true：ローダーの重心センタリングを止めてファイルの底面原点を維持する
	//   （センタリングされると描画だけ半分沈む＝長年の「ブロックが浮く/埋まる」の根本原因だった）
	modelManager->LoadModel("craftSponge",  "resources/block/craft", "block_1x1x1_sponge.obj", true);
	modelManager->LoadModel("craftLayer",   "resources/block/craft", "block_1x1x1_layer.obj", true);
	modelManager->LoadModel("craftSlope45", "resources/block/craft", "slope_1x1_rolls.obj", true);
	modelManager->LoadModel("craftSlope26", "resources/block/craft", "slope_2x1_rolls.obj", true);
	modelManager->LoadModel("flowerOrange", "resources/block/craft", "flower_orange.obj", true);
	modelManager->LoadModel("flowerWhite",  "resources/block/craft", "flower_white_face.obj", true);
	// 性質つきブロック：既存モデルを別名で読み、着色して見分ける
	auto loadTintedBlock = [&](const char* name, const char* file, const Vector4& color){
		modelManager->LoadModel(name, "resources/block/craft", file, true);
		if ( auto* model = modelManager->FindModel(name) ) {
			if ( model->GetMaterial() ) { model->GetMaterial()->color = color; }
		}
	};
	loadTintedBlock("craftSpring",     "block_1x1x1_sponge.obj", { 0.5f, 1.0f, 0.55f, 1.0f }); // ジャンプ台（緑）
	loadTintedBlock("craftHatena",     "block_1x1x1_layer.obj",  { 1.0f, 0.8f, 0.15f, 1.0f }); // ？ブロック（金）
	loadTintedBlock("craftHatenaUsed", "block_1x1x1_layer.obj",  { 0.45f, 0.42f, 0.4f, 1.0f }); // 使用済み（灰）
	loadTintedBlock("craftCloud",      "block_1x1x1_layer.obj",  { 0.9f, 0.97f, 1.0f, 1.0f }); // すり抜け床（白）
	// 大型ブロック（road_system_4）：横長2m（進行方向）と2×2m台座
	modelManager->LoadModel("craftWide",     "resources/block/craft", "block_2x1x1_sponge.obj", true);
	modelManager->LoadModel("craftPedestal", "resources/block/craft", "block_2x2x1_layer.obj", true);
	// レール可視化用モデル（通常=緑 / 穴=赤 の2モデル。マテリアルはモデル単位で共有のため別モデルが必要）
	modelManager->CreateCubeModel("railLineCube", 1.0f);
	modelManager->CreateCubeModel("railLineCubeHole", 1.0f);

	// パーティクルグループ
	//   ※ 卵の煙／殻の飛び散りは加算パーティクルだと明るい背景で見えないため、
	//      EggSystem 側で実体(Obj3d)の小球として描画する
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");

	// テクスチャ
	TextureManager* textureManager = TextureManager::GetInstance();
	textures_["uvChecker"]     = textureManager->Load("resources/uvChecker.png");
	textures_["monsterBall"]   = textureManager->Load("resources/monsterBall.png");
	textures_["fence"]         = textureManager->Load("resources/fence.png");
	textures_["circle"]        = textureManager->Load("resources/circle.png");
	textures_["circle2"]       = textureManager->Load("resources/circle2.png");
	textures_["noise0"]        = textureManager->Load("Resources/noise0.png");
	textures_["noise1"]        = textureManager->Load("Resources/noise1.png");
	textures_["gradationLine"] = textureManager->Load("Resources/gradationLine.png");
	textures_["white"]         = textureManager->Load("resources/block/white1x1.png");
	// 道アトラスの先読み：RoadMesh はレール編集のたび（=フレーム途中）に参照するので、
	// ここでキャッシュに載せておく（実行中のテクスチャ読み込みはデバッグレイヤーが嫌うため）
	textures_["roadAtlas"]     = textureManager->Load("resources/road/road_atlas.png");
	textures_["skybox"]        = textureManager->LoadCube("resources/StandardCubeMap.dds");
	// 環境マップ（Obj3d 全般の映り込み）
	Obj3dCommon::GetInstance()->SetEnvironmentTexture(textures_["skybox"].srvIndex);

	// ブロックの一括描画を準備（見た目グループごとに1ドローコール）。
	//   モデルとテクスチャが揃った後に呼ぶ（ディゾルブ用SRVの束縛先が必要なため）
	blockSystem_.Initialize(textures_["uvChecker"].srvIndex);

	// SDF演出のパイプライン構築（産卵・舌で食べる・敵の消滅・溶け道）
	eggSystem_.InitializeBirthFx(commandList);
	swallow_.InitializeEatFx(commandList);
	combat_.InitializeDissolveFx(commandList);
	dissolveRoad_.Initialize(commandList);
	// 溶け道の調整UIを SDF パネル（SDF化の管理画面）の最下部へ差し込む
	//   ※「調整項目」ウィンドウ内から同名 Begin で追記する方式は、タブが同じドックに
	//     入ると片方しか描画されず絶対に表示されないため、フック方式で確実に出す
	SDFManager::GetInstance()->SetExtraPanelUI([this]{ dissolveRoad_.DrawImGui(); });
}

// メインカメラ／デバッグカメラの生成・登録
void GamePlayScene::SetupCameras(){
	camera_ = Camera::Create(); // ウィンドウサイズ等は内部で自動取得
	camera_->SetTranslation({ 0.0f, 2.0f, -15.0f });
	// 既定（アクティブ）カメラに設定 → 以降の Obj3d::Create は自動でこのカメラを使う
	Obj3dCommon::GetInstance()->SetDefaultCamera(camera_.get());

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();
	EditorManager::GetInstance()->SetDebugCamera(debugCamera_.get()); // メニュー「表示」でON/OFF
	EditorManager::GetInstance()->SetCamera(camera_.get());
}

// 展示物・プレイヤーの見た目・HUD（Obj3d::Create がデフォルトカメラを掴むため、必ず SetupCameras の後）
void GamePlayScene::SetupVisuals(){
	demo_.Initialize(DirectXCommon::GetInstance()->GetCommandList(), textures_);
	avatar_.Initialize(textures_["skybox"].srvIndex);
	aimThrow_.Initialize(textures_["circle2"].srvIndex); // 狙い用カーソル（構え中だけ表示）
	hud_.Initialize(textures_["circle2"].srvIndex);

	// GPUパーティクル基盤の初期化（エミッターのデモは DemoShowcase が持つ）
	GPUParticleManager::GetInstance()->Initialize(
		DirectXCommon::GetInstance(), SrvManager::GetInstance(), "resources/uvChecker.png");
}

// プレイヤー・敵エディタ・レール・各種シーン部品の用意
void GamePlayScene::SetupGameplay(){
	player_ = std::make_unique<Player>();
	player_->Initialize();
	enemyEditor_ = std::make_unique<EnemyEditor>();
	enemyEditor_->Initialize();

	// 卵・吐き出し弾がブロックと道に当たるようにする
	obstacles_.Initialize(&blockSystem_, &railField_.GetRails());
	eggSystem_.SetObstacleQuery(
		[this](const Vector3& from, const Vector3& to, float radius, Vector3& outHitPos) -> bool {
			return obstacles_.Sweep(from, to, radius, outHitPos);
		});

	// 戦闘（踏みつけ/卵命中）＋ヒット演出（エフェクト用テクスチャを渡す）
	combat_.Initialize(textures_["circle"].srvIndex, textures_["skybox"].srvIndex);

	// エフェクトの事前生成：初撃破の瞬間に Obj3d を数十個作るとゲームが固まるので、
	// ロード中のいま全部作ってプールに積んでおく。パフの数は最悪フレーム
	// （卵トレイル約13個借用中＋命中バースト8＋割れ演出6＋投げ煙6）を賄う量＝返却上限に合わせる
	combat_.Prewarm(camera_.get(), 4);
	eggSystem_.PrewarmPuffPool("fxSphere", 32);
	eggSystem_.PrewarmPuffPool("eggShell", 28);

	// ノードエディタの「→ ゲーム値」に調整値を登録する（ノードからプレイ中の挙動をリアルタイムに動かせる）。
	//   ※ポインタ登録なので、シーン終了時（Finalize）に必ず解除する
	EditorManager* editorManager = EditorManager::GetInstance();
	editorManager->ClearNodeGameValues(); // シーン再初期化時の二重登録防止
	editorManager->RegisterNodeGameValue("プレイヤー移動速度",   player_->MoveSpeedPtr(),  0.5f, 20.0f);
	editorManager->RegisterNodeGameValue("ジャンプ力",           player_->JumpPowerPtr(),  1.0f, 20.0f);
	editorManager->RegisterNodeGameValue("卵の投げ初速",         aimThrow_.ThrowSpeedNormalPtr(), 1.0f, 40.0f);
	editorManager->RegisterNodeGameValue("ロックオン投げ初速",   aimThrow_.ThrowSpeedLockPtr(),   1.0f, 60.0f);
	editorManager->RegisterNodeGameValue("飲み込みの届く距離",   swallow_.SwallowReachPtr(), 0.5f, 10.0f);
	editorManager->RegisterNodeGameValue("飲み込みクールタイム", swallow_.SwallowCooldownPtr(), 0.0f, 2.0f);

	// レールはエディタ保持の最新データから構築（編集・緑線・プレイヤーを同じデータに一本化）
	SyncRailsFromEditor();
}

// =====================================================================
//  エディタの編集をシーンへ反映
// =====================================================================
void GamePlayScene::SyncFromEditors(){
	ApplyRailRemaps();       // レールの作り直し（下のライブ同期）より先に済ませること
	SyncRailsLive();

	// ブロックのノード錨を維持（レール編集で曲線長が変わってもブロックが道に沿って滑らない）。
	//   レール同期（railField_ 再構築）の後に呼ぶこと。dist が引き直されたら blockVersion が上がり
	//   SyncBlockEdits が拾って見た目も追従する
	if ( LevelEditor* levelEditor = EditorManager::GetInstance()->GetLevelEditor() ) {
		levelEditor->GetRailEditor()->UpdateBlockAnchors(railField_.GetRails());
	}

	SyncEnemyEdits();
	SyncBlockEdits();
	HandleCameraRequests();
}

// レールの数や並びが変わっていたら（削除・その元に戻す/やり直し）、敵のレール番号を付け替える。
//   消えたレールの敵は外し、よみがえったレールの敵は戻す。コインとブロックはレールエディタ側で付け替え済み。
//   敵は敵エディタが配置の正本なのでここで行う
void GamePlayScene::ApplyRailRemaps(){
	LevelEditor* levelEditor = EditorManager::GetInstance()->GetLevelEditor();
	if ( !enemyEditor_ || !levelEditor ) return;
	RailEditor::RailRemap remap;
	while ( levelEditor->GetRailEditor()->ConsumeRailRemap(remap) ) {
		enemyEditor_->ApplyRailRemap(remap.oldToNew, EnemyLevelConvert::ToSpawnDatas(remap.revived));
		railErasedPending_ = true; // 番号が変わったので、この後のレール作り直しで距離を張り直さない
	}
}

// レールのライブ同期：エディタで編集されたら緑線とプレイヤー用データを作り直す。
//   ドラッグ中は「最大10回/秒の軽量同期」に間引き、マウスアップ後に本同期を1回行う
void GamePlayScene::SyncRailsLive(){
	EditorManager* editorManager = EditorManager::GetInstance();
	const bool dragging = editorManager->IsRailDragging();
	const bool changed = ( editorManager->GetRailEditVersion() != railField_.Version() );
	railSyncTimer_ += 1.0f / 60.0f;
	if ( changed && dragging ) {
		railFullSyncPending_ = true; // ドラッグが終わったら本生成する
		if ( railSyncTimer_ >= 0.1f ) { // 10Hz
			railSyncTimer_ = 0.0f;
			SyncRailsFromEditor(true);
		}
	} else if ( !dragging && ( changed || railFullSyncPending_ ) ) {
		railFullSyncPending_ = false;
		SyncRailsFromEditor(false);
	}
}

void GamePlayScene::SyncEnemyEdits(){
	if ( !enemyEditor_ ) return;
	EditorManager* editorManager = EditorManager::GetInstance();

	// マップが読み込まれたら、保存済みの敵配置をエディタへ復元する。
	//   敵ゼロのマップでも必ず反映する（以前は空だとスキップ→前マップの敵が残留し、
	//   そのまま保存すると別マップの敵が紛れ込むバグがあった）
	int mapLoadVersion = editorManager->GetMapLoadVersion();
	if ( mapLoadVersion != lastMapLoadVersion_ ) {
		lastMapLoadVersion_ = mapLoadVersion;
		enemyEditor_->SetSpawnDatas(EnemyLevelConvert::ToSpawnDatas(editorManager->GetEditorEnemyData()));
	}

	// 追加・削除・編集があったら即リスポーン＆保存用データへ反映
	//   （実体は同じ種類なら使い回すので、数値をドラッグしている間に毎フレーム通っても軽い）
	if ( enemyEditor_->ConsumeChanged() ) {
		editorManager->SetEditorEnemyData(EnemyLevelConvert::ToLevelEnemies(enemyEditor_->GetSpawnDatas()));
		SpawnEnemies();
	}
}

// ブロックのライブ同期：ペイント配置/削除のたびにブロックだけ作り直す
//   （レール・道・敵はそのまま＝クリック連打しても軽い）
void GamePlayScene::SyncBlockEdits(){
	EditorManager* editorManager = EditorManager::GetInstance();
	int blockVersion = editorManager->GetEditorBlockVersion();
	if ( blockVersion == lastBlockVersion_ ) return;
	lastBlockVersion_ = blockVersion;
	blockSystem_.Sync(editorManager->GetEditorBlocks(), &railField_.GetRails());
	if ( player_ ) { player_->SetBlocks(&blockSystem_); }
}

void GamePlayScene::HandleCameraRequests(){
	EditorManager* editorManager = EditorManager::GetInstance();
	const auto& rails = railField_.GetRails();

	// 敵エディタの「カメラをここへ」：その敵が画面の中央に来る位置へカメラを引いて置く
	int focusIndex = -1;
	if ( enemyEditor_ && enemyEditor_->ConsumeFocusRequest(focusIndex) ) {
		const EnemySpawnData& spawnData = enemyEditor_->GetSpawnDatas()[focusIndex];
		EditorCameraUtil::FocusOnRail(*camera_, rails, spawnData.railIndex, spawnData.distance,
		                              Enemy::PickHeightOf(spawnData));
	}
	// 配置ビュー（レール展開図）の「カメラをここへ」
	int focusRail = -1; float focusDist = 0.0f, focusHeight = 0.0f;
	if ( editorTools_.ConsumeStripFocusRequest(focusRail, focusDist, focusHeight) ) {
		EditorCameraUtil::FocusOnRail(*camera_, rails, focusRail, focusDist, focusHeight);
	}
	// Blenderインポータからの「カメラに適用」要求
	Vector3 requestPos, requestRot;
	if ( BlenderImporter* importer = editorManager->GetBlenderImporter() ) {
		if ( importer->ConsumeCameraRequest(requestPos, requestRot) ) {
			camera_->SetTranslation(requestPos);
			camera_->SetRotation(requestRot);
		}
	}
	// カメラエディタからの「この画角をプレビュー」要求
	if ( LevelEditor* levelEditor = editorManager->GetLevelEditor() ) {
		if ( levelEditor->GetRailEditor()->ConsumeCameraPreviewRequest(requestPos, requestRot) ) {
			camera_->SetTranslation(requestPos);
			camera_->SetRotation(requestRot);
		}
	}
}

// レールをエディタ最新へ作り直し、敵・コイン・ブロックも配置し直す。
//   レール本体・緑線・動きは RailField が担当。敵は RailField の責務外なので Sync 後に生成する。
//   simple=true はドラッグ中の軽量同期：道は簡易リボンのみ・敵の張り直しや再生成は行わない
//   （マウスアップ後に simple=false の本同期が1回走って最終形になる）
void GamePlayScene::SyncRailsFromEditor(bool simple){
	EditorManager* editorManager = EditorManager::GetInstance();
	const uint32_t whiteTex = textures_.count("white") ? textures_["white"].srvIndex : 0;

	if ( simple ) {
		railField_.Sync(camera_.get(), whiteTex);                     // レール本体＋緑線
		roadMesh_.Build(railField_.GetRails(), camera_.get(), true);  // 道は簡易プレビュー
		return;
	}

	// 敵のピン留め：編集前のレールで各敵のワールド位置を覚え、作り直した後にその場所へ張り直す。
	//   ※マップ読込直後のフレームは対象外：今の敵配置は旧マップの敵なので、
	//     張り直して保存データへ書き戻すと読込した新マップの敵配置を潰してしまう。
	//   ※レール番号が変わった直後も対象外：作り直す前のレール配列とは番号が合わない
	const bool repin = enemyEditor_
		&& ( editorManager->GetMapLoadVersion() == lastMapLoadVersion_ ) && !railErasedPending_;
	railErasedPending_ = false;
	EnemyRailPin pin;
	if ( repin ) {
		// 動くレールのプレビュー中でも、置いた場所（基準の位置）で覚える。
		//   このすぐ後の Sync でどのみち基準の位置へ戻るので、先に戻しても見た目は変わらない
		railField_.ResetMotion();
		pin.Capture(*enemyEditor_, railField_.GetRails());
	}

	railField_.Sync(camera_.get(), whiteTex);               // レール本体＋緑線を作り直す
	roadMesh_.Build(railField_.GetRails(), camera_.get());  // レール下の道メッシュも敷き直す
	dissolveRoad_.Build(railField_.GetRails());             // SDF溶け道のパネル敷設点も打ち直す

	if ( repin ) {
		pin.Apply(*enemyEditor_, railField_.GetRails());
		// マップ保存用データにも張り直した距離を反映（保存した時に位置がズレないように）
		editorManager->SetEditorEnemyData(EnemyLevelConvert::ToLevelEnemies(enemyEditor_->GetSpawnDatas()));
	}
	SpawnEnemies();

	// マップのスタート地点をプレイヤーへ（Initialize/リスポーンで使われる）
	if ( player_ ) { player_->SetSpawn(railField_.GetStartRail(), railField_.GetStartDistance()); }
	stageFlow_.Reset(); // マップが変わったらゴール状態はリセット

	// カメラ演出ゾーン・コイン・ブロックもエディタの配置から作り直す
	camCtrl_.Sync(editorManager->GetEditorCameraZones(), railField_.GetRails());
	coinSystem_.Sync(editorManager->GetEditorCoins(), railField_.GetRails());
	blockSystem_.Sync(editorManager->GetEditorBlocks(), &railField_.GetRails());
	lastBlockVersion_ = editorManager->GetEditorBlockVersion();
	if ( player_ ) { player_->SetBlocks(&blockSystem_); }
}

void GamePlayScene::SpawnEnemies(){
	if ( !enemyEditor_ ) { enemyMgr_.Clear(); return; }
	enemyMgr_.Spawn(enemyEditor_->GetSpawnDatas(), railField_.GetRails());
}

// =====================================================================
//  更新
// =====================================================================
void GamePlayScene::Update(){
	hitFeel_.UpdateHitStop();     // 踏みつけ等のヒットストップ
	SyncFromEditors();            // エディタ編集（レール／敵／カメラ要求）をシーンへ反映
	UpdateCameraAndPostEffect();  // カメラ更新＋シェイク＋踏みつけポストエフェクト

	EngineMode currentMode = EditorManager::GetInstance()->GetMode();
	HandleModeTransition(currentMode);
	if ( currentMode == EngineMode::Play ) { UpdatePlayMode(); }
	UpdateSceneVisuals(currentMode);

	// 初めて敵を倒した瞬間のフリーズを無くすため、起動直後に演出を一度通しておく
	screenFx_.UpdateWarmup(combat_, currentMode == EngineMode::Play);

	// タイトルへ戻る
	if ( Input::GetInstance()->Triggerkey(DIK_T) ) {
		SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
	}
}

// カメラ更新（デバッグカメラ＋ヒット時のシェイク）とヒット点中心のポストエフェクト
void GamePlayScene::UpdateCameraAndPostEffect(){
	if ( debugCamera_ ) { debugCamera_->Update(camera_.get()); }
	hitFeel_.ApplyCameraShake(camera_.get());          // ヒット時に一瞬揺らす
	camera_->Update();
	hitFeel_.UpdateImpactPostEffect(camera_.get());    // カメラ確定後にスクリーン投影する
}

// Edit↔Play の切り替わり瞬間のリセット処理
void GamePlayScene::HandleModeTransition(EngineMode current){
	if ( prevMode_ == EngineMode::Edit && current == EngineMode::Play ) { OnPlayStart(); }
	if ( prevMode_ == EngineMode::Play && current == EngineMode::Edit ) { OnEditStart(); }
	prevMode_ = current;
}

// エディット → プレイ：最新レールで確定し、プレイヤーをスタートへ、エフェクト・卵を消す
void GamePlayScene::OnPlayStart(){
	screenFx_.EnablePlayLook(); // クラフト世界観の常時ポストエフェクト（プレイ中だけ）
	SyncRailsFromEditor();
	// 右クリック「ここからテストプレイ」：この1回だけ開始位置を上書きする
	//   （SyncRailsFromEditor が通常スタートを SetSpawn した後に上書き。
	//     プレイ中に落ちた時のリスポーンもこの地点になる＝そこだけ何度も試せる）
	editorTools_.ConsumeTestPlayRequest(testPlayRail_, testPlayDist_);
	if ( testPlayRail_ >= 0 && testPlayRail_ < ( int ) railField_.GetRails().size() ) {
		player_->SetSpawn(testPlayRail_, testPlayDist_);
	}
	player_->Initialize();
	player_->SetMovementLocked(false);
	camCtrl_.Reset();        // 向き切替トリガーの状態を初期化（前回プレイの向きを持ち越さない）
	demo_.OnPlayStart();     // デモの HitEffect を消す
	combat_.ClearEffects();
	eggSystem_.Initialize();
	aimThrow_.Reset();       // 構え状態を解除
	swallow_.Reset();        // 舌アクションを解除（敵が作り直されるので target_ を確実に手放す）
	coinSystem_.ResetPlay(); // コインを全部復活させる
	blockSystem_.ResetPlay(); // ？ブロックを未使用に戻す
}

// プレイ → エディット：動くレールを基準位置に戻す（編集と表示を一致させる）
void GamePlayScene::OnEditStart(){
	screenFx_.DisablePlayLook();
	railField_.ResetMotion();
	testPlayRail_ = -1;      // テストプレイの開始位置上書きは1回で解除（次は通常スタート）
	coinSystem_.ResetPlay(); // 取得済みコインを復活（エディタで配置が見えなくならないように）
	// カメラ関連の後始末：
	//   ・回転フリーズ中に Stop しても時間が止まったままにならないよう Reset（内部で TimeScale を戻す）
	//   ・ゾーンで視野角を変えたまま戻るとエディタ画面が広角/望遠のままになるので標準に戻す
	camCtrl_.Reset();
	camera_->SetFovY(0.78f);
}

// プレイ中（時間が動いている時）のゲーム進行
void GamePlayScene::UpdatePlayMode(){
	const float deltaTime = Time::GetInstance()->GetDeltaTime();

	// 動くレール → プレイヤー → 敵 の順で更新（位置の整合のため）。
	//   「乗ったら動き出す」レールの発動判定用に、接地中のプレイヤーのレール番号を渡す
	const int ridingRail = player_->IsGrounded() ? player_->GetCurrentRail() : -1;
	railField_.UpdateMotion(deltaTime, ridingRail);
	railField_.DrawWaitingLiftMarkers(deltaTime);

	// カメラの向きを渡す：向き切替（180°回り込み等）の後も「Dで画面の右へ」進めるように、
	// プレイヤー側でキー→ワールド方向の割り当てを回す
	player_->SetCameraYaw(camera_->GetRotation().y);
	// ベロを出している間は振り向き禁止（移動は可）。出したまま反転して見た目が破綻するのを防ぐ
	player_->SetTurnLocked(swallow_.IsTongueActive());
	// 卵の構え中は狙い（カーソル）方向を向く（後ろ狙いなら振り向く）
	player_->SetFaceOverride(aimThrow_.IsAiming(), aimThrow_.GetAimYaw());
	// 構え中は後ろの列の先頭の卵を非表示（手に持っている扱い。キャンセルで列に戻る）
	eggSystem_.SetAimHolding(aimThrow_.IsAiming());
	// 構え中はベロ(E)と産卵(左Ctrl)を発動禁止（両手が卵でふさがっている）
	swallow_.SetActionBlocked(aimThrow_.IsAiming());
	player_->Update(railField_.GetRails());

	// 敵の移動＋吸い込みTick。吸い込み完了 → お腹に+1（口元で緑がふわっと）。
	//   ブロックを渡す＝パトロールの敵が壁で引き返す（貫通防止）
	enemyMgr_.Update(railField_.GetRails(), player_->GetPosition(), deltaTime, [&](const Vector3& pos){
		eggSystem_.AddToBelly();
		eggSystem_.SpawnSwallowFx(pos);
	}, &blockSystem_);

	combat_.Update(*player_, enemyMgr_, eggSystem_, hitFeel_, camera_.get());               // 当たり判定＋踏みつけ
	swallow_.Update(*player_, enemyMgr_, eggSystem_, hitFeel_, camera_.get(), deltaTime);  // 舌・産卵
	aimThrow_.Update(*player_, enemyMgr_, eggSystem_, camera_.get(), deltaTime);           // 卵の構え・投げ

	// 卵の追従・飛行・割れの更新
	float yaw = player_->GetRotation().y;
	eggSystem_.Update(player_->GetPosition(), { std::sin(yaw), 0.0f, std::cos(yaw) }, deltaTime);

	// ゴール判定＋ゴールマーカー
	stageFlow_.Update(player_->GetPosition(), railField_, hitFeel_);

	// プレイ中カメラ（プレイヤー追従＋カメラ演出ゾーン）
	camCtrl_.Update(camera_.get(), player_->GetPosition(), railField_.GetRails(),
		debugCamera_ && debugCamera_->IsActive(), deltaTime);
	hitFeel_.NotifyCameraOverridden(); // カメラ位置を上書きしたのでシェイクの自己相殺をリセット

	// デモ入力（Space=BGM+HitEffect / P=パーティクル）。デモ表示OFF（表示メニュー）の間は入力ごと無効
	if ( EditorManager::GetInstance()->IsDemoVisible() ) {
		demo_.UpdatePlay(Input::GetInstance(), camera_.get(), textures_, bgmFile_);
	}

	// エフェクトの更新（timeScale 適用 deltaTime → ヒットストップで一緒に止まる）
	combat_.UpdateEffects(deltaTime);
}

// モードに関わらず毎フレーム行う見た目の更新
void GamePlayScene::UpdateSceneVisuals(EngineMode mode){
	EditorManager* editorManager = EditorManager::GetInstance();
	const bool playing = ( mode == EngineMode::Play );
	const Vector3 playerPos = player_->GetPosition();

	// Editモード中も敵の見た目（WVP行列）を毎フレーム更新する。
	//   Obj3d::Update は「呼ばれた時のカメラ行列」を焼き込むため、これが無いと
	//   デバッグカメラを動かした時に敵が古い行列のまま描かれ、画面に貼り付いて付いてくる。
	//   dt=0 で呼ぶのでパトロール等の移動は起きない
	if ( !playing ) { enemyMgr_.Update(railField_.GetRails(), playerPos, 0.0f, nullptr, &blockSystem_); }

	// カメラ演出ゾーンの可視化（球=発動範囲 / 白い箱=カメラ位置の目安。編集中も見える）
	camCtrl_.DrawZoneMarkers(railField_.GetRails());

	// レールと道
	railField_.UpdateMarkers(); // レール緑線（カメラ移動に追従）
	roadMesh_.SetJointVisible(editorManager->GetEditorJointVisible());
	// 動くレールのエディタプレビュー：Playを押さなくても動きを再生して組み方を確認できる
	if ( mode == EngineMode::Edit ) { railField_.UpdateEditorPreview(editorManager->GetEditorRailMotionPreview()); }
	roadMesh_.Update(railField_.GetRails());              // 道メッシュ（動くレール追従＋カメラ追従）
	dissolveRoad_.Update(playerPos, 1.0f / 60.0f);        // SDF溶け道の現れ/溶け（エディタ中も動きが見える）

	// コイン（回転・浮遊はエディタ中も見せる。取得判定はプレイ中のみ）とブロック
	coinSystem_.Update(playerPos, playing, 1.0f / 60.0f);
	blockSystem_.Update();

	// 落下ミス→リスポーン：アイリスワイプで閉じて→開く
	if ( playing ) {
		if ( player_->ConsumeFellRespawn() ) {
			screenFx_.StartMissIris();
			// 動くレールを基準位置へ戻す（「片道で行ったきり」の列車が戻らず詰むのを防ぐ。
			//   「乗ったら動き出す」は待機に、「出現する道」は消えた状態に戻る＝区間をやり直せる）
			railField_.ResetMotion();
		}
		screenFx_.UpdateIris({ playerPos.x, playerPos.y + 0.8f, playerPos.z }, *camera_); // 円の中心＝胸元
	}

	// ？ブロックから出たコイン：カウント加算＋金色の粒が飛び出す演出
	Vector3 bumpCoinPos {};
	while ( blockSystem_.ConsumeBumpCoin(bumpCoinPos) ) {
		coinSystem_.AddBonus(1);
		for ( int i = 0; i < 6; ++i ) {
			float angle = ( float ) i / 6.0f * 6.2831853f;
			Vector3 vel = { std::cos(angle) * 1.5f, 3.5f + ( float ) ( i % 3 ) * 0.6f, std::sin(angle) * 1.5f };
			eggSystem_.SpawnPuff(bumpCoinPos, vel, { 1.0f, 0.85f, 0.2f, 1.0f }, 0.12f, 0.5f, "fxSphere");
		}
	}

	// プレイヤーの見た目（Play中は追従、Edit中はスタート地点のプレビュー）
	PlayerAvatar::Context avatarContext;
	avatarContext.playing   = playing;
	avatarContext.player    = player_.get();
	avatarContext.railField = &railField_;
	avatarContext.swallow   = &swallow_;
	avatarContext.aimThrow  = &aimThrow_;
	avatarContext.eggSystem = &eggSystem_;
	avatar_.Update(avatarContext);

	// SDF看板の近接表示：プレイ中はプレイヤー位置を基準に「近づいた時だけ表示」が効く。
	// エディット中は配置作業ができるよう常に全表示（Clear）
	if ( playing ) { SDFManager::GetInstance()->SetViewerPosition(playerPos); }
	else           { SDFManager::GetInstance()->ClearViewerPosition(); }

	hud_.Update(eggSystem_, aimThrow_.IsAiming(), coinSystem_);

	PostEffect::GetInstance()->Update();
	ParticleManager::GetInstance()->Update(camera_.get());

	// 展示物の見た目更新（デモ表示OFFの間はスキップ）
	if ( editorManager->IsDemoVisible() ) { demo_.UpdateVisuals(Input::GetInstance(), camera_.get()); }
}

// =====================================================================
//  描画
// =====================================================================
void GamePlayScene::Draw(){
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();
	GPUParticleManager::GetInstance()->Dispatch(commandList);

	PostEffect::GetInstance()->PreDrawSceneMRT(commandList);   // MRT開始
	DrawScene3D(commandList);
	PostEffect::GetInstance()->PostDrawSceneMRT(commandList);  // MRT終了（2枚のキャンバスを読み込みモードへ）

	ComposeFrame(commandList);
	DrawScreenUI(commandList);
}

// 3D一式を MRT（色＋マスク）へ描く。不透明 → インスタンシング → 透明・加算 → SDF → デバッグ線 の順
void GamePlayScene::DrawScene3D(ID3D12GraphicsCommandList* commandList){
	EditorManager* editorManager = EditorManager::GetInstance();
	const bool demoVisible = editorManager->IsDemoVisible(); // デモ展示のON/OFF（表示メニュー）
	const bool playing = ( editorManager->GetMode() == EngineMode::Play );

	Obj3dCommon::GetInstance()->PreDraw(commandList);
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(9, textures_["skybox"].srvIndex);

	// --- 不透明 ---
	if ( demoVisible ) { demo_.DrawOpaque(); } // 展示物（回転キューブ・スキンメッシュ）
	avatar_.Draw(playing, player_.get());      // プレイヤー（無敵中は点滅）＋手持ちの卵
	enemyMgr_.Draw();
	eggSystem_.Draw();
	coinSystem_.Draw();
	swallow_.Draw();                           // 舌（伸ばす/引き込む動作中だけ）
	roadMesh_.Draw();                          // 道メッシュ（Edit/Play どちらでも見える本番の見た目）
	// レール経路の緑線マーカーは「エディット中だけ」（Play中・リリース版では道メッシュだけが残る）
	if ( editorManager->GetMode() == EngineMode::Edit ) { railField_.DrawMarkers(); }
	editorManager->Draw();

	// --- インスタンシング ---
	blockSystem_.Draw(camera_.get()); // 配置ブロック（見た目グループごとに1ドローコール）

	// --- 透明・加算合成（順番が大事：不透明を全部描き切った後）---
	if ( demoVisible ) { demo_.DrawAdditive(); } // オーラ2種＋SpaceデモのHitEffect
	combat_.Draw();                              // 踏みつけ/命中の立体エフェクト
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);
	GPUParticleManager::GetInstance()->Draw(commandList);

	// --- SDFボリューム（専用PSOに切り替えるので、通常のObj3d描画が全部終わった後に描く）---
	if ( demoVisible ) { demo_.DrawSdf(commandList); } // SDF卵のエロージョン/モーフデモ
	eggSystem_.DrawBirthFx(commandList);   // 産卵エロージョン演出中のSDF卵
	combat_.DrawDissolveFx(commandList);   // 倒された敵がSDFで溶けて消える演出
	dissolveRoad_.Draw(commandList);       // SDF溶け道
	swallow_.DrawEatFx(commandList);       // 舌で捕まえた敵がSDFで溶けて消える演出
	SDFManager::GetInstance()->DrawVolumes(commandList); // エディタで配置した3Dボリューム

	// デバッグ描画：MRT（シーンRT）内で線を描く → ポストエフェクト/Bloomを通って
	//   Game View にも単体表示にも反映される（深度テストありで3D形状に隠れる）
	if ( showDebugGrid_ ) {
		DebugDraw::GetInstance()->Grid(20.0f, 1.0f, { 0.3f, 0.3f, 0.35f, 0.5f }, 0.0f);
	}
	DebugDraw::GetInstance()->Render(camera_.get());
}

// ポストエフェクト → Bloom → SDF（文字/画像）の焼き込み → バックバッファへ最終出力
void GamePlayScene::ComposeFrame(ID3D12GraphicsCommandList* commandList){
	PostEffect* postEffect = PostEffect::GetInstance();
	Bloom* bloom = Bloom::GetInstance();

	postEffect->Draw(commandList, false); // バックバッファへの最終出力は FinalBlit に任せる
	bloom->Render(commandList, postEffect->GetSrvIndex(), postEffect->GetMaskSrvIndex()); // 「色」と「マスク」
	uint32_t finalSrv = bloom->GetResultSrvIndex();

	// SDF（文字/画像）を最終画像に焼き込む → エディタの Game View にもそのまま映る。
	//   Bloom有効時は合成RT、無効時は PostEffect の最終RTが finalSrv の実体なので、
	//   どちらの場合も「FinalBlit が読むテクスチャ」へ焼き込めばフルスクリーンにも映る
	RenderTexture* sdfTarget = bloom->IsEnabled() ? bloom->GetCombineTexture() : postEffect->GetFinalTexture();
	SDFManager::GetInstance()->DrawIntoTexture(commandList, sdfTarget);

	// エディタに最終的なゲーム画面のSRVを渡す（Game View 表示用）
	EditorManager::GetInstance()->SetGameViewSrvIndex(finalSrv);
	// 最終結果をバックバッファへ（エディタアクティブ時はRTVのセットのみ行い描画はスキップ）
	postEffect->FinalBlit(commandList, finalSrv, EditorManager::GetInstance()->IsActive());
}

// スプライト・HUD
void GamePlayScene::DrawScreenUI(ID3D12GraphicsCommandList* commandList){
	SpriteCommon::GetInstance()->PreDraw(commandList);
	aimThrow_.DrawSprite(); // 構え中だけ狙いカーソル
	// 卵・コインのHUD（プレイ中のみ。エディット中は編集の邪魔になるので出さない）
	if ( EditorManager::GetInstance()->GetMode() == EngineMode::Play ) { hud_.Draw(commandList); }
	TextManager::GetInstance()->Draw();
}

// =====================================================================
//  デバッグUI（ImGui）
// =====================================================================
void GamePlayScene::DrawDebugUI(){
#ifdef USE_IMGUI
	EditorManager* editorManager = EditorManager::GetInstance();

	// 共有の「インスペクター (詳細設定)」へ合流する組（アイコンモードでは詳細パネルOFF時に丸ごと省略）
	if ( editorManager->IsPanelVisible(EditorManager::Panel_Inspector) ) { DrawInspectorUI(); }
	// 展示物のパネル（SDF卵のエロージョン操作）。デモ表示OFFの間はウィンドウごと出さない
	if ( editorManager->IsDemoVisible() ) { demo_.DrawImGui(); }
	stageFlow_.DrawDebugUI(); // ゴール到達表示

	DrawHitShapes();

	SceneEditContext editContext = MakeEditContext();
	editorTools_.DrawPanels(editContext);
	editorTools_.UpdateGameView(editContext);
#endif
}

SceneEditContext GamePlayScene::MakeEditContext(){
	EditorManager* editorManager = EditorManager::GetInstance();
	SceneEditContext context;
	context.editor      = editorManager;
	context.gameView    = &editorManager->GetGameViewMouse();
	context.rails       = &railField_.GetRails();
	context.railField   = &railField_;
	context.levelEditor = editorManager->GetLevelEditor();
	context.railEditor  = context.levelEditor ? context.levelEditor->GetRailEditor() : nullptr;
	context.enemyEditor = enemyEditor_.get();
	context.blockSystem = &blockSystem_;
	context.coinSystem  = &coinSystem_;
	context.camera      = camera_.get();
	context.player      = player_.get();
	context.editing     = ( editorManager->GetMode() == EngineMode::Edit );
	return context;
}

// 当たり判定の形をワイヤーで表示（パネルを閉じていても、チェックが入っていれば出す）
void GamePlayScene::DrawHitShapes(){
	if ( !showHitShapes_ ) return;
	for ( auto& enemy : enemyMgr_.GetEnemies() ) { enemy->DrawHitShape({ 1.0f, 0.3f, 0.3f, 1.0f }); }
	combat_.DrawPlayerHitShape(*player_, { 0.3f, 1.0f, 0.4f, 1.0f });
	blockSystem_.DrawHitShapes({ 0.3f, 0.85f, 1.0f, 0.8f });
	eggSystem_.DrawHitShapes({ 1.0f, 0.9f, 0.2f, 1.0f });
}

void GamePlayScene::DrawInspectorUI(){
#ifdef USE_IMGUI
	Obj3dCommon::GetInstance()->DrawDebugUI();
	camera_->DrawDebugUI();
	debugCamera_->DrawDebugUI();
	ParticleManager::GetInstance()->DrawDebugUI();
	TextManager::GetInstance()->DrawDebugUI();

	ImGui::Begin("インスペクター (詳細設定)");

	if ( ImGui::CollapsingHeader("レール表示・カメラ視点 (Rail Debug)") ) {
		bool showMarkers = railField_.ShowMarkers();
		if ( ImGui::Checkbox("レール経路を表示", &showMarkers) ) { railField_.SetShowMarkers(showMarkers); }
		camCtrl_.DrawDebugUI(); // プレイ中カメラ（プレイヤー追従＋カメラ演出ゾーン）
		ImGui::Text("マーカー数: %d", railField_.MarkerCount());
		if ( ImGui::Button("マーカー再構築") ) { railField_.RebuildMarkers(); }

		// --- 道の設定（危険帯の長さ／両面描画／再生成）---
		ImGui::Separator();
		ImGui::TextDisabled("道の設定:");
		bool roadVisible = roadMesh_.IsVisible();
		if ( ImGui::Checkbox("道を表示", &roadVisible) ) { roadMesh_.SetVisible(roadVisible); }
		bool cullNone = roadMesh_.IsCullNone();
		if ( ImGui::Checkbox("両面描画（OFF=背面カリングで軽量化）", &cullNone) ) {
			roadMesh_.SetCullNone(cullNone); // 即時反映（再生成不要）
		}
		int cornerStyle = roadMesh_.GetCornerStyle();
		const char* cornerStyleLabels[] = { "自動（角度で判定）", "いつも丸広場（ヨッシー風）", "丸なし（角ばり）" };
		ImGui::SetNextItemWidth(200.0f);
		if ( ImGui::Combo("曲がり角の形", &cornerStyle, cornerStyleLabels, 3) ) {
			roadMesh_.SetCornerStyle(cornerStyle);
			roadMesh_.Build(railField_.GetRails(), camera_.get()); // 選んだ瞬間に道を作り直して反映
		}
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip("レールが曲がって繋がる角の見た目：\n 自動＝鋭い角はマイター、大きく回る角は丸広場\n いつも丸広場＝全部の角に丸い広場を出す\n 丸なし＝丸広場を出さず角ばった接続にする");
		float warnLength = roadMesh_.GetWarnLength();
		ImGui::SetNextItemWidth(160.0f);
		if ( ImGui::SliderFloat("危険帯の長さ(m)", &warnLength, 0.5f, 5.0f, "%.1f") ) {
			roadMesh_.SetWarnLength(warnLength);
		}
		// スライダーを離した時に道を作り直して反映（ドラッグ中の連続再生成はしない）
		if ( ImGui::IsItemDeactivatedAfterEdit() ) { roadMesh_.Build(railField_.GetRails(), camera_.get()); }
		ImGui::SameLine();
		if ( ImGui::Button("道を再生成") ) {
			EditorManager* editorManager = EditorManager::GetInstance();
			roadMesh_.Build(railField_.GetRails(), camera_.get());
			dissolveRoad_.Build(railField_.GetRails());
			coinSystem_.Sync(editorManager->GetEditorCoins(), railField_.GetRails());
			blockSystem_.Sync(editorManager->GetEditorBlocks(), &railField_.GetRails());
		}
		ImGui::Text("SDF溶け道: チェーン点 %d / 描画チャンク %d", dissolveRoad_.PieceCount(), dissolveRoad_.ActiveCount());
		ImGui::SetNextItemWidth(160.0f);
		ImGui::SliderFloat("プレイヤーモデル高さ補正(m)", avatar_.ModelYOffsetPtr(), -0.6f, 0.6f, "%.2f");
		if ( ImGui::IsItemHovered() ) ImGui::SetTooltip("プレイヤーの足元と道の上面が合うように調整（マイナスで下がる）");
		ImGui::Text("道メッシュ/ピース数: %d", roadMesh_.TileCount());
		ImGui::Text("道の頂点数: %d / 三角形: %d", roadMesh_.VertexCount(), roadMesh_.TriangleCount());

		// --- カメラ視点プリセット（レールを編集しやすく）---
		ImGui::Separator();
		ImGui::TextDisabled("カメラ視点プリセット:");
		if ( ImGui::Button("トップビュー（真上から）") ) { EditorCameraUtil::TopView(*camera_, railField_.GetRails()); }
		ImGui::SameLine();
		if ( ImGui::Button("斜め視点に戻す") ) { EditorCameraUtil::DefaultAngle(*camera_); }
		ImGui::TextDisabled("※デバッグカメラONなら右ドラッグで自由に回せます");
	}

	// デバッグ描画（DebugDraw）の表示設定
	if ( ImGui::CollapsingHeader("デバッグ描画 (DebugDraw)") ) {
		ImGui::Checkbox("グリッドを表示", &showDebugGrid_);
		ImGui::Checkbox("当たり判定を表示", &showHitShapes_);
		if ( ImGui::IsItemHovered() ) {
			ImGui::SetTooltip("敵（赤）・プレイヤー（緑）・ブロック（水色）・飛んでいる卵（黄）の\n"
				"当たり判定の形を線で表示する。見た目とずれていないかの確認用");
		}
		ImGui::TextDisabled("Box/Sphere/Line はコードから積む。Game View にも表示されます");
	}
	// 当たり判定のふるまい
	if ( ImGui::CollapsingHeader("当たり判定 (Collision)") ) {
		bool contactKnockback = combat_.IsContactKnockback();
		if ( ImGui::Checkbox("敵に横からぶつかると弾かれる", &contactKnockback) ) {
			combat_.SetContactKnockback(contactKnockback);
		}
		ImGui::TextDisabled("OFF にすると敵をすり抜ける（踏みつけ・卵・舌は OFF でも当たる）");
	}
	ImGui::End();
#endif
}

// =====================================================================
//  終了
// =====================================================================
void GamePlayScene::Finalize(){
	EditorManager* editorManager = EditorManager::GetInstance();
	// エディタが保持している外部ポインタをリセット（ダングリングポインタ防止）
	editorManager->ResetSceneReferences();
	// ノードエディタに登録したゲーム値（player_ 等のポインタ）も解除する
	editorManager->ClearNodeGameValues();
	// SDFパネルへ差し込んだ溶け道設定UI（this をキャプチャ）も解除する
	SDFManager::GetInstance()->SetExtraPanelUI(nullptr);

	GPUParticleManager::GetInstance()->Finalize();
	textures_.clear();
}
