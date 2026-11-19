#pragma once
// =====================================================================
//  UiScale：2D の絵（HUD・カード・パネル・リザルト）を置く時の、基準の画面の大きさ。
//   ・絵の大きさと置き場所は、1920×1080 の画面で決めてある（tools の絵を作るスクリプトも同じ）
//   ・実際の画面がそれより小さい・大きい時は、同じ割合で縮める・広げる
//  前は 1080.0f・1920.0f を、使う所ごとに数字で書いていた（10 個のファイルに 16 か所）
// =====================================================================
namespace UiScale {

    inline constexpr float kBaseWidth = 1920.0f;  // 基準の画面の幅（ピクセル）
    inline constexpr float kBaseHeight = 1080.0f; // 基準の画面の高さ

    // 画面の高さに合わせた倍率（横に長い画面でも、縦を基準にする）
    constexpr float FromHeight(float screenHeight){ return screenHeight / kBaseHeight; }

    // 画面に全部が収まる倍率（縦と横の、小さい方）
    constexpr float Fit(float screenWidth, float screenHeight){
        const float byWidth = screenWidth / kBaseWidth;
        const float byHeight = screenHeight / kBaseHeight;
        return byWidth < byHeight ? byWidth : byHeight;
    }

} // namespace UiScale
