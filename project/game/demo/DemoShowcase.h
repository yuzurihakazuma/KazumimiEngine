#pragma once
// =====================================================================
//  DemoShowcase：エンジン機能の展示用オブジェクトをまとめて管理するクラス。
//   ・回転キューブ（ディゾルブ＋ブルーム＋環境マップ＋ギズモ操作対象）
//   ・リングオーラ／円柱オーラ（加算合成＋UVスクロール）
//   ・スキンメッシュの人形（Skinning。定位置でその場歩き。エディタのアニメ操作対象）
//   ・GPUパーティクルのエミッター（Gキーで更新デモ）
//   ・SDFボリュームの卵（エロージョン⇔モーフのデモ＋ImGuiパネル）
//   ・Spaceキーの HitEffect デモ／Pキーのパーティクルバースト
//
//  ゲーム本編（レール・プレイヤー・敵・卵）とは無関係の「見本」なので、
//  GamePlayScene から分離した。他のシーンでも Initialize→Update→Draw を
//  同じ順で呼ぶだけで同じ展示が出せる。丸ごと消したい時はこのクラスの
//  生成をやめるだけでよい。
// =====================================================================
#include "engine/graphics/TextureManager.h" // TextureData
#include "engine/particle/GPUParticleEmitter.h"
#include "engine/math/struct.h"

#include <d3d12.h>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

class Obj3d;
class SkinnedObj3d;
class SDFVolumeObject;
class HitEffect;
class Camera;
class Input;

class DemoShowcase {
public:
    DemoShowcase();
    ~DemoShowcase(); // unique_ptr 群のため cpp 側で定義

    // モデル・SDF・スプライトの生成（シーンの LoadResources/SetupDemoObjects から呼ぶ。
    // commandList は SDF ボリュームの転送用。textures は読み込み済みテクスチャ辞書）
    void Initialize(ID3D12GraphicsCommandList* commandList,
                    std::unordered_map<std::string, TextureData>& textures);

    // プレイ中のデモ入力（Space=BGM+HitEffect / P=パーティクルバースト）と HitEffect の進行
    void UpdatePlay(Input* input, Camera* camera,
                    std::unordered_map<std::string, TextureData>& textures,
                    const std::string& bgmFile);

    // モード問わず毎フレームの見た目更新（UVスクロール・SDF自動エロージョン・
    // スキンメッシュ・GPUパーティクル(Gキー)・エミッター）
    void UpdateVisuals(Input* input, Camera* camera);

    // Edit→Play 切替時のリセット（出しっぱなしの HitEffect を消す）
    void OnPlayStart();

    // --- 描画（呼び出し順はシーンの描画パスに合わせる）---
    void DrawOpaque();                                  // 不透明（回転キューブ・スキンメッシュ）
    void DrawAdditive();                                // オーラ2種＋HitEffect（加算合成）
    void DrawSdf(ID3D12GraphicsCommandList* commandList); // SDF卵（専用PSO。MRTパス末尾で）

    // SDF卵のエロージョン操作パネル
    void DrawImGui();

private:
    // 回転キューブ（ディゾルブ＋ブルーム。エディタのギズモ操作対象）
    std::unique_ptr<Obj3d> dissolveCube_;

    // リングオーラ（地面の魔法陣風）と円柱オーラ（加算合成＋UVスクロール）
    std::unique_ptr<Obj3d> auraRing_;
    float auraRingScroll_ = 0.0f;
    std::unique_ptr<Obj3d> auraCylinder_;
    float auraCylinderScroll_ = 0.0f;

    // スキンメッシュの人形（Skinning機能の展示。定位置でその場歩き）
    std::unique_ptr<SkinnedObj3d> skinnedHuman_;

    // GPUパーティクルのエミッター
    GPUParticleEmitter particleEmitter_;

    // SDFボリュームの卵（エロージョン⇔モーフのデモ）
    std::unique_ptr<SDFVolumeObject> sdfEgg_;
    bool  sdfErodeAuto_ = true; // エロージョン自動アニメ（溶ける⇔生える）
    float sdfErodeTime_ = 0.0f;

    // Spaceキーのデモで出す立体エフェクト
    std::list<std::unique_ptr<HitEffect>> hitEffects_;
};
