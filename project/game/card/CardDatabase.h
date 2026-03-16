#pragma once
#include <string>
#include <unordered_map>

//---------効果の種類---------
enum class CardEffectType {
	Attack,     //攻撃
	Heal,       //回復
	Defense,    //防御
	Special     //特殊
};

//---------カードのデータ構造---------
struct Card {
	int id;                    //ID
	std::string name;          //カード名
	int cost;                  //コスト
	CardEffectType effectType; // 効果の種類
	int effectValue;           // 効果の数値（ダメージ量など）
	std::string description;   // 説明文
	std::string modelName;     // 3Dモデルやテクスチャの名前
	std::string effectName;    // エフェクト名
	std::string seName;        // 効果音名
};

class CardDatabase {
private:

	//全カードのデータを保存しておく
	static std::unordered_map<int, Card> database_;

public:

	//初期化
	static void Initialize(const std::string& filePath);

	//IDを指定してカードデータを取得
	static Card GetCardData(int id);

	// 現在登録されているカードの総数を返す
	static size_t GetCardCount() { return database_.size(); }


};

