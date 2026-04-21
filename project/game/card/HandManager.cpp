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
	// 1. まずモデルのデータを探す
	Model *cardModelData = ModelManager::GetInstance()->FindModel(newCard.modelName);

	// もしモデルが見つからなかったら弾く
	if (cardModelData == nullptr) {
		return false;
	}

	// =========================================================
	// ★ 修正ポイント①：消えかかっている（ディゾルブ中の）枠を再利用する！
	// =========================================================
	for (size_t i = 0; i < hand_.size(); ++i) {
		if (isDissolving_[i]) {
			// ディゾルブ中のカードを、拾った新しいカードで上書きする
			hand_[i] = newCard;

			auto model = std::make_unique<Obj3d>();
			model->Initialize(cardModelData);
			model->SetCamera(camera_);
			model->SetNoiseTexture(noiseTextureIndex_);
			model->SetDissolveThreshold(0.0f);
			// 再利用枠でもカード種別に合わせてディゾルブ色を更新する
			if (newCard.effectType == CardEffectType::Attack) {
				model->SetDissolveColor({ 1.0f, 0.2f, 0.05f });
			} else if (newCard.effectType == CardEffectType::Heal) {
				model->SetDissolveColor({ 0.1f, 1.0f, 0.2f });
			} else if (newCard.effectType == CardEffectType::Defense) {
				model->SetDissolveColor({ 0.0f, 0.5f, 1.0f });
			} else if (newCard.effectType == CardEffectType::Special) {
				model->SetDissolveColor({ 0.7f, 0.2f, 1.0f });
			}

			handModels_[i] = std::move(model);

			// ディゾルブをキャンセルして、新しいカードを実体化させる
			isDissolving_[i] = false;
			dissolveThresholds_[i] = 0.0f;

			return true; // 取得成功！
		}
	}

	// =========================================================
	// ★ 修正ポイント②：ディゾルブ中の枠がなく、まだ上限に達していなければ普通に追加
	// =========================================================
	if (hand_.size() < maxHandSize_) {
		hand_.push_back(newCard);

		auto model = std::make_unique<Obj3d>();
		model->Initialize(cardModelData);
		model->SetCamera(camera_);
		model->SetNoiseTexture(noiseTextureIndex_);
		model->SetDissolveThreshold(0.0f);
		// 新規追加時もカード種別に応じた色でディゾルブさせる
		if (newCard.effectType == CardEffectType::Attack) {
			model->SetDissolveColor({ 1.0f, 0.2f, 0.05f });
		} else if (newCard.effectType == CardEffectType::Heal) {
			model->SetDissolveColor({ 0.1f, 1.0f, 0.2f });
		} else if (newCard.effectType == CardEffectType::Defense) {
			model->SetDissolveColor({ 0.0f, 0.5f, 1.0f });
		} else if (newCard.effectType == CardEffectType::Special) {
			model->SetDissolveColor({ 0.7f, 0.2f, 1.0f });
		}

		handModels_.push_back(std::move(model));
		isDissolving_.push_back(false);
		dissolveThresholds_.push_back(0.0f);

		return true; // 取得成功！
	}

	// 完全に満杯なら拾えない（交換フェイズへ）
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
			dissolveThresholds_[i] += 0.015f; // 消える速度

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
			pos.z -= 0.3f;  // 手前に出す
			rot.x = 0.0f;   // 角度をまっすぐ正面に向ける
		}

		handModels_[i]->SetScale({ 0.3f,0.3f,0.3f });

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
		return { -1, "Unknown", 0, CardEffectType::Special, 0, "Error", "None", "None", "None", CardRarity::Common, false };
	}

	// ディゾルブ中のカードは使えないようにする
	if (selectedCardIndex_ >= 0 &&
		selectedCardIndex_ < static_cast<int>(isDissolving_.size()) &&
		isDissolving_[selectedCardIndex_]) {
		return { -1, "Unknown", 0, CardEffectType::Special, 0, "Error", "None", "None", "None", CardRarity::Common, false };
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

	if (newCard.effectType == CardEffectType::Attack) {
		model->SetDissolveColor({ 1.0f, 0.2f, 0.05f });
	} else if (newCard.effectType == CardEffectType::Heal) {
		model->SetDissolveColor({ 0.1f, 1.0f, 0.2f });
	} else if (newCard.effectType == CardEffectType::Defense) {
		model->SetDissolveColor({ 0.0f, 0.5f, 1.0f });
	} else if (newCard.effectType == CardEffectType::Special) {
		model->SetDissolveColor({ 0.7f, 0.2f, 1.0f });
	}

	handModels_[selectedCardIndex_] = std::move(model);
	isDissolving_[selectedCardIndex_] = false;
	dissolveThresholds_[selectedCardIndex_] = 0.0f;

	return true;
}

