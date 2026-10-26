#pragma once
// =====================================================================
//  StageBackdrop：本編のステージに置くクラフトの背景（地面・空・丘・雲・木・柵・看板・草花）。
//   置き場所は2つのファイルを重ねて作る：
//     ・自動配置 resources/map/<ステージ>_backdrop.json の "objects"
//         （tools/stage_intro/make_stage_backdrop.py がマップ（道）から作る。道を直したら流し直す）
//         { "model": "ground_block_step", "pos": [x, y, z], "rotY": 180, "scale": [sx, sy, sz] }
//         （"offset" と書いた物は、Build に渡した origin からの相対位置。"rot": [x, y, z]（度）なら X・Z 回りも）
//     ・手直し resources/map/<ステージ>_backdrop_edit.json（エディタの「背景」パネルで保存）
//         { "added": [ {model,pos,rot,scale} ], "removed": [ {model,pos} ], "moved": [ {model,from,pos,rot,scale} ] }
//         自動配置の物は「モデル名＋元の位置」で探して当てる（作り直しても同じ所にある物には手直しが残る）
//   同じモデルはまとめて1回で描く（InstancedGroup）。Obj3d は作らず、行列を直接計算して渡す
//   （地面のタイルや草花は数百個あるので、1個ずつ描くと重い）。
//   モデルは resources/craft/models（タイトルと同じキット）。LoadKitModels で読む
// =====================================================================
#include "engine/3d/obj/Obj3d.h"
#include "engine/math/struct.h"
#include "externals/nlohmann/json.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Camera;
class InstancedGroup;

// 地面のブロックの種類（どれも ground_block_step の上の面の画像だけが違う）
enum class GroundKind { Grass, Path, Dirt };

// 背景の1つ（エディタが直接さわる）
struct BackdropItem {
    std::string model;
    Vector3 pos { 0.0f, 0.0f, 0.0f };
    Vector3 rot { 0.0f, 180.0f, 0.0f };   // 度
    Vector3 scale { 1.0f, 1.0f, 1.0f };
    int  source = -1;      // 自動配置の何番目か（-1＝手で足した物）
    int  group = 0;        // まとまり（線で引いた道など。0＝なし）。選ぶ時にまとめて選ぶ
    bool removed = false;  // 消した（自動配置の物は「消した」と覚えておく）
    bool operator==(const BackdropItem& other) const;
};

class StageBackdrop {
public:
    StageBackdrop();
    ~StageBackdrop();

    // クラフトのキットのモデルを全部読む（底面原点のまま・つやなし）。タイトルと本編で共通
    static void LoadKitModels();
    // キットのモデル名の一覧（resources/craft/models の .gltf。名前順）
    static std::vector<std::string> KitModelNames();

    // 自動配置の一覧から作る。noiseSrv＝ディゾルブのノイズ画像（一括描画が必ず束縛するので、読み込み済みの物を渡す）
    void Build(const nlohmann::json& list, const Vector3& origin, uint32_t noiseSrv);
    void Clear();
    void Update(const Camera* camera); // カメラの行列を反映（毎フレーム）
    // 全体をまとめて縮めて動かす（ステージ選択の箱庭：本物のステージを台座の上に小さく載せる）。
    //   scale=1・yaw=0・translate=0 で元のまま。拡縮は等倍（縦横同じ）だけ
    void SetRoot(float scale, float yawRadians, const Vector3& translate);
    void Draw(const Camera* camera);   // 一括描画（BaseScene::OnDrawInstanced から）
    size_t GetObjectCount() const;

    // ===== ファイル =====
    static std::string BasePath(const std::string& stage); // resources/map/<stage>_backdrop.json（自動配置）
    static std::string EditPath(const std::string& stage); // resources/map/<stage>_backdrop_edit.json（手直し）
    // 自動配置＋手直しを読む。outMissed＝手直しのうち当たらなかった数
    void LoadStage(const std::string& stage, uint32_t noiseSrv, int* outMissed = nullptr);
    // 自動配置だけ読み直して、渡した手直しを重ねる（「自動配置をやり直す」の後）
    void ReloadBase(const std::string& stage, const nlohmann::json& edits, int* outMissed = nullptr);
    // 手直しを保存（一時ファイル→前のファイルを backup/ へ→置き換え）
    bool SaveEdits(const std::string& stage) const;
    // 手直しを全部捨てて自動配置のままに戻す
    void ResetToGenerated() { SetItems(generated_); }

    // ===== 手直し =====
    // 手直しを自動配置へ重ねる（Build の後）。当たらなかった数を返す（作り直しで無くなった物）
    int  ApplyEdits(const nlohmann::json& edits);
    nlohmann::json MakeEdits() const; // 今の状態と自動配置の差（手直しファイルの中身）
    bool HasEdits() const;

    // 置いてある物（消した物も入る。removed を見ること）
    const std::vector<BackdropItem>& Items() const { return items_; }
    // 中身を入れ替える（元に戻す・やり直し・ブラシ）。描画も作り直す
    void SetItems(const std::vector<BackdropItem>& items);
    // 1つだけ変える（動かしている最中など。数が変わらない時はこちらが軽い）
    void SetItem(int index, const BackdropItem& item);
    int  AddItem(const BackdropItem& item);  // 足した番号
    void RemoveItem(int index);               // 消す（自動配置の物は印だけ、足した物は取り除く）

