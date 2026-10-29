#!/usr/bin/env python3
"""Fetch build dependencies (JUCE + VST3 SDK) through the local HTTP proxy.

The host's Schannel stack cannot acquire credentials inside this sandbox, so
git/curl TLS fails. Python's OpenSSL stack works, so we download zips here and
let CMake consume them from disk.
"""
import io
import json
import os
import sys
import urllib.request
import zipfile

PROXY = os.environ.get("DSH_PROXY", "http://127.0.0.1:10808")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXTERNAL = os.path.join(ROOT, "external")


def build_opener():
    proxy = urllib.request.ProxyHandler({"http": PROXY, "https": PROXY})
    return urllib.request.build_opener(proxy)


OPENER = build_opener()


def get(url, timeout=120):
    req = urllib.request.Request(url, headers={"User-Agent": "neonrift-build/1.0"})
    return OPENER.open(req, timeout=timeout)


def fetch_json(url):
    with get(url, timeout=60) as r:
        return json.loads(r.read().decode("utf-8"))


def download(url, dest, label):
    print(f"[fetch] {label}: {url}", flush=True)
    tmp = dest + ".part"
    with get(url) as r, open(tmp, "wb") as f:
        total = int(r.headers.get("Content-Length") or 0)
        done = 0
        while True:
            chunk = r.read(1 << 20)
            if not chunk:
                break
            f.write(chunk)
            done += len(chunk)
            if total:
                pct = 100.0 * done / total
                print(f"\r  {label}: {done/1e6:.1f}/{total/1e6:.1f} MB ({pct:.0f}%)", end="", flush=True)
        print("", flush=True)
    os.replace(tmp, dest)
    return dest


def extract(zip_path, dest_dir):
    print(f"[unzip] {os.path.basename(zip_path)} -> {dest_dir}", flush=True)
    with zipfile.ZipFile(zip_path) as z:
        names = z.namelist()
        top = names[0].split("/")[0]
        for n in names:
            z.extract(n, dest_dir)
    return os.path.join(dest_dir, top)


def main():
    os.makedirs(EXTERNAL, exist_ok=True)

    # --- JUCE: latest stable 8.x tag -------------------------------------
    juce_dir = os.path.join(EXTERNAL, "JUCE")
    if os.path.isdir(os.path.join(juce_dir, "modules")):
        print("[skip] JUCE already present", flush=True)
    else:
        tags = fetch_json("https://api.github.com/repos/juce-framework/JUCE/tags?per_page=100")
        stable = []
        for t in tags:
            name = t["name"].lstrip("v")
            # Only plain dotted numeric tags ("8.0.10"); skip "7.0.16-security".
            if not all(seg.isdigit() for seg in name.split(".")):
                continue
            parts = [int(p) for p in name.split(".")]
            stable.append((tuple(parts), t["name"]))
        stable.sort()
        tag = stable[-1][1]
        print(f"[info] JUCE tag = {tag}", flush=True)
        zp = os.path.join(EXTERNAL, "juce.zip")
        download(f"https://github.com/juce-framework/JUCE/archive/refs/tags/{tag}.zip", zp, "JUCE")
        extracted = extract(zp, EXTERNAL)
        if os.path.isdir(juce_dir):
            import shutil
            shutil.rmtree(juce_dir)
        os.replace(extracted, juce_dir)
        os.remove(zp)

    # --- VST3 SDK: pinned by JUCE, but pre-fetched so CMake stays offline
    vst3_dir = os.path.join(EXTERNAL, "vst3sdk")
    if os.path.isdir(os.path.join(vst3_dir, "pluginterfaces")):
        print("[skip] VST3 SDK already present", flush=True)
    else:
        tag = "v3.7.12_build_20"
        zp = os.path.join(EXTERNAL, "vst3sdk.zip")
        download(f"https://github.com/steinbergmedia/vst3sdk/archive/refs/tags/{tag}.zip", zp, "VST3 SDK")
        extracted = extract(zp, EXTERNAL)
        if os.path.isdir(vst3_dir):
            import shutil
            shutil.rmtree(vst3_dir)
        os.replace(extracted, vst3_dir)
        os.remove(zp)

    print("[done] deps ready", flush=True)
    print(f"JUCE   = {os.path.join(juce_dir, 'CMakeLists.txt')}")
    print(f"VST3SDK= {os.path.join(vst3_dir, 'pluginterfaces')}")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:  # noqa: BLE001
        print(f"[error] {type(exc).__name__}: {exc}", file=sys.stderr)
        raise
