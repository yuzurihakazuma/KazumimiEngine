"""撮影機能の録画を GIF と mp4 にまとめる（エンジンのパネル「撮影 (Capture)」から呼ばれる）。

使い方:
    連番 PNG（capture/rec_日時/00000.png 〜。60fps）
        python tools/capture/make_gif.py <連番のフォルダ> [--width 960] [--fps 30] [--no-mp4]
    直前を保存（capture/replay_日時.rgb。縮めた RGB24 を続けて書いた物）
        python tools/capture/make_gif.py <file.rgb> --raw 640x360 --rate 30 [--width 640] [--fps 30]
        できたら .rgb は消す

できるもの: 入力と同じ名前の .gif（幅 width・fps コマ・色を最適化）と .mp4（H.264）
ffmpeg は PATH にあればそれを、無ければ Python の imageio-ffmpeg に入っている ffmpeg を使う。
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def find_ffmpeg():
    exe = shutil.which("ffmpeg")
    if exe:
        return exe
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except Exception:
        return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source")
    parser.add_argument("--width", type=int, default=960)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--no-mp4", action="store_true")
    parser.add_argument("--raw", help="RGB24 の大きさ WxH（直前を保存の .rgb）")
    parser.add_argument("--rate", type=int, default=30, help="--raw の時の、1秒あたりの枚数")
    args = parser.parse_args()

    ffmpeg = find_ffmpeg()
    if not ffmpeg:
        print("ffmpeg が見つからない（pip install imageio-ffmpeg で入る）")
        return 3

    source = Path(args.source)
    if args.raw:
        if not source.exists():
            print("ファイルが無い:", source)
            return 2
        stem = source.with_suffix("")
        inputs = ["-f", "rawvideo", "-pix_fmt", "rgb24", "-s", args.raw, "-r", str(args.rate), "-i", str(source)]
    else:
        if not sorted(source.glob("*.png")):
            print("連番の PNG が無い:", source)
            return 2
        stem = source
        inputs = ["-framerate", "60", "-i", str(source / "%05d.png")]

    gif = Path(str(stem) + ".gif")
    vf = f"fps={args.fps},scale={args.width}:-2:flags=lanczos,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle"
    result = subprocess.run([ffmpeg, "-y", "-loglevel", "error"] + inputs + ["-vf", vf, str(gif)])
    if result.returncode != 0:
        return result.returncode
    print("GIF:", gif)

    if not args.no_mp4:
        mp4 = Path(str(stem) + ".mp4")
        result = subprocess.run([ffmpeg, "-y", "-loglevel", "error"] + inputs
                                + ["-vf", "scale=trunc(iw/2)*2:trunc(ih/2)*2", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", str(mp4)])
        if result.returncode != 0:
            return result.returncode
        print("mp4:", mp4)

    if args.raw:
        os.remove(source)  # できたら、縮めた生のデータは消す
    return 0


if __name__ == "__main__":
    sys.exit(main())
