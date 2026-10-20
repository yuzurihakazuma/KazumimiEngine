#pragma once
// =====================================================================
//  RailTransferDebugger：レールからレールへ移る時の違和感を「数値で見る」ための道具
//   （布ロード企画書 フェーズ1。直す前に測る）。パネル「乗り換えデバッガ」。
//   1. Game View への重ね描き … レールの中心線・道の左右の端・RMF フレーム（接線/横/上の矢印）・
//                                接続点（緑/黄/赤）・足元からレール中心への線・軌跡（乗り換えた所は色違い）
//   2. 接続点インスペクタ     … 全接続の位置ギャップ・接線/上/横の角度差。行クリックでカメラが寄る。
//                                赤・黄の行には「端点を一致させる」「接線を揃える」
//   3. 乗り換えログ           … 乗り換えのたびに1行（CSV に書き出せる）
//   4. 時系列グラフ           … 1フレームの移動量・速度・横のずれ・レール中心からの距離（乗り換えに縦線）
//   5. 再現の補助             … スロー・コマ送り・乗り換えで一時停止・入力の記録と再生
// =====================================================================
#include "engine/math/struct.h"
#include "game/player/Player.h"

#include <cstdint>
#include <string>
#include <vector>

class SplineRail;
class Camera;
class RailEditor;
class EditorManager;

class RailTransferDebugger {
public:
    ~RailTransferDebugger();
    // プレイヤーの更新の前（入力の記録・再生）。replaying の間は自動操縦で入力を差し込む
    void BeforePlayerUpdate(Player& player, const std::vector<SplineRail>& rails);
    // プレイヤーの更新の後（このフレームの記録）。dt=0（一時停止中）は記録しない
    void Record(const Player& player, const std::vector<SplineRail>& rails, const Camera* camera, float dt);

    // 重ね描き（DebugDraw）とパネル。Edit でも接続点は見える
    void DrawOverlay(const std::vector<SplineRail>& rails, const Player* player, bool playing);
    void DrawWindow(const std::vector<SplineRail>& rails, Player* player, RailEditor* railEditor, EditorManager* editor);
    // コマ送り・乗り換えで一時停止（毎フレーム、ゲームの更新の後に呼ぶ）
    void UpdateTimeControl(bool playing);

    // 接続点インスペクタの「カメラをここへ」の要求（あれば true）
    bool ConsumeFocusRequest(int& outRail, float& outDist);
    bool IsOverlayVisible() const { return showOverlay_; }

    // 接続点1つの状態（レールの端どうし / 端と別レールの本体）
    struct Connection {
        bool  endToBody = false;   // true=端→本体（T字・合流）。角度は見ない
        int   railA = -1; bool aFront = false; float aDist = 0.0f;
        int   railB = -1; bool bFront = false; float bDist = 0.0f;
        Vector3 pos {};
        Vector3 bodyPos {};        // 端→本体：本体の上の一番近い点
        float gap = 0.0f;          // m
        float tangentDeg = 0.0f;   // 進む向きの角度差
        float upDeg = 0.0f;
        float rightDeg = 0.0f;
        int   status = 0;          // 0=緑 / 1=黄 / 2=赤 / 3=曲がり角（45°以上の角。わざと曲げた所）
    };
    static std::vector<Connection> MeasureConnections(const std::vector<SplineRail>& rails);

private:
    struct Sample {
        uint64_t frame = 0;
        Vector3 position {}, base {};
        float move = 0.0f;       // 1フレームの移動量（見た目の位置）
        float speed = 0.0f;      // m/s（見た目の位置）
        float lateral = 0.0f;    // レールの横方向のずれ（符号付き。見た目の位置とレール上の点の差）
        float offRail = 0.0f;    // レール中心からの水平距離
        float cameraTurn = 0.0f; // カメラの向きの1フレーム変化（度）
        int   rail = -1;
        bool  transfer = false;
        bool  clamped = false;
        bool  inAir = false;
    };
    struct LogRow {
        uint64_t frame = 0;
        Player::TransferKind kind = Player::TransferKind::None;
        int fromRail = -1; float fromDist = 0.0f;
        int toRail = -1;   float toDist = 0.0f;
        float overshoot = 0.0f, carried = 0.0f;
        float warp = 0.0f;           // レール上の点の飛び（はみ出しを持ち越した分を引いた残り）
        float visibleOffset = 0.0f;  // 見た目の平滑化で隠したずれ（乗り換え直後）
        Vector3 before {}, after {};
        float speedBefore = 0.0f, speedAfter = 0.0f;
        float lateralBefore = 0.0f, lateralAfter = 0.0f;
    };

    void DrawConnectionTable(const std::vector<SplineRail>& rails, RailEditor* railEditor, bool editing);
    void DrawLog();
    void DrawGraphs();
    void DrawReplay(Player* player, bool playing);
    void StartAutoTest(const std::vector<SplineRail>& rails);
    void StepAutoTest(Player& player);
    void WriteCsv();
    void FixGap(const Connection& c, RailEditor* railEditor);
    void FixTangent(const Connection& c, const std::vector<SplineRail>& rails, RailEditor* railEditor);

    // 表示
    bool showOverlay_ = true;
    bool showFrames_ = true;
    bool showBounds_ = true;
    bool showTrail_ = true;
    bool showOnlyBad_ = false;
    float halfWidth_ = 1.0f;     // 道の左右の端（布ロードの幅の半分）

    // 記録
    std::vector<Sample> samples_;      // 直近（リング）
    size_t sampleHead_ = 0;
    std::vector<LogRow> log_;
    Sample last_ {};
    bool hasLast_ = false;
    float lastCameraYaw_ = 0.0f;
    float lastDt_ = 1.0f / 60.0f;
    std::string status_;

    // 時間
    int  slowIndex_ = 0;         // 0=等速 / 1=1/2 / 2=1/4 / 3=1/8
    bool paused_ = false;
    bool stepRequested_ = false;
    bool stepping_ = false;
    bool pauseOnTransfer_ = false;
    bool transferHappened_ = false;
    bool controlledLastFrame_ = false; // 前のフレームでこの道具が時間の倍率を決めたか

    // 入力の記録・再生
    std::vector<PlayerInput::AutoPilot> recorded_;
    int  recordStartRail_ = -1;
    float recordStartDist_ = 0.0f;
    bool recording_ = false;
    bool replaying_ = false;
    size_t replayIndex_ = 0;
    bool replayStartPending_ = false;

    // カメラの要求
    bool  focusRequested_ = false;
    int   focusRail_ = -1;
    float focusDist_ = 0.0f;
    int   selectedConnection_ = -1;
    std::vector<Connection> lastConnections_; // 直近に測った接続点（自動の書き出し用）

    // 全接続の自動テスト：接続の手前に立たせて、端へ向かって歩かせる（1か所ずつ）
    struct AutoCase { int rail; float startDist; float moveX; float moveZ; std::string label; };
    std::vector<AutoCase> autoCases_;
    int  autoIndex_ = -1;        // -1=止まっている
    int  autoFrame_ = 0;
    bool autoStartRequested_ = false;
    int  autoDelay_ = 0;         // 環境変数で自動開始する時の待ち（フレーム）
    std::string lastCsvPath_;
};
