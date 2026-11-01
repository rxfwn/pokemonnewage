"""
Sons de la progression (montée de niveau, attaque apprise, séquence d'évolution), synthétisés : rien à télécharger.

    python Tools/Progression/make_progression_sounds.py <dossier_de_sortie>

Écrit des .wav 16 bits mono 44,1 kHz (SW_*.wav) qu'import_progression_unreal.py importe dans Unreal
(il lance aussi ce script tout seul). Mélodies originales et provisoires : on pourra les remplacer par
de vrais sons (épopée Audio) sans toucher au code, il suffit de garder les mêmes noms d'assets.
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


def note(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def tone(freq, duration, attack=0.01, release=0.08, shape="sine", freq_end=None, volume=1.0, vibrato=0.0):
    n = int(RATE * duration)
    out, phase = [], 0.0
    for i in range(n):
        t = i / RATE
        f = freq if freq_end is None else freq + (freq_end - freq) * (i / n)
        if vibrato:
            f *= 1.0 + vibrato * math.sin(2 * math.pi * 5.5 * t)
        phase += 2 * math.pi * f / RATE
        if shape == "square":
            v = 0.5 if math.sin(phase) >= 0 else -0.5
        elif shape == "triangle":
            v = 2.0 / math.pi * math.asin(math.sin(phase))
        elif shape == "bell":
            v = math.sin(phase) + 0.35 * math.sin(2.76 * phase) * math.exp(-6 * t)
        else:
            v = math.sin(phase)
        env = min(1.0, t / attack) * min(1.0, max(0.0, duration - t) / release)
        out.append(v * env * volume)
    return out


def place(track, sound, start):
    i0 = int(start * RATE)
    if len(track) < i0 + len(sound):
        track.extend([0.0] * (i0 + len(sound) - len(track)))
    for i, v in enumerate(sound):
        track[i0 + i] += v
    return track


def melody(notes, step, shape="triangle", length=0.9, volume=1.0, start=0.0, track=None):
    """notes : numéros MIDI (None = silence), un par pas de `step` secondes"""
    track = track if track is not None else []
    for k, m in enumerate(notes):
        if m is not None:
            place(track, tone(note(m), step * length, shape=shape, volume=volume), start + k * step)
    return track


def level_up():
    """Petit jingle joyeux qui monte (environ 1,2 s)"""
    t = melody([72, 76, 79, 84], 0.11, shape="square", length=0.8, volume=0.5)
    place(t, tone(note(88), 0.5, shape="bell", release=0.4, volume=0.6), 0.44)
    place(t, tone(note(84), 0.5, shape="triangle", release=0.4, volume=0.5), 0.44)
    return t


def move_learned():
    t = melody([79, 76, 79, 84], 0.12, shape="triangle", length=0.85, volume=0.7)
    place(t, tone(note(88), 0.45, shape="bell", release=0.35, volume=0.6), 0.48)
    return t


def evolution_start():
    """Scintillement qui monte : le Pokémon commence à briller (1,6 s)"""
    t = tone(220, 1.6, attack=0.6, release=0.4, freq_end=880, shape="sine", volume=0.4, vibrato=0.01)
    rnd = random.Random(7)
    for k in range(14):
        place(t, tone(note(rnd.choice([84, 86, 88, 91, 93, 96])), 0.18, shape="bell", release=0.15, volume=0.25), 0.1 + k * 0.1)
    return t


def evolution_music():
    """Boucle de l'alternance des silhouettes : arpège qui accélère sur une nappe (≈ 8,5 s)"""
    t = []
    chords = [[57, 60, 64], [53, 57, 60], [55, 59, 62], [52, 56, 59]]  # la m, fa, sol, mi
    time, step = 0.0, 0.24
    k = 0
    while time < 8.2:
        chord = chords[(k // 8) % len(chords)]
        place(t, tone(note(chord[k % 3] + 12), step * 0.9, shape="triangle", volume=0.45), time)
        if k % 8 == 0:
            for m in chord:
                place(t, tone(note(m), min(step * 8, 2.0), attack=0.2, release=0.5, shape="sine", volume=0.18), time)
        time += step
        step = max(0.07, step * 0.985)
        k += 1
    return t


def evolution_wobble():
    """La Ball bouge sur l'herbe : petit cliquetis sec (0,2 s)"""
    rnd = random.Random(11)
    n = int(RATE * 0.2)
    t = [rnd.uniform(-1, 1) * math.exp(-60 * i / RATE) * 0.5 for i in range(n)]
    place(t, tone(note(52), 0.12, attack=0.002, release=0.1, shape="triangle", volume=0.6), 0.0)
    place(t, [v * 0.6 for v in t[:int(RATE * 0.03)]], 0.07)
    return t


def evolution_flash():
    """Souffle lumineux au moment du flash (0,9 s)"""
    rnd = random.Random(3)
    n = int(RATE * 0.9)
    noise = [rnd.uniform(-1, 1) * math.exp(-4 * i / RATE) * min(1.0, i / (0.02 * RATE)) * 0.35 for i in range(n)]
    place(noise, tone(1200, 0.9, attack=0.01, release=0.6, freq_end=300, volume=0.4), 0.0)
    return noise


def evolution_done():
    """Fanfare de félicitations (≈ 2,6 s)"""
    t = melody([67, 67, 67, 72, None, 71, 72, 76], 0.14, shape="square", length=0.85, volume=0.4)
    place(t, tone(note(79), 1.2, attack=0.02, release=0.8, shape="square", volume=0.4, vibrato=0.006), 1.12)
    for m in (60, 64, 67, 72):
        place(t, tone(note(m), 1.5, attack=0.05, release=1.0, shape="triangle", volume=0.25), 1.12)
    return t


def evolution_cancel():
    """L'évolution s'arrête : notes qui descendent (1 s)"""
    t = melody([72, 69, 65, 62], 0.16, shape="triangle", length=0.9, volume=0.6)
    place(t, tone(note(57), 0.45, release=0.35, shape="sine", volume=0.5), 0.64)
    return t


SOUNDS = {
    "SW_LevelUp": level_up,
    "SW_MoveLearned": move_learned,
    "SW_Evolution_Wobble": evolution_wobble,
    "SW_Evolution_Start": evolution_start,
    "SW_Evolution_Music": evolution_music,
    "SW_Evolution_Flash": evolution_flash,
    "SW_Evolution_Done": evolution_done,
    "SW_Evolution_Cancel": evolution_cancel,
}


def main(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for name, make in SOUNDS.items():
        path = os.path.join(out_dir, name + ".wav")
        write_wav(path, make())
        print(path)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "Generated", "Sounds"))
