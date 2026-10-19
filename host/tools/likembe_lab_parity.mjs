// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Prints the browser lab engine's event stream (docs/demo/likembe_engine.js) in the
// same format and with the same options as host/tools/likembe_dump.cpp.
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
const here = path.dirname(fileURLToPath(import.meta.url));
const E = createRequire(import.meta.url)(path.join(here, '../../docs/demo/likembe_engine.js'));
const a = process.argv.slice(2), opt = {};
for (let i = 0; i + 1 < a.length; i += 2) opt[a[i].replace(/^--/, '')] = parseInt(a[i + 1], 10);
const p = E.defaults(opt.part || 0);
const map = {seed: 'seed', part: 'part', dens: 'density_pct', root: 'root', oct: 'octave', var: 'variation_pct', pass: 'passage_idx'};
for (const [k, v] of Object.entries(opt)) {
  if (map[k]) p[map[k]] = v;
  else if (k === 'grid') p.grid16 = v === 16 ? 1 : 0;
  else if (k !== 'bars') { console.error(`unknown option --${k}`); process.exit(2); }
}
process.stdout.write(E.dump(p, opt.bars || 16));
