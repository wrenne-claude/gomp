#!/usr/bin/env node
// Extracts omp's settings schema (id, type, default, enum values, ui tab/group/label/description)
// from the compiled omp binary and writes it as JSON. gomp bundles the result so the Settings
// screen can render labelled, grouped, typed controls; values themselves always come from
// `omp config list --json` at runtime.
//
// Usage: node tools/extract-omp-schema.mjs [path/to/omp] > resources/schema/omp-settings.json
//
// Each setting is declared in omp's bundle as `<ident> = ae({ id: "...", ... })`. We bracket-match
// the object literal and evaluate it in an empty vm context: any identifier the literal references
// resolves to an inert helper that returns its first argument as a string (descriptions call key-
// label formatters like `Le("escape")`), never to a real global.

import { readFileSync } from "node:fs";
import { execFileSync } from "node:child_process";
import vm from "node:vm";

const binPath = process.argv[2] ?? execFileSync("sh", ["-c", "command -v omp"]).toString().trim();
const text = readFileSync(binPath).toString("latin1");

function matchObject(src, start) {
  let depth = 0;
  let quote = null;
  for (let i = start; i < src.length; i++) {
    const c = src[i];
    if (quote) {
      if (c === "\\") { i++; continue; }
      if (c === quote) quote = null;
      continue;
    }
    if (c === '"' || c === "'" || c === "`") { quote = c; continue; }
    if (c === "{" || c === "[" || c === "(") depth++;
    else if (c === "}" || c === "]" || c === ")") {
      depth--;
      if (depth === 0) return src.slice(start, i + 1);
    }
  }
  return null;
}

const undefinedIdentifiers = new Proxy(Object.create(null), {
  has: (_t, key) => key !== "undefined",
  get: () => (...args) => (args.length ? String(args[0]) : ""),
});
const context = vm.createContext(undefinedIdentifiers);

const settings = new Map();
const marker = /\bae\(\{\s*id: "([A-Za-z0-9_.]+)"/g;
let m;
while ((m = marker.exec(text)) !== null) {
  const objStart = text.indexOf("{", m.index);
  const literal = matchObject(text, objStart);
  if (!literal) continue;
  let obj;
  try {
    obj = vm.runInContext(`(${literal})`, context, { timeout: 50 });
  } catch {
    continue;
  }
  if (!obj || typeof obj.id !== "string" || typeof obj.type !== "string") continue;
  let ui;
  try {
    ui = JSON.parse(JSON.stringify(obj.ui ?? {}));
  } catch {
    ui = {};
  }
  const entry = {
    id: obj.id,
    type: obj.type,
    default: obj.default ?? null,
    values: Array.isArray(obj.values) ? obj.values : undefined,
    tab: ui.tab ?? null,
    group: ui.group ?? null,
    label: ui.label ?? null,
    description: ui.description ?? null,
    options: Array.isArray(ui.options)
      ? ui.options.filter((o) => o && typeof o === "object").map((o) => ({
          value: o.value, label: o.label ?? String(o.value), description: o.description ?? null,
        }))
      : undefined,
  };
  for (const k of Object.keys(entry)) if (entry[k] === undefined) delete entry[k];
  const json = JSON.stringify(entry);
  // Duplicate declarations exist in the bundle; keep the richest one.
  const prev = settings.get(obj.id);
  if (!prev || JSON.stringify(prev).length < json.length) settings.set(obj.id, entry);
}

let version = "unknown";
try {
  version = execFileSync(binPath, ["--version"], { timeout: 10000 }).toString().trim();
} catch {}

const out = {
  generatedFrom: version,
  settings: [...settings.values()].sort((a, b) => a.id.localeCompare(b.id)),
};
process.stdout.write(JSON.stringify(out, null, 1) + "\n");
process.stderr.write(`extracted ${out.settings.length} settings from ${version}\n`);
