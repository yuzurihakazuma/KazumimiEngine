#pragma once
// =====================================================================
//  PlaceContextMenu：Game View の右クリックメニュー（エディット中・ブロック配置モードOFF時）。
//   ・レールの近く：ここからテストプレイ / 敵・コイン・ブロックをその場に配置
//   ・近くにある既存の敵・ブロック・コイン：設定を開く / 削除 / 種類の切替
//   ・表示の切替（動くレールのプレビュー再生）
//  パネルを開かずに置ける最短ルート。右ドラッグはカメラの回転に使うので、
//  「動かさず右クリックを離した」時だけ開く
// =====================================================================
struct SceneEditContext;

class PlaceContextMenu {
public:
    // anyDragActive=true の間は開かない（つかみ移動の最中・ガイドハンドル圏内。
    //   ハンドルの右クリック＝点の削除と、配置メニューが二重発動しないように）
    void Update(const SceneEditContext& context, bool anyDragActive);

    // 「ここからテストプレイ」の要求を取り出す（あれば true）。
    //   シーンが次の Edit→Play でこの地点から開始する
    bool ConsumeTestPlayRequest(int& outRail, float& outDist);

private:
    void OpenIfRequested(const SceneEditContext& context, bool anyDragActive);
    void DrawPlaceItems(const SceneEditContext& context);
    void DrawExistingItems(const SceneEditContext& context);

    // メニューを開いた瞬間のレール/距離
    int   placeRail_ = -1;
    float placeDist_ = 0.0f;
    // 右クリック地点の近くにあった既存配置物（-1=なし）
    int   enemyIndex_ = -1;
    int   blockIndex_ = -1;
    int   coinIndex_  = -1;

    bool  testPlayRequested_ = false;
    int   testPlayRail_ = -1;
    float testPlayDist_ = 0.0f;
};
