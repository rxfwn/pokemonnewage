#!/usr/bin/env python3
"""
Serveur de mises à jour du jeu Pokémon (docs/MISES_A_JOUR.md).

Tourne sur le Raspberry Pi, à côté du serveur de saves. Il garde la dernière version packagée du
jeu, et le launcher de chaque joueur vient y chercher les fichiers qui ont changé.
Python 3.9+ seulement, aucune bibliothèque à installer (marche aussi sur Windows pour tester).

    python3 pokemon_update_server.py cle-publication   # affiche la clé pour publier (la crée au besoin)
    python3 pokemon_update_server.py versions          # liste les versions gardées
    python3 pokemon_update_server.py                   # lance le serveur

Rangement (dans « data_dir ») :
    blobs/<sha256>          chaque fichier du jeu, rangé par son empreinte : un fichier qui ne
                            change pas d'une version à l'autre n'est gardé et envoyé qu'une fois
    versions/<version>.json la liste des fichiers de chaque version (le « manifest »)
    current.json            la version que les launchers téléchargent

Adresses :
    GET  /health                    le serveur répond
    GET  /manifest                  la version actuelle (JSON)
    GET  /blobs/<sha256>            un fichier (reprise possible avec l'en-tête Range)
    POST /missing      [clé]        corps {"hashes": [...]} -> ceux que le serveur n'a pas encore
    PUT  /blobs/<sha256> [clé]      envoie un fichier (vérifié avant d'être gardé)
    PUT  /manifest     [clé]        publie une version (tous ses fichiers doivent être là)

Lire ne demande pas de clé : le serveur n'est joignable que par Tailscale, donc seulement par les
amis à qui tu as partagé le Pi. Publier demande la clé de publication (en-tête X-Publish-Key).
"""

import hashlib
import hmac
import json
import os
import re
import secrets
import sys
import threading
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent
CONFIG_PATH = BASE_DIR / "config.json"

DEFAULT_CONFIG = {
    # 0.0.0.0 = toutes les cartes réseau. Mettre l'adresse Tailscale du Pi (100.x.x.x)
    # pour n'écouter que sur Tailscale.
    "host": "0.0.0.0",
    "port": 8788,
    # Dossier des builds : mettre un chemin sur la clé ou le disque USB (le jeu fait plusieurs Go),
    # par exemple "/mnt/jeu/mises-a-jour"
    "data_dir": "data",
    # Versions gardées (pour revenir en arrière) ; les fichiers qui ne servent plus sont effacés
    "keep_versions": 3,
    # Clé pour publier (créée par « cle-publication »)
    "publish_key": "",
}

SHA256 = re.compile(r"^[0-9a-f]{64}$")
VERSION = re.compile(r"^[A-Za-z0-9._-]{1,64}$")
CHUNK = 1024 * 1024
LOCK = threading.Lock()


def now_iso():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def load_config():
    if not CONFIG_PATH.exists():
        save_config(DEFAULT_CONFIG)
    with CONFIG_PATH.open(encoding="utf-8") as f:
        config = json.load(f)
    for key, value in DEFAULT_CONFIG.items():
        config.setdefault(key, value)
    return config


def save_config(config):
    tmp = CONFIG_PATH.with_suffix(".tmp")
    with tmp.open("w", encoding="utf-8") as f:
        json.dump(config, f, indent=2, ensure_ascii=False)
    os.replace(tmp, CONFIG_PATH)


