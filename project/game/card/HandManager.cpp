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

	// カードとカードの間隔
	float spacing = 0.3f;

	// 手札が画面の「中央」に揃うように、最初のカードのX座標を計算する
	float totalWidth = (hand_.size() - 1) * spacing;
	float startX = -totalWidth / 2.0f;

	for (int i = 0; i < handModels_.size(); ++i) {

		// 基本の位置
		Vector3 pos = { startX + (i * spacing), -1.0f, 3.0f };

		// 基本の角度：少しだけ上向き（顔に向けるように）傾ける
		Vector3 rot = { -0.2f, 0.0f, 0.0f };

		// 選択中のカードだけ特別扱い（上に浮いて、手前に来て、まっすぐ向く）
		if (i == selectedCardIndex_) {
			pos.y += 0.3f;  // 上に浮かせる
			pos.z -= 0.5f;  // 手前に出す
			rot.x = 0.0f;   // 角度をまっすぐ正面に向ける
		}

		handModels_[i]->SetScale({ 0.5f,0.5f,0.5f });

		// モデルに座標と角度をセットして更新
		handModels_[i]->SetTranslation(pos);
		handModels_[i]->SetRotation(rot);
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

bool HandManager::SwapSelectedCard(const Card &newCard) {
	if (hand_.empty()) {
		return false;
	}

	//新しいカードの３Dモデルを探す
	Model *cardModelData = ModelManager::GetInstance()->FindModel(newCard.modelName);
	if (cardModelData == nullptr) {
		return false;
	}

	//カードを新しいカードに上書きできる
	hand_[selectedCardIndex_] = newCard;

	//見た目を作り直して入れ替える
	auto model = std::make_unique<Obj3d>();
	model->Initialize(cardModelData);
	model->SetCamera(camera_);

	handModels_[selectedCardIndex_] = std::move(model);

	return true;
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