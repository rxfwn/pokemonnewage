#!/usr/bin/env python3
"""
Publie une nouvelle version du jeu sur le Raspberry Pi (docs/MISES_A_JOUR.md).

À lancer sur ton PC après avoir packagé le jeu dans Unreal :

    python publier_version.py "D:\\Builds\\Windows"
    python publier_version.py "D:\\Builds\\Windows" --note "Arène d'Argenta"

Le script :
  1. écrit version.txt dans le build (le jeu s'en sert pour vérifier qu'on a la même version en multijoueur) ;
  2. calcule l'empreinte de chaque fichier (gardée en cache : seuls les fichiers modifiés sont relus) ;
  3. demande au Pi ceux qu'il n'a pas encore et n'envoie que ceux-là ;
  4. publie la liste des fichiers : les launchers téléchargent la nouvelle version à leur prochaine ouverture.

Réglages dans publier_config.json, à côté de ce script (créé au premier lancement, jamais dans Git) :
    {"server": "http://100.x.x.x:8788", "publish_key": "<clé affichée par cle-publication sur le Pi>"}
"""

import argparse
import hashlib
import http.client
import json
import os
import sys
import time
from datetime import datetime
from pathlib import Path
from urllib.parse import urlsplit

BASE_DIR = Path(__file__).resolve().parent
CONFIG_PATH = BASE_DIR / "publier_config.json"
CACHE_PATH = BASE_DIR / ".cache_publication.json"
CHUNK = 1024 * 1024

# Fichiers du build qu'on n'envoie pas : symboles de débogage (gros et inutiles pour jouer),
# dossiers créés en jouant, fichiers du launcher
EXCLUDED_SUFFIXES = (".pdb",)
EXCLUDED_DIRS = {"Saved"}
EXCLUDED_NAMES = {".launcher_etat.json"}


def load_config():
    if not CONFIG_PATH.exists():
        with CONFIG_PATH.open("w", encoding="utf-8") as f:
            json.dump({"server": "http://100.x.x.x:8788", "publish_key": ""}, f, indent=2)
        print(f"J'ai créé {CONFIG_PATH.name} : mets-y l'adresse du Pi et la clé de publication, puis relance.")
        sys.exit(1)
    with CONFIG_PATH.open(encoding="utf-8") as f:
        config = json.load(f)
    if "x.x" in config.get("server", "") or not config.get("publish_key"):
        print(f"Complète {CONFIG_PATH} (adresse du Pi et clé de publication), puis relance.")
        sys.exit(1)
    return config


class Server:
    def __init__(self, url, key):
        parts = urlsplit(url if "://" in url else "http://" + url)
        self.host, self.port = parts.hostname, parts.port or 80
        self.key = key

    def connect(self, timeout=60):
        return http.client.HTTPConnection(self.host, self.port, timeout=timeout)

    def request_json(self, method, path, payload=None):
        conn = self.connect()
        body = json.dumps(payload).encode("utf-8") if payload is not None else None
        headers = {"X-Publish-Key": self.key, "Content-Type": "application/json"}
        conn.request(method, path, body=body, headers=headers)
        response = conn.getresponse()
        data = response.read()
        conn.close()
        try:
            result = json.loads(data.decode("utf-8"))
        except ValueError:
            result = {"error": data[:200].decode("utf-8", "replace")}
        if response.status != 200:
            raise RuntimeError(f"{method} {path} : {response.status} {result.get('error', '')}")
        return result

    def upload(self, path, sha, size, progress):
        conn = self.connect(timeout=300)
        conn.putrequest("PUT", f"/blobs/{sha}")
        conn.putheader("X-Publish-Key", self.key)
        conn.putheader("Content-Type", "application/octet-stream")
        conn.putheader("Content-Length", str(size))
        conn.endheaders()
        with path.open("rb") as f:
            while True:
                block = f.read(CHUNK)
                if not block:
                    break
                conn.send(block)
                progress(len(block))
        response = conn.getresponse()
        data = response.read()
        conn.close()
        if response.status != 200:
            raise RuntimeError(f"envoi de {path.name} : {response.status} {data[:200].decode('utf-8', 'replace')}")


