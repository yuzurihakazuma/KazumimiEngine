#pragma once
// =====================================================================
//  BlockRenderer：ブロックの一括描画（InstancedGroup）と、継ぎ目に咲く紙花の自動配置。
//   見た目は種類ごと（＋使用済み？ブロック・花2種）のグループに分け、グループごとに
//   1ドローコールで描く。何百個置いても種類数ぶんで描き切る。
//   Obj3d は行列計算専用のプール（伸びるだけ）から借りる＝ペイントのたびに
//   Obj3d（GPUリソース2本持ち）を作り直さない
// =====================================================================
#include "game/stage/BlockShape.h"

#include <cstdint>
#include <memory>
#include <vector>

class Obj3d;
class Camera;
class InstancedGroup;
class BlockGrid;
class SplineRail;

class BlockRenderer {
public:
    BlockRenderer();
    ~BlockRenderer(); // InstancedGroup を前方宣言で持つため cpp 側で定義

    // 一括描画グループの準備（モデル生成後に1回だけ）。noiseSrv はディゾルブ用の
    // ダミーテクスチャ（閾値0なので見た目には影響しないが、束縛先として必要）
    void Initialize(uint32_t noiseSrvIndex);

    // blocks を種類ごとの見た目グループへ振り分け、並んだブロックの継ぎ目へ花を置き直す
    //   （配置が変わった時・？ブロックが使用済みになった時に呼ぶ）
    void Rebuild(const std::vector<PlacedBlock>& blocks, const BlockGrid& grid);
    // 位置・向きの更新（動くレール追従＋インスタンス行列の収集）
    void Update(const std::vector<PlacedBlock>& blocks, const std::vector<SplineRail>& rails);
    // 見た目グループごとに1ドローコール
    void Draw(const Camera* camera);

private:
    struct LookGroup {
        std::vector<Obj3d*> objs;      // プールから借りた行列計算機（非所有）
        std::vector<int> blockIndices; // objs[i] が対応するブロック番号（花は -1-花番号）
        std::unique_ptr<InstancedGroup> batch;
    };
    // 花の配置（動くレール追従のため rail/dist/高さで持つ）
    struct FlowerSpot {
        int   rail = 0;
        float dist = 0.0f;      // 継ぎ目の位置（セルの中間）
        float side = 0.0f;
        float topY = 0.0f;      // 咲かせる高さ（ブロック上面。レール面からの相対）
        float yawOffset = 0.0f; // 少しランダムに回す
        int   kind = 0;         // 0=オレンジ / 1=白
    };

    void PlaceFlowers(const std::vector<PlacedBlock>& blocks, const BlockGrid& grid);
    void EnsurePool(size_t count); // プールを count 個まで育てる（縮めない）
    void UpdateLook(LookGroup& look, const std::vector<PlacedBlock>& blocks, const std::vector<SplineRail>& rails);

    LookGroup typeLooks_[BlockShape::kTypeCount]; // 種類別のブロック
    LookGroup usedHatenaLook_;                    // 使用済みの？ブロック（灰色）
    LookGroup flowerLooks_[2];                    // 花（0=オレンジ / 1=白い顔つき）
    std::vector<FlowerSpot> flowers_;
    std::vector<std::unique_ptr<Obj3d>> objPool_; // 行列計算用プール
};
