"""撮影機能の連番 PNG（capture/rec_日時/00000.png 〜）を GIF と mp4 にまとめる。

使い方:
    python tools/capture/make_gif.py <連番のフォルダ> [--width 960] [--fps 30] [--no-mp4]

できるもの: フォルダと同じ名前の .gif（幅 width・fps コマ）と .mp4（60fps・元の大きさ）
ffmpeg は PATH にあればそれを、無ければ Python の imageio-ffmpeg に入っている ffmpeg を使う。
（エンジンのパネル「撮影 (Capture)」の「GIF・mp4 を作る」から呼ばれる）
"""
import argparse
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
    parser.add_argument("folder")
    parser.add_argument("--width", type=int, default=960)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--no-mp4", action="store_true")
    args = parser.parse_args()

    folder = Path(args.folder)
    frames = sorted(folder.glob("*.png"))
    if not frames:
        print("連番の PNG が無い:", folder)
        return 2
    ffmpeg = find_ffmpeg()
    if not ffmpeg:
        print("ffmpeg が見つからない（pip install imageio-ffmpeg で入る）")
        return 3

    pattern = str(folder / "%05d.png")
    gif = folder.with_suffix(".gif")
    vf = f"fps={args.fps},scale={args.width}:-1:flags=lanczos,split[a][b];[a]palettegen[p];[b][p]paletteuse"
    result = subprocess.run([ffmpeg, "-y", "-loglevel", "error", "-framerate", "60", "-i", pattern, "-vf", vf, str(gif)])
    if result.returncode != 0:
        return result.returncode
    print("GIF:", gif)

    if not args.no_mp4:
        mp4 = folder.with_suffix(".mp4")
        result = subprocess.run([ffmpeg, "-y", "-loglevel", "error", "-framerate", "60", "-i", pattern,
                                 "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "18", str(mp4)])
        if result.returncode != 0:
            return result.returncode
        print("mp4:", mp4)
    return 0


if __name__ == "__main__":
    sys.exit(main())