def write_atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("wb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)


def check_manifest(manifest):
    """Renvoie un message d'erreur, ou None si le manifest est correct"""
    if not isinstance(manifest, dict):
        return "manifest illisible"
    if not VERSION.match(str(manifest.get("version", ""))):
        return "version invalide (lettres, chiffres, . _ - seulement)"
    if not isinstance(manifest.get("exe"), str) or not manifest["exe"]:
        return "exe manquant"
    files = manifest.get("files")
    if not isinstance(files, dict) or not files:
        return "liste de fichiers vide"
    for path, info in files.items():
        parts = path.split("/")
        if path.startswith("/") or "\\" in path or ":" in path or ".." in parts or "" in parts:
            return f"chemin refusé : {path}"
        if not isinstance(info, dict) or not SHA256.match(str(info.get("sha256", ""))):
            return f"empreinte invalide : {path}"
        if not isinstance(info.get("size"), int) or info["size"] < 0:
            return f"taille invalide : {path}"
    if manifest["exe"] not in files:
        return "l'exe n'est pas dans la liste des fichiers"
    return None


class Store:
    def __init__(self, config):
        data_dir = Path(config["data_dir"])
        self.root = (data_dir if data_dir.is_absolute() else BASE_DIR / data_dir).resolve()
        self.keep = max(1, int(config["keep_versions"]))
        (self.root / "blobs").mkdir(parents=True, exist_ok=True)
        (self.root / "versions").mkdir(parents=True, exist_ok=True)

    def blob(self, sha):
        return self.root / "blobs" / sha

    def current(self):
        path = self.root / "current.json"
        if not path.exists():
            return None
        with path.open(encoding="utf-8") as f:
            return json.load(f)

    def versions(self):
        """Manifests gardés, du plus ancien au plus récent"""
        result = []
        for path in (self.root / "versions").glob("*.json"):
            try:
                with path.open(encoding="utf-8") as f:
                    result.append(json.load(f))
            except (OSError, ValueError):
                continue
        return sorted(result, key=lambda m: m.get("published_at", ""))

    def publish(self, manifest):
        manifest["published_at"] = now_iso()
        data = json.dumps(manifest, ensure_ascii=False, indent=1).encode("utf-8")
        write_atomic(self.root / "versions" / f"{manifest['version']}.json", data)
        # Le changement de version se fait d'un coup : un launcher ne voit jamais une version à moitié publiée
        write_atomic(self.root / "current.json", data)
        self.prune()

    def prune(self):
        versions = self.versions()
        current = self.current()
        old = [m for m in versions[: max(0, len(versions) - self.keep)]
               if not current or m["version"] != current["version"]]
        for manifest in old:
            (self.root / "versions" / f"{manifest['version']}.json").unlink(missing_ok=True)
        used = set()
        for manifest in self.versions():
            used.update(info["sha256"] for info in manifest["files"].values())
        for path in (self.root / "blobs").iterdir():
            # Les .part sont des envois en cours : on ne les touche pas
            if SHA256.match(path.name) and path.name not in used:
                path.unlink(missing_ok=True)


class Handler(BaseHTTPRequestHandler):
    server_version = "PokemonUpdateServer/1"
    protocol_version = "HTTP/1.1"
    config = None
    store = None

    def log_message(self, fmt, *args):
        sys.stderr.write("%s %s - %s\n" % (now_iso(), self.address_string(), fmt % args))

    def send_json(self, status, payload):
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def error(self, status, message):
        self.send_json(status, {"error": message})

    def parts(self):
        return [p for p in self.path.split("?")[0].split("/") if p]

    def can_publish(self):
        key = self.config.get("publish_key", "")
        given = self.headers.get("X-Publish-Key", "")
        return bool(key) and bool(given) and hmac.compare_digest(key.encode(), given.encode())

    def read_json_body(self, limit=64 * 1024 * 1024):
        length = int(self.headers.get("Content-Length", "0") or 0)
        if length <= 0 or length > limit:
            return None
        try:
            return json.loads(self.rfile.read(length).decode("utf-8"))
        except ValueError:
            return None

    # ---------------------------------------------------------------- Lecture

    def do_GET(self):
        parts = self.parts()
        if parts == ["health"]:
            current = self.store.current()
            self.send_json(HTTPStatus.OK, {"ok": True, "time": now_iso(),
                                           "version": current["version"] if current else None})
        elif parts == ["manifest"]:
            current = self.store.current()
            if not current:
                self.error(HTTPStatus.NOT_FOUND, "aucune version publiée")
                return
            self.send_json(HTTPStatus.OK, current)
        elif len(parts) == 2 and parts[0] == "blobs" and SHA256.match(parts[1]):
            self.send_blob(parts[1])
        else:
            self.error(HTTPStatus.NOT_FOUND, "adresse inconnue")

    def send_blob(self, sha):
        path = self.store.blob(sha)
        try:
            f = path.open("rb")
        except OSError:
            self.error(HTTPStatus.NOT_FOUND, "fichier inconnu")
            return
        with f:
            size = os.fstat(f.fileno()).st_size
            start = 0
            match = re.match(r"^bytes=(\d+)-$", self.headers.get("Range", ""))
            if match and int(match.group(1)) < size:
                start = int(match.group(1))
                self.send_response(HTTPStatus.PARTIAL_CONTENT)
                self.send_header("Content-Range", f"bytes {start}-{size - 1}/{size}")
            else:
                self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(size - start))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            f.seek(start)
            try:
                while True:
                    block = f.read(CHUNK)
                    if not block:
                        break
                    self.wfile.write(block)
            except (BrokenPipeError, ConnectionResetError):
                # Le launcher a été fermé pendant le téléchargement : il reprendra là où il en était
                self.close_connection = True

    # ---------------------------------------------------------------- Publication

    def do_POST(self):
        if self.parts() != ["missing"]:
            self.error(HTTPStatus.NOT_FOUND, "adresse inconnue")
            return
        if not self.can_publish():
            self.error(HTTPStatus.UNAUTHORIZED, "clé de publication manquante ou fausse")
            return
        body = self.read_json_body()
        hashes = body.get("hashes") if isinstance(body, dict) else None
        if not isinstance(hashes, list) or not all(isinstance(h, str) and SHA256.match(h) for h in hashes):
            self.error(HTTPStatus.BAD_REQUEST, "liste d'empreintes invalide")
            return
        missing = sorted({h for h in hashes if not self.store.blob(h).exists()})
        self.send_json(HTTPStatus.OK, {"missing": missing})

    def do_PUT(self):
        parts = self.parts()
        if not self.can_publish():
            # Le corps n'est pas lu : on ferme la connexion après la réponse
            self.close_connection = True
            self.error(HTTPStatus.UNAUTHORIZED, "clé de publication manquante ou fausse")
            return
        if len(parts) == 2 and parts[0] == "blobs" and SHA256.match(parts[1]):
            self.receive_blob(parts[1])
        elif parts == ["manifest"]:
            manifest = self.read_json_body()
            problem = check_manifest(manifest)
            if problem:
                self.error(HTTPStatus.BAD_REQUEST, problem)
                return
            missing = [p for p, info in manifest["files"].items() if not self.store.blob(info["sha256"]).exists()]
            if missing:
                self.error(HTTPStatus.CONFLICT, f"{len(missing)} fichier(s) pas encore envoyé(s), ex. {missing[0]}")
                return
            with LOCK:
                self.store.publish(manifest)
            total = sum(info["size"] for info in manifest["files"].values())
            self.log_message("version %s publiée : %d fichiers, %.1f Go", manifest["version"],
                             len(manifest["files"]), total / 1e9)
            self.send_json(HTTPStatus.OK, {"ok": True, "version": manifest["version"]})
        else:
            self.close_connection = True
            self.error(HTTPStatus.NOT_FOUND, "adresse inconnue")

    def receive_blob(self, sha):
        length = int(self.headers.get("Content-Length", "-1") or -1)
        if length < 0:
            self.close_connection = True
            self.error(HTTPStatus.LENGTH_REQUIRED, "taille manquante")
            return
        target = self.store.blob(sha)
        tmp = target.with_name(f"{sha}.{secrets.token_hex(4)}.part")
        digest = hashlib.sha256()
        remaining = length
        try:
            with tmp.open("wb") as f:
                while remaining > 0:
                    block = self.rfile.read(min(CHUNK, remaining))
                    if not block:
                        break
                    digest.update(block)
                    f.write(block)
                    remaining -= len(block)
                f.flush()
                os.fsync(f.fileno())
            if remaining > 0:
                tmp.unlink(missing_ok=True)
                self.close_connection = True
                self.error(HTTPStatus.BAD_REQUEST, "envoi coupé")
                return
            if digest.hexdigest() != sha:
                tmp.unlink(missing_ok=True)
                self.error(HTTPStatus.BAD_REQUEST, "le fichier reçu ne correspond pas à son empreinte")
                return
            os.replace(tmp, target)
        except OSError as e:
            tmp.unlink(missing_ok=True)
            self.close_connection = True
            self.error(HTTPStatus.INSUFFICIENT_STORAGE, f"écriture impossible sur le serveur : {e.strerror}")
            return
        self.send_json(HTTPStatus.OK, {"ok": True})


