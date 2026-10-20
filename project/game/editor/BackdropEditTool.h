#pragma once
// =====================================================================
//  BackdropEditTool：背景エディタ（クラフトの背景＝木・草・花・柵・岩などを Game View で直す）。
//   パネル「背景エディタ (Backdrop)」と、Game View の上の操作。
//   ・道具：選ぶ（クリック・Shift で追加・空いた所をドラッグで範囲選択・つかんで地面の上を動かす・ギズモ）
//           置く（パレットで選んだ物をクリックした地面へ）／ブラシ（草むら・花畑を塗る）／消しゴム
//           地面（1.2m の升目を塗る：草・紙の道・段ボールの土・穴・段を上げる/下げる。地面の無い所にも作れる）
//   ・Delete＝消す、Ctrl+D＝複製、Ctrl+Z / Ctrl+Y＝元に戻す / やり直す、Ctrl+S＝保存、Esc＝選ぶのをやめる
//   ・右クリック（動かさずに離す）＝メニュー（消す・複製・同じ種類を全部選ぶ・地面に合わせる）
//   ・ファイルエディタやパレットからモデルを Game View へドラッグ＆ドロップでも置ける
//   ・保存は「自動配置からの差」だけ（StageBackdrop::SaveEdits）。自動配置を作り直しても手直しは残る
//  「背景を編集する」が ON の間は、Game View のクリックは背景に使う（レールの選択や敵・コインのつかみは止める）
// =====================================================================
#include "game/stage/StageBackdrop.h"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

struct SceneEditContext;
class Obj3d;

class BackdropEditTool {
public:
    BackdropEditTool();
    ~BackdropEditTool();

    // パレットの小さな絵を読む（シーンの読み込み時に。描画の途中で読むとデバッグレイヤーが止まるため）
    void LoadThumbnails();

    // パネル（表示中だけ）
    void DrawPanel(const SceneEditContext& context);
    // Game View の上の操作。背景を編集している時は true を返す（ほかの道具を止める）
    bool UpdateGameView(const SceneEditContext& context);
    // 編集をやめる（Play に入った・パネルを閉じた）。ギズモを手放す
    void Cancel(const SceneEditContext& context);

    bool IsDirty() const { return dirty_; }
    // 背景を編集しているか（Edit モード・パネル表示中・「背景を編集する」ON）。この間は Game View のクリックを背景に使う
    bool IsEditing(const SceneEditContext& context) const;

private:
    enum class Tool { Select, Place, Brush, Eraser, Ground };
    // 地面ブラシで塗る物
    enum class GroundPaint { Grass, Path, Dirt, Hole, Raise, Lower, PathLine };

    // --- 履歴 ---
    void PushUndo(StageBackdrop& backdrop);
    void Undo(StageBackdrop& backdrop);
    void Redo(StageBackdrop& backdrop);
    void Changed() { dirty_ = true; }

    // --- 操作 ---
    void HandleDrop(const SceneEditContext& context);
    void HandleKeys(const SceneEditContext& context);
    void UpdateSelect(const SceneEditContext& context, int hover);
    void UpdatePlace(const SceneEditContext& context);
    void UpdateBrush(const SceneEditContext& context, bool erase);
    void UpdateGround(const SceneEditContext& context);
    void UpdatePathLine(const SceneEditContext& context);
    void BuildPath(StageBackdrop& backdrop);   // linePoints_ から、つながった道（帯と丸）を作る
    void ApplyGroundCell(StageBackdrop& backdrop, int column, int row);
    // 升目の上の飾り（草・花・岩など）：消す／高さを一緒に動かす
    void ClearCellDecor(StageBackdrop& backdrop, int column, int row, bool includeProps);
    void MoveCellDecor(StageBackdrop& backdrop, int column, int row, float fromTop, float delta);
    void DrawGroundPanel();
    void UpdateContextMenu(const SceneEditContext& context, int hover);
    void UpdateGizmo(const SceneEditContext& context);
    void ReleaseGizmo(const SceneEditContext& context);
    void SyncProxy(const StageBackdrop& backdrop);
    void DeleteSelection(StageBackdrop& backdrop);
    void DuplicateSelection(StageBackdrop& backdrop);
    void SelectSameModel(const StageBackdrop& backdrop, const std::string& model);
    void SnapSelectionToGround(StageBackdrop& backdrop);
    void Save(const SceneEditContext& context);
    void Regenerate(const SceneEditContext& context);

