#pragma once
// =====================================================================
//  BaseScene：全シーン共通の土台。
//   どのシーンでもゲームプレイシーンと同じ見た目・同じエディタ連携で動くように、
//   シーンに依存しない部分をここにまとめる：
//     ・メインカメラ／デバッグカメラ（エディタへの登録込み）と環境マップ
//     ・描画の流れ：3D（MRT）→ ポストエフェクト → Bloom → SDFの焼き込み → 画面出力 → スプライト
//     ・デバッグ描画（グリッド・DebugDraw）、パーティクル、GPUパーティクル、3D SDF ボリューム
//     ・エディタで置いた配置物の描画、Blenderインポータからの「カメラに適用」
//     ・共有の「インスペクター (詳細設定)」の共通項目
//     ・エンジン機能の展示（DemoShowcase。シーンごとに使うか選べる）
//
//  各シーンは On～ の差し込み口に、そのシーンだけの処理を書く。
//  レール・敵・ブロックなどのゲームの中身はゲームプレイシーンの差し込み口に置く（他のシーンには出ない）
// =====================================================================
#include "engine/scene/IScene.h"
#include "engine/math/struct.h"
#include "game/craft/CraftLighting.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <d3d12.h>

class Camera;
class DebugCamera;
class DemoShowcase;
class RenderTexture;
class CraftStage;
class CraftStageView;
class CraftEditor;
class SplineRail;

class BaseScene : public IScene {
public:
    BaseScene();
    ~BaseScene() override;

    // 共通の流れ（シーンは下の On～ を上書きする）
    void Initialize() final;
    void Finalize() final;
    void Update() final;
    void Draw() final;
    void DrawDebugUI() final;

protected:
    // --- シーンごとに選べる共通機能（コンストラクタで設定する）---
    struct Features {
        bool demoShowcase = false; // エンジン機能の展示（回転キューブ・オーラ・SDF卵・スキンメッシュ人形）
        bool overlay2D    = false; // OnDrawOverlay2D を使う（最終画像へ重ねる2D。Game View にも映る）
        bool railEditing  = false; // エディタのレール編集を使う（false＝Game View にレールの線・ノードを出さない）
        // このシーン専用のマップ（エディタで置いた物の保存先）。空＝ステージのマップをそのまま使う。
        //   タイトルの背景のように、ステージとは別の配置を持つシーンが指定する
        std::string sceneMap;
        // 箱庭のステージ（地面・木・草花などクラフトの配置。resources/stage/*.stage.json）。空＝使わない。
        //   エディタの「箱庭エディタ」パネルで配置・保存できる
        std::string craftStage;
    };
    Features features_;

    // --- 差し込み口：初期化・終了 ---
    virtual void OnLoadResources() {}   // カメラより先：音・モデル・テクスチャの読み込み
    virtual void OnInitialize() {}      // カメラ・共通機能の用意が済んだ後：シーンの物を作る
    virtual void OnFinalize() {}        // 共通の後始末の前

    // --- 差し込み口：更新（呼ばれる順） ---
    virtual void OnPreUpdate() {}       // カメラ更新の前（エディタ編集の反映など）
    virtual void UpdateCamera();        // 既定：デバッグカメラ → カメラ行列。揺れ等を足す時に上書き
    virtual void OnUpdate() {}          // シーンの進行
    // SDF看板の「近づいた時だけ表示」の基準位置（無ければ false＝常に全表示）
    virtual bool GetSdfViewerPosition(Vector3& /*outPos*/) const { return false; }
    // 箱庭エディタの「レール沿いに並べる」で使うレール（レールの無いシーンは nullptr）
    virtual const std::vector<SplineRail>* GetCraftRails() const { return nullptr; }

    // --- 差し込み口：描画（3Dは MRT の中。呼ばれる順） ---
    virtual void OnDrawOpaque(ID3D12GraphicsCommandList* /*commandList*/) {}      // 不透明（Obj3d の準備済み）
    virtual void OnDrawInstanced() {}                                             // インスタンシングの一括描画
    virtual void OnDrawTransparent(ID3D12GraphicsCommandList* /*commandList*/) {} // 透明・加算（パーティクルの前）
    virtual void OnDrawSdf(ID3D12GraphicsCommandList* /*commandList*/) {}         // SDFボリューム（専用PSO）
    virtual void OnDrawOverlay2D(ID3D12GraphicsCommandList* /*commandList*/) {}   // 最終画像へ重ねる2D（SDF文字・ロゴ）
    virtual void OnDrawUI(ID3D12GraphicsCommandList* /*commandList*/) {}          // スプライト（画面出力の後）

    // --- 差し込み口：ImGui ---
    virtual void OnDrawInspector() {}         // 共有の「インスペクター (詳細設定)」ウィンドウの中へ足す項目
    virtual void OnDrawDebugDrawOptions() {}  // インスペクターの「デバッグ描画」の中へ足す項目
    virtual void OnDebugUI() {}               // シーン独自のウィンドウ

    // --- シーンが使う共通の物 ---
    Camera*       GetCamera() const { return camera_.get(); }
    DebugCamera*  GetDebugCamera() const { return debugCamera_.get(); }
    DemoShowcase* Demo() const { return demo_.get(); } // features_.demoShowcase=false なら nullptr
    uint32_t      EnvironmentMapSrv() const { return envMapSrv_; }
    bool          IsDemoVisible() const;             // 展示を使っていて、表示メニューで表示中
    CraftStage* GetCraftStage() const { return craftStage_.get(); } // 箱庭のデータ（features_.craftStage が空なら nullptr）
    CraftStageView* CraftView() const { return craftView_.get(); } // 箱庭の描画（features_.craftStage が空なら nullptr）

private:
    void SetupCameras();
    void SetupDemo();
    void SyncDemoPresence();
    void HandleEditorCameraRequests();
    void UpdateCommonVisuals();
    void DrawScene3D(ID3D12GraphicsCommandList* commandList);
    void ComposeFrame(ID3D12GraphicsCommandList* commandList);
    void DrawOverlayInto(ID3D12GraphicsCommandList* commandList, RenderTexture* target);
    void DrawInspector();
    void SetupCraftStage();
    void UpdateCraftStage();

    std::unique_ptr<CraftStage>     craftStage_;
    std::unique_ptr<CraftStageView> craftView_;
    std::unique_ptr<CraftEditor>    craftEditor_;
    CraftLighting::Saved            craftLightSaved_; // 光のプロフィールを切り替える前の光（終了時に戻す）

    std::unique_ptr<Camera>       camera_;
    std::unique_ptr<DebugCamera>  debugCamera_;
    std::unique_ptr<DemoShowcase> demo_;
    uint32_t envMapSrv_ = 0;
};
