#include "CardDatabase.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>

std::unordered_map<int, Card> CardDatabase::database_;

void CardDatabase::Initialize(const std::string &filePath) {
    database_.clear();

    // ファイルを開く
    std::ifstream file(filePath);

    // 開けなかった場合のエラーチェック
    if (!file.is_open()) {
        return;
    }

    std::string line;
    bool isFirstLine = true;

    //ファイルを1行ずつ読み込む
    while (std::getline(file,line)) {
        //最初の1行目はデータじゃないので飛ばす
        if (isFirstLine) {
            isFirstLine = false;
            continue;
        }

        //空行あったら飛ばす
        if (line.empty())continue;

        //読み込んだ1行を、カンマで分離する処理
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> columns;

        //カンマで区切りで文字を取り出し、columnsに保存
        while (std::getline(ss,token,',')) {
            columns.push_back(token);
        }

        //正しく分離できたかチェック
        if (columns.size() >= 9) {
            Card card;
            card.id = std::stoi(columns[0]);
            card.name = columns[1];
            card.cost = std::stoi(columns[2]);
            card.effectType = static_cast<CardEffectType>(std::stoi(columns[3]));
            card.effectValue = std::stoi(columns[4]);
            card.description = columns[5];
            card.modelName = columns[6];
            card.effectName = columns[7];
            card.seName = columns[8];

            //辞典に登録
            database_[card.id] = card;
        }
    }

    //使い終わったらファイルは閉じる
    file.close();

}


Card CardDatabase::GetCardData(int id) {
    if (database_.find(id) != database_.end()) {
        return database_[id];
    }
    return { -1, "Unknwon",0,CardEffectType::Special,0,"Error","None","None","None" };
}
