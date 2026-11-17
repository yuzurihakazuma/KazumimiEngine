#pragma once
// =====================================================================
//  StageRecords：ステージごとの記録（ゲームを動かしている間だけ。ファイルには残さない）
//   ・bestCoins     … 1回のプレイでためたコインのいちばん多い数
//   ・comboFlowers  … 見つけたコンボ花（コンボの的ごとの名前）。一度見つけたら、やり直しても残る
//   ・clearCount    … クリアした回数（2 回目からクリア演出を縮める・ワールドマップで次のマスへ道が伸びる）
//   ・stars         … 取った星（リザルトの 3 行ごとの名前。1 ステージに 3 つ）。一度取ったら残る
//   本編（GamePlayScene / EggCombo / ClearFlow）が書き、ワールドマップ（StageSelectScene → WorldMapPanel）が読む
//   ヘッダだけ（.cpp は無い）
// =====================================================================
#include "externals/nlohmann/json.hpp"
#include "engine/utils/JsonFile.h"
#include "engine/base/Singleton.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

struct StageRecord {
    int bestCoins = 0;
    std::set<std::string> comboFlowers;
    int clearCount = 0;             // クリアした回数
    std::set<std::string> stars;    // 取った星（"combo"・"health"・"enemies"）
    bool hanamaru = false;          // 全部そろえて、花丸をもらった
};

class StageRecords : public Singleton<StageRecords> {
	friend class Singleton<StageRecords>; // 土台（Singleton）だけが作れる
public:
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

    // --- クリアと卵スタンプ ---
    // クリアした（回数を足す）。ワールドマップが「いまクリアして来た」と分かるように、ステージの名前も覚える
    void NoteClear(const std::string& stage){
        ++records_[stage].clearCount;
        justCleared_ = stage;
    }
    int ClearCount(const std::string& stage) const{
        auto found = records_.find(stage);
        return found == records_.end() ? 0 : found->second.clearCount;
    }
    bool IsCleared(const std::string& stage) const{ return ClearCount(stage) > 0; }
    // いまクリアして来たステージ（読むと消える。無ければ空）。ワールドマップが、次のマスへ道を伸ばす演出に使う
    std::string ConsumeJustCleared(){
        std::string stage;
        stage.swap(justCleared_);
        return stage;
    }
    // 1 ステージで取れる星の数（リザルトの 3 行）
    static constexpr int kStarsPerStage = 3;
    // 星を足す。ids＝今回取れた星の名前。新しく増えた数を返す
    int AddStars(const std::string& stage, const std::vector<std::string>& ids, bool hanamaru){
        StageRecord& record = records_[stage];
        int added = 0;
        for ( const std::string& id : ids ) { added += record.stars.insert(id).second ? 1 : 0; }
        record.hanamaru = record.hanamaru || hanamaru;
        return added;
    }
    bool HasStar(const std::string& stage, const std::string& id) const{
        auto found = records_.find(stage);
        return found != records_.end() && found->second.stars.count(id) > 0;
    }
    int StarCount(const std::string& stage) const{
        auto found = records_.find(stage);
        return found == records_.end() ? 0 : ( int ) found->second.stars.size();
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
        json root;
        if ( JsonFile::Load(dir + "_backdrop.json", root) ) { countIn(root.value("objects", json::array()), 1); }
        if ( JsonFile::Load(dir + "_backdrop_edit.json", root) ) {
            countIn(root.value("added", json::array()), 1);
            countIn(root.value("removed", json::array()), -1);
        }
        return ( std::max )( count, 0 );
    }

private:
    StageRecords() = default;
    std::map<std::string, StageRecord> records_;
    std::string justCleared_; // いまクリアして来たステージ（ConsumeJustCleared）
};