    // --- 置く物 ---
    BackdropItem MakeItem(const std::string& model, const Vector3& at);
    bool CanSelect(const StageBackdrop& backdrop, int index) const;

    // --- 見た目 ---
    void DrawBox(const StageBackdrop& backdrop, int index, const Vector4& color) const;
    void DrawModelGhost(const std::string& model, const Vector3& at, const Vector4& color) const;
    void DrawCircle(const StageBackdrop& backdrop, const Vector3& center, float radius, const Vector4& color) const;
    void DrawPalette();
    void DrawSelectionInfo(const SceneEditContext& context);

    // 状態
    bool  enabled_ = true;          // 「背景を編集する」
    Tool  tool_ = Tool::Select;
    bool  includeGround_ = false;   // 地面・丘・空も選べる
    std::string placeModel_ = "grass_tuft_00";
    // 置く時のばらつき
    bool  randomTurn_ = true;  float turnRange_ = 25.0f;
    float scaleMin_ = 1.6f, scaleMax_ = 2.0f;
    // ブラシ
    float brushRadius_ = 1.6f;
    float brushSpacing_ = 0.7f;      // 草どうしの間（m）
    std::map<std::string, bool> brushModels_;
    bool  eraseDecorOnly_ = true;    // 消しゴムは草・花だけ
    // 地面ブラシ
    GroundPaint groundPaint_ = GroundPaint::Path;
    int   groundSize_ = 1;           // 塗る升目の数（1辺）
    float groundHeight_ = 1.44f;     // 地面の無い所に作る時の上面の高さ
    bool  pickHeight_ = true;        // 塗り始めた所の地面の高さを使う
    bool  alignHeight_ = false;      // 塗った所の高さもそろえる
    bool  clearDecor_ = true;        // 道・穴にした所の草花をどける
    std::set<std::pair<int, int>> strokeCells_; // このひと塗りで塗った升目（段の上げ下げを1回だけにする）
    Vector3 lastStrokePoint_ {};                // 前のフレームに塗った所（間を埋める）
    // 線で引く道
    std::vector<Vector3> linePoints_;           // 引いている途中の線（地面の上の点）
    float pathWidth_ = 1.4f;                    // 道の幅（m）

    std::vector<int> selection_;     // 選んでいる物（先頭＝ギズモの対象）
    std::vector<std::vector<BackdropItem>> undo_, redo_;
    bool dirty_ = false;
    std::string status_;

    // 選ぶ道具のマウス
    enum class Drag { None, Pending, Move, Box };
    Drag    drag_ = Drag::None;
    Vector2 dragStart_ {};
    int     pressItem_ = -1;
    Vector3 moveAnchor_ {};          // つかんだ時に、物の根元が地面で当たった所
    Vector2 grabOffset_ {};          // つかんだ所 → 物の根元（画面上）
    std::vector<BackdropItem> moveStart_; // つかんだ時の選んだ物
    bool    moveDuplicated_ = false;

    // ブラシ
    bool    stroking_ = false;
    Vector3 lastStamp_ {};
    uint32_t rng_ = 12345u;

    // 右クリックメニュー
    int menuItem_ = -1;
    Matrix4x4 rightPressViewProj_ {}; // 右ボタンを押した時のカメラ（離すまでに動いたらメニューを開かない）

    // ギズモ（背景は Obj3d を持たないので、選んだ物の代わりの Obj3d を動かして差を当てる）
    std::unique_ptr<Obj3d> proxy_;
    std::string proxyModel_;
    Vector3 proxyT_ {}, proxyR_ {}, proxyS_ { 1.0f, 1.0f, 1.0f };
    bool    gizmoEditing_ = false;

    // パレットの絵（モデル名 → SRV 番号）
    std::map<std::string, uint32_t> thumbs_;
    std::vector<std::string> paletteModels_;
};
