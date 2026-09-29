#include "game/scene/BaseScene.h"
#include "game/demo/DemoShowcase.h"

#include "engine/base/DirectXCommon.h"
#include "engine/base/Input.h"
#include "engine/camera/Camera.h"
#include "engine/camera/DebugCamera.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3dCommon.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/graphics/PipelineManager.h"
#include "engine/graphics/RenderTexture.h"
#include "engine/graphics/SrvManager.h"
#include "engine/graphics/TextureManager.h"
#include "engine/particle/GPUParticleManager.h"
#include "engine/particle/ParticleManager.h"
#include "engine/postEffect/PostEffect.h"
#include "engine/sdf/SDFManager.h"
#include "engine/utils/EditorManager.h"
#include "engine/utils/Level/BlenderImporter.h"
#include "engine/utils/TextManager.h"
#include "Bloom.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <string>
#include <unordered_map>

BaseScene::BaseScene() = default;
BaseScene::~BaseScene() = default;

// =====================================================================
//  初期化：シーンの読み込み → 環境マップ・カメラ → 共通機能 → シーンの物
// =====================================================================
void BaseScene::Initialize(){
    OnLoadResources();

    // 環境マップ（キューブマップ）。Object3D系シェーダーはキューブマップSRVを参照するため、
    //   束縛しないとスロット0の2Dテクスチャが刺さり GPU-BASED VALIDATION の次元不一致で落ちる
    envMapSrv_ = TextureManager::GetInstance()->LoadCube("resources/StandardCubeMap.dds").srvIndex;
    Obj3dCommon::GetInstance()->SetEnvironmentTexture(envMapSrv_);

    SetupCameras();
    // レールを使わないシーンでは、エディタが Game View にレールの線・ノードを出さない
    EditorManager::GetInstance()->SetRailEditingEnabled(features_.railEditing);

    // パーティクル（Pキーのバースト等が使う粒のグループ）と GPUパーティクル基盤
    ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");
    GPUParticleManager::GetInstance()->Initialize(
        DirectXCommon::GetInstance(), SrvManager::GetInstance(), "resources/uvChecker.png");

    if ( features_.demoShowcase ) { SetupDemo(); }

    OnInitialize();
}

// メインカメラ／デバッグカメラの生成・エディタへの登録
void BaseScene::SetupCameras(){
    camera_ = Camera::Create(); // ウィンドウサイズ等は内部で自動取得
    camera_->SetTranslation({ 0.0f, 2.0f, -15.0f });
    // 既定（アクティブ）カメラに設定 → 以降の Obj3d::Create は自動でこのカメラを使う
    Obj3dCommon::GetInstance()->SetDefaultCamera(camera_.get());

    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize();
    EditorManager::GetInstance()->SetDebugCamera(debugCamera_.get()); // メニュー「表示」でON/OFF
    EditorManager::GetInstance()->SetCamera(camera_.get());
}

// エンジン機能の展示。使うモデル・テクスチャはここで読む（どのシーンでも単独で出せるように）。
//   Obj3d::Create がデフォルトカメラを掴むため、必ずカメラの後
void BaseScene::SetupDemo(){
    ModelManager* modelManager = ModelManager::GetInstance();
    modelManager->LoadModel("animatedCube", "resources/AnimatedCube", "AnimatedCube.gltf");
    modelManager->LoadModel("human", "resources/human", "walk.gltf");

    TextureManager* textureManager = TextureManager::GetInstance();
    std::unordered_map<std::string, TextureData> textures;
    textures["skybox"]        = textureManager->LoadCube("resources/StandardCubeMap.dds");
    textures["uvChecker"]     = textureManager->Load("resources/uvChecker.png");
    textures["noise0"]        = textureManager->Load("Resources/noise0.png");
    textures["gradationLine"] = textureManager->Load("Resources/gradationLine.png");

    demo_ = std::make_unique<DemoShowcase>();
    demo_->Initialize(DirectXCommon::GetInstance()->GetCommandList(), textures);
}

bool BaseScene::IsDemoVisible() const{
    return demo_ && EditorManager::GetInstance()->IsDemoVisible();
}

// =====================================================================
//  終了
// =====================================================================
void BaseScene::Finalize(){
    OnFinalize();
    EditorManager* editorManager = EditorManager::GetInstance();
    // エディタが保持している外部ポインタ（カメラ・ギズモ対象など）をリセット（ダングリングポインタ防止）
    editorManager->ResetSceneReferences();
    // ノードエディタに登録したゲーム値（シーンの変数へのポインタ）も解除する
    editorManager->ClearNodeGameValues();
    GPUParticleManager::GetInstance()->Finalize();
    demo_.reset();
}

