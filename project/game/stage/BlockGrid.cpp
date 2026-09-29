#include "game/stage/BlockGrid.h"

#include <cmath>

int BlockGrid::CellOf(float dist){ return ( int ) std::lround(dist); }

void BlockGrid::Rebuild(const std::vector<PlacedBlock>& blocks){
    blocks_ = &blocks;
    cells_.clear();
    for ( int i = 0; i < ( int ) blocks.size(); ++i ) {
        cells_[KeyOf(blocks[i].rail, CellOf(blocks[i].dist))].push_back(i);
    }
}

int BlockGrid::Find(int rail, int cell, int level, float side) const{
    if ( !blocks_ ) return -1;
    auto found = cells_.find(KeyOf(rail, cell));
    if ( found == cells_.end() ) return -1;
    for ( int index : found->second ) {
        const PlacedBlock& block = ( *blocks_ )[index];
        if ( block.level == level && std::abs(block.side - side) < 0.51f ) return index;
    }
    return -1;
}
