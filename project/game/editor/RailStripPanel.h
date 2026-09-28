#pragma once
// =====================================================================
//  RailStripPanel：配置ビュー（レール展開図）
//   ブロック・敵・コインを ImGui のパネルの中だけで配置するための2Dエディタ。
//
//   このゲームのワールドはマップチップ（マス目）ではないが、配置データは
//   「どのレールの・何m地点の・何段目」というレール基準で持っている。
//   そこでレール1本をまっすぐに伸ばして横から見た図（展開図）にすると、
//     横 = レール上の距離（1m刻み） / 縦 = 段（1段=1m）
//   のマス目になり、曲がったレールも坂のレールもマス目エディタとして扱える。
//
//   ・ここで行った編集は既存のデータ（RailEditor のブロック/コイン、EnemyEditor の敵）を
//     直接書き換えるので、Game View・保存・元に戻す はそのまま連動する
//   ・マウスで指しているマスは Game View 側にも枠で表示する（3Dのどこに置かれるかが分かる）
// =====================================================================
#include "engine/math/struct.h"
#include "engine/utils/Level/LevelData.h"
#include "game/enemy/Enemy.h"
#include <functional>
#include <vector>

class SplineRail;
class RailEditor;
class LevelEditor;
class EnemyEditor;
class BlockSystem;

class RailStripPanel {
public:
    // ウィンドウ名。EditorManager のパネル表（kPanelIcons）の名前と必ず一致させること
    static constexpr const char* kWindowTitle = "配置ビュー (レール展開図)";

    // 毎フレーム、シーンから受け取るもの（どれも所有しない）
    struct Context {
        const std::vector<SplineRail>* rails = nullptr; // 実行時レール（RailField）
        RailEditor*  railEditor  = nullptr;             // ブロック・コインの編集先
        LevelEditor* levelEditor = nullptr;             // コイン編集の未保存マーク用
        EnemyEditor* enemyEditor = nullptr;             // 敵の編集先
        const BlockSystem* blockSystem = nullptr;       // Game View への枠表示・斜面の向き
        bool  editable = true;                          // false=表示だけ（プレイ中）
        int   startRail = -1;  float startDist = 0.0f;  // スタート地点
        int   goalRail  = -1;  float goalDist  = 0.0f;  // ゴール地点（-1=未設定）
        bool  hasPlayer = false;                        // プレイ中のプレイヤー位置を出すか
        int   playerRail = -1;
        Vector3 playerPos { 0.0f, 0.0f, 0.0f };
        std::function<void()> onCoinsChanged;           // コインを変えた後に呼ぶ（CoinSystem の作り直し）
    };

    void Draw(const Context& context);

    // 「カメラをここへ」の要求を取り出す（あれば true）。height はレール面からの高さ(m)
    bool ConsumeFocusRequest(int& outRail, float& outDist, float& outHeight);
    // 「敵の設定を開く」の要求を取り出す（あれば true。シーンが敵エディタのパネルを開く）
    bool ConsumeOpenEnemyPanelRequest();

private:
    enum class Tool { Block, Enemy, Coin };
    enum class StrokeKind { None, Place, Replace, Erase, RectFill, RectErase, Move };
    enum class EnemyDrag { None, Move, RangeMin, RangeMax };

    // 展開図の寸法（このフレームの値。DrawCanvas の最初に決める）
    struct Layout {
        float originX = 0.0f, originY = 0.0f; // キャンバス左上（スクリーン座標）
        float cellPx  = 28.0f;                // 1m = 何ピクセルか
        float padLeft = 0.0f;
        float rulerHeight = 0.0f;
        float groundY = 0.0f;                 // 道の上面のスクリーンY
        float groundHeight = 0.0f;            // 道の帯の厚み(px)
        float infoTop = 0.0f, infoHeight = 0.0f; // 下の情報帯（高さの変化・接続）
        float width = 0.0f, height = 0.0f;    // キャンバス全体
        float railLength = 0.0f;
        int   lastCell = 0;                   // 置ける一番端のマス（距離）
        int   levels = 8;                     // 表示する段数
    };
    float DistToX(const Layout& layout, float dist) const;
    float HeightToY(const Layout& layout, float heightMeters) const;
    float XToDist(const Layout& layout, float screenX) const;
    float YToHeight(const Layout& layout, float screenY) const;

    // --- 窓の各部 ---
    void SyncCurrentRail(const Context& context);
    // レールを開く。arrivalDist が 0 以上なら、その距離が表示の中央に来るようにする
    void SelectRail(const Context& context, int railIndex, float arrivalDist = -1.0f);
    // 敵を選ぶ（このパネルの中で選んだ時用。選択の変化を「外から選ばれた」と取り違えないよう覚えておく）
    void SelectEnemy(const Context& context, int index);
    void DrawHeader(const Context& context);
    void DrawToolbar(const Context& context);
    void DrawViewOptions();
    void DrawCanvas(const Context& context);

    // --- 展開図の描画 ---
    void DrawBackdrop(const Context& context, const Layout& layout) const;
    void DrawRailInfo(const Context& context, const Layout& layout);
    void DrawBlocks(const Context& context, const Layout& layout) const;
    void DrawCoins(const Context& context, const Layout& layout) const;
    void DrawEnemies(const Context& context, const Layout& layout) const;
    void DrawMarkers(const Context& context, const Layout& layout) const;
    void DrawLevelLabels(const Layout& layout) const;

    // --- 道具ごとの操作 ---
    void UpdateBlockTool(const Context& context, const Layout& layout, bool hovered);
    void UpdateEnemyTool(const Context& context, const Layout& layout, bool hovered);
    void UpdateCoinTool(const Context& context, const Layout& layout, bool hovered);
    void DrawEnemyContextMenu(const Context& context);
    void CancelInteractions();