# -------------------------------------------------------------------- Commandes

def show_publish_key():
    config = load_config()
    if not config["publish_key"]:
        config["publish_key"] = secrets.token_urlsafe(24)
        save_config(config)
        print("Clé de publication créée.")
    print(f"Clé de publication (à garder pour toi, à mettre dans publier_config.json) : {config['publish_key']}")
    return 0


def list_versions():
    config = load_config()
    store = Store(config)
    current = store.current()
    versions = store.versions()
    if not versions:
        print("Aucune version publiée.")
    for manifest in versions:
        total = sum(info["size"] for info in manifest["files"].values())
        mark = "  <- actuelle" if current and manifest["version"] == current["version"] else ""
        note = f" ({manifest['note']})" if manifest.get("note") else ""
        print(f"- {manifest['version']}{note} : {total / 1e9:.2f} Go, publiée le {manifest.get('published_at')}{mark}")
    return 0


def use_version(version):
    """Revenir à une version gardée : python3 pokemon_update_server.py utiliser <version>"""
    config = load_config()
    store = Store(config)
    path = store.root / "versions" / f"{version}.json"
    if not VERSION.match(version) or not path.exists():
        print(f"Version {version} introuvable. Versions gardées :")
        return list_versions() or 1
    with path.open(encoding="utf-8") as f:
        manifest = json.load(f)
    write_atomic(store.root / "current.json", json.dumps(manifest, ensure_ascii=False, indent=1).encode("utf-8"))
    print(f"Les launchers téléchargeront maintenant la version {version}.")
    return 0


def run_server():
    config = load_config()
    Handler.config = config
    Handler.store = Store(config)
    if not config["publish_key"]:
        print("Attention : pas de clé de publication. Crée-la avec : python3 pokemon_update_server.py cle-publication")
    server = ThreadingHTTPServer((config["host"], int(config["port"])), Handler)
    server.daemon_threads = True
    current = Handler.store.current()
    print(f"Serveur de mises à jour sur {config['host']}:{config['port']} (builds dans {Handler.store.root}), "
          f"version actuelle : {current['version'] if current else 'aucune'}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("Arrêt.")
    return 0


def main(argv):
    if len(argv) == 2 and argv[1] == "cle-publication":
        return show_publish_key()
    if len(argv) == 2 and argv[1] == "versions":
        return list_versions()
    if len(argv) == 3 and argv[1] == "utiliser":
        return use_version(argv[2])
    if len(argv) == 1:
        return run_server()
    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