def list_build_files(build):
    files = []
    for root, dirs, names in os.walk(build):
        dirs[:] = [d for d in dirs if d not in EXCLUDED_DIRS]
        for name in names:
            if name in EXCLUDED_NAMES or name.lower().endswith(EXCLUDED_SUFFIXES):
                continue
            files.append(Path(root) / name)
    return sorted(files)


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            block = f.read(CHUNK)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def hash_files(build, files):
    """Empreintes, avec un cache (taille + date) pour ne pas relire les gros .pak qui n'ont pas changé"""
    try:
        with CACHE_PATH.open(encoding="utf-8") as f:
            cache = json.load(f)
    except (OSError, ValueError):
        cache = {}
    result = {}
    for i, path in enumerate(files, 1):
        stat = path.stat()
        key = str(path.resolve())
        entry = cache.get(key)
        if not entry or entry["size"] != stat.st_size or entry["mtime"] != stat.st_mtime_ns:
            print(f"  empreinte {i}/{len(files)} : {path.relative_to(build)} ({stat.st_size / 1e6:.0f} Mo)")
            entry = {"size": stat.st_size, "mtime": stat.st_mtime_ns, "sha256": sha256_file(path)}
            cache[key] = entry
        result[path.relative_to(build).as_posix()] = {"size": entry["size"], "sha256": entry["sha256"]}
    with CACHE_PATH.open("w", encoding="utf-8") as f:
        json.dump(cache, f)
    return result


def find_exe(build):
    exes = sorted(p.name for p in build.glob("*.exe"))
    if not exes:
        print(f"Aucun .exe dans {build}. Donne le dossier qui contient PokemonNewAge.exe "
              "(celui créé par Plateformes > Windows > Packager le projet).")
        sys.exit(1)
    preferred = [e for e in exes if e.lower().startswith("pokemonnewage")]
    return (preferred or exes)[0]


def main():
    parser = argparse.ArgumentParser(description="Publie le build packagé sur le serveur de mises à jour.")
    parser.add_argument("build", help="dossier du build (celui qui contient PokemonNewAge.exe)")
    parser.add_argument("--note", default="", help="petite description de la version, affichée par le launcher")
    args = parser.parse_args()

    config = load_config()
    server = Server(config["server"], config["publish_key"])
    build = Path(args.build).resolve()
    if not build.is_dir():
        print(f"Dossier introuvable : {build}")
        return 1
    exe = find_exe(build)

    try:
        health = server.request_json("GET", "/health")
    except (OSError, RuntimeError) as e:
        print(f"Le Pi ne répond pas ({e}). Vérifie que Tailscale est connecté et que le serveur tourne.")
        return 1
    print(f"Serveur joint, version actuelle sur le Pi : {health.get('version') or 'aucune'}")

    version = datetime.now().strftime("%Y.%m.%d-%H%M%S")
    (build / "version.txt").write_text(version, encoding="utf-8")
    print(f"Version {version} : calcul des empreintes...")
    files = hash_files(build, list_build_files(build))
    total = sum(info["size"] for info in files.values())

    hashes = sorted({info["sha256"] for info in files.values()})
    missing = set()
    for start in range(0, len(hashes), 2000):
        missing.update(server.request_json("POST", "/missing", {"hashes": hashes[start:start + 2000]})["missing"])
    to_send = {}
    for rel, info in files.items():
        if info["sha256"] in missing and info["sha256"] not in to_send:
            to_send[info["sha256"]] = (build / rel, info["size"])
    send_total = sum(size for _, size in to_send.values())
    print(f"{len(files)} fichiers ({total / 1e9:.2f} Go) ; à envoyer : {len(to_send)} ({send_total / 1e9:.2f} Go)")

    sent = 0
    started = time.time()
    last_print = [0.0]

    def progress(n):
        nonlocal sent
        sent += n
        now = time.time()
        if now - last_print[0] > 1 or sent == send_total:
            last_print[0] = now
            speed = sent / max(0.1, now - started) / 1e6
            print(f"\r  envoi : {sent / 1e6:.0f} / {send_total / 1e6:.0f} Mo ({speed:.1f} Mo/s)   ", end="", flush=True)

    for sha, (path, size) in to_send.items():
        for attempt in range(3):
            try:
                server.upload(path, sha, size, progress)
                break
            except (OSError, RuntimeError) as e:
                if attempt == 2:
                    print(f"\nÉchec de l'envoi de {path.name} : {e}")
                    return 1
                print(f"\n  coupure ({e}), nouvel essai...")
                time.sleep(3)
    if to_send:
        print()

    manifest = {"version": version, "note": args.note, "exe": exe, "files": files}
    server.request_json("PUT", "/manifest", manifest)
    print(f"Version {version} publiée. Les launchers la téléchargeront à leur prochaine ouverture.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