    // ===== 地面（升目）=====
    //   地面は 1.2m の升目に並んだ段ボールのブロック（自動配置は 1〜3升の大きさでまとめてある）。
    //   升目ごとに塗る時は、まとめたブロックを1升ずつに割ってから変える
    static const char* GroundModel(GroundKind kind);
    float GridCell() const { return gridCell_; }
    void  CellOf(float x, float z, int& outColumn, int& outRow) const;
    Vector3 CellCenter(int column, int row) const;                   // y は 0
    bool  CellTop(int column, int row, float& outTop, GroundKind* outKind = nullptr) const; // その升目の地面の上面
    void  PaintCell(int column, int row, GroundKind kind, float top); // 種類と高さを決める（無ければ作る）
    void  EraseCell(int column, int row);                            // 地面を無くす（穴）
    // 範囲の中の1升ブロックを、同じ種類・同じ高さどうしで大きい長方形（1辺3升まで）にまとめ直す
    //   （塗ると1升ずつに割れるので、塗った後にまとめて見た目を自動配置と同じにする）
    void  MergeCells(int column0, int row0, int column1, int row1);
    // 線で引く道の部品（紙の道・段ボールのふち。帯と丸）
    static constexpr const char* kPathStrip = "path_strip";
    static constexpr const char* kPathDisc  = "path_disc";
    static constexpr const char* kPathEdgeStrip = "path_edge_strip";
    static constexpr const char* kPathEdgeDisc  = "path_edge_disc";
    // 新しいまとまりの番号（今あるどれとも重ならない）
    int   NextGroup() const;
    static constexpr float kStepHeight = 0.55f;                      // 段の高さ（自動配置の段々と同じ）

    // 選べない物（段ボールの底）か。地面のブロックは includeGround の時だけ選べる
    bool IsFloor(int index) const;
    bool IsGround(int index) const;

    // マウスの光線で一番手前の物（-1=なし）。outDistance＝光線の長さ
    int  Pick(const Vector3& origin, const Vector3& dir, bool includeGround, float* outDistance = nullptr) const;
    // マウスの光線が当たる地面（地面のブロックの上面）。無ければ false
    bool HitGround(const Vector3& origin, const Vector3& dir, Vector3& outPoint) const;
    // ある点の地面の高さ（真上から下へ光線。無ければ false）
    bool GroundHeightAt(float x, float z, float& outY) const;
    // 世界での大きさ（向きを含めた箱を、軸にそろえた箱で囲む）
    bool WorldBounds(int index, Vector3& outMin, Vector3& outMax) const;
    // 物の世界での形（向き付きの箱の8すみ）
    bool WorldCorners(int index, Vector3 outCorners[8]) const;
    // モデルの元の大きさ（拡縮1・向きなしの箱。紙の切り絵は少し厚くしてある）
    bool ModelBounds(const std::string& model, Vector3& outMin, Vector3& outMax) const { return LocalBounds(model, outMin, outMax); }

private:
    struct Group {
        std::string model;
        std::vector<int> items;                            // この組に入る物の番号
        std::vector<Obj3d::TransformationMatrix> matrices; // 毎フレーム詰め直す
        std::unique_ptr<InstancedGroup> batch;
        uint32_t capacity = 0;
    };
    struct Cache {
        Matrix4x4 world;
        Matrix4x4 inverse;
        Matrix4x4 inverseTranspose;
        Vector3 boundsMin, boundsMax; // 世界での箱（軸にそろえた）
        bool hasBounds = false;
    };

    static Matrix4x4 MakeWorld(const BackdropItem& item);
    void UpdateCache(int index);
    void RebuildGroups(); // 組み分けと GPU の入れ物（足りない時だけ大きく作り直す）
    bool LocalBounds(const std::string& model, Vector3& outMin, Vector3& outMax) const;
    // 光線と物の箱。当たれば true（outT＝光線の長さ）
    bool RayHit(int index, const Vector3& origin, const Vector3& dir, float& outT) const;

    // 升目の、その升目にかかっている地面のブロック（無ければ -1）
    int  GroundAtCell(int column, int row) const;
    // まとめたブロックを1升ずつに割る（割った後のこの升目のブロックの番号）
    int  SplitToCell(int index, int column, int row);
    void BlockSize(const BackdropItem& item, int& outWidth, int& outDepth) const; // ブロックが何升ぶんの大きさか（横・奥）
    void ReadGrid(const nlohmann::json& content);    // 自動配置の "grid"（無ければブロックから推測）

    float gridCell_ = 1.2f;
    float gridOriginX_ = 0.0f, gridOriginZ_ = 0.0f;
    std::vector<BackdropItem> generated_; // 自動配置そのまま
    std::vector<BackdropItem> items_;     // 手直しを重ねた今の状態
    std::vector<Cache>        cache_;     // items_ と同じ並び
    std::vector<Group>        groups_;
    bool groupsDirty_ = true;
    bool rootActive_ = false;
    Matrix4x4 root_ {};          // 全体の行列（箱庭）
    Matrix4x4 rootNormal_ {};    // 法線の向きだけ（回転。等倍の拡縮は法線の向きを変えない）
    uint32_t noiseSrv_ = 0;
    mutable std::map<std::string, std::pair<Vector3, Vector3>> boundsCache_; // モデル → 元の大きさ
};
