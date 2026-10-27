"""
Petits sons du Pokédex (clic, bip, ouverture, fermeture, erreur, sauvegarde), synthétisés : aucun fichier à télécharger.

    python Tools/Pokedex/make_pokedex_sounds.py <dossier_de_sortie>

Écrit des .wav 16 bits mono 44,1 kHz (SW_Pokedex_*.wav) qu'import_pokedex_unreal.py importe dans Unreal.
Ce sont des sons provisoires : on pourra les remplacer par de vrais bruitages (AUD) sans toucher au code,
il suffit de changer les sons dans DA_PokedexModel.
"""

import math
import os
import random
import struct
import sys
import wave

RATE = 44100


def write_wav(path, samples):
    peak = max(1e-6, max(abs(s) for s in samples))
    gain = 0.85 / peak
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * gain)) * 32767)) for s in samples))


def tone(freq, duration, attack=0.004, release=0.04, shape="sine", freq_end=None):
    n = int(RATE * duration)
    out, phase = [], 0.0
    for i in range(n):
        t = i / RATE
        f = freq if freq_end is None else freq + (freq_end - freq) * (i / n)
        phase += 2 * math.pi * f / RATE
        if shape == "square":
            v = 0.6 if math.sin(phase) >= 0 else -0.6
        elif shape == "triangle":
            v = 2.0 / math.pi * math.asin(math.sin(phase))
        else:
            v = math.sin(phase)
        env = min(1.0, t / attack) * min(1.0, (duration - t) / release)
        out.append(v * env)
    return out


def silence(duration):
    return [0.0] * int(RATE * duration)


def mix(*tracks):
    n = max(len(t) for t in tracks)
    return [sum(t[i] for t in tracks if i < len(t)) for i in range(n)]


def click():
    """Bouton en plastique qui s'enfonce : petit choc + résonance aiguë très courte."""
    rnd = random.Random(4)
    n = int(RATE * 0.05)
    noise = [rnd.uniform(-1, 1) * math.exp(-i / (RATE * 0.004)) for i in range(n)]
    body = [math.sin(2 * math.pi * 2300 * i / RATE) * math.exp(-i / (RATE * 0.008)) * 0.5 for i in range(n)]
    return mix(noise, body)


def beep():
    return tone(1760, 0.07, shape="triangle")


def nav():
    return [s * 0.6 for s in tone(1320, 0.035, shape="triangle", release=0.02)]


def open_sound():
    """Charnière + petite mélodie de démarrage."""
    rnd = random.Random(7)
    n = int(RATE * 0.18)
    swish = [rnd.uniform(-1, 1) * 0.25 * math.sin(math.pi * i / n) for i in range(n)]
    notes = silence(0.12) + tone(988, 0.07, shape="triangle") + tone(1319, 0.07, shape="triangle") + tone(1976, 0.12, shape="triangle")
    return mix(swish, click() + silence(0.0), notes)


def close_sound():
    rnd = random.Random(9)
    n = int(RATE * 0.15)
    swish = [rnd.uniform(-1, 1) * 0.25 * math.sin(math.pi * i / n) for i in range(n)]
    snap = silence(0.13) + click()
    notes = tone(1319, 0.06, shape="triangle") + tone(880, 0.09, shape="triangle")
    return mix(swish, snap, notes)


def error():
    return tone(220, 0.09, shape="square") + silence(0.04) + tone(196, 0.14, shape="square")


def save():
    return (tone(1047, 0.08, shape="triangle") + tone(1319, 0.08, shape="triangle")
            + tone(1568, 0.08, shape="triangle") + tone(2093, 0.22, shape="triangle", release=0.15))


SOUNDS = {
    "SW_Pokedex_Click": click,
    "SW_Pokedex_Beep": beep,
    "SW_Pokedex_Nav": nav,
    "SW_Pokedex_Open": open_sound,
    "SW_Pokedex_Close": close_sound,
    "SW_Pokedex_Error": error,
    "SW_Pokedex_Save": save,
}


def main():
    out_dir = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__)))
    os.makedirs(out_dir, exist_ok=True)
    for name, make in SOUNDS.items():
        path = os.path.join(out_dir, name + ".wav")
        write_wav(path, make())
        print("Son écrit :", path)


if __name__ == "__main__":
    main()
