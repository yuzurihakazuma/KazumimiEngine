#pragma once
#include <vector>
#include <string>

//---------カードのデータ構造---------
struct Card {
	int id;           // カードのID
	std::string name; //カード名
	int cost;         //使用する為のコスト

};

class HandManager {
private:

	std::vector<Card> hand_;     //現在の手札
	const int maxHandSize_ = 10; //最大手札枚数


public:
	//初期化
	void Initialize();

	//ダンジョンでカードを拾う
	bool AddCard(const Card &newCard);

	//指定した番号のカード情報を取得
	Card GetCard(int index)const;

	//カードを使った後に、手札から消す処理
	void RemoveCard(int index);

	//現在の手札の枚数を取得
	int GetHandSize() const;

	//手札の全情報を取得
	const std::vector<Card> &GetHandList();

};