Card HandManager::GetCard(int index) const {
	if (index >= 0 && index < static_cast<int>(hand_.size())) {
		return hand_[index];
	}
	return Card{ -1, "Unknown", 0, CardEffectType::Special, 0, "Error", "None", "None", "None", CardRarity::Common, false }; //エラー回避用のダミーカード
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
		// 使用直前に選択中カードの色を再反映して、ブルーム色ずれを防ぐ
		if (hand_[selectedCardIndex_].effectType == CardEffectType::Attack) {
			handModels_[selectedCardIndex_]->SetDissolveColor({ 1.0f, 0.2f, 0.05f });
		} else if (hand_[selectedCardIndex_].effectType == CardEffectType::Heal) {
			handModels_[selectedCardIndex_]->SetDissolveColor({ 0.1f, 1.0f, 0.2f });
		} else if (hand_[selectedCardIndex_].effectType == CardEffectType::Defense) {
			handModels_[selectedCardIndex_]->SetDissolveColor({ 0.0f, 0.5f, 1.0f });
		} else if (hand_[selectedCardIndex_].effectType == CardEffectType::Special) {
			handModels_[selectedCardIndex_]->SetDissolveColor({ 0.7f, 0.2f, 1.0f });
		}
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

void HandManager::AddPendingCard(const Card &pendingCard) {
	// 上限を無視して強制的に末尾に追加
	hand_.push_back(pendingCard);

	// モデルデータを探して作成
	Model *cardModelData = ModelManager::GetInstance()->FindModel(pendingCard.modelName);
	if (cardModelData == nullptr)return;

	auto model = std::make_unique<Obj3d>();
	model->Initialize(cardModelData);
	model->SetCamera(camera_);
	model->SetNoiseTexture(noiseTextureIndex_);
	model->SetDissolveThreshold(0.0f);

	// カードの種類による色設定
	if (pendingCard.effectType == CardEffectType::Attack) {
		model->SetDissolveColor({ 1.0f,0.2f,0.05f });
	} else if (pendingCard.effectType == CardEffectType::Heal) {
		model->SetDissolveColor({ 0.1f, 1.0f, 0.2f });
	} else if (pendingCard.effectType == CardEffectType::Defense) {
		model->SetDissolveColor({ 0.0f, 0.5f, 1.0f });
	} else if (pendingCard.effectType == CardEffectType::Special) {
		model->SetDissolveColor({ 0.7f, 0.2f, 1.0f });
	}

	handModels_.push_back(std::move(model));
	isDissolving_.push_back(false);
	dissolveThresholds_.push_back(0.0f);

	// ★ 拾ったカード（一番右）にカーソルを強制的に合わせておく
	selectedCardIndex_ = static_cast<int>(hand_.size()) - 1;
}

	

void HandManager::RemoveCardImmediate(int index) {
	if (index < 0 || index >= static_cast<int>(hand_.size())) return;

	hand_.erase(hand_.begin() + index);
	handModels_.erase(handModels_.begin() + index);
	isDissolving_.erase(isDissolving_.begin() + index);
	dissolveThresholds_.erase(dissolveThresholds_.begin() + index);

	// ★ 修正1：ここも selectedCardIndex_ に直しました
	if (selectedCardIndex_ >= static_cast<int>(hand_.size())) {
		selectedCardIndex_ = static_cast<int>(hand_.size()) - 1;
	}
	if (selectedCardIndex_ < 0) {
		selectedCardIndex_ = 0;
	}
}
#include "HandManager.h"

#include "Engine/3D/Model/ModelManager.h"

namespace {
// カード種別ごとのディゾルブ色をモデルへ反映する
void ApplyCardDissolveColor(Obj3d* model, CardEffectType effectType) {
	if (model == nullptr) {
		return;
	}

	if (effectType == CardEffectType::Attack) {
		model->SetDissolveColor({ 1.0f, 0.2f, 0.05f });
	} else if (effectType == CardEffectType::Heal) {
		model->SetDissolveColor({ 0.1f, 1.0f, 0.2f });
	} else if (effectType == CardEffectType::Defense) {
		model->SetDissolveColor({ 0.0f, 0.5f, 1.0f });
	} else if (effectType == CardEffectType::Special) {
		model->SetDissolveColor({ 0.7f, 0.2f, 1.0f });
	}
}
}
