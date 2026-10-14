#pragma once
// =====================================================================
//  ISceneTransition：シーン遷移の演出（紙芝居など）の差し込み口。
//   シーンマネージャが持つので、シーンが切り替わっても演出は途切れない。
//   次のシーンへの切り替えは、演出の側が「画面が隠れている間」に SceneManager::ChangeScene で頼む
// =====================================================================
#include <d3d12.h>

class RenderTexture;

class ISceneTransition {
public:
	virtual ~ISceneTransition() = default;

	// 毎フレーム、シーンの更新より前に呼ばれる（実時間の経過秒）
	virtual void Update(float deltaTime) = 0;
	// 演出の最中か（最中だけ DrawInto が呼ばれる）
	virtual bool IsActive() const = 0;
	// シーンの最終画像（target）の上に重ねて描く。シーンの合成の最後に呼ばれるので、エディタの Game View にも映る。
	//   target は「読み取り用」の状態で渡され、同じ状態に戻して返すこと
	virtual void DrawInto(ID3D12GraphicsCommandList* commandList, RenderTexture* target) = 0;
	// 画面（今の描き先）へ直接、シーンの UI の上に重ねる。エディタを出していない時に、シーンの UI の後で呼ばれる
	virtual void DrawOverlay(ID3D12GraphicsCommandList* /*commandList*/) {}
	// 調整用の ImGui（インスペクターの中に出る）
	virtual void DrawDebugUI() {}
	// 終了処理（GPU のリソースを、デバイスより先に放す）
	virtual void Finalize() {}
};
