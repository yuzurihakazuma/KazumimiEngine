#include "GameHud.h"
#include "game/egg/EggSystem.h"
#include "game/stage/CoinSystem.h"
#include "engine/2D/Sprite.h"
#include "engine/base/WindowProc.h"
#include "engine/graphics/TextureManager.h"

#include <algorithm>
#include <cstring>

namespace {
	constexpr const char* kDigitChars = "0123456789/x+"; // hud_digits.png の並び（x＝×）
	constexpr float kDigitCellW = 64.0f, kDigitCellH = 96.0f;
	constexpr int   kMaxDigits = 16;
	// 1080p での置き場所
	constexpr float kLeft = 22.0f, kTop = 18.0f;    // 札の左上
	constexpr float kEggX = 70.0f, kEggStep = 50.0f, kEggY = 66.0f, kEggW = 44.0f, kEggH = 54.0f;
	constexpr float kBellyY = 104.0f, kBellySize = 16.0f;
	constexpr float kCoinX = 70.0f, kCoinY = 144.0f, kCoinSize = 44.0f;
	constexpr float kCountH = 50.0f, kCoinCountH = 44.0f;

	std::unique_ptr<Sprite> Make(const char* path){
		return Sprite::Create(TextureManager::GetInstance()->Load(path).srvIndex, { 0.0f, 0.0f });
	}
}

GameHud::GameHud() = default;
GameHud::~GameHud() = default;

void GameHud::Initialize(){
	board_ = Make("resources/ui/hud_board.png");
	board_->SetAnchorPoint({ 0.0f, 0.0f });
	coin_ = Make("resources/ui/hud_coin.png");
	eggs_.clear();
	emptyEggs_.clear();
	belly_.clear();
	for ( int i = 0; i < EggSystem::kMaxEggs; ++i ) {
		eggs_.push_back(Make("resources/ui/hud_egg.png"));
		emptyEggs_.push_back(Make("resources/ui/hud_egg_empty.png"));
		belly_.push_back(Make("resources/ui/hud_belly.png"));
	}
	const uint32_t digits = TextureManager::GetInstance()->Load("resources/ui/hud_digits.png").srvIndex;
	digits_.clear();
	for ( int i = 0; i < kMaxDigits; ++i ) {
		auto sprite = Sprite::Create(digits, { 0.0f, 0.0f });
		sprite->SetAnchorPoint({ 0.0f, 0.5f });
		digits_.push_back(std::move(sprite));
	}
}

void GameHud::Update(const EggSystem& eggSystem, bool aiming, const CoinSystem& coinSystem){
	held_ = eggSystem.HeldCount();
	belly_count_ = eggSystem.BellyCount();
	// 構え中は1個を手に持っている扱い＝列から1個減らして見せる。
	// キャンセルすると構えが解けて自動的に列へ戻る（実際の消費は投げた瞬間だけ）
	if ( aiming && held_ > 0 ) { --held_; }
	coins_ = coinSystem.CollectedCount();
	coinTotal_ = coinSystem.TotalCount();
	scale_ = ( float ) WindowProc::GetInstance()->GetClientHeight() / 1080.0f;
}

void GameHud::Place(Sprite* sprite, float x, float y, float width, float height, float alpha){
	sprite->SetPosition({ x * scale_, y * scale_ });
	sprite->SetSize({ width * scale_, height * scale_ });
	sprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
	sprite->Update();
	sprite->Draw();
}

float GameHud::DrawNumber(const std::string& text, float x, float y, float height){
	const float width = height * kDigitCellW / kDigitCellH;
	const float step = width * 0.78f; // 字のふちが重なるくらいに詰める
	for ( char c : text ) {
		const char* found = std::strchr(kDigitChars, c);
		if ( !found || nextDigit_ >= ( int ) digits_.size() ) { continue; }
		Sprite* sprite = digits_[nextDigit_++].get(); // 1枚を1フレームに2回描くと上書きされるので、字ごとに別の物
		sprite->SetTextureRect(( float ) ( found - kDigitChars ) * kDigitCellW, 0.0f, kDigitCellW, kDigitCellH);
		Place(sprite, x, y, width, height);
		x += step;
	}
	return x + ( width - step );
}

void GameHud::Draw(ID3D12GraphicsCommandList* /*commandList*/){
	if ( !board_ ) { return; }
	nextDigit_ = 0;
	const bool showCoins = coinTotal_ > 0; // コインが1枚も置かれていないマップでは出さない
	// 段ボールの札（中身に合わせて伸ばす）
	const float boardW = kEggX + kEggStep * ( EggSystem::kMaxEggs - 1 ) + 150.0f;
	const float boardH = showCoins ? 182.0f : 128.0f;
	board_->SetAnchorPoint({ 0.0f, 0.0f });
	Place(board_.get(), kLeft - 14.0f, kTop - 14.0f, boardW + 28.0f, boardH + 28.0f);

	// 卵：持っている分は卵の切り絵、空きは点線の卵。その下に、お腹にためた分のオレンジの丸
	for ( int i = 0; i < EggSystem::kMaxEggs; ++i ) {
		const float x = kEggX + kEggStep * ( float ) i;
		Place(i < held_ ? eggs_[i].get() : emptyEggs_[i].get(), x, kEggY, kEggW, kEggH);
		if ( i < belly_count_ ) { Place(belly_[i].get(), x, kBellyY, kBellySize, kBellySize); }
	}
	// 「×N」（お腹にためた分があれば「+n」）
	const float countX = kEggX + kEggStep * ( EggSystem::kMaxEggs - 1 ) + 34.0f;
	std::string count = "x" + std::to_string(held_);
	if ( belly_count_ > 0 ) { count += "+" + std::to_string(belly_count_); }
	DrawNumber(count, countX, kEggY, kCountH);

	// コイン「n/全体」
	if ( showCoins ) {
		Place(coin_.get(), kCoinX, kCoinY, kCoinSize, kCoinSize);
		DrawNumber(std::to_string(coins_) + "/" + std::to_string(coinTotal_), kCoinX + kCoinSize * 0.7f, kCoinY, kCoinCountH);
	}
}
