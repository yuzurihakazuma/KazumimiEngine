#pragma once
#include "engine/math/struct.h"

#include <memory>

namespace KazumimiEngine { class Obj3d; }

// =====================================================================
//  TitleMenu：タイトルのメニュー（丸い的が3つ：つづき／スタート／せってい）。
//   ・的は順番にぴょこっと出てくる
//   ・選んでいる的（恐竜がいちばん近い的）は大きくなって、ゆっくり弾む。ほかは少し暗くなる
//   ・ベロが当たると後ろへぐらっと揺れる。まだ中身の無い項目は横にぶるぶる震える
//  項目の絵は resources/title/menu_*.png（TitleAssets が的の面に貼る）
// =====================================================================
class TitleMenu {
public:
    enum Item { Item_Continue, Item_Start, Item_Options, Item_Count };

    TitleMenu();
    ~TitleMenu();

    void Initialize();
    void Finalize();
    void Update(float deltaTime);
    void Draw();

    void Reset();            // 出る前（見えない状態）へ戻す。位置と選択は初期値に戻さない
    void Show();             // 左から順番に出す
    void ShowImmediately();  // 出る演出を飛ばして全部出す
    bool IsReady() const;    // 全部出終わったか（選べる状態か）

    // 選んでいる的（-1＝どれも選んでいない）。恐竜が近づいた的をシーンが選ぶ
    void SetSelected(int item);
    int  GetSelected() const{ return selected_; }
    int  FindNearest(const KazumimiEngine::Vector3& worldPosition, float maxDistance) const;
    KazumimiEngine::Vector3 GetTargetCenter(int item) const; // 的の面の中心（ベロの狙い先）

    void Hit(int item);      // ベロが当たった
    void Refuse(int item);   // まだ使えない項目を選んだ
    // 倒れている角度と向き（紙芝居遷移の退場。向きは真後ろ=0、右奥が正）
    void SetFall(int item, float angle, float direction){ targets_[item].fallAngle = angle; targets_[item].fallDirection = direction; }

    // 立てる位置（書き換えは調整ウィンドウ用）
    const KazumimiEngine::Vector3& Position(int item) const{ return targets_[item].position; }
    void SetPosition(int item, const KazumimiEngine::Vector3& position){ targets_[item].position = position; }

private:
    // メニューの的 1 つ（見た目・位置・出てくるまでの待ちと、出てからの時間）
    struct Target {
        std::unique_ptr<KazumimiEngine::Obj3d> object;
        KazumimiEngine::Vector3 position {};
        float appearDelay = 0.0f;   // 出始めるまでの待ち(秒)
        float appearTime  = -1.0f;  // 出始めてからの秒数（負＝まだ出ていない）
        float hitTime     = -1.0f;  // ベロが当たってからの秒数（負＝揺れていない）
        float refuseTime  = -1.0f;  // 断ってからの秒数（負＝震えていない）
        float focus       = 0.0f;   // 選ばれ具合（0〜1。なめらかに変わる）
        float fallAngle   = 0.0f;   // 倒れている角度
        float fallDirection = 0.0f; // 倒れる向き
    };

    Target targets_[Item_Count];
    int   selected_ = -1;
    float time_ = 0.0f;
    bool  shown_ = false;
};
