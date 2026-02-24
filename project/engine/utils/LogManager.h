#pragma once
// --- 標準ライブラリ ---
#include <string>
#include <fstream>
namespace logs{
	class LogManager{
	public:

		// -------------------- 文字列変換 --------------------

		/// <summary>
		/// std::string → std::wstring へ変換
		/// </summary>
		std::wstring ConvertString(const std::string& str);

		/// <summary>
		/// std::wstring → std::string へ変換
		/// </summary>
		std::string ConvertString(const std::wstring& str);


		// -------------------- 初期化・終了処理 --------------------

		/// <summary>
		/// ログシステムの初期化
		/// </summary>
		void Initialize();

		/// <summary>
		/// ログシステムの終了処理
		/// </summary>
		void Finalize();


		// -------------------- ログ出力 --------------------

		/// <summary>
		/// ログメッセージを出力
		/// </summary>
		void Log(const std::string& message);

		/// <summary>
		/// ログメッセージを出力
		/// </summary>
		void Log(const std::wstring& message);


		// -------------------- シングルトンアクセス --------------------

		/// <summary>
		/// シングルトンとして LogManager のインスタンスを取得
		/// </summary>
		static LogManager* GetInstance(){
			static LogManager instance;
			return &instance;
		}

	private:

		// -------------------- 内部リソース --------------------

		std::ofstream logStream_;   // ログファイル書き込み用ストリーム
	};


}// namespace logs

