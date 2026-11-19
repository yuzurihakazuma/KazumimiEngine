[![DebugBuild](https://github.com/yuzurihakazuma/KazumimiEngine/actions/workflows/DebugBuild.yml/badge.svg?branch=%E5%80%8B%E4%BA%BA%E5%88%B6%E4%BD%9Cmain)](https://github.com/yuzurihakazuma/KazumimiEngine/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/yuzurihakazuma/KazumimiEngine/actions/workflows/Release.yml/badge.svg?branch=%E5%80%8B%E4%BA%BA%E5%88%B6%E4%BD%9Cmain)](https://github.com/yuzurihakazuma/KazumimiEngine/actions/workflows/Release.yml)
[![DevelopmentBuild](https://github.com/yuzurihakazuma/KazumimiEngine/actions/workflows/Development.yml/badge.svg?branch=%E5%80%8B%E4%BA%BA%E5%88%B6%E4%BD%9Cmain)](https://github.com/yuzurihakazuma/KazumimiEngine/actions/workflows/Development.yml)

# たまごザウルスの大冒険

自作エンジン **KazumimiEngine**（DirectX 12 / C++20）で作っている、紙と段ボールの工作の世界を走る 3D アクションゲームです。
恐竜がレールの上を走り、ベロで敵を食べて卵にし、卵を投げてしかけを動かしながらゴールを目指します。

- 開発環境：Visual Studio 2022（v143）／ Windows 11 ／ DirectX 12
- ソリューション：`project/CG2_00_01.sln`（`CG2_00_01` は、授業のビルド設定を保つための内部名です）
- 構成：`Debug`（エディタ付き・デバッグ）／ `Development`（エディタ付き・Release の速さ）／ `Release`（製品版）
- 警告：レベル 4・警告をエラーとして扱う（3 つの構成ともリビルドで 0 warning）

## フォルダ構成

```
project/
  engine/      自作エンジン（ゲームの中身を知らない層。game/ を include しない）
    base/        ウィンドウ・DirectX の土台・入力・時間・Framework
    graphics/    パイプライン・テクスチャ・一括描画（InstancedGroup）
    2d/ 3d/      スプライト・モデル・アニメーション
    math/        ベクトル・行列・補間
    scene/       IScene・ISceneTransition・SceneManager
    utils/       エディタ・マップの読み書き・JSON・Signal・StepSequence
  game/        ゲーム（たまごザウルスの大冒険）
    scene/       タイトル・ステージ選択・本編（BaseScene を継承）
    player/      恐竜（入力・ジャンプ・向き・ブロックとの当たり・ベロ・卵投げ）
    enemy/ egg/  敵・卵とコンボ
    rail/        レール（つなぎ方・動くレール・列車・道のメッシュ）
    stage/       ブロック・コイン・背景・ステージ開始の演出・記録
    clear/ gameover/ transition/   クリア演出・ゲームオーバー・紙芝居の画面遷移
    worldmap/ title/ ui/ effect/   ステージ選択の箱庭・タイトル・HUD・紙吹雪
    editor/      ゲーム画面の上で使うエディタ（配置ビュー・背景エディタなど）
  resources/   モデル・画像・音・マップ（JSON）・調整値（JSON）
tools/         素材を作るスクリプト（Blender / Python）と、描画なしで動かす確かめ用の道具
docs/          エディタの使い方・クラス図
```

## 設計で意識していること

| 考え方 | 使っている所 |
|---|---|
| 依存の向き | `engine/` は `game/` を知らない（エンジンからゲームへの `#include` は 0）。シーンの生成は `AbstractSceneFactory`（エンジン）を `SceneFactory`（ゲーム）が実装して渡す |
| namespace | エンジン層は全部 `namespace KazumimiEngine` の中。ゲームのヘッダは `KazumimiEngine::Vector3` のように名前を付けて書き、`using namespace` は `.cpp` だけ |
| Template Method | `Framework`（初期化・更新・描画の流れ）を `Game` が継承。`BaseScene` が共通の流れを持ち、各シーンは `OnDrawOpaque` などの決まった所だけを書く |
| Factory Method | `AbstractSceneFactory` → `SceneFactory`（シーンの名前から実体を作る） |
| Strategy | 敵の種類ごとの動き `EnemyBehavior` → `WalkerBehavior`・`BiterBehavior`・`FlyerBehavior`（`Enemy` は種類で分岐しない）。画面遷移 `ISceneTransition` → `KamishibaiTransition`（`SceneManager` は遷移の中身を知らない） |
| State | ゲームオーバー演出 `GameOverSequence` の 12 個の段は、`PhaseState` の派生クラス（`FreezeState`・`CrumpleState`・`ChooseState` …）。次の段へ進むかは各状態が決める |
| State（場面の列） | `StepSequence<TContext>` と `IStep`。クリア演出は `GateStep` → `GoalStep` → `CurtainStep` を順に実行し、場面クラス自身が終わりを決める |
| Observer | `Signal<Args...>`。敵を倒した・コンボが変わった・ダメージを受けた、を集計や HUD へ通知する |
| Memento | レールエディタの元に戻す・やり直し（`RailEditor::RailSnapshot`） |
| Builder | `GraphicsPipelineBuilder`・`RootSignatureBuilder` |
| Object Pool | ブロックの行列計算用の `Obj3d`（`BlockRenderer`）・敵の実体の使い回し（`EnemyManager`） |
| Dirty Flag | 背景の一括描画の組み分け（`StageBackdrop`）は、変わった時だけ作り直す |
| Singleton | `Singleton<T>`（CRTP）に 1 つにまとめ、マネージャだけで使う |
| カプセル化 | メンバ変数は private。`SplineRail` は設定・つながり・動き・列車・渡し板のまとまりごとに、読む入口（`Config()`）と書き換える入口（`EditConfig()`）を分けている |
| データ駆動 | マップ・敵とコインの配置・背景・列車・カメラ・クリア演出の秒数と置き場所は JSON。敵の種類ごとの値は表（`Enemy.cpp` の `kTypeTable`） |

クラス図（シーンのつなぎ・敵のふるまい・ゲームオーバーの状態）は [docs/class_diagram.md](docs/class_diagram.md) にあります。

![クラス図](docs/class_diagram.png)

## 確かめ方

- `tools/railsim`・`tools/blocksim`：ゲーム本体と同じ `Player`・レール・ブロックのコードを、描画なしでビルドして動かします（乗り移り・当たり・列車の乗り降りを、起動せずに確かめられます）
- `tools/review/audit.py`：クラスコメント・値渡しの引数・include の書き方などを数えます
- `tools/review/make_class_diagram.py`：クラス図（`docs/class_diagram.png`）を描き直します
- GitHub Actions：3 つの構成のビルドと、不要なファイルが入っていないかの確認

エディタの使い方は [docs/エディタの使い方.md](docs/エディタの使い方.md) にあります。
