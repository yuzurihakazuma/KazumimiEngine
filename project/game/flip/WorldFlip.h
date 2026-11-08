#pragma once
// =====================================================================
//  WorldFlip：カメラ反転（表から見る ⇔ 裏から見る）。
//   別の世界へ移るのではなく、同じステージをカメラが反対側から見るだけ。
//   ・切り替えポイント（resources/map/<ステージ>_flip.json の points）：レールの上に置いた所を通ると、
//     「裏へ / 表へ / 反転」のとおりにカメラがプレイヤーのまわりを弧を描いて180°回り込む。置いた所でだけ切り替わる
//   ・レールごとの向き（railSides。表から見る / 裏から見る / 変えない）も使える：向きの違うレールへ乗り移ると回る
//   ・「？」スイッチ（工作の文字盤 FlipDial）に卵を当てても回る（くるっと回ってヒットストップ → 回り込み）。
//     回っている間はゲームの時間を止める（入力・敵・当たり判定も止まる）。回り終えたら少しの間スイッチは使えない（灰色）
//   ・裏から見ている間は、恐竜とカメラの間に入る背の高い背景（木・丘など）を隠す（地面は残す）
//   ・裏から見ると夕方になる（カメラの回り具合に合わせて、光が夕日の色に・画面にうすく橙・裏のカメラから見える側に夕焼けの空）
//   ・入力はもともとカメラの向き基準（PlayerInput）なので、裏から見ても「D＝画面の右」
//   ・エディタ：左のパネル「反転」。Game View の道をクリックしてスイッチを置く・レールの向きを塗る、
//     スイッチの所から「つなぐ道」（スイッチの道から奥へ延びる道＋その先の裏から見る道）を作る、表/裏のカメラで見る
//   ・確かめ用：環境変数 CG2_DEBUG_KEYS で F8＝カメラを反転
// =====================================================================
#include "engine/3d/obj/Obj3dCommon.h"
#include "engine/math/struct.h"
#include "externals/nlohmann/json.hpp"

#include <memory>
#include <string>
#include <vector>

class Camera;
class EggSystem;
class FlipDial;
class Obj3d;
class Player;
class PlayCameraController;
class RailEditor;
class RailField;
class StageBackdrop;
struct SceneEditContext;

class WorldFlip {
public:
    enum class Side { Both = 0, Front = 1, Back = 2 }; // レールのカメラの向き

    // シーンの部品（持たない）
    struct Context {
        Player*               player = nullptr;
        RailField*            railField = nullptr;
        PlayCameraController* camCtrl = nullptr;
        StageBackdrop*        backdrop = nullptr;
        EggSystem*            eggs = nullptr;
        Camera*               camera = nullptr;
    };

    WorldFlip();
    ~WorldFlip();

    void Initialize(const Context& context, const std::string& stage);
    void Finalize();

    void OnPlayStart(){ ResetToFront(); }
    void OnEditStart(){ ResetToFront(); }
    void ResetToFront(); // 表のカメラへ戻す（プレイ開始・落ちた・やりなおし）

    // プレイ中の進行のはじめ。回り込み中（ゲームの時間を止めている）なら true → シーンはそのフレームのゲームを進めない
    //   debugCameraActive：デバッグカメラを使っている間はカメラを動かさない
    bool Update(bool debugCameraActive);
    // 飛んでいる卵と「？」スイッチの当たり（卵を投げた後・卵の更新の前に呼ぶ）
    void CheckEggHits();
    // 見た目（毎フレーム、モードに関わらず）：スイッチ・隠す背景
    void UpdateVisuals(bool playing);
    void Draw();

    bool IsFlipping() const{ return phase_ != Phase::None; }
    bool IsBackView() const{ return back_; }
    // 裏から見ている間、カメラ側の物を隠す基準（恐竜の位置と、そこからの距離）。隠さない時は false。
    //   恐竜より gap 以上カメラ側（+z）にある、背の高い物を隠す（背景・ブロック・敵・スイッチ）
    bool GetBackCut(bool playing, Vector3& outPivot, float& outGap) const;
    // 反転スイッチの当たりの球（卵の奥投げが、スイッチの後ろの背景をロックしないように）
    void GetSwitchSpheres(std::vector<Vector3>& outCenters, std::vector<float>& outRadii) const;

    // エディタ（ImGui の中で毎フレーム呼ぶ）：パネル「カメラ反転」・Game View のクリックで置く・名札
    void UpdateEditor(const SceneEditContext& context);

private:
    // 切り替えポイント：レールの上の1点。通った（入った）時に、mode のとおりカメラの向きを決める
    struct FlipPoint {
        int   rail = -1;
        float dist = 0.0f;
        int   mode = 2;          // 0=反転（通るたびに入れ替え）/ 1=表へ / 2=裏へ
        float radius = 1.5f;     // 通ったとみなす半径(m)
        Vector3 pos {};          // レールの上の位置（毎フレーム求め直す）
        bool  valid = false;     // pos が求まっているか（レールが無い・まだ求めていない間は見ない）
        bool  inside = false;    // 今、半径の中にいるか（入った瞬間だけ切り替える）
    };
    struct Switch {
        int   rail = -1;            // 載せているレール（-1＝位置 pos のまま。古いデータは読み込んだ後に近くのレールへ載せる）
        float dist = 0.0f;          // レール上の距離(m)
        float height = 2.3f;        // 道からの高さ(m)（スイッチの底。文字盤の真ん中はその FlipDial::kCenterAbove 上）
        Vector3 pos {};             // スイッチの底（レールに載っていれば毎フレーム求め直す）
        float hitRadius = 1.1f;     // 卵の当たり半径（見た目より少し大きく）
        float spin = 0.0f;          // くるっと回る（当たった時）
        int   linkRail = -1;        // 「つなぐ道を作る」で作った、奥へ延びる道（置き直し用）
        int   backRail = -1;        //   その先の、裏から見る道
        std::unique_ptr<FlipDial> dial; // 見た目
    };
    enum class Phase { None, Hit, Turn };

