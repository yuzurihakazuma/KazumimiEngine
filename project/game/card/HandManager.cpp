#include "HandManager.h"

#include "Engine/3D/Model/ModelManager.h"


void HandManager::Initialize(Camera *camera) {
	hand_.clear();
	handModels_.clear();
	selectedCardIndex_ = 0;
	camera_ = camera;
}

bool HandManager::AddCard(const Card &newCard) {
	//拾えたらtrue、拾えなかったらfalse
	if (hand_.size() < maxHandSize_) {
		// 1. まずモデルのデータを探す
		Model *cardModelData = ModelManager::GetInstance()->FindModel(newCard.modelName);

		// ★超重要：もしモデルが見つからなかったら、絶対にここで弾く！！
		// これがないと空っぽのモデルを描画しようとしてDirectX12がクラッシュします
		if (cardModelData == nullptr) {
			return false;
		}

		// 2. 無事にモデルが見つかった場合だけ、手札に追加する
		hand_.push_back(newCard);

		auto model = std::make_unique<Obj3d>();
		model->Initialize(cardModelData); // ここは絶対に通るようになる

		model->SetCamera(camera_);

		handModels_.push_back(std::move(model));

		return true;
	}
	return false;
}

void HandManager::Update() {
	//手札がなければ何位もしない
	if (hand_.empty()) {
		return;
	}
	auto input = Input::GetInstance();

	//左右キーで選んでいるカードの切り替え
	if (input->Triggerkey(DIK_RIGHT)) {
		selectedCardIndex_++;
		//一番右に行ったらループ
		if (selectedCardIndex_ >= hand_.size()) {
			selectedCardIndex_ = 0;
		}
	}

	if (input->Triggerkey(DIK_LEFT)) {
		selectedCardIndex_--;
		//一番左に行ったらループ
		if (selectedCardIndex_ < 0) {
			selectedCardIndex_ = static_cast<int>(hand_.size()) - 1;
		}
	}

	//3Dモデルをきれいに並べる
	float startX = -5.0f; //最初のカードのX座標
	float spacing = 1.0f; //カード同士のスペース

	

	for (int i = 0; i < handModels_.size(); ++i) {
		//基本位置
		Vector3 pos = { startX + (i * spacing),-2.0f,-4.0f };

		//選ばれているカードだけ、すこし上に浮かせて手元に出す
		if (i == selectedCardIndex_) {
			pos.y += 1.0f; //1.0上に浮かせる
			pos.z -= 1.0f; // 1.0手前に出す
		}

		handModels_[i]->SetTranslation(pos);
		handModels_[i]->Update();
	}
}

void HandManager::Draw() {
	// 3Dモデルを描画する
	for (auto &model : handModels_) {
		model->Draw();
	}
}

Card HandManager::GetSelectedCard() const {
	if (hand_.empty()) {
		// CardDatabase.hの構造体に合わせてエラーカードを返す
		return { -1, "Unknown", 0, CardEffectType::Special, 0, "Error", "None", "None", "None" };
	}
	return hand_[selectedCardIndex_];
}

void HandManager::RemoveSelectedCard() {
	if (hand_.empty()) return;

	// データと見た目の両方を消す！
	hand_.erase(hand_.begin() + selectedCardIndex_);
	handModels_.erase(handModels_.begin() + selectedCardIndex_);

	// 消した後に、カーソルの位置がおかしくならないように調整する
	if (selectedCardIndex_ >= hand_.size()) {
		selectedCardIndex_ = static_cast<int>(hand_.size()) - 1;
	}
	if (selectedCardIndex_ < 0) {
		selectedCardIndex_ = 0;
	}
}

Card HandManager::GetCard(int index) const {
	if (index >= 0 && index < hand_.size()) {
		return hand_[index];
	}
	return Card{ -1, "Unknown", 0, CardEffectType::Special, 0, "Error", "None", "None", "None" }; //エラー回避用のダミーカード
}

void HandManager::RemoveCard(int index) {
	if (index >= 0 && index < hand_.size()) {
		hand_.erase(hand_.begin() + index);
		handModels_.erase(handModels_.begin() + index);
	}
}

int HandManager::GetHandSize() const {
	return static_cast<int>(hand_.size());
}

const std::vector<Card> &HandManager::GetHandList() {
	return hand_;
}