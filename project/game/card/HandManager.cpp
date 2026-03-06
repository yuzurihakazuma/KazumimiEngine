#include "HandManager.h"

void HandManager::Initialize() {
	hand_.clear();
}

bool HandManager::AddCard(const Card &newCard) {
	//拾えたらtrue、拾えなかったらfalse
	if (hand_.size() < maxHandSize_) {
		hand_.push_back(newCard);
		return true;
	}
	return false;
}

Card HandManager::GetCard(int index) const {
	if (index >= 0 && index < hand_.size()) {
		return hand_[index];
	}
	return Card{-1,"Invalid",0}; //エラー回避用のダミーカード
}

void HandManager::RemoveCard(int index) {
	if (index >= 0 && index < hand_.size()) {
		hand_.erase(hand_.begin() + index);
	}
}

int HandManager::GetHandSize() const {
	return static_cast<int>(hand_.size());
}

const std::vector<Card> &HandManager::GetHandList() {
	return hand_;
}