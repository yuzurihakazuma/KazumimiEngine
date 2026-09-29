#include "GamePlayScene.h"
// --- ゲーム固有のファイル ---
#include "TitleScene.h"
#include "game/player/Player.h"
#include "game/enemy/EnemyEditor.h"
#include "game/enemy/EnemyLevelConvert.h"
#include "game/enemy/EnemyRailPin.h"
#include "game/camera/EditorCameraUtil.h"
#include "game/scene/GamePlayAssets.h"
#include "game/demo/DemoShowcase.h"
#include "game/editor/PlaySceneInspector.h"

// --- エンジン側のファイル ---
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

GamePlayScene::GamePlayScene(){
	features_.demoShowcase = true; // エンジン機能の展示を出す
	features_.railEditing  = true; // レール・道・敵の配置を編集する
}
GamePlayScene::~GamePlayScene() = default;

// =====================================================================
//  初期化：読み込み（BaseScene がカメラを作る前）→ 見た目 → ゲーム部品
// =====================================================================
// BGM・SE・モデル・テクスチャの読み込みと、SDF演出のパイプライン構築
void GamePlayScene::OnLoadResources(){
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();

	// 音・モデル・テクスチャ（ゲーム中に名前で引かれる物は全部ここで先に読む）
	GamePlayAssets::Load(bgmFile_, textures_);

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

// カメラ・共通機能の用意が済んだ後（Obj3d::Create がデフォルトカメラを掴むため）
void GamePlayScene::OnInitialize(){
	avatar_.Initialize(textures_["skybox"].srvIndex);
	aimThrow_.Initialize(textures_["circle2"].srvIndex); // 狙い用カーソル（構え中だけ表示）
	hud_.Initialize(textures_["circle2"].srvIndex);
	SetupGameplay();
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
	combat_.Prewarm(GetCamera(), 4);
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
		EditorCameraUtil::FocusOnRail(*GetCamera(), rails, spawnData.railIndex, spawnData.distance,
		                              Enemy::PickHeightOf(spawnData));
	}
	// 配置ビュー（レール展開図）の「カメラをここへ」
	int focusRail = -1; float focusDist = 0.0f, focusHeight = 0.0f;
	if ( editorTools_.ConsumeStripFocusRequest(focusRail, focusDist, focusHeight) ) {
		EditorCameraUtil::FocusOnRail(*GetCamera(), rails, focusRail, focusDist, focusHeight);
	}
	// カメラエディタからの「この画角をプレビュー」要求（Blenderからのカメラ要求は BaseScene が受け持つ）
	Vector3 requestPos, requestRot;
	if ( LevelEditor* levelEditor = editorManager->GetLevelEditor() ) {
		if ( levelEditor->GetRailEditor()->ConsumeCameraPreviewRequest(requestPos, requestRot) ) {
			GetCamera()->SetTranslation(requestPos);
			GetCamera()->SetRotation(requestRot);
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
		railField_.Sync(GetCamera(), whiteTex);                     // レール本体＋緑線
		roadMesh_.Build(railField_.GetRails(), GetCamera(), true);  // 道は簡易プレビュー
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

	railField_.Sync(GetCamera(), whiteTex);               // レール本体＋緑線を作り直す
	roadMesh_.Build(railField_.GetRails(), GetCamera());  // レール下の道メッシュも敷き直す
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
void GamePlayScene::OnPreUpdate(){
	hitFeel_.UpdateHitStop();     // 踏みつけ等のヒットストップ
	SyncFromEditors();            // エディタ編集（レール／敵／カメラ要求）をシーンへ反映
}

void GamePlayScene::OnUpdate(){
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
void GamePlayScene::UpdateCamera(){
	if ( GetDebugCamera() ) { GetDebugCamera()->Update(GetCamera()); }
	hitFeel_.ApplyCameraShake(GetCamera());          // ヒット時に一瞬揺らす
	GetCamera()->Update();
	hitFeel_.UpdateImpactPostEffect(GetCamera());    // カメラ確定後にスクリーン投影する
}

// SDF看板の近接表示：プレイ中はプレイヤー位置を基準に「近づいた時だけ表示」が効く
//   （エディット中は配置作業ができるよう常に全表示）
bool GamePlayScene::GetSdfViewerPosition(Vector3& outPos) const{
	if ( EditorManager::GetInstance()->GetMode() != EngineMode::Play || !player_ ) return false;
	outPos = player_->GetPosition();
	return true;
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
	if ( Demo() ) { Demo()->OnPlayStart(); } // デモの HitEffect を消す
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
	GetCamera()->SetFovY(0.78f);
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
	player_->SetCameraYaw(GetCamera()->GetRotation().y);
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

	combat_.Update(*player_, enemyMgr_, eggSystem_, hitFeel_, GetCamera());               // 当たり判定＋踏みつけ
	swallow_.Update(*player_, enemyMgr_, eggSystem_, hitFeel_, GetCamera(), deltaTime);  // 舌・産卵
	aimThrow_.Update(*player_, enemyMgr_, eggSystem_, GetCamera(), deltaTime);           // 卵の構え・投げ

	// 卵の追従・飛行・割れの更新
	float yaw = player_->GetRotation().y;
	eggSystem_.Update(player_->GetPosition(), { std::sin(yaw), 0.0f, std::cos(yaw) }, deltaTime);

	// ゴール判定＋ゴールマーカー
	stageFlow_.Update(player_->GetPosition(), railField_, hitFeel_);

	// プレイ中カメラ（プレイヤー追従＋カメラ演出ゾーン）
	camCtrl_.Update(GetCamera(), player_->GetPosition(), railField_.GetRails(),
		GetDebugCamera() && GetDebugCamera()->IsActive(), deltaTime);
	hitFeel_.NotifyCameraOverridden(); // カメラ位置を上書きしたのでシェイクの自己相殺をリセット

	// デモ入力（Space=BGM+HitEffect / P=パーティクル）。デモ表示OFF（表示メニュー）の間は入力ごと無効
	if ( IsDemoVisible() ) {
		Demo()->UpdatePlay(Input::GetInstance(), GetCamera(), textures_, bgmFile_);
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
		screenFx_.UpdateIris({ playerPos.x, playerPos.y + 0.8f, playerPos.z }, *GetCamera()); // 円の中心＝胸元
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

	hud_.Update(eggSystem_, aimThrow_.IsAiming(), coinSystem_);
}

// =====================================================================
//  描画
// =====================================================================
// 不透明（Obj3d の準備・環境マップの束縛は BaseScene が済ませている）
void GamePlayScene::OnDrawOpaque(ID3D12GraphicsCommandList* /*commandList*/){
	const bool playing = ( EditorManager::GetInstance()->GetMode() == EngineMode::Play );
	avatar_.Draw(playing, player_.get());      // プレイヤー（無敵中は点滅）＋手持ちの卵
	enemyMgr_.Draw();
	eggSystem_.Draw();
	coinSystem_.Draw();
	swallow_.Draw();                           // 舌（伸ばす/引き込む動作中だけ）
	roadMesh_.Draw();                          // 道メッシュ（Edit/Play どちらでも見える本番の見た目）
	// レール経路の緑線マーカーは「エディット中だけ」（Play中・リリース版では道メッシュだけが残る）
	if ( !playing ) { railField_.DrawMarkers(); }
}

void GamePlayScene::OnDrawInstanced(){
	blockSystem_.Draw(GetCamera()); // 配置ブロック（見た目グループごとに1ドローコール）
}

void GamePlayScene::OnDrawTransparent(ID3D12GraphicsCommandList* /*commandList*/){
	combat_.Draw(); // 踏みつけ/命中の立体エフェクト
}

void GamePlayScene::OnDrawSdf(ID3D12GraphicsCommandList* commandList){
	eggSystem_.DrawBirthFx(commandList);   // 産卵エロージョン演出中のSDF卵
	combat_.DrawDissolveFx(commandList);   // 倒された敵がSDFで溶けて消える演出
	dissolveRoad_.Draw(commandList);       // SDF溶け道
	swallow_.DrawEatFx(commandList);       // 舌で捕まえた敵がSDFで溶けて消える演出
}

// スプライト・HUD（画面出力の後）
void GamePlayScene::OnDrawUI(ID3D12GraphicsCommandList* commandList){
	aimThrow_.DrawSprite(); // 構え中だけ狙いカーソル
	// 卵・コインのHUD（プレイ中のみ。エディット中は編集の邪魔になるので出さない）
	if ( EditorManager::GetInstance()->GetMode() == EngineMode::Play ) { hud_.Draw(commandList); }
}

// =====================================================================
//  デバッグUI（ImGui）
// =====================================================================
void GamePlayScene::OnDebugUI(){
#ifdef USE_IMGUI
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
	context.camera      = GetCamera();
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

// 共有の「インスペクター (詳細設定)」へ足す項目（レール表示・道・視点プリセット・当たり判定）
void GamePlayScene::OnDrawInspector(){
	PlaySceneInspector::Targets targets;
	targets.camera           = GetCamera();
	targets.railField        = &railField_;
	targets.roadMesh         = &roadMesh_;
	targets.dissolveRoad     = &dissolveRoad_;
	targets.cameraController = &camCtrl_;
	targets.coinSystem       = &coinSystem_;
	targets.blockSystem      = &blockSystem_;
	targets.combat           = &combat_;
	targets.avatar           = &avatar_;
	PlaySceneInspector::Draw(targets);
}

// インスペクターの「デバッグ描画」へ足す項目
void GamePlayScene::OnDrawDebugDrawOptions(){
	PlaySceneInspector::DrawHitShapeToggle(&showHitShapes_);
}

// =====================================================================
//  終了
// =====================================================================
void GamePlayScene::OnFinalize(){
	// SDFパネルへ差し込んだ溶け道設定UI（this をキャプチャ）を解除する
	//   （エディタの外部ポインタ・ノードのゲーム値・GPUパーティクルの後始末は BaseScene が行う）
	SDFManager::GetInstance()->SetExtraPanelUI(nullptr);
	textures_.clear();
}
