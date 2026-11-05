"""Trouve les matériaux d'yeux (et leur texture) des modèles Let's Go convertis (FBX) -> Tools/Pokemon/eye_materials.json.

Les matériaux s'appellent M_Pikachu_1, _2... et les textures T_Pikachu_1... : le seul endroit où l'on voit
lesquels sont des yeux, c'est le nom d'origine de l'image dans le FBX (pm0025_00_Eye_col).
setup_follower_assets.py lit le JSON dans Unreal pour créer les matériaux « yeux fermés ».

    python Tools/Pokemon/find_eye_materials.py <dossier des modèles dézippés> [sortie.json]
"""

import glob
import json
import os
import struct
import sys
def parse(path):
    d=open(path,'rb').read()
    ver=struct.unpack_from('<I',d,23)[0]; wide=ver>=7500
    def node(o):
        if wide: end,n,plen=struct.unpack_from('<QQQ',d,o); o+=24
        else: end,n,plen=struct.unpack_from('<III',d,o); o+=12
        nl=d[o]; o+=1
        if end==0: return None,o
        name=d[o:o+nl].decode(); o+=nl
        props=[]
        for _ in range(n):
            t=chr(d[o]); o+=1
            if t in 'SR': l=struct.unpack_from('<I',d,o)[0]; o+=4; v=d[o:o+l]; o+=l; props.append(v.decode('utf-8','replace') if t=='S' else v)
            elif t=='L': props.append(struct.unpack_from('<q',d,o)[0]); o+=8
            elif t=='I': props.append(struct.unpack_from('<i',d,o)[0]); o+=4
            elif t=='D': props.append(struct.unpack_from('<d',d,o)[0]); o+=8
            elif t=='F': o+=4; props.append(None)
            elif t=='Y': o+=2; props.append(None)
            elif t in 'CB': o+=1; props.append(None)
            elif t in 'fdlib':
                ln,enc,cl=struct.unpack_from('<III',d,o); o+=12+cl; props.append(None)
            else: raise ValueError(t)
        kids=[]
        while o<end:
            k,o2=node(o)
            if k is None: o=o2; break
            kids.append(k); o=o2
        o=end
        return (name,props,kids),o
    o=27; top=[]
    while o<len(d)-200:
        n,o=node(o)
        if n is None: break
        top.append(n)
    return top
def eyes(path):
    top=parse(path)
    objs={}; conns=[]
    for n in top:
        if n[0]=='Objects':
            for c in n[2]:
                objs[c[1][0]]=(c[0],c[1][1].split('\x00')[0], c)
        if n[0]=='Connections':
            for c in n[2]: conns.append((c[1][1],c[1][2]))
    res = {}
    for vid, (typ, name, node) in objs.items():
        if typ != 'Video' or 'Eye' not in name or 'Iris' in name:
            continue
        # Nom de l'image dans le FBX (T_Pikachu_3.png) = nom de la texture importée dans Unreal
        files = [k[1][0] for k in node[2] if k[0] in ('RelativeFilename', 'Filename') and k[1] and k[1][0]]
        texture = os.path.splitext(os.path.basename(files[0].replace('\\', '/')))[0] if files else ''
        for a, b in conns:
            if a == vid and b in objs and objs[b][0] == 'Texture':
                for a2, b2 in conns:
                    if a2 == b and b2 in objs and objs[b2][0] == 'Material':
                        res[objs[b2][1]] = texture
    return dict(sorted(res.items()))


def main():
    folder = sys.argv[1]
    target = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "eye_materials.json")
    out = {}
    for path in sorted(glob.glob(os.path.join(folder, "*", "*.fbx"))):
        found = eyes(path)
        print(os.path.basename(path), found)
        if found:
            out.setdefault(os.path.basename(os.path.dirname(path)), {}).update(found)
    with open(target, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"{sum(len(v) for v in out.values())} matériau(x) d'yeux -> {target}")


if __name__ == "__main__":
    main()