    bool Load();
    bool Save();
    nlohmann::json SaveData() const; // 保存する中身（未保存の変更があるかの比べにも使う）
    Side RailSide(int rail) const;
    void SetRailSide(int rail, Side side);
    void BeginTurn(bool withHit, int switchIndex); // withHit：スイッチに当たった（ヒットストップ・音）
    void FinishTurn();
    void ApplyBackdrop(bool playing);
    void ApplyEvening(float amount);   // 夕方の度合い（0=昼 → 1=夕方）を光・画面の色・空へ
    void RestoreDaylight();            // 光と画面の色を元に戻す
    void MakeSwitchObject(Switch& sw);
    void UpdateSwitchAnchors();
    void UpdatePointAnchors();
    bool UpdatePoints();       // 入ったポイントがあれば回り始める（回り始めたら true）
    bool SwitchRailPoint(const Switch& sw, Vector3& outPos, Vector3& outTangent) const;
    Vector3 CameraSideOf(const Vector3& tangent) const; // 表のレールから見て、表のカメラ側（手前）の向き

    Context context_;
    std::string stage_;
    std::vector<Switch> switches_;
    std::vector<int>    railSides_;     // レール番号 → カメラの向き（無い所は両方）
    std::vector<FlipPoint> points_;     // 切り替えポイント

    // 時間（実時間の秒）
    float hitStop_ = 0.25f;
    float flipTime_ = 0.9f;
    float cooldownTime_ = 1.2f;
    float radiusGrow_ = 0.3f;   // 回る途中に距離を何割広げるか
    float occludeGap_ = 1.0f;   // 裏から見る時：恐竜よりこれ以上カメラ側にある背の高い背景を隠す(m)

    // 夕方（裏から見ると）
    bool  eveningOnBack_ = true;
    float eveningStrength_ = 1.0f;
    Vector3 eveningLight_ { 1.0f, 0.6f, 0.38f };  // 夕日の色
    float eveningTint_ = 0.12f;                   // 画面に重ねる橙の濃さ
    float evening_ = 0.0f;                        // 今の夕方の度合い
    std::unique_ptr<Obj3d> eveningSky_;           // 夕焼けの空（裏のカメラから見える側に立てる板）
    bool  lightSaved_ = false;
    Obj3dCommon::DirectionalLightData savedLight_ {};

    bool  back_ = false;        // 今、裏のカメラか（回り始めた時点で切り替わる）
    bool  cutActive_ = false;   // 今のフレーム、カメラ側の物を隠しているか（UpdateVisuals で決める）
    float cutMinZ_ = 0.0f;      // これより +z（カメラ側）は隠す
    Phase phase_ = Phase::None;
    float time_ = 0.0f;
    float cooldown_ = 0.0f;
    float look_ = 0.0f;         // カメラの回り具合（0=表 → 1=裏）
    float lookFrom_ = 0.0f;     // 回り始めの look_
    int   lastRail_ = -1;       // 前のフレームに乗っていたレール（乗り移った時だけ向きを見る）

    // エディタ
    enum class Tool { None, SwitchPos, PaintRail, PlacePoint };
    void DrawEditorWindow(const SceneEditContext& context);
    void UpdateGameViewTool(const SceneEditContext& context);
    void DrawGameViewLabels(const SceneEditContext& context);
    void BuildLinkRoute(int switchIndex, RailEditor* railEditor); // スイッチの所から奥へつなぐ道と、裏から見る道を作る
    void FrameEditorCamera(Camera* camera, int switchIndex, bool back) const;
    int  selected_ = 0;
    bool showDebug_ = true;      // 名札と線
    bool showInPlay_ = false;    // プレイ中も出す
    bool panelOpen_ = false;     // パネル「カメラ反転」が開いているか（前のフレームの値）
    Tool tool_ = Tool::None;     // Game View のクリックで何をするか
    int  paintSide_ = 2;         // レールを塗る時の向き（0=両方 / 1=表 / 2=裏）
    int  pointMode_ = 2;         // 置くポイントの切り替え方（0=反転 / 1=表へ / 2=裏へ）
    int  selectedPoint_ = -1;
    float linkDepth_ = 12.0f;    // つなぐ道：スイッチの道からどれだけ離すか(m)
    bool  linkTowardCamera_ = true; // true＝表のカメラ側（手前）へ＝裏から見て奥へ延びる / false＝表の奥へ
    float linkBefore_ = 8.0f;    //   裏から見る道の、つなぐ所より前の長さ(m)
    float linkAfter_ = 20.0f;    //   後ろの長さ(m)
    bool  previewBack_ = false;  // 編集中に「裏のカメラで見る」を押した（背の高い背景を隠す）
    Vector3 previewPivot_ {};    //   その時に見ている所
    std::string savedJson_;      // 最後に読んだ・保存した中身（未保存の印）
};
