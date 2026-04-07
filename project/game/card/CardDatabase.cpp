#include "game/card/CardDatabase.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <random>

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
        if (columns.size() >= 11) {
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
            card.rarity = static_cast<CardRarity>(std::stoi(columns[9]));

            // CSVの11番目のデータ(インデックス10)が "0" じゃなければ true にする！
            card.canEnemyUse = (std::stoi(columns[10]) != 0);

            //辞典に登録
            database_[card.id] = card;
        }
    }

    //使い終わったらファイルは閉じる
    file.close();

}

void CardDatabase::LoadAdditionalCards(const std::string &filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    bool isFirstLine = true;

    while (std::getline(file, line)) {
        if (isFirstLine) {
            isFirstLine = false;
            continue;
        }
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> columns;

        while (std::getline(ss, token, ',')) {
            columns.push_back(token);
        }

        if (columns.size() >= 11) {
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
            card.rarity = static_cast<CardRarity>(std::stoi(columns[9]));

            // CSVの11番目のデータ(インデックス10)が "0" じゃなければ true にする！
            card.canEnemyUse = (std::stoi(columns[10]) != 0);

            // 辞書に追加！（ID101などがここに入ります）
            database_[card.id] = card;
        }
    }
}


Card CardDatabase::GetCardData(int id) {
    if (database_.find(id) != database_.end()) {
        return database_[id];
    }
    return { -1, "Unknwon", 0, CardEffectType::Special, 0, "Error", "None", "None", "None", CardRarity::Common, false };
}

Card CardDatabase::GetRandomEnemyUsableCard() {
    std::vector<Card> usableCards;

    // データベース内の全カードをチェックして、敵が使えるやつだけリストに入れる
    for (const auto &pair : database_) {
        if (pair.second.canEnemyUse) {
            usableCards.push_back(pair.second);
        }
    }

    // 万が一、使えるカードが1枚も設定されていなかった時の保険（ID:1を返す）
    if (usableCards.empty()) {
        return GetCardData(1);
    }

    // リストの中からランダムに1つ選ぶ
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, (int)usableCards.size() - 1);

    return usableCards[dist(gen)];
}

Card CardDatabase::GetRandomPlayerCard() {
    std::vector<Card> dropCards;

	// データベースの中身を全部チェックする
    for (const auto &pair : database_) {
        // IDが100未満、かつ「IDが1ではない」カードだけをリストに入れる
        if (pair.second.id > 1 && pair.second.id < 100) {
            dropCards.push_back(pair.second);
        }
    }

    // 万が一拾えるカードがなかったら、エラーで回避でID2枚選ぶ
    if (dropCards.empty()) {
        return GetCardData(2);
    }

    // プレイヤー用リストの中からランダムに1枚選らぶ
    int randomIndex = rand() % dropCards.size();
    return dropCards[randomIndex];
}