// =====================================================================
//  更新
// =====================================================================
void BaseScene::Update(){
    OnPreUpdate();
    HandleEditorCameraRequests();
    UpdateCamera();
    OnUpdate();
    UpdateCommonVisuals();
}

void BaseScene::UpdateCamera(){
    if ( debugCamera_ ) { debugCamera_->Update(camera_.get()); }
    camera_->Update();
}

// Blenderインポータからの「カメラに適用」要求を反映
void BaseScene::HandleEditorCameraRequests(){
    BlenderImporter* importer = EditorManager::GetInstance()->GetBlenderImporter();
    if ( !importer ) return;
    Vector3 position, rotation;
    if ( importer->ConsumeCameraRequest(position, rotation) ) {
        camera_->SetTranslation(position);
        camera_->SetRotation(rotation);
    }
}

// モードに関わらず毎フレーム行う共通の見た目の更新
void BaseScene::UpdateCommonVisuals(){
    // SDF看板の近接表示：基準位置があれば「近づいた時だけ表示」、無ければ常に全表示
    Vector3 viewer {};
    if ( GetSdfViewerPosition(viewer) ) { SDFManager::GetInstance()->SetViewerPosition(viewer); }
    else                                 { SDFManager::GetInstance()->ClearViewerPosition(); }

    PostEffect::GetInstance()->Update();
    ParticleManager::GetInstance()->Update(camera_.get());

    // 展示物の見た目更新（デモ表示OFFの間はスキップ）
    if ( IsDemoVisible() ) { demo_->UpdateVisuals(Input::GetInstance(), camera_.get()); }
}

// =====================================================================
//  描画
// =====================================================================
void BaseScene::Draw(){
    auto commandList = DirectXCommon::GetInstance()->GetCommandList();
    GPUParticleManager::GetInstance()->Dispatch(commandList);

    PostEffect::GetInstance()->PreDrawSceneMRT(commandList);   // MRT開始
    DrawScene3D(commandList);
    PostEffect::GetInstance()->PostDrawSceneMRT(commandList);  // MRT終了（2枚のキャンバスを読み込みモードへ）

    ComposeFrame(commandList);

    // スプライト・UI
    SpriteCommon::GetInstance()->PreDraw(commandList);
    OnDrawUI(commandList);
    TextManager::GetInstance()->Draw();
}

// 3D一式を MRT（色＋マスク）へ描く。不透明 → インスタンシング → 透明・加算 → SDF → デバッグ線 の順
void BaseScene::DrawScene3D(ID3D12GraphicsCommandList* commandList){
    const bool demoVisible = IsDemoVisible();

    Obj3dCommon::GetInstance()->PreDraw(commandList);
    // 環境マップを束縛（PreDraw でルートシグネチャが張り直されるため毎フレーム必要）
    SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(9, envMapSrv_);

    // --- 不透明 ---
    if ( demoVisible ) { demo_->DrawOpaque(); } // 展示物（回転キューブ・スキンメッシュ）
    OnDrawOpaque(commandList);
    EditorManager::GetInstance()->Draw();        // エディタで置いた配置物

    // --- インスタンシング ---
    OnDrawInstanced();

    // --- 透明・加算合成（順番が大事：不透明を全部描き切った後）---
    if ( demoVisible ) { demo_->DrawAdditive(); } // オーラ2種＋SpaceデモのHitEffect
    OnDrawTransparent(commandList);
    PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
    ParticleManager::GetInstance()->Draw(commandList);
    GPUParticleManager::GetInstance()->Draw(commandList);

    // --- SDFボリューム（専用PSOに切り替えるので、通常のObj3d描画が全部終わった後に描く）---
    if ( demoVisible ) { demo_->DrawSdf(commandList); } // SDF卵のエロージョン/モーフデモ
    OnDrawSdf(commandList);
    SDFManager::GetInstance()->DrawVolumes(commandList); // エディタで配置した3Dボリューム

    // デバッグ描画：MRT（シーンRT）内で線を描く → ポストエフェクト/Bloomを通って
    //   Game View にも単体表示にも反映される（深度テストありで3D形状に隠れる）
    if ( showDebugGrid_ ) {
        DebugDraw::GetInstance()->Grid(20.0f, 1.0f, { 0.3f, 0.3f, 0.35f, 0.5f }, 0.0f);
    }
    DebugDraw::GetInstance()->Render(camera_.get());
}

