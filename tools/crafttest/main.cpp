// 箱庭ステージのデータ・元に戻す・保存の確認（エンジン無しで動く小さなテスト）
//   build_and_run.bat で実行。失敗すると "NG" を出して終了コード 1
#include "game/craft/CraftStage.h"
#include "game/craft/editor/CraftCommands.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
static int failures = 0;
#define CHECK(cond) do { if ( !( cond ) ) { std::printf("NG: %s (line %d)\n", #cond, __LINE__); ++failures; } else { std::printf("ok: %s\n", #cond); } } while ( 0 )

static CraftObject MakeObject(const char* asset, float x){
    CraftObject object;
    object.id = CraftStage::NewId();
    object.asset = asset;
    object.position = { x, 0.0f, 0.0f };
    return object;
}

int main(){
    const std::string dir = "crafttest_tmp";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const std::string path = dir + "/stage_test.stage.json";

    CraftStage stage;
    CraftHistory history;
    CHECK(!history.IsDirty());

    // 置く → 元に戻す → やり直す
    CraftObject grass = MakeObject("grass_tuft_00", 1.0f);
    history.Execute(stage, std::make_unique<CraftCreateCommand>(grass));
    CHECK(stage.GetObjects().size() == 1);
    CHECK(history.IsDirty());
    history.Undo(stage);
    CHECK(stage.GetObjects().empty());
    CHECK(!history.IsDirty()); // 何もしていない時点まで戻れば未保存ではない
    history.Redo(stage);
    CHECK(stage.GetObjects().size() == 1);

    // 削除して元に戻すと、同じ id・同じ設定で復活する
    CraftObject* placed = stage.FindMutable(grass.id);
    placed->params["speed"] = 1.5f;
    const CraftObject copy = *placed;
    history.Execute(stage, std::make_unique<CraftDeleteCommand>(copy));
    CHECK(stage.Find(grass.id) == nullptr);
    history.Undo(stage);
    CHECK(stage.Find(grass.id) != nullptr);
    CHECK(stage.Find(grass.id)->params.value("speed", 0.0f) == 1.5f);

    // 変形（ドラッグ1回＝1回分）
    CraftObject after = *stage.Find(grass.id);
    after.position.x = 9.0f;
    history.Execute(stage, std::make_unique<CraftTransformCommand>(*stage.Find(grass.id), after));
    CHECK(stage.Find(grass.id)->position.x == 9.0f);
    history.Undo(stage);
    CHECK(stage.Find(grass.id)->position.x == 1.0f);
    history.Redo(stage);

    // まとめ（ブラシ1回）：子を全部まとめて1回で戻る
    auto group = std::make_unique<CraftGroupCommand>("ブラシで撒く");
    for ( int i = 0; i < 5; ++i ) {
        auto create = std::make_unique<CraftCreateCommand>(MakeObject("flower_red", ( float ) i));
        create->Execute(stage);
        group->Add(std::move(create));
    }
    history.PushDone(std::move(group));
    CHECK(stage.GetObjects().size() == 6);
    history.Undo(stage);
    CHECK(stage.GetObjects().size() == 1);
    history.Redo(stage);

    // 保存すると「*」が消え、元に戻すと付き、保存時点まで戻すとまた消える
    CHECK(stage.Save(path));
    history.MarkSaved();
    CHECK(!history.IsDirty());
    history.Undo(stage);
    CHECK(history.IsDirty());
    history.Redo(stage);
    CHECK(!history.IsDirty());

    // 元に戻した後に新しい操作をすると、やり直しは捨てられ、保存時点へは戻れない（常に未保存）
    history.Undo(stage);
    history.Execute(stage, std::make_unique<CraftCreateCommand>(MakeObject("paper_cup", 3.0f)));
    CHECK(!history.CanRedo());
    CHECK(history.IsDirty());

    // 2回目の保存でバックアップができる
    CHECK(stage.Save(path));
    CHECK(fs::exists(dir + "/backup"));

    // 読み込むと同じ配置
    CraftStage loaded;
    CHECK(loaded.Load(path));
    CHECK(loaded.GetObjects().size() == stage.GetObjects().size());
    CHECK(loaded.Find(grass.id) != nullptr);

    // id の重複は、後の方に新しい id が振られる
    {
        std::ofstream file(dir + "/dup.stage.json");
        file << R"({"version":1,"name":"dup","objects":[{"id":"00000000000000aa","asset":"a"},{"id":"00000000000000aa","asset":"b"}]})";
    }
    CraftStage duplicated;
    CHECK(duplicated.Load(dir + "/dup.stage.json"));
    CHECK(duplicated.GetObjects().size() == 2);
    CHECK(duplicated.GetObjects()[0].id != duplicated.GetObjects()[1].id);
    CHECK(!duplicated.GetLoadWarnings().empty());

    // JSON として壊れていたら読み込みを中止し、今の中身は残す
    {
        std::ofstream file(dir + "/broken.stage.json");
        file << "{ \"objects\": [ ";
    }
    CraftStage broken;
    broken.Add(MakeObject("keep", 0.0f));
    CHECK(!broken.Load(dir + "/broken.stage.json"));
    CHECK(broken.GetObjects().size() == 1);

    // 一覧に無いアセット名も、保存・読み込みで消えない
    CraftStage unknown;
    unknown.Add(MakeObject("no_such_asset", 0.0f));
    CHECK(unknown.Save(dir + "/unknown.stage.json"));
    CraftStage unknownLoaded;
    CHECK(unknownLoaded.Load(dir + "/unknown.stage.json"));
    CHECK(unknownLoaded.GetObjects().size() == 1 && unknownLoaded.GetObjects()[0].asset == "no_such_asset");

    // 一時ファイルが残っていない（保存の最後に本体へ改名されている）
    CHECK(!fs::exists(path + ".tmp"));

    std::printf(failures ? "\n%d NG\n" : "\nALL OK\n", failures);
    fs::remove_all(dir);
    return failures ? 1 : 0;
}
