#pragma once
// =====================================================================
//  GameOverScreen：ゲームオーバーの画面（ヨッシー クラフトワールドの画面のように、フェルトと段ボールの工作）。
//   紫のフェルトの背景に、ギザギザの縁の緑の板。「GAME OVER」が上から落ちてきて弾み、
//   「やりなおす」「ステージをえらぶ」のボタンが下からすべり込む。指さしの手袋が選んでいるボタンをさす。
//   板の左下には、しょんぼり寝ころぶ恐竜。右下には、勝ちほこるドングリン（敵）
//   置き場所は 1920×1080 を基準に、画面の大きさに合わせて縮める（素材は tools/gameover/make_gameover_assets.py）
// =====================================================================
#include "engine/math/struct.h"

#include <memory>

class Sprite;

class GameOverScreen {
public:
    GameOverScreen();
    ~GameOverScreen();

    void Initialize();
    void Finalize();

    // 出す（板 → 文字 → ボタン → 恐竜・敵 → 手袋の順に出てくる）。boardShown＝板はもう出ている（紙玉が開いて板になった）
    void Open(float duration, bool boardShown = false);
    // しまう（全部ちぢんで消える）。keepBoard＝板だけ残す（そのあと板の紙が丸まる）
    void Close(float duration, bool keepBoard = false);
    void SetBoardVisible(bool visible){ boardVisible_ = visible; }
    const Vector2& BoardSize() const{ return boardSize_; } // 板の絵の大きさ（1920×1080 基準の画素）
    void SetBackground(float alpha){ backgroundAlpha_ = alpha; }
    void SetChoice(int choice);
    bool IsOpened() const{ return state_ == State::Opened; }
    bool IsClosed() const{ return state_ == State::Closed && closeTime_ >= closeDuration_; }

    void Update(float deltaTime);
    void Draw();

private:
    enum class State { Hidden, Opening, Opened, Closed };
    void DrawAt(Sprite* sprite, const Vector2& size, float x, float y, float scale, float alpha, float rotation = 0.0f);

    std::unique_ptr<Sprite> background_, board_, title_, dino_, enemy_, hand_;
    std::unique_ptr<Sprite> buttons_[2], buttonsOn_[2];
    Vector2 boardSize_ {}, titleSize_ {}, dinoSize_ {}, enemySize_ {}, handSize_ {}, buttonSize_ {};

    State state_ = State::Hidden;
    float time_ = 0.0f, openDuration_ = 1.0f;
    float closeTime_ = 0.0f, closeDuration_ = 0.25f;
    float backgroundAlpha_ = 0.0f;
    int   choice_ = 0;
    float choiceTime_ = 0.0f; // 選び直してからの秒数（手袋がすっと動く・ボタンが弾む）
    float handY_ = 0.0f;      // 手袋の今の高さ（なめらかに追う）
    bool  boardShown_ = false, keepBoard_ = false, boardVisible_ = true;
};
