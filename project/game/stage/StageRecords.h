#pragma once
// =====================================================================
//  StageRecords：ステージごとの記録（ゲームを動かしている間だけ。ファイルには残さない）
//   ・bestCoins     … 1回のプレイでためたコインのいちばん多い数
//   ・comboFlowers  … 見つけたコンボ花（コンボの的ごとの名前）。一度見つけたら、やり直しても残る
//   本編（GamePlayScene / EggCombo）が書き、ワールドマップ（StageSelectScene → WorldMapPanel）が読む
//   ヘッダだけ（.cpp は無い）
// =====================================================================
#include "externals/nlohmann/json.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <string>

struct StageRecord {
    int bestCoins = 0;
    std::set<std::string> comboFlowers;
};

class StageRecords {
public:
    static StageRecords* GetInstance(){
        static StageRecords instance;
        return &instance;
    }
    StageRecord& Get(const std::string& stage){ return records_[stage]; }

    void NoteCoins(const std::string& stage, int coins){
        StageRecord& record = records_[stage];
        record.bestCoins = ( std::max )( record.bestCoins, coins );
    }
    // 見つけた（初めてなら true）
    bool FindComboFlower(const std::string& stage, const std::string& spot){
        return records_[stage].comboFlowers.insert(spot).second;
    }
    int ComboFlowersFound(const std::string& stage) const{
        auto found = records_.find(stage);
        return found == records_.end() ? 0 : ( int ) found->second.comboFlowers.size();
    }

    // ステージにあるコンボの的の数（＝コンボ花の数）。背景の自動配置と手直しのファイルから数える（ワールドマップ用）
    static int CountComboTargets(const std::string& stage){
        using json = nlohmann::json;
        const std::string dir = "resources/map/" + stage;
        int count = 0;
        auto countIn = [&](const json& list, int sign){
            if ( !list.is_array() ) { return; }
            for ( const json& item : list ) {
                if ( item.is_object() && item.value("model", std::string()) == "combo_target" ) { count += sign; }
            }
        };
        std::ifstream base(dir + "_backdrop.json");
        if ( base ) {
            json root = json::parse(base, nullptr, false);
            if ( !root.is_discarded() ) { countIn(root.value("objects", json::array()), 1); }
        }
        std::ifstream edit(dir + "_backdrop_edit.json");
        if ( edit ) {
            json root = json::parse(edit, nullptr, false);
            if ( !root.is_discarded() ) {
                countIn(root.value("added", json::array()), 1);
                countIn(root.value("removed", json::array()), -1);
            }
        }
        return ( std::max )( count, 0 );
    }

private:
    StageRecords() = default;
    std::map<std::string, StageRecord> records_;
};
