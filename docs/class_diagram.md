# クラス図（設計の一部）

「たまごザウルスの大冒険」のうち、**エンジンとゲームのつなぎ（シーン）**・**敵のふるまいと、当たった時の結果**・**ゲームオーバー演出の状態**を抜き出した UML クラス図です。

- 画像：[class_diagram.png](class_diagram.png)（`python tools/review/make_class_diagram.py` で描き直せます）
- 下は同じ内容の Mermaid 版です（GitHub の上でそのまま図になります）

![クラス図](class_diagram.png)

## 記号

| 記号 | 意味 | この図での使い分け |
|---|---|---|
| `<|--` 白い三角・実線 | 継承（汎化） | 三角が親の側 |
| `*--` 黒いひし形 | コンポジション | `std::unique_ptr` や値のメンバ。持ち主が消えると一緒に消える |
| `o--` 白いひし形 | 集約 | ポインタで借りているだけ。持ち主は別にいる |
| `..>` 点線の矢印 | 依存 | 使う・作るだけで、メンバには持たない |

- `«abstract»` は、純粋仮想関数を持つクラスだけに付けています。C++ には interface が無いので `«interface»` は使わず、派生との線はどれも実線の継承です。
- `BaseScene` と `PhaseState` は、仮想関数に全部既定の中身があるので、`«abstract»` を付けていません。

## Mermaid

```mermaid
classDiagram
    direction TB

    namespace KazumimiEngine {
        class Framework {
            <<abstract>>
            +Run()
            +Initialize()
            +Update()
            +DrawScene()*
        }
        class AbstractSceneFactory {
            <<abstract>>
            +CreateScene(name)*
        }
        class SceneManager {
            <<Singleton>>
            +ChangeScene(next)
            +SetSceneFactory(factory)
            +SetTransition(transition)
        }
        class IScene {
            <<abstract>>
            +Initialize()*
            +Update()*
            +Draw()*
            +Finalize()*
        }
        class ISceneTransition {
            <<abstract>>
            +Update(dt)*
            +IsActive()*
            +DrawInto(target)*
        }
    }

    class Game {
        +Initialize()
        +Update()
        +DrawScene()
    }
    class SceneFactory {
        +CreateScene(name)
    }
    class BaseScene {
        +Initialize()
        +Update()
        +Draw()
        #OnInitialize()
        #OnUpdate()
        #OnDrawOpaque()
    }
    class KamishibaiTransition {
        <<Singleton>>
        +Play(definitionName)
        +Update(dt)
        +DrawInto(target)
    }
    class TitleScene
    class StageSelectScene
    class GamePlayScene
    class EnemyManager {
        +Spawn(spawns, rails)
        +Update(rails, playerPos, dt)
    }
    class Enemy {
        +Initialize(spawn)
        +Update()
        +Draw()
        +ReceiveHit(hit, context)
    }
    class EnemyBehavior {
        <<abstract>>
        +Create(type)$
        +StartClip(patrols)*
        +Update(model, frame)*
        +ReceiveHit(hit, enemy)*
        +Reset()
    }
    class WalkerBehavior
    class BiterBehavior
    class FlyerBehavior
    class EnemyHit {
        <<abstract>>
        +OnWalker(enemy, context)
        +OnBiter(enemy, context)
        +OnFlyer(enemy, context)
        #Resolve(enemy, context)*
    }
    class StompHit
    class BodyHit
    class SpitHit
    class EggHit
    class GameOverFlow
    class GameOverSequence {
        +Start()
        +Update()
        -Enter(phase)
    }
    class PhaseState {
        +Length(sequence)
        +Enter(sequence)
        +Update(sequence, t, dt)
    }
    class FreezeState
    class CrumpleState
    class ChooseState
    class LeaveState

    Framework <|-- Game
    AbstractSceneFactory <|-- SceneFactory
    IScene <|-- BaseScene
    ISceneTransition <|-- KamishibaiTransition
    BaseScene <|-- TitleScene
    BaseScene <|-- StageSelectScene
    BaseScene <|-- GamePlayScene
    EnemyBehavior <|-- WalkerBehavior
    EnemyBehavior <|-- BiterBehavior
    EnemyBehavior <|-- FlyerBehavior
    EnemyHit <|-- StompHit
    EnemyHit <|-- BodyHit
    EnemyHit <|-- SpitHit
    EnemyHit <|-- EggHit
    PhaseState <|-- FreezeState
    PhaseState <|-- CrumpleState
    PhaseState <|-- ChooseState
    PhaseState <|-- LeaveState

    SceneManager "1" *-- "0..1" IScene : currentScene_
    SceneManager "1" o-- "0..1" AbstractSceneFactory : sceneFactory_
    SceneManager "1" o-- "0..1" ISceneTransition : transition_
    Game "1" *-- "1" SceneFactory
    SceneFactory ..> BaseScene : create
    GamePlayScene "1" *-- "1" EnemyManager
    GamePlayScene "1" *-- "1" GameOverFlow
    EnemyManager "1" *-- "0..*" Enemy : enemies_
    Enemy "1" *-- "1" EnemyBehavior : behavior_
    EnemyBehavior ..> EnemyHit : use
    GameOverFlow "1" *-- "1" GameOverSequence : sequence_
    GameOverSequence "1" *-- "12" PhaseState : states_
```

## 読み方

- **依存の向き**：`engine/`（`namespace KazumimiEngine`）は `game/` を知りません。シーンを作る役は、エンジンの `AbstractSceneFactory` をゲームの `SceneFactory` が実装して `SceneManager` に渡します（`SceneManager` は借りているだけなので集約）。
- **Template Method**：`BaseScene` が初期化・更新・描画の流れを持ち、各シーンは `OnInitialize` / `OnUpdate` / `OnDrawOpaque` などの決まった所だけを書きます。
- **Strategy**：`Enemy` は敵の種類で分岐しません。種類ごとの動き（歩く・噛みつく・飛ぶ）は `EnemyBehavior` の派生が持ち、派生を選ぶのは `EnemyBehavior::Create` だけです。
- **Double Dispatch**：敵に何かが当たった時の結果は「当たった物の種類 × 敵の種類」で決まります。`Enemy::ReceiveHit` が敵の種類の `ReceiveHit`（1 段目）を呼び、それが当たった物の `OnWalker` / `OnBiter` / `OnFlyer`（2 段目）を呼びます。当たった物は `StompHit`（踏みつけ）・`BodyHit`（体当たり）・`SpitHit`（吐き出した敵ボール）・`EggHit`（卵）です。
- **State**：`GameOverSequence` の 12 個の段（止まる → くしゃくしゃ → 選ぶ → …）は、それぞれ `PhaseState` の派生クラスです。次の段へ進むかどうかは各状態が決めます。図には 4 個だけ載せています（ほか：`RevealState` / `CaptureState` / `OpenState` / `ScreenState` / `ResumeShotState` / `UnscreenState` / `FoldState` / `RebuildState`）。
