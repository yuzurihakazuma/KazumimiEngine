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
	// 札は「上・真ん中・下」の 3 枚に分けて描く（1 枚を縦に伸ばすと、上下の点線まで内側へ寄って中身に重なるため）
	board_ = Make("resources/ui/hud_board.png");
	board_->SetAnchorPoint({ 0.0f, 0.0f });
	boardMid_ = Make("resources/ui/hud_board.png");
	boardMid_->SetAnchorPoint({ 0.0f, 0.0f });
	boardBottom_ = Make("resources/ui/hud_board.png");
	boardBottom_->SetAnchorPoint({ 0.0f, 0.0f });
	coin_ = Make("resources/ui/hud_coin.png");
	comboLabel_ = Make("resources/ui/hud_combo.png");   // tools/hud/make_combo_assets.py
	comboLabel_->SetAnchorPoint({ 0.5f, 0.5f });
	flower_ = Make("resources/ui/hud_flower.png");
	flower_->SetAnchorPoint({ 0.5f, 0.5f });
	comboFlower_ = Make("resources/ui/hud_combo_flower.png"); // tools/hud/make_combo_assets.py
	comboFlower_->SetAnchorPoint({ 0.5f, 0.5f });
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

// 札：上と下（点線とふち）は絵の大きさのまま、真ん中だけ縦に伸ばす
void GameHud::DrawBoard(float x, float y, float width, float height){
	constexpr float kImageW = 448.0f, kImageH = 178.0f; // hud_board.png（tools/hud/make_hud_assets.py）
	constexpr float kCap = 44.0f;                       // 上・下の、伸ばさない高さ（絵の画素。点線は 20〜22 / 155〜157 の所）
	const float k = width / kImageW;                    // 絵の画素 → HUD（1080p）
	const float cap = ( std::min )( kCap * k, height * 0.5f );
	board_->SetTextureRect(0.0f, 0.0f, kImageW, kCap);
	Place(board_.get(), x, y, width, cap);
	boardMid_->SetTextureRect(0.0f, kCap, kImageW, kImageH - kCap * 2.0f);
	Place(boardMid_.get(), x, y + cap, width, height - cap * 2.0f);
	boardBottom_->SetTextureRect(0.0f, kImageH - kCap, kImageW, kCap);
	Place(boardBottom_.get(), x, y + height - cap, width, cap);
}

void GameHud::Draw(ID3D12GraphicsCommandList* /*commandList*/){
	if ( !board_ ) { return; }
	nextDigit_ = 0;
	const bool showCoins = coinTotal_ > 0; // コインが1枚も置かれていないマップでは出さない
	const bool showFlowers = flowerTotal_ > 0; // かくれ花が隠れている木があるステージだけ
	const bool showComboFlowers = comboFlowerTotal_ > 0; // コンボの的があるステージだけ
	// 段ボールの札（中身に合わせて伸ばす）
	const float boardW = kEggX + kEggStep * ( EggSystem::kMaxEggs - 1 ) + 150.0f;
	const float boardH = ( showCoins ? 182.0f : 128.0f ) + ( showFlowers ? 56.0f : 0.0f ) + ( showComboFlowers ? 56.0f : 0.0f );
	DrawBoard(kLeft - 14.0f, kTop - 14.0f, boardW + 28.0f, boardH + 28.0f);

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
		// たまった数だけ（コンボや？ブロックでも増えるので「置いてある数」とは比べない）
		DrawNumber(std::to_string(coins_), kCoinX + kCoinSize * 0.7f, kCoinY, kCoinCountH);
	}
	// かくれ花「n/全体」（見つけた瞬間はぽんと大きく）
	if ( showFlowers ) {
		const float y = ( showCoins ? kCoinY + 56.0f : kCoinY );
		const float size = kCoinSize * 1.1f * ( 1.0f + 0.5f * flowerPop_ );
		Place(flower_.get(), kCoinX, y, size, size); // コインと同じく、アイコンの真ん中が kCoinX（前は右下へずれて数字に重なっていた）
		DrawNumber(std::to_string(flowers_) + "/" + std::to_string(flowerTotal_), kCoinX + kCoinSize * 0.7f, y, kCoinCountH);
	}
	// コンボ花「見つけた数/全体」（コンボの的から ×8 を出すと増える。見つけた瞬間はぽんと大きく）
	if ( showComboFlowers ) {
		const float y = ( showCoins ? kCoinY + 56.0f : kCoinY ) + ( showFlowers ? 56.0f : 0.0f );
		const float size = kCoinSize * 1.15f * ( 1.0f + 0.6f * comboFlowerPop_ );
		Place(comboFlower_.get(), kCoinX, y, size, size);
		DrawNumber(std::to_string(comboFlowers_) + "/" + std::to_string(comboFlowerTotal_), kCoinX + kCoinSize * 0.7f, y, kCoinCountH);
	}
	// コンボ：画面の上の真ん中に「コンボ」の札と「×n」。当てた瞬間にぽんと大きく
	if ( combo_ > 0 && comboAlpha_ > 0.0f ) {
		const float centerX = 960.0f * ( float ) WindowProc::GetInstance()->GetClientWidth() / ( 1920.0f * scale_ );
		// 消える時は薄くせずに縮む（α で薄くすると、画面の α まで書き換わって黒っぽく抜ける）
		const float pop = ( 1.0f + 0.35f * comboPop_ ) * comboAlpha_;
		Place(comboLabel_.get(), centerX - 70.0f, 130.0f, 364.0f * pop, 105.0f * pop);
		DrawNumber("x" + std::to_string(combo_), centerX + 130.0f, 130.0f, 84.0f * pop);
	}
}
