"""Petits sons du lancer de Poké Ball (SCRUM-40), synthétisés : aucun fichier à télécharger.

    python Tools/Capture/make_ball_sounds.py [dossier_de_sortie]

Écrit SW_Ball_Throw, Open, Shake, Caught, BrokeFree (.wav 16 bits mono 44,1 kHz), par défaut dans
Tools/Capture/Generated/. import_pokeball_unreal.py les fabrique tout seul s'ils manquent.
Sons provisoires : on les remplace par de vrais bruitages en changeant les sons de BP_PokeBallThrow
(ou les fichiers de Content/Capture/Sons), sans toucher au code.
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
    gain = 0.8 / peak
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * gain)) * 32767)) for s in samples))


def envelope(i, n, attack, release):
    t, duration = i / RATE, n / RATE
    return min(1.0, t / max(attack, 1e-4)) * min(1.0, max(0.0, duration - t) / max(release, 1e-4))


def tone(freq, duration, freq_end=None, attack=0.005, release=0.05, volume=1.0):
    n = int(RATE * duration)
    out, phase = [], 0.0
    for i in range(n):
        f = freq if freq_end is None else freq * (freq_end / freq) ** (i / n)
        phase += 2 * math.pi * f / RATE
        out.append(math.sin(phase) * envelope(i, n, attack, release) * volume)
    return out


def noise(duration, smooth, seed, attack=0.01, release=0.1, volume=1.0, swell=False):
    """Bruit filtré (passe-bas simple) : smooth proche de 1 = plus sourd."""
    rnd = random.Random(seed)
    n = int(RATE * duration)
    out, value = [], 0.0
    for i in range(n):
        value = value * smooth + rnd.uniform(-1, 1) * (1 - smooth)
        shape = math.sin(math.pi * i / n) if swell else envelope(i, n, attack, release)
        out.append(value * shape * volume)
    return out


def click(seed, pitch=3200.0, volume=1.0):
    """Choc de plastique : impulsion de bruit très courte + petite résonance aiguë."""
    return mix(noise(0.012, 0.2, seed, attack=0.0005, release=0.01, volume=volume),
               tone(pitch, 0.05, attack=0.0005, release=0.045, volume=0.4 * volume))


def silence(duration):
    return [0.0] * int(RATE * duration)


def mix(*tracks):
    n = max(len(t) for t in tracks)
    return [sum(t[i] for t in tracks if i < len(t)) for i in range(n)]


def sequence(*parts):
    out = []
    for part in parts:
        out.extend(part)
    return out


SOUNDS = {
    # Ça fend l'air
    "SW_Ball_Throw": lambda: noise(0.42, 0.93, 1, swell=True),
    # La Ball s'ouvre : déclic puis rayon d'énergie qui monte
    "SW_Ball_Open": lambda: mix(click(2, 2600),
                                sequence(silence(0.03), tone(300, 0.4, freq_end=1400, attack=0.02, release=0.15, volume=0.6)),
                                sequence(silence(0.03), noise(0.4, 0.6, 3, attack=0.02, release=0.2, volume=0.3))),
    # Elle bascule : deux petits chocs
    "SW_Ball_Shake": lambda: sequence(click(4, 1800, 0.8), silence(0.11), click(5, 1600, 0.6)),
    # Clic du bouton qui se verrouille, puis petit carillon
    "SW_Ball_Caught": lambda: mix(click(6, 4000),
                                  sequence(silence(0.12), tone(1046.5, 0.18, release=0.1, volume=0.5)),
                                  sequence(silence(0.26), tone(1568.0, 0.35, release=0.3, volume=0.5))),
    # Elle s'ouvre d'un coup : pop puis son qui retombe
    "SW_Ball_BrokeFree": lambda: mix(noise(0.08, 0.5, 7, attack=0.001, release=0.07),
                                     tone(1200, 0.35, freq_end=250, attack=0.002, release=0.2, volume=0.6)),
}


def main(output):
    os.makedirs(output, exist_ok=True)
    paths = []
    for name, make in SOUNDS.items():
        path = os.path.join(output, name + ".wav")
        write_wav(path, make())
        paths.append(path)
    return paths


if __name__ == "__main__":
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "Generated")
    for written in main(out_dir):
        print(written)
