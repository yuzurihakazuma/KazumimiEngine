#pragma once
// =====================================================================
//  BlockGrid：(レール, 1mセル) → そのセルにあるブロック番号 の索引。
//   当たり判定・隣接判定をこの索引で引くので、ブロックの個数が増えても速度が落ちない
// =====================================================================
#include "game/stage/BlockShape.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

class BlockGrid {
public:
    // 距離 → 1mセル番号
    static int CellOf(float dist);

    // blocks から索引を作り直す（blocks は索引を使う間ずっと生きていること）
    void Rebuild(const std::vector<PlacedBlock>& blocks);

    // そのセル・段・横位置にあるブロック番号（-1=なし）
    int  Find(int rail, int cell, int level, float side) const;
    bool Has(int rail, int cell, int level, float side) const { return Find(rail, cell, level, side) >= 0; }

    // dist のセルから前後 cellRange セル以内にあるブロック番号を順に fn(index) へ渡す
    template <class Fn>
    void ForEachNear(int rail, float dist, int cellRange, Fn&& fn) const{
        const int center = CellOf(dist);
        for ( int cell = center - cellRange; cell <= center + cellRange; ++cell ) {
            auto found = cells_.find(KeyOf(rail, cell));
            if ( found == cells_.end() ) continue;
            for ( int index : found->second ) { fn(index); }
        }
    }

private:
    // (レール, 1mセル) を1つの整数キーにまとめる
    static uint64_t KeyOf(int rail, int cell){
        return ( static_cast< uint64_t >( static_cast< uint32_t >( rail ) ) << 32 ) | static_cast< uint32_t >( cell );
    }

    const std::vector<PlacedBlock>* blocks_ = nullptr;
    std::unordered_map<uint64_t, std::vector<int>> cells_;
};
