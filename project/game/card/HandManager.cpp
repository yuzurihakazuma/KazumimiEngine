#include "HandManager.h"

#include "Engine/3D/Model/ModelManager.h"

void HandManager::Initialize(Camera* camera, uint32_t noiseTextureIndex) {
	hand_.clear();
	handModels_.clear();
	isDissolving_.clear();
	dissolveThresholds_.clear();
	selectedCardIndex_ = 0;
	camera_ = camera;
	noiseTextureIndex_ = noiseTextureIndex;
}

bool HandManager::AddCard(const Card& newCard) {
	//拾えたらtrue、拾えなかったらfalse
	if (hand_.size() < maxHandSize_) {
		// 1. まずモデルのデータを探す
		Model* cardModelData = ModelManager::GetInstance()->FindModel(newCard.modelName);

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

		// ディゾルブ用の設定
		model->SetNoiseTexture(noiseTextureIndex_);
		model->SetDissolveThreshold(0.0f);

		handModels_.push_back(std::move(model));

		// ディゾルブ状態も追加
		isDissolving_.push_back(false);
		dissolveThresholds_.push_back(0.0f);

		return true;
	}
	return false;
}

void HandManager::Update() {
	//手札がなければ何もしない
	if (hand_.empty()) {
		return;
	}
	auto input = Input::GetInstance();

	//左右キーで選んでいるカードの切り替え
	if (input->Triggerkey(DIK_RIGHT)) {
		selectedCardIndex_++;
		//一番右に行ったらループ
		if (selectedCardIndex_ >= static_cast<int>(hand_.size())) {
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
	float totalWidth = (static_cast<float>(hand_.size()) - 1.0f) * spacing;
	float startX = -totalWidth / 2.0f;

	// 消すカードを後でまとめて処理するための配列
	std::vector<int> removeIndices;

	for (int i = 0; i < static_cast<int>(handModels_.size()); ++i) {

		// ディゾルブ中なら進行度を増やす
		if (isDissolving_[i]) {
			dissolveThresholds_[i] += 0.05f; // 消える速度

			if (dissolveThresholds_[i] > 1.0f) {
				dissolveThresholds_[i] = 1.0f;
			}

			handModels_[i]->SetDissolveThreshold(dissolveThresholds_[i]);

			// 完全に消えたら削除予約
			if (dissolveThresholds_[i] >= 1.0f) {
				removeIndices.push_back(i);
				continue;
			}
		}

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

	// 後ろから削除
	for (int i = static_cast<int>(removeIndices.size()) - 1; i >= 0; --i) {
		RemoveCard(removeIndices[i]);
	}
}

void HandManager::Draw() {
	// 3Dモデルを描画する
	for (auto& model : handModels_) {
		model->Draw();
	}
}

Card HandManager::GetSelectedCard() const {
	if (hand_.empty()) {
		// CardDatabase.hの構造体に合わせてエラーカードを返す
		return { -1, "Unknown", 0, CardEffectType::Special, 0, "Error", "None", "None", "None" };
	}

	// ディゾルブ中のカードは使えないようにする
	if (selectedCardIndex_ >= 0 &&
		selectedCardIndex_ < static_cast<int>(isDissolving_.size()) &&
		isDissolving_[selectedCardIndex_]) {
		return { -1, "Unknown", 0, CardEffectType::Special, 0, "Error", "None", "None", "None" };
	}

	return hand_[selectedCardIndex_];
}

void HandManager::RemoveSelectedCard() {
	if (hand_.empty()) return;

	// データと見た目の両方を消す！
	hand_.erase(hand_.begin() + selectedCardIndex_);
	handModels_.erase(handModels_.begin() + selectedCardIndex_);
	isDissolving_.erase(isDissolving_.begin() + selectedCardIndex_);
	dissolveThresholds_.erase(dissolveThresholds_.begin() + selectedCardIndex_);

	// 消した後に、カーソルの位置がおかしくならないように調整する
	if (selectedCardIndex_ >= static_cast<int>(hand_.size())) {
		selectedCardIndex_ = static_cast<int>(hand_.size()) - 1;
	}
	if (selectedCardIndex_ < 0) {
		selectedCardIndex_ = 0;
	}
}

bool HandManager::SwapSelectedCard(const Card& newCard) {
	if (hand_.empty()) {
		return false;
	}

	//新しいカードの３Dモデルを探す
	Model* cardModelData = ModelManager::GetInstance()->FindModel(newCard.modelName);
	if (cardModelData == nullptr) {
		return false;
	}

	//カードを新しいカードに上書きできる
	hand_[selectedCardIndex_] = newCard;

	//見た目を作り直して入れ替える
	auto model = std::make_unique<Obj3d>();
	model->Initialize(cardModelData);
	model->SetCamera(camera_);
	model->SetNoiseTexture(noiseTextureIndex_);
	model->SetDissolveThreshold(0.0f);

	handModels_[selectedCardIndex_] = std::move(model);
	isDissolving_[selectedCardIndex_] = false;
	dissolveThresholds_[selectedCardIndex_] = 0.0f;

	return true;
}

Card HandManager::GetCard(int index) const {
	if (index >= 0 && index < static_cast<int>(hand_.size())) {
		return hand_[index];
	}
	return Card{ -1, "Unknown", 0, CardEffectType::Special, 0, "Error", "None", "None", "None" }; //エラー回避用のダミーカード
}

void HandManager::RemoveCard(int index) {
	if (index >= 0 && index < static_cast<int>(hand_.size())) {
		hand_.erase(hand_.begin() + index);
		handModels_.erase(handModels_.begin() + index);
		isDissolving_.erase(isDissolving_.begin() + index);
		dissolveThresholds_.erase(dissolveThresholds_.begin() + index);

		if (selectedCardIndex_ >= static_cast<int>(hand_.size())) {
			selectedCardIndex_ = static_cast<int>(hand_.size()) - 1;
		}
		if (selectedCardIndex_ < 0) {
			selectedCardIndex_ = 0;
		}
	}
}

int HandManager::GetHandSize() const {
	return static_cast<int>(hand_.size());
}

const std::vector<Card>& HandManager::GetHandList() {
	return hand_;
}

void HandManager::StartDissolveSelectedCard() {
	if (hand_.empty()) {
		return;
	}

	if (selectedCardIndex_ < 0 || selectedCardIndex_ >= static_cast<int>(hand_.size())) {
		return;
	}

	// すでにディゾルブ中なら何もしない
	if (isDissolving_[selectedCardIndex_]) {
		return;
	}

	isDissolving_[selectedCardIndex_] = true;
	dissolveThresholds_[selectedCardIndex_] = 0.0f;

	if (handModels_[selectedCardIndex_]) {
		handModels_[selectedCardIndex_]->SetDissolveThreshold(0.0f);
	}
}

bool HandManager::IsSelectedCardDissolving() const {
	if (hand_.empty()) {
		return false;
	}

	if (selectedCardIndex_ < 0 || selectedCardIndex_ >= static_cast<int>(isDissolving_.size())) {
		return false;
	}

	return isDissolving_[selectedCardIndex_];
}