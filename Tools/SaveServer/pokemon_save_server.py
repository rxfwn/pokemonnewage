#!/usr/bin/env python3
"""
Serveur de saves du jeu Pokémon (docs/SERVEUR_SAVES.md).

Garde la sauvegarde de chaque joueur et sa carte dresseur, pour une poignée d'amis sur un réseau
privé (Tailscale). Python 3.9+ seulement, aucune bibliothèque à installer : marche pareil sur le
Raspberry Pi et sur un PC Windows.

    python pokemon_save_server.py ajouter-joueur Marie     # crée le joueur et affiche sa clé
    python pokemon_save_server.py joueurs                  # liste les joueurs
    python pokemon_save_server.py                          # lance le serveur

Sécurité minimale voulue : une clé secrète par joueur (chacun n'écrit que sa save), taille maximale
par fichier, et le serveur ne doit être joignable que par Tailscale (aucun port ouvert sur la box).
"""

import hmac
import json
import os
import re
import secrets
import shutil
import sys
import threading
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent
CONFIG_PATH = BASE_DIR / "config.json"

DEFAULT_CONFIG = {
    # 0.0.0.0 = toutes les cartes réseau. Mettre l'adresse Tailscale du serveur (100.x.x.x)
    # pour n'écouter que sur Tailscale.
    "host": "0.0.0.0",
    "port": 8787,
    "data_dir": "data",
    # Versions gardées de chaque save, pour revenir en arrière
    "keep_versions": 10,
    # Taille maximale d'une save ou d'une carte, en octets
    "max_size": 1024 * 1024,
    # Joueur -> clé secrète (créés par « ajouter-joueur »)
    "players": {},
}

