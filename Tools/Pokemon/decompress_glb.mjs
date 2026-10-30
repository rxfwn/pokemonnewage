// Retire la compression Draco des .glb (pour le module `pip install bpy`, livré sans Draco).
// Inutile avec Blender installé normalement : il lit le Draco tout seul.
//
//   npm install @gltf-transform/core@4 @gltf-transform/extensions@4 draco3dgltf
//   node Tools/Pokemon/decompress_glb.mjs <dossier entrée> <dossier sortie>
//
// Garde la même arborescence (regular/1.glb, shiny/1.glb...).

import fs from 'node:fs';
import path from 'node:path';
import { NodeIO } from '@gltf-transform/core';
import { ALL_EXTENSIONS } from '@gltf-transform/extensions';
import draco3d from 'draco3dgltf';

const [src, out] = process.argv.slice(2);
if (!src || !out) {
  console.error('Usage : node decompress_glb.mjs <entrée> <sortie>');
  process.exit(1);
}

const io = new NodeIO()
  .registerExtensions(ALL_EXTENSIONS)
  .registerDependencies({ 'draco3d.decoder': await draco3d.createDecoderModule() });

for (const category of fs.readdirSync(src)) {
  const dir = path.join(src, category);
  if (!fs.statSync(dir).isDirectory()) continue;
  fs.mkdirSync(path.join(out, category), { recursive: true });
  for (const file of fs.readdirSync(dir).filter((f) => f.endsWith('.glb'))) {
    const doc = await io.read(path.join(dir, file));
    for (const ext of doc.getRoot().listExtensionsUsed()) {
      if (ext.extensionName === 'KHR_draco_mesh_compression') ext.dispose();
    }
    await io.write(path.join(out, category, file), doc);
    console.log(`${category}/${file}`);
  }
}
