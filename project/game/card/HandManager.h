#pragma once
#include <vector>
#include <string>
#include "game/card/CardDatabase.h"

#include "engine/3d/obj/Obj3d.h"
#include "engine/base/Input.h"

class Camera;

class HandManager {
private:

	std::vector<Card> hand_;     //現在の手札
	std::vector<std::unique_ptr<Obj3d>> handModels_; //カードの見た目

	int selectedCardIndex_ = 0;   //現在選んでいるカードの番号
	const int maxHandSize_ = 10;  //最大手札枚数

	Camera *camera_ = nullptr;

public:
	//初期化
	void Initialize(Camera *camera);

	//ダンジョンでカードを拾う
	bool AddCard(const Card &newCard);

	//更新
	void Update();

	//描画
	void Draw();

	//指定した番号のカード情報を取得
	Card GetCard(int index)const;

	//カードを使った後に、手札から消す処理
	void RemoveCard(int index);

	//現在の手札の枚数を取得
	int GetHandSize() const;

	//手札の全情報を取得
	const std::vector<Card> &GetHandList();

	//今選んでいるカードを取得
	Card GetSelectedCard() const;
	void RemoveSelectedCard();//次のカードを選ぶ

};