// ポストエフェクト → Bloom → SDF（文字/画像）の焼き込み → バックバッファへ最終出力
void BaseScene::ComposeFrame(ID3D12GraphicsCommandList* commandList){
    PostEffect* postEffect = PostEffect::GetInstance();
    Bloom* bloom = Bloom::GetInstance();

    postEffect->Draw(commandList, false); // バックバッファへの最終出力は FinalBlit に任せる
    bloom->Render(commandList, postEffect->GetSrvIndex(), postEffect->GetMaskSrvIndex()); // 「色」と「マスク」
    uint32_t finalSrv = bloom->GetResultSrvIndex();

    // SDF（文字/画像）を最終画像に焼き込む → エディタの Game View にもそのまま映る。
    //   Bloom有効時は合成RT、無効時は PostEffect の最終RTが finalSrv の実体なので、
    //   どちらの場合も「FinalBlit が読むテクスチャ」へ焼き込めばフルスクリーンにも映る
    RenderTexture* finalTarget = bloom->IsEnabled() ? bloom->GetCombineTexture() : postEffect->GetFinalTexture();
    SDFManager::GetInstance()->DrawIntoTexture(commandList, finalTarget);
    if ( features_.overlay2D ) { DrawOverlayInto(commandList, finalTarget); }

    // エディタに最終的なゲーム画面のSRVを渡す（Game View 表示用）
    EditorManager::GetInstance()->SetGameViewSrvIndex(finalSrv);
    // 最終結果をバックバッファへ（エディタアクティブ時はRTVのセットのみ行い描画はスキップ）
    postEffect->FinalBlit(commandList, finalSrv, EditorManager::GetInstance()->IsActive());
}

// シーンの2D（SDF文字・ロゴ）を最終画像へ重ねる。SDFManager::DrawIntoTexture と同じ手順
void BaseScene::DrawOverlayInto(ID3D12GraphicsCommandList* commandList, RenderTexture* target){
    if ( !target ) return;
    DirectXCommon* dxCommon = DirectXCommon::GetInstance();

    // 読み取り用 → 描画用 に遷移
    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = target->GetResource().Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList->ResourceBarrier(1, &barrier);

    // 描画先を設定（クリアはしない＝既存の絵の上に重ねる）
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = target->GetRtvHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon->GetDsvHandle();
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f,
        ( float ) dxCommon->GetClientWidth(), ( float ) dxCommon->GetClientHeight(), 0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0, ( LONG ) dxCommon->GetClientWidth(), ( LONG ) dxCommon->GetClientHeight() };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

    OnDrawOverlay2D(commandList);

    // 描画用 → 読み取り用 に戻す（この後 FinalBlit / Game View がサンプルする）
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);
}

// =====================================================================
//  デバッグUI（ImGui）
// =====================================================================
void BaseScene::DrawDebugUI(){
#ifdef USE_IMGUI
    // 共有の「インスペクター (詳細設定)」へ合流する組（アイコンモードでは詳細パネルOFF時に丸ごと省略）
    if ( EditorManager::GetInstance()->IsPanelVisible(EditorManager::Panel_Inspector) ) { DrawInspector(); }
    // 展示物のパネル（SDF卵のエロージョン操作）。デモ表示OFFの間はウィンドウごと出さない
    if ( IsDemoVisible() ) { demo_->DrawImGui(); }
    OnDebugUI();
#endif
}

void BaseScene::DrawInspector(){
#ifdef USE_IMGUI
    // エンジン側の各部品のUI（同じ「詳細設定」ウィンドウへ合流する）
    Obj3dCommon::GetInstance()->DrawDebugUI();
    camera_->DrawDebugUI();
    debugCamera_->DrawDebugUI();
    ParticleManager::GetInstance()->DrawDebugUI();
    TextManager::GetInstance()->DrawDebugUI();

    ImGui::Begin("インスペクター (詳細設定)");
    OnDrawInspector();
    if ( ImGui::CollapsingHeader("デバッグ描画 (DebugDraw)") ) {
        ImGui::Checkbox("グリッドを表示", &showDebugGrid_);
        OnDrawDebugDrawOptions();
        ImGui::TextDisabled("Box/Sphere/Line はコードから積む。Game View にも表示されます");
    }
    ImGui::End();
#endif
}
