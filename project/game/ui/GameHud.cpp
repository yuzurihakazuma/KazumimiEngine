#include "GameHud.h"
#include "game/egg/EggSystem.h"
#include "game/stage/CoinSystem.h"
#include "engine/2D/Sprite.h"
#include "engine/sdf/SDFText.h"
#include "engine/sdf/SDFManager.h"

#include <string>

GameHud::GameHud() = default;
GameHud::~GameHud() = default;

void GameHud::Initialize(uint32_t circleSrv){
	eggSlots_.clear();
	bellyIcons_.clear();
	for ( int i = 0; i < EggSystem::kMaxEggs; ++i ) {
		auto slotSprite = Sprite::Create(circleSrv, { 36.0f + i * 42.0f, 34.0f });
		slotSprite->SetSize({ 32.0f, 32.0f });
		eggSlots_.push_back(std::move(slotSprite));

		auto bellyIcon = Sprite::Create(circleSrv, { 36.0f + i * 42.0f, 72.0f });
		bellyIcon->SetSize({ 16.0f, 16.0f });
		bellyIcons_.push_back(std::move(bellyIcon));
	}

	// 卵保持数の数字（SDFText。アイコン列の右に「×N（＋お腹）」。小さくてもフチ付きで潰れない）
	eggCountText_ = std::make_unique<SDFText>();
	eggCountText_->Initialize();
	eggCountText_->SetFontSize(34.0f);
	eggCountText_->SetPosition(36.0f + EggSystem::kMaxEggs * 42.0f + 10.0f, 16.0f);
	eggCountText_->SetColor({ 0.75f, 1.0f, 0.8f, 1.0f });
	eggCountText_->SetOutlineWidth(0.22f);
	eggCountText_->SetOutlineColor({ 0.05f, 0.12f, 0.05f, 1.0f });

	// コイン取得数（SDFText。卵HUDの下に金色で「コイン n/全体」）
	coinCountText_ = std::make_unique<SDFText>();
	coinCountText_->Initialize();
	coinCountText_->SetFontSize(30.0f);
	coinCountText_->SetPosition(36.0f, 104.0f);
	coinCountText_->SetColor({ 1.0f, 0.85f, 0.25f, 1.0f });
	coinCountText_->SetOutlineWidth(0.22f);
	coinCountText_->SetOutlineColor({ 0.25f, 0.15f, 0.02f, 1.0f });
}

void GameHud::Update(const EggSystem& eggSystem, bool aiming, const CoinSystem& coinSystem){
	int held  = eggSystem.HeldCount();
	int belly = eggSystem.BellyCount();
	// 構え中は1個を手に持っている扱い＝列から1個減らして見せる。
	// キャンセルすると構えが解けて自動的に列へ戻る（実際の消費は投げた瞬間だけ）
	if ( aiming && held > 0 ) { --held; }
	for ( int i = 0; i < ( int ) eggSlots_.size(); ++i ) {
		eggSlots_[i]->SetColor(( i < held )
			? Vector4 { 0.55f, 1.0f, 0.6f, 0.95f }    // 保持中＝ヨッシー緑
			: Vector4 { 0.25f, 0.25f, 0.28f, 0.35f }); // 空き＝薄いグレー
		eggSlots_[i]->Update();
	}
	for ( int i = 0; i < ( int ) bellyIcons_.size(); ++i ) {
		// お腹にためた数だけオレンジで表示（それ以外は完全透明）
		bellyIcons_[i]->SetColor({ 1.0f, 0.75f, 0.25f, ( i < belly ) ? 0.9f : 0.0f });
		bellyIcons_[i]->Update();
	}
	// 数字表示：「×保持数」＋お腹にいる分は「（＋n）」
	if ( eggCountText_ ) {
		std::string countText = "×" + std::to_string(held);
		if ( belly > 0 ) { countText += "（＋" + std::to_string(belly) + "）"; }
		eggCountText_->SetText(countText);
	}
	// コイン取得数（コインが1枚も置かれていないマップでは出さない）
	showCoinCount_ = ( coinSystem.TotalCount() > 0 );
	if ( coinCountText_ ) {
		coinCountText_->SetText(showCoinCount_
			? "コイン " + std::to_string(coinSystem.CollectedCount()) + "/" + std::to_string(coinSystem.TotalCount())
			: "");
	}
}

void GameHud::Draw(ID3D12GraphicsCommandList* commandList){
	for ( auto& slotSprite : eggSlots_ )  { slotSprite->Draw(); }
	for ( auto& bellyIcon : bellyIcons_ ) { bellyIcon->Draw(); }
	// 数字（SDFText。スプライトと同じターゲットへ描く）
	if ( eggCountText_ ) {
		SDFManager::GetInstance()->DrawTextItem(commandList, *eggCountText_, "jpdot");
	}
	if ( coinCountText_ && showCoinCount_ ) {
		SDFManager::GetInstance()->DrawTextItem(commandList, *coinCountText_, "jpdot");
	}
}