PLAYER_NAME = re.compile(r"^[A-Za-z0-9_-]{1,32}$")
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
    """Écrit d'un coup : une coupure de courant ne laisse jamais un fichier à moitié écrit."""
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("wb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)


class Store:
    """data/saves/<joueur>/current.sav, meta.json, historique/ ; data/cards/<joueur>.json"""

    def __init__(self, config):
        self.root = (BASE_DIR / config["data_dir"]).resolve()
        self.keep = max(1, int(config["keep_versions"]))

    def save_dir(self, player):
        return self.root / "saves" / player

    def meta(self, player):
        path = self.save_dir(player) / "meta.json"
        if not path.exists():
            return None
        with path.open(encoding="utf-8") as f:
            return json.load(f)

    def read_save(self, player):
        path = self.save_dir(player) / "current.sav"
        return path.read_bytes() if path.exists() else None

    def write_save(self, player, data, client_saved_at):
        folder = self.save_dir(player)
        current = folder / "current.sav"
        old = self.meta(player) or {"version": 0}
        # L'ancienne save part dans l'historique avant d'être remplacée
        if current.exists():
            history = folder / "historique"
            history.mkdir(parents=True, exist_ok=True)
            shutil.copy2(current, history / f"v{old['version']:06d}.sav")
            versions = sorted(history.glob("v*.sav"))
            for extra in versions[: max(0, len(versions) - self.keep)]:
                extra.unlink()
        write_atomic(current, data)
        meta = {
            "version": old["version"] + 1,
            "saved_at": client_saved_at or now_iso(),
            "received_at": now_iso(),
            "size": len(data),
        }
        write_atomic(folder / "meta.json", json.dumps(meta, indent=2).encode("utf-8"))
        return meta

    def write_card(self, player, card):
        write_atomic(self.root / "cards" / f"{player}.json", json.dumps(card, ensure_ascii=False, indent=2).encode("utf-8"))

    def read_cards(self):
        cards = []
        folder = self.root / "cards"
        for path in sorted(folder.glob("*.json")) if folder.exists() else []:
            try:
                with path.open(encoding="utf-8") as f:
                    card = json.load(f)
                card["Player"] = path.stem
                cards.append(card)
            except (OSError, ValueError):
                continue
        return cards


class Handler(BaseHTTPRequestHandler):
    server_version = "PokemonSaveServer/1"
    config = None
    store = None

    # ---------------------------------------------------------------- Outils

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

    def authenticated_player(self):
        """Le joueur dont la clé est dans l'en-tête X-Pokemon-Key, ou None"""
        key = self.headers.get("X-Pokemon-Key", "")
        for player, player_key in self.config["players"].items():
            if key and hmac.compare_digest(key.encode(), player_key.encode()):
                return player
        return None

    def route(self):
        """('saves', 'Marie') pour /saves/Marie ; ('cards', None) pour /cards"""
        parts = [p for p in self.path.split("?")[0].split("/") if p]
        if not parts:
            return None, None
        if len(parts) == 1:
            return parts[0], None
        if len(parts) == 2 and PLAYER_NAME.match(parts[1]):
            return parts[0], parts[1]
        return "invalid", None

    def read_body(self):
        length = int(self.headers.get("Content-Length", "0") or 0)
        if length <= 0:
            self.error(HTTPStatus.BAD_REQUEST, "corps vide")
            return None
        if length > int(self.config["max_size"]):
            self.error(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "fichier trop gros")
            return None
        return self.rfile.read(length)

    # ---------------------------------------------------------------- Requêtes

    def do_GET(self):
        kind, player = self.route()
        if kind == "health":
            self.send_json(HTTPStatus.OK, {"ok": True, "time": now_iso()})
            return
        me = self.authenticated_player()
        if not me:
            self.error(HTTPStatus.UNAUTHORIZED, "clé manquante ou fausse")
            return

        if kind == "saves" and player:
            # Chacun ne lit que sa propre save
            if player != me:
                self.error(HTTPStatus.FORBIDDEN, "ce n'est pas ta save")
                return
            with LOCK:
                meta = self.store.meta(player)
                data = self.store.read_save(player)
            if data is None:
                self.error(HTTPStatus.NOT_FOUND, "pas encore de save")
                return
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("X-Save-Version", str(meta["version"]))
            self.send_header("X-Saved-At", meta["saved_at"])
            self.end_headers()
            self.wfile.write(data)
        elif kind == "info" and player:
            if player != me:
                self.error(HTTPStatus.FORBIDDEN, "ce n'est pas ta save")
                return
            with LOCK:
                meta = self.store.meta(player)
            self.send_json(HTTPStatus.OK, meta or {"version": 0})
        elif kind == "cards" and not player:
            # Les cartes des amis se lisent avec n'importe quelle clé valide
            with LOCK:
                cards = self.store.read_cards()
            self.send_json(HTTPStatus.OK, {"cards": cards})
        else:
            self.error(HTTPStatus.NOT_FOUND, "adresse inconnue")

    def do_PUT(self):
        kind, player = self.route()
        me = self.authenticated_player()
        if not me:
            self.error(HTTPStatus.UNAUTHORIZED, "clé manquante ou fausse")
            return
        if not player or player != me:
            self.error(HTTPStatus.FORBIDDEN, "chacun n'écrit que ses propres fichiers")
            return

        if kind == "saves":
            data = self.read_body()
            if data is None:
                return
            # Une save Unreal commence toujours par « GVAS » : on refuse tout autre fichier
            if not data.startswith(b"GVAS"):
                self.error(HTTPStatus.BAD_REQUEST, "ce n'est pas une save du jeu")
                return
            with LOCK:
                meta = self.store.write_save(player, data, self.headers.get("X-Saved-At", ""))
            self.log_message("save de %s : version %d, %d octets", player, meta["version"], meta["size"])
            self.send_json(HTTPStatus.OK, meta)
        elif kind == "cards":
            data = self.read_body()
            if data is None:
                return
            try:
                card = json.loads(data.decode("utf-8"))
                if not isinstance(card, dict):
                    raise ValueError
            except ValueError:
                self.error(HTTPStatus.BAD_REQUEST, "carte illisible")
                return
            with LOCK:
                self.store.write_card(player, card)
            self.send_json(HTTPStatus.OK, {"ok": True})
        else:
            self.error(HTTPStatus.NOT_FOUND, "adresse inconnue")


# -------------------------------------------------------------------- Commandes

def add_player(name):
    if not PLAYER_NAME.match(name):
        print("Nom invalide : lettres, chiffres, - et _ seulement (32 au plus), sans espace ni accent.")
        return 1
    config = load_config()
    if name in config["players"]:
        print(f"{name} existe déjà. Sa clé : {config['players'][name]}")
        return 0
    key = secrets.token_urlsafe(24)
    config["players"][name] = key
    save_config(config)
    print(f"Joueur {name} ajouté.")
    print(f"Sa clé (à lui donner, et à taper dans le jeu) : {key}")
    return 0


def list_players():
    config = load_config()
    store = Store(config)
    if not config["players"]:
        print("Aucun joueur. Ajoute-en un : python pokemon_save_server.py ajouter-joueur Marie")
    for name in config["players"]:
        meta = store.meta(name)
        detail = f"version {meta['version']}, sauvé le {meta['saved_at']}" if meta else "pas encore de save"
        print(f"- {name} : {detail}")
    return 0


def run_server():
    config = load_config()
    Handler.config = config
    Handler.store = Store(config)
    if not config["players"]:
        print("Attention : aucun joueur. Ajoute-en un avec : python pokemon_save_server.py ajouter-joueur Marie")
    server = ThreadingHTTPServer((config["host"], int(config["port"])), Handler)
    print(f"Serveur de saves sur {config['host']}:{config['port']} (données dans {Handler.store.root})")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("Arrêt.")
    return 0


def main(argv):
    if len(argv) >= 3 and argv[1] == "ajouter-joueur":
        return add_player(argv[2])
    if len(argv) == 2 and argv[1] == "joueurs":
        return list_players()
    if len(argv) == 1:
        return run_server()
    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
