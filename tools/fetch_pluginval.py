#!/usr/bin/env python3
"""Fetch the latest pluginval (VST3 validator) Windows build through the proxy."""
import json
import os
import urllib.request
import zipfile

PROXY = os.environ.get("DSH_PROXY", "http://127.0.0.1:10808")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "external", "pluginval")

opener = urllib.request.build_opener(
    urllib.request.ProxyHandler({"http": PROXY, "https": PROXY})
)


def get(url, timeout=120):
    req = urllib.request.Request(url, headers={"User-Agent": "neonrift-build/1.0"})
    return opener.open(req, timeout=timeout)


def main():
    if os.path.isdir(OUT) and any(n.lower().startswith("pluginval") for n in os.listdir(OUT)):
        print("[skip] pluginval already present")
        return

    release = json.loads(get("https://api.github.com/repos/Tracktion/pluginval/releases/latest").read())
    print("[info] pluginval", release["tag_name"])

    for asset in release["assets"]:
        name = asset["name"]
        if "windows" in name.lower() and name.endswith(".zip"):
            dest = os.path.join(ROOT, "external", "pluginval.zip")
            print("[fetch]", name)
            with get(asset["browser_download_url"], 240) as r, open(dest, "wb") as f:
                f.write(r.read())
            with zipfile.ZipFile(dest) as z:
                z.extractall(OUT)
            os.remove(dest)
            print("[done]", OUT)
            return

    raise RuntimeError("no Windows zip asset found")


if __name__ == "__main__":
    main()