    // --- ブロックの判定 ---
    // 指している位置（距離・段）にあるブロックの番号（GetBlocks の番号。-1=なし）
    int  FindBlockAt(const Context& context, float dist, int level) const;
    // そのマスに type のブロックを置けるか（隣のブロックと重ならないか）。
    //   ignoreIndex のブロックは無いものとして扱う（移動の時、動かしている本人とは比べない）
    bool CanPlaceBlock(const Context& context, float dist, int level, int type, int ignoreIndex = -1) const;
    // マウスの位置から、実際に置く距離を決める（置ける場所が無ければ false）。
    //   まずマスの中心、だめなら半マスずらした位置を試す（2m のブロックを隣へぴったり付けられる）
    bool ResolvePlaceDist(const Context& context, float mouseDist, int level, int type,
                          int ignoreIndex, float& outDist) const;
    void ApplyBlockStroke(const Context& context, int cellDist, int level, float probeDist);

    // --- 状態 ---
    int   currentRail_ = 0;
    int   lastEditorRail_ = -1;       // レールエディタ側の選択（変わったら追従する）
    int   pendingFollowRail_ = -1;    // まだ開けない（作ったばかり等）ので、開けるようになったら開くレール
    bool  followEditorRail_ = true;   // レールエディタの選択に合わせる
    int   sideLayer_ = 0;             // 編集する横の位置（-2〜+2。0=道の中心）
    bool  showOtherSides_ = true;     // 他の横位置のブロックも薄く表示
    float cellPx_ = 28.0f;            // 拡大率（1m のピクセル数）
    int   visibleLevels_ = 5;         // 表示する段数
    bool  scrollResetPending_ = true; // レールを切り替えた直後：表示位置を先頭へ戻す
    bool  panning_ = false;           // 中ボタンドラッグで表示を動かしている
    bool  canvasHovered_ = false;     // このフレーム、マウスが展開図の上にあるか
    float viewCenterDist_ = 0.0f;     // 見えている範囲の中央の距離（「カメラをここへ」ボタン用）
    float canvasViewWidth_ = 0.0f;    // 展開図の見えている幅（「全体」ボタンの拡大率の計算用）
    int   pendingRailSelect_ = -1;    // 展開図の中のリンクで選ばれたレール（描画の後で切り替える）
    float pendingRailDist_ = -1.0f;   // そのレールで最初に見せる距離（-1=先頭）
    float scrollToDist_ = -1.0f;      // この距離を表示の中央へ持ってくる（-1=なし）
    bool  fitRequested_ = false;      // 「全体」ボタン：レール全体が入る拡大率にする
    int   lastSelectedEnemy_ = -1;    // 前のフレームの敵の選択（外で選択が変わったことの検出用）
    int   lastDrawFrame_ = -1;        // 最後に描いたフレーム（描かれない間があったら途中の操作を捨てる）
    int   lastLevels_ = 0;            // 前のフレームの段数（操作の途中で段数が変わらないようにする）
    bool  overLinkLabel_ = false;     // このフレーム、マウスがレールのつながりのラベルの上にあるか
    float rangeGrabOffset_ = 0.0f;    // 動ける範囲の■をつかんだ時の、■とマウスの距離の差
    bool  rangeAmbiguous_ = false;    // つかんだ■の両端が重なっていた（どちらを動かすかはドラッグの向きで決める）

    Tool  tool_ = Tool::Block;
    EnemyType enemyType_ = EnemyType::Zako;

    // ブロックの塗り
    StrokeKind stroke_ = StrokeKind::None;
    int   strokeButton_ = 0;          // 塗りに使っているマウスボタン（0=左 / 1=右）
    int   strokeLastDist_ = 0, strokeLastLevel_ = 0;
    int   rectStartDist_ = 0, rectStartLevel_ = 0;
    int   rectEndDist_ = 0,   rectEndLevel_ = 0;

    // ブロックの移動（Ctrl＋ドラッグ）
    int       moveIndex_ = -1;        // 動かしているブロック（GetBlocks の番号）
    BlockData moveOrig_ {};           // つかんだ時の中身（途中で配列が変わっていないかの照合用）
    float     moveDist_ = 0.0f;       // 移動先の距離
    int       moveLevel_ = 0;         // 移動先の段
    bool      moveValid_ = false;     // 移動先に置けるか
    bool      moveDragged_ = false;   // 実際に動かし始めたか（クリックだけなら何もしない）

    // 敵のドラッグ
    EnemyDrag enemyDrag_ = EnemyDrag::None;
    int   enemyDragIndex_ = -1;
    bool  enemyDragChanged_ = false;
    float enemyGrabMouseY_ = 0.0f;    // つかんだ時のマウスY（上下に動かしたかの判定）
    bool  enemyHoverAdjust_ = false;  // 上下ドラッグで浮く高さを変えている
    int   contextEnemy_ = -1;         // 右クリックメニューの対象

    // コインのドラッグ
    int      coinDragIndex_ = -1;
    CoinData coinDragData_ {};
    CoinData coinDragOrig_ {};

    // 指している場所（下の説明行と Game View の枠表示に使う）
    bool  hoverValid_ = false;
    float hoverDist_ = 0.0f;
    float hoverHeight_ = 0.0f;

    // シーンへの要求
    bool  focusPending_ = false;
    int   focusRail_ = 0;
    float focusDist_ = 0.0f, focusHeight_ = 0.0f;
    bool  openEnemyPanelPending_ = false;
};
