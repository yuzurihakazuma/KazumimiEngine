# -*- coding: utf-8 -*-
"""
カードを使う遷移の定義（resources/transition/*.json）を作る。カードが出来上がっていく動き（台紙が入る → 星がポン →
布が転がって帯が広がる → キャラが登場 → 名前がポン）は3つとも同じで、終わりははさみで切って分かれる。

  python tools/kamishibai/make_transition_timelines.py

  card.json        読み込み中のカード（ステージ選択 ⇔ タイトル、ゲーム → タイトル）
  title_open.json  タイトルの終わり（段ボールが倒れる → カード → ステージ選択）
  stage_intro.json ステージ開始カード（→ ゲーム。カードが分かれた後にプレイヤーの演技）

  時刻は秒。インスペクターの「紙芝居遷移」で直して保存もできる（その時はこのスクリプトを流し直すと戻る）
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "project", "resources", "transition"))
WHOOSH = "resources/kamishibai/se_whoosh.wav"
POP = "resources/kamishibai/se_pon.wav"


def part(name, type_, event, delay, duration=0.0, **params):
    data = {"name": name, "type": type_, "event": event, "delay": round(delay, 3), "duration": duration}
    if params:
        data["params"] = params
    return data


def build_up(event):
    """カードが出来上がっていく（event からの秒数）"""
    parts = [
        part("台紙が入る", "introElement", event, 0.0, 0.35, id="backing", motion="slideIn"),
        part("台紙の音", "sound", event, 0.0, sound=WHOOSH, volume=0.7),
        part("小さな卵がポン", "introElement", event, 0.3, 0.28, id="smallEgg", motion="pop"),
        part("ロードちゅうの点", "introElement", event, 0.35, id="dots", motion="show"),
    ]
    for i in range(6):
        parts.append(part("星%dがポン" % (i + 1), "introElement", event, 0.2 + i * 0.07, 0.28, id="star%d" % i, motion="pop"))
    for t in (0.2, 0.34, 0.48):
        parts.append(part("星の音", "sound", event, t, sound=POP, volume=0.6))
    parts += [
        part("布が転がって広がる", "introElement", event, 0.4, 0.8, id="banner", motion="unroll"),
        part("布の音", "sound", event, 0.4, sound="resources/kamishibai/se_roll.wav", volume=0.8),
        part("恐竜がジャンプで登場", "introElement", event, 0.8, 0.55, id="dino", motion="jumpIn"),
        part("恐竜の着地の音", "sound", event, 1.23, sound=POP, volume=0.7),
        part("敵が走り込む", "introElement", event, 0.85, 0.55, id="enemy", motion="runIn"),
        part("卵が転がってくる", "introElement", event, 0.9, 0.5, id="egg", motion="rollIn"),
        part("名前がポン", "introElement", event, 1.2, 0.3, id="name", motion="pop"),
        part("名前の音", "sound", event, 1.2, sound=POP, volume=0.9),
        # 出来上がってから裏で読み込む（読み込みの重い1フレームで、貼っている途中の動きが止まらないように）
        part("次のシーンを読む", "loadScene", event, 1.55),
    ]
    return parts


def cut(event):
    """はさみで切って分かれる（event からの秒数）。分かれ終わるのは 1.0 秒後"""
    return [
        part("はさみで切る", "introCut", event, 0.0, 1.0, cutTime=0.4, lift=1.25, drop=0.5),
        part("はさみの音", "sound", event, 0.0, sound="resources/kamishibai/se_snip.wav", volume=0.9),
        part("分かれる音", "sound", event, 0.4, sound=WHOOSH, volume=0.8),
    ]


def write(name, to, parts):
    with open(os.path.join(OUT, name + ".json"), "w", encoding="utf-8") as f:
        f.write(json.dumps({"to": to, "parts": parts}, ensure_ascii=False, indent=2) + "\n")
    print("wrote", name, len(parts), "parts")


# 読み込み中のカード
write("card", "STAGESELECT", build_up("start") + [
    part("読み込みと最短時間を待つ", "waitAll", "start", 0.0, events=["loaded"], minTime=2.1, emit="ready"),
] + cut("ready") + [part("おわり", "end", "ready", 1.05)])

# タイトルの終わり：段ボールが倒れている途中からカードが入る
CARD_START = 1.35
write("title_open", "STAGESELECT", [
    part("段ボールが倒れる", "sceneExit", "start", 0.05, 3.0, emit="exited"),
    part("カードを出し始める", "waitAll", "start", 0.0, events=[], minTime=CARD_START, emit="cardStart"),
] + build_up("cardStart") + [
    part("拍子木", "sound", "loaded", 0.15, sound="resources/kamishibai/se_hyoshigi.wav", volume=1.0),
    part("読み込みと最短時間を待つ", "waitAll", "start", 0.0, events=["loaded"], minTime=CARD_START + 2.1, emit="ready"),
] + cut("ready") + [part("おわり", "end", "ready", 1.05)])

# ステージ開始カード：分かれた後にプレイヤーの演技（ステージ開始演出 実装企画書の時刻を、切る 0.5 秒の分だけ後ろへ）
D = 0.5
write("stage_intro", "GAMEPLAY", build_up("start") + [
    part("読み込みと最短時間を待つ", "waitAll", "start", 0.0, events=["loaded"], minTime=2.6, emit="ready"),
    part("ステージへ：演出の準備", "sceneCue", "loaded", 0.0, cue="introStart"),
] + cut("ready") + [
    part("上下の帯が出る", "letterbox", "ready", 0.4, 0.3, **{"in": True}),
    part("こっちを見て手を振る", "sceneCue", "ready", 0.3 + D, cue="introTurn"),
    part("ロールを押す", "sceneCue", "ready", 1.5 + D, cue="introPush"),
    part("ロールが転がり出す", "sceneCue", "ready", 2.4 + D, cue="introRollGo"),
    part("ロールの音", "sound", "ready", 2.4 + D, sound="resources/kamishibai/se_roll.wav", volume=0.8),
    part("ジャンプでこっちを向く", "sceneCue", "ready", 2.5 + D, cue="introAction"),
    part("キラキラ", "sceneCue", "ready", 3.0 + D, cue="introSparkle"),
    part("キラキラの音", "sound", "ready", 3.0 + D, sound=POP, volume=0.6),
    part("カメラが後ろへ戻る", "sceneCue", "ready", 3.0 + D, cue="introReturn"),
    part("上下の帯が引っ込む", "letterbox", "ready", 3.0 + D, 0.3, **{"in": False}),
    part("操作開始", "sceneCue", "ready", 3.6 + D, cue="introEnd"),
    part("おわり", "end", "ready", 3.6 + D),
])
