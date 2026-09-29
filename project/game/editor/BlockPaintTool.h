#pragma once
// =====================================================================
//  BlockPaintTool：ブロック配置モード（マリオメーカー風：クリックで置く/消す）。
//   ・キー操作（Game View にマウスがある時だけ）：B＝配置モードの切替 /
//     数字 1〜8＝その段に固定 / 0＝段の固定を解除
//   ・置き方：1個 / 柱（下まで埋める）/ 階段（ドラッグで1段ずつ上がる）/ 範囲フィル（矩形）
//   ・左クリック＝置く（押しっぱなしで塗り続ける）/ Shift＝一時的に消しゴム /
//     Alt＝指しているブロックを選んでいる種類へ塗り替え / 動かさない右クリック＝消す
//   ・ゴースト（緑=置く / 水色=道の脇 / 赤=消す / 黄=塗り替え）と、マウス横の説明
//  配置先は「レール × 距離(1mセル) × 段 × 横ずれ」。結果は RailEditor のブロック配置へ入る
// =====================================================================
struct SceneEditContext;
class RailEditor;

class BlockPaintTool {
public:
    // B / 数字キー。エディット中に毎フレーム呼ぶ
    void UpdateHotkeys(const SceneEditContext& context);
    // 配置モード中に毎フレーム呼ぶ（モードが切れている時は Cancel を呼ぶ）
    void Update(const SceneEditContext& context);
    // 途中の範囲フィル・塗りを捨てる（残すと次に配置モードへ戻った瞬間に古い範囲が塗られる）
    void Cancel();

    // 配置先のセル（レール × 距離 × 段 × 横ずれ）
    struct Cell {
        int   rail  = -1;
        float dist  = 0.0f;
        int   level = 0;
        float side  = 0.0f;
    };

private:
    static constexpr int kMaxLevel = 7; // 置ける一番上の段（0始まり）

    // 画面上の近さでセルを選ぶ時の条件
    struct PickOptions {
        int   levelMin = 0;           // 選べる段の範囲
        int   levelMax = 0;
        bool  sideFree = false;       // true=道の脇（横ずれ）も選べる
        float fixedSide = 0.0f;       // sideFree=false の時の横ずれ
        bool  preferExisting = false; // 置いてあるブロックのセルを選びやすくする（消す時用）
        int   stickRail = -1;         // このレールへ吸い付く
        int   onlyRail  = -1;         // このレールだけを対象にする（-1=全レール）
    };
    enum class Action { None, Place, Erase, Replace };

    // その回のフレームで決まったこと（ゴーストとクリックの動作が同じ判断を使う）
    struct Frame {
        // 配置の設定（RailEditor の配置パネルの値）
        int    paintType = 0;
        int    paintShape = 0;          // 0=1個 / 1=柱 / 2=階段 / 3=範囲フィル
        bool   eraseMode = false;       // 消しゴム（Shift で一時的にも）
        bool   centerOnly = false;      // 道の中心だけに置く
        bool   levelLocked = false;     // 段を固定中
        int    lockedLevel = 0;
        bool   faceMode = false;        // 面に積む（false=断面から選ぶ）
        bool   mouseUsable = false;     // マウスが Game View の上でギズモ操作中でない

        Cell   placeCell, pointedCell;  // 置く先 / 指している既存ブロック
        bool   hasPlace = false, hasPointed = false;
        bool   strokeActive = false;    // 押しっぱなしで塗っている途中
        Action action = Action::None;
        Cell   target;
    };

    bool PickByScreen(const SceneEditContext& context, const PickOptions& options, Cell& outCell) const;
    void FindPointedCells(const SceneEditContext& context, Frame& frame) const;
    void DecideAction(const SceneEditContext& context, Frame& frame) const;
    void DrawGuides(const SceneEditContext& context, const Frame& frame) const;
    void DrawGhost(const SceneEditContext& context, Frame& frame) const;
    void DrawRectFillPreview(const SceneEditContext& context) const;
    void ApplyInput(const SceneEditContext& context, const Frame& frame);
    void ApplyPaint(const SceneEditContext& context, const Cell& cell);
    void CommitRectFill(const SceneEditContext& context);

    // 塗り（押した瞬間に置く/消すを決めて塗り続ける）
    bool  erasing_ = false;
    Cell  lastPaint_ { -1, 0.0f, -1, 0.0f }; // level<0 = 塗っていない

    // 範囲フィル（塗り方=矩形）のドラッグ状態（始点〜終点をボタンを離した時に一括適用）
    bool  rectActive_ = false;
    bool  rectErase_ = false;
    Cell  rectStart_;
    float rectEndDist_ = 0.0f;
    int   rectEndLevel_ = 0;
};
