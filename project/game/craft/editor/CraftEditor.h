#pragma once
// =====================================================================
//  CraftEditor：箱庭エディタ（クラフトの配置を、ゲーム画面を見ながら ImGui で編集する）。
//   パネル「箱庭エディタ (Craft)」の中に：ツールバー（保存・読み直し・元に戻す・スナップ・道具）／
//   アウトライナー（層ごとの表示・ロック、一覧）／インスペクタ（位置・回転・拡縮・層）／
//   アセット（manifest の分類ごと）／ブラシ・レール沿いの設定／履歴
//
//   置き方は3つ：
//   ・1個ずつ置く … アセットを選ぶとマウスの下に仮置きが出る。左クリックで置き、続けて置ける。Esc で終わる
//   ・まとめて撒く（散布ブラシ）… 地面をドラッグすると円の中に飾りを撒く。Shift を押しながらだと消す
//   ・レールに沿って並べる … 間隔・横の距離・左右を決めて、柵や花をレールに沿って並べる
//   選ぶ・動かす：クリックで選び、ギズモ（W 移動 / E 回転 / R 拡縮）で動かす。Delete 削除、Ctrl+D 複製、
//   F 選んだ物へカメラを寄せる、Ctrl+Z / Ctrl+Y 元に戻す・やり直し、Ctrl+S 保存
//
//   ステージは必ずコマンド（CraftCommands）を通して書き換える。ギズモのドラッグ1回・入力欄の編集1回・
//   ブラシの1ストロークが、それぞれ1回の「元に戻す」で戻る
// =====================================================================
#include "game/craft/CraftStage.h"
#include "game/craft/editor/CraftCommands.h"

#include <memory>
#include <string>
#include <vector>

class Camera;
class CraftStageView;
class Obj3d;
class SplineRail;

class CraftEditor {
public:
    // エディタが毎フレーム受け取るもの（所有しない）
    struct Context {
        CraftStage*     stage = nullptr;
        CraftStageView* view = nullptr;
        Camera*         camera = nullptr;
        const std::vector<SplineRail>* rails = nullptr; // レールを持たないシーンは nullptr
        std::string     path;                           // ステージのファイル
    };

    CraftEditor();
    ~CraftEditor();

    void Initialize(Camera* camera);
    void Finalize();
    // ステージを読み込んだ直後に呼ぶ（履歴を空に・自動保存の確認）
    void OnStageLoaded(const Context& context, bool loadFailed);

    // 毎フレーム（エディタを出していなくても）：自動保存・ギズモ対象の付け替え
    void BeforeViewUpdate(const Context& context);
    void AfterViewUpdate(const Context& context);
    // パネルと Game View 上の操作（エディタを出している時、シーンの DebugUI から呼ぶ）
    void DrawUI(const Context& context);
    // 仮置き（置く道具の時だけ）
    void DrawGhost();

    bool IsDirty() const{ return history_.IsDirty(); }
    bool Save(const Context& context);

private:
    enum class Tool { Select, Place, Brush, Rail };

    // --- 画面 ---
    void DrawToolbar(const Context& context);
    void DrawOutliner(const Context& context);
    void DrawInspector(const Context& context);
    void DrawAssets(const Context& context);
    void DrawBrushSettings();
    void DrawRailSettings(const Context& context);
    void DrawHistory();
    void DrawRestorePrompt(const Context& context);

    // --- Game View の操作 ---
    void HandleHotkeys(const Context& context, bool gameViewHovered, bool windowFocused);
    void UpdateGizmoDrag(const Context& context);
    void UpdateSelectClick(const Context& context);
    void UpdatePlace(const Context& context);
    void UpdateBrush(const Context& context);
    void DrawSelection(const Context& context) const;

    // --- 操作の中身 ---
    void Select(uint64_t id);
    void DeleteSelected(const Context& context);
    void DuplicateSelected(const Context& context);
    void FocusSelected(const Context& context);
    void PlaceAlongRail(const Context& context);
    void Undo(const Context& context);
    void Redo(const Context& context);
    CraftObject MakeObject(const std::string& asset, const Vector3& position, float yawDeg, float scale) const;

    // --- マウス・当たり ---
    bool MouseRay(Vector3& origin, Vector3& direction) const;
    bool RaycastGround(const Context& context, const Vector3& origin, const Vector3& direction, Vector3& hit) const;
    bool GroundBelow(const Context& context, float x, float z, float fallbackY, float& outY) const;
    uint64_t PickObject(const Context& context, const Vector3& origin, const Vector3& direction) const;
    Vector3 SnapPosition(const CraftObject& object, const Vector3& position) const;

    CraftHistory history_;
    Tool tool_ = Tool::Select;
    uint64_t selectedId_ = 0;
    bool ownsGizmo_ = false;          // エディタのギズモ対象をこのエディタが使っているか

    // ギズモ・入力欄で編集中の「変更前」
    bool        dragging_ = false;
    CraftObject dragBefore_;
    bool        fieldEditing_ = false;
    CraftObject fieldBefore_;
    nlohmann::json paramsBefore_;

    // 層の表示・ロック（背景と地面は最初ロック：飾りを触るつもりで地面を動かす事故を防ぐ）
    bool layerLocked_[CraftLayer_Count] = { true, true, false, false };
    char filter_[64] = {};

    // スナップ（0=2m / 1=0.5m / 2=なし）
    int  snapMode_ = 1;
    // 置く時の基準の大きさ（キットはジオラマ用の寸法なので、本編の大きさへ合わせる倍率）
    float baseScale_ = 1.8f;

    // 1個ずつ置く
    std::string placeAsset_;
    std::unique_ptr<Obj3d> ghost_;
    bool  ghostVisible_ = false;
    float placeYawDeg_ = 0.0f;

    // 散布ブラシ
    struct BrushEntry { std::string asset; float weight = 0.0f; };
    std::vector<BrushEntry> brushAssets_;
    float brushRadius_ = 2.5f;
    float brushDensity_ = 0.5f;    // 1m² あたりの数
    float brushScaleMin_ = 0.8f, brushScaleMax_ = 1.2f;
    float brushSpacing_ = 0.6f;    // 最小間隔(m)
    std::unique_ptr<CraftGroupCommand> brushStroke_;
    bool  brushErasing_ = false;

    // レール沿い
    int   railIndex_ = 0;
    std::string railAsset_ = "fence_sticks";
    float railInterval_ = 2.4f;
    float railOffset_ = 1.5f;
    int   railSide_ = 2;           // 0=左 / 1=右 / 2=両側
    int   railFacing_ = 0;         // 0=レールに沿う / 1=レールの方を向く

    // 自動保存・警告
    float autosaveTimer_ = 0.0f;
    bool  restorePrompt_ = false;
    bool  loadFailed_ = false;
    std::string message_;
    float messageTime_ = 0.0f;
    Camera* camera_ = nullptr;
};
