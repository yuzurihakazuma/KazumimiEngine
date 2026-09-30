#pragma once
// =====================================================================
//  TitleAssets：タイトルシーンが使うモデル・画像・音の一括読み込み。
//   実行中（フレームの途中）の読み込みは D3D12 のデバッグレイヤーが嫌うので、ここで全部先に読む。
//
//   ・クラフトキット（resources/craft/models/*.gltf）は、フォルダにある物を全部読む。
//     モデル名＝ファイル名（拡張子なし）。マップ（resources/map/title.json）の type と一致する
//   ・恐竜が走るレールと道は本編と同じ物（RailField / RoadMesh）なので、その素材もここで読む
//   ・ロゴ看板と、メニューの的3つは、同じモデルを別名で読んで面の画像だけ差し替える
//     （画像は resources/title/。同じファイル名で上書きすれば絵を変えられる）
// =====================================================================
namespace TitleAssets {
    // モデル名（TitleScene の各部品が Obj3d::Create に渡す）
    inline constexpr const char* kLogoBoardModel = "titleLogoBoard";
    inline constexpr const char* kMenuModels[3] = { "titleMenuContinue", "titleMenuStart", "titleMenuOptions" };
    inline constexpr const char* kDeskModel = "titleDesk"; // 机の天板（マップが type として参照する）
    // 紙ふぶきの1枚（色ちがいの紙。TitlePaperBits が使う）
    inline constexpr int kPaperBitModelCount = 4;
    inline constexpr const char* kPaperBitModels[kPaperBitModelCount] = {
        "titlePaperWhite", "titlePaperYellow", "titlePaperOrange", "titlePaperGreen" };

    // 効果音
    inline constexpr const char* kThrowSe = "resources/se/eggThrow.wav";
    inline constexpr const char* kHitSe   = "resources/se/eggHit.wav";

    void Load();
}
