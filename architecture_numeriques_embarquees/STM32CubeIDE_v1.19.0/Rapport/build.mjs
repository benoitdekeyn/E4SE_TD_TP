// Convertit le rapport Markdown en un unique fichier HTML (CSS, JS, polices, KaTeX et Mermaid inclus).
// Configuration : rapport.config.jsonc
// Usage : node build.mjs            -> écrit <nom du .md>.html
//         node build.mjs --watch    -> sert la page sur http://localhost:4173 avec rechargement automatique
import fs from 'node:fs';
import path from 'node:path';
import http from 'node:http';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import MarkdownIt from 'markdown-it';
import texmath from 'markdown-it-texmath';
import katex from 'katex';
import hljs from 'highlight.js';
import GithubSlugger from 'github-slugger';

const require = createRequire(import.meta.url);
const ROOT = path.dirname(fileURLToPath(import.meta.url));
const cfgArg = process.argv.indexOf('--config');
const CONFIG_FILE = cfgArg > 0 ? path.resolve(process.argv[cfgArg + 1]) : path.join(ROOT, 'rapport.config.jsonc');
const WATCH = process.argv.includes('--watch');
const PORT = Number(process.env.PORT) || 4173;

const DEFAULTS = {
  source: 'Rapport.md',
  mediaDirs: ['media'],
  outputDir: '.',
  embedImages: false,
  title: null,
  language: 'fr',
  theme: 'auto',
  layout: { sidebarWidth: 320, sidebarMinWidth: 200, sidebarMaxWidth: 700, contentWidthPercent: 90, tocLevels: [2, 4] },
  style: {
    fontFamily: "-apple-system, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif",
    codeFontFamily: "ui-monospace, 'Cascadia Code', Consolas, 'SFMono-Regular', monospace",
    fontSize: '16px',
    codeFontSize: '85%',
    highlight: { light: 'github', dark: 'github-dark' },
    mermaidTheme: 'auto',
    mermaid: { layout: 'dagre', direction: null },
    colors: { light: {}, dark: {} },
    customCss: null,
  },
};

const MIME = {
  '.html': 'text/html; charset=utf-8', '.css': 'text/css', '.js': 'text/javascript',
  '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.gif': 'image/gif',
  '.svg': 'image/svg+xml', '.webp': 'image/webp', '.pdf': 'application/pdf', '.txt': 'text/plain; charset=utf-8',
};

class ConfigError extends Error {}

// ---------- configuration ----------
function stripJsonComments(text) {
  let out = '';
  let inStr = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    const n = text[i + 1];
    if (inStr) {
      out += c;
      if (c === '\\') out += text[++i];
      else if (c === '"') inStr = false;
    } else if (c === '"') { inStr = true; out += c; }
    else if (c === '/' && n === '/') { while (i < text.length && text[i] !== '\n') i++; out += '\n'; }
    else if (c === '/' && n === '*') { i += 2; while (i < text.length && !(text[i] === '*' && text[i + 1] === '/')) i++; i++; }
    else out += c;
  }
  return out.replace(/,(\s*[}\]])/g, '$1'); // virgules finales tolérées
}

const isObj = (v) => v && typeof v === 'object' && !Array.isArray(v);
function merge(base, over) {
  const out = { ...base };
  for (const [k, v] of Object.entries(over || {})) out[k] = isObj(v) && isObj(base[k]) ? merge(base[k], v) : v;
  return out;
}

function loadConfig() {
  let user = {};
  if (fs.existsSync(CONFIG_FILE)) {
    try { user = JSON.parse(stripJsonComments(fs.readFileSync(CONFIG_FILE, 'utf8'))); }
    catch (e) { throw new ConfigError(`rapport.config.jsonc invalide : ${e.message}`); }
  }
  const cfg = merge(DEFAULTS, user);
  cfg.sourcePath = path.resolve(ROOT, cfg.source);
  if (!fs.existsSync(cfg.sourcePath)) throw new ConfigError(`Fichier source introuvable : ${cfg.source}`);
  cfg.mediaPaths = [].concat(cfg.mediaDirs || []).map((d) => path.resolve(ROOT, d));
  for (const d of cfg.mediaPaths) if (!fs.existsSync(d)) console.warn(`  ⚠ dossier d'images introuvable : ${path.relative(ROOT, d)}`);
  if (!['auto', 'light', 'dark'].includes(cfg.theme)) throw new ConfigError(`theme doit valoir auto, light ou dark (reçu "${cfg.theme}")`);
  cfg.outputFile = path.resolve(ROOT, cfg.outputDir, `${path.parse(cfg.sourcePath).name}.html`);
  const bad = (m) => { throw new ConfigError(`style.mermaid : ${m}`); };
  cfg.style.mermaid = normalizeMermaidOptions(cfg.style.mermaid, bad);
  return cfg;
}

// ---------- ressources ----------
const modDir = (name) => path.dirname(require.resolve(`${name}/package.json`));
const readAsset = (...p) => fs.readFileSync(path.join(...p), 'utf8');
const esc = (s) => MarkdownIt().utils.escapeHtml(s);
const safeScript = (js) => js.replace(/<\/script/gi, '<\\/script');
const dataUri = (file) => `data:${MIME[path.extname(file).toLowerCase()] || 'application/octet-stream'};base64,${fs.readFileSync(file).toString('base64')}`;

// Préfixe chaque règle d'une feuille highlight.js pour qu'elle ne s'applique qu'à un thème de la page.
function scopedHljsCss(name, scope) {
  const file = path.join(modDir('highlight.js'), 'styles', `${name}.min.css`);
  if (!fs.existsSync(file)) {
    const all = fs.readdirSync(path.dirname(file)).filter((f) => f.endsWith('.min.css')).map((f) => f.replace('.min.css', ''));
    throw new ConfigError(`Thème de code "${name}" inconnu. Disponibles : ${all.join(', ')}`);
  }
  return fs.readFileSync(file, 'utf8')
    .replace(/\/\*[\s\S]*?\*\//g, '')
    .replace(/([^{}]+)\{([^{}]*)\}/g, (_, sel, body) => {
      const selectors = sel.split(',').map((s) => s.trim());
      // Le cadre des blocs (fond, marges) est géré par style.css : on ne garde que la couleur du texte.
      if (selectors.every((s) => /(^|\s)(pre\s+)?(code)?\.hljs$/.test(s))) {
        body = body.split(';').filter((d) => /^\s*color\s*:/.test(d)).join(';');
      }
      return `${selectors.map((s) => `${scope} ${s}`).join(',')}{${body}}`;
    });
}

function katexCss() {
  const dir = path.join(modDir('katex'), 'dist');
  return readAsset(dir, 'katex.min.css').replace(
    /url\(fonts\/([^)]+?)\.woff2\) format\("woff2"\),url\([^)]+\) format\("woff"\),url\([^)]+\) format\("truetype"\)/g,
    (_, name) => `url(${dataUri(path.join(dir, 'fonts', `${name}.woff2`))}) format("woff2")`,
  );
}

const COLOR_VARS = { bg: '--bg', fg: '--fg', muted: '--muted', border: '--border', sidebarBg: '--sidebar-bg', accent: '--accent', codeBg: '--code-bg', quote: '--quote' };
function colorRules(colors, selector) {
  const decl = Object.entries(colors || {}).map(([k, v]) => {
    if (!COLOR_VARS[k]) throw new ConfigError(`style.colors : clé inconnue "${k}" (valides : ${Object.keys(COLOR_VARS).join(', ')})`);
    return `${COLOR_VARS[k]}:${v}`;
  });
  return decl.length ? `${selector}{${decl.join(';')}}\n` : '';
}

// ---------- Mermaid : mise en page et direction ----------
const MERMAID_LAYOUTS = { dagre: 'dagre', hierarchical: 'dagre', hierarchique: 'dagre', hiérarchique: 'dagre', elk: 'elk', adaptive: 'elk', adaptatif: 'elk' };
const MERMAID_DIRECTIONS = { tb: 'TB', td: 'TB', bt: 'BT', lr: 'LR', rl: 'RL' };

function normalizeMermaidOptions(raw, onError) {
  const out = {};
  if (raw.layout != null) {
    out.layout = MERMAID_LAYOUTS[String(raw.layout).toLowerCase()];
    if (!out.layout) { onError(`layout "${raw.layout}" inconnu (dagre/hierarchical ou elk/adaptive)`); delete out.layout; }
  }
  if (raw.direction != null) {
    out.direction = MERMAID_DIRECTIONS[String(raw.direction).toLowerCase()];
    if (!out.direction) { onError(`direction "${raw.direction}" inconnue (TB, BT, LR, RL)`); delete out.direction; }
  }
  return out;
}

function parseMermaidOptions(words, defaults, onError) {
  const raw = {};
  for (const w of words) {
    const m = /^(layout|direction)=(.+)$/i.exec(w);
    if (m) raw[m[1].toLowerCase()] = m[2];
    else onError(`option "${w}" ignorée (attendu layout=… ou direction=…)`);
  }
  return { ...normalizeMermaidOptions(defaults, () => {}), ...normalizeMermaidOptions(raw, onError) };
}

function applyMermaidOptions(src, opts, onWarn) {
  const lines = src.replace(/\r\n/g, '\n').split('\n');
  let start = 0;
  if (lines[0].trim() === '---') { // frontmatter YAML éventuel
    const end = lines.indexOf('---', 1);
    if (end > 0) start = end + 1;
  }
  if (opts.direction) {
    const h = lines.findIndex((l, i) => i >= start && l.trim() && !/^\s*%%/.test(l));
    const type = h >= 0 ? lines[h].trim() : '';
    if (/^(flowchart|graph)\b/.test(type)) {
      lines[h] = lines[h].replace(/^(\s*(?:flowchart|graph))(?:\s+(?:TB|TD|BT|LR|RL))?/, `$1 ${opts.direction === 'TB' ? 'TD' : opts.direction}`);
    } else if (/^stateDiagram(-v2)?\b/.test(type)) {
      const indent = (lines[h + 1] || '').match(/^\s*/)[0] || '    ';
      // Une ligne "direction" au premier niveau (hors état composite) est remplacée, sinon ajoutée.
      let depth = 0, top = -1;
      for (let i = h + 1; i < lines.length; i++) {
        if (depth === 0 && /^\s*direction\s+\w+\s*$/.test(lines[i])) { top = i; break; }
        if (/\{\s*$/.test(lines[i])) depth++;
        if (/^\s*\}/.test(lines[i])) depth--;
      }
      if (top >= 0) lines[top] = `${indent}direction ${opts.direction}`;
      else lines.splice(h + 1, 0, `${indent}direction ${opts.direction}`);
    } else {
      onWarn(`direction ignorée (types gérés : flowchart/graph, stateDiagram) ; type : "${type.split(/\s+/)[0]}"`);
    }
  }
  if (opts.layout === 'elk') lines.splice(start, 0, '%%{init: {"layout": "elk"}}%%');
  return lines.join('\n');
}

// Les modules ESM du plugin ELK sont regroupés en un script autonome (chargé seulement si un diagramme l'utilise).
async function elkBundle() {
  const { build } = await import('esbuild');
  const entry = path.join(modDir('@mermaid-js/layout-elk'), 'dist', 'mermaid-layout-elk.core.mjs');
  const res = await build({ entryPoints: [entry], bundle: true, write: false, format: 'iife', globalName: '__elk', minify: true, platform: 'browser', logLevel: 'silent' });
  return res.outputFiles[0].text;
}

// ---------- rendu ----------
async function render(cfg) {
  const warnings = [];
  const source = fs.readFileSync(cfg.sourcePath, 'utf8');

  const md = new MarkdownIt({ html: true, linkify: true, typographer: false });
  md.use(texmath, { engine: katex, delimiters: 'dollars', katexOptions: { throwOnError: false, strict: 'ignore' } });

  let hasMermaid = false;
  let usesElk = false;
  let mermaidCount = 0;
  md.renderer.rules.fence = (tokens, idx) => {
    const token = tokens[idx];
    const lang = token.info.trim().split(/\s+/)[0].toLowerCase();
    if (lang === 'mermaid') {
      hasMermaid = true;
      const where = `diagramme Mermaid n°${++mermaidCount}`;
      const opts = parseMermaidOptions(token.info.trim().split(/\s+/).slice(1), cfg.style.mermaid, (m) => warnings.push(`${where} : ${m}`));
      if (opts.layout === 'elk') usesElk = true;
      return `<pre class="mermaid">${esc(applyMermaidOptions(token.content, opts, (m) => warnings.push(`${where} : ${m}`)))}</pre>\n`;
    }
    let body;
    if (lang && hljs.getLanguage(lang)) body = hljs.highlight(token.content, { language: lang, ignoreIllegals: true }).value;
    else {
      if (lang) warnings.push(`langage de code inconnu : "${lang}"`);
      body = esc(token.content);
    }
    const label = lang ? `<span class="code-lang">${esc(lang)}</span>` : '';
    return `<div class="code-block">${label}<button class="copy-btn" type="button" aria-label="Copier le code">Copier</button><pre><code class="hljs${lang ? ` language-${esc(lang)}` : ''}">${body}</code></pre></div>\n`;
  };

  // Identifiants de titres identiques à ceux de GitHub, pour que les liens [..](#...) marchent pareil.
  const slugger = new GithubSlugger();
  const headings = [];
  md.core.ruler.push('heading_ids', (state) => {
    const toks = state.tokens;
    for (let i = 0; i < toks.length; i++) {
      if (toks[i].type !== 'heading_open') continue;
      const inline = toks[i + 1];
      const text = (inline.children || [])
        .filter((t) => t.type === 'text' || t.type === 'code_inline')
        .map((t) => t.content).join('').trim();
      const id = slugger.slug(text);
      toks[i].attrSet('id', id);
      headings.push({ level: Number(toks[i].tag.slice(1)), text, id });
      const link = new state.Token('html_inline', '', 0);
      link.content = `<a class="anchor" href="#${id}" aria-label="Lien vers cette section">#</a>`;
      inline.children.push(link);
    }
  });

  const sourceName = path.basename(cfg.sourcePath).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const selfRef = new RegExp(`^(\\./)?${sourceName}(?=#|$)`, 'i');
  const internalLinks = [];
  md.core.ruler.push('links', (state) => {
    const walk = (tokens) => {
      for (const t of tokens) {
        if (t.type === 'link_open') {
          let href = (t.attrGet('href') || '').replace(selfRef, '');
          t.attrSet('href', href || '#');
          if (href.startsWith('#')) internalLinks.push(href);
          else if (/^https?:\/\//i.test(href)) {
            t.attrSet('target', '_blank');
            t.attrSet('rel', 'noopener noreferrer');
          }
        }
        if (t.children) walk(t.children);
      }
    };
    walk(state.tokens);
  });

  let body = md.render(source);
  const hasMath = /class="katex/.test(body);

  // Vérifications : ancres internes et fichiers locaux (relatifs au .md)
  const baseDir = path.dirname(cfg.sourcePath);
  const ids = new Set(headings.map((h) => h.id));
  for (const href of new Set(internalLinks)) {
    let id = href.slice(1);
    try { id = decodeURIComponent(id); } catch { /* fragment déjà brut */ }
    if (id && !ids.has(id)) warnings.push(`lien interne cassé : ${href}`);
  }
  const localFile = (ref) => {
    if (/^([a-z][a-z0-9+.-]*:|\/\/|#)/i.test(ref)) return null;
    let f = ref.split(/[?#]/)[0];
    try { f = decodeURIComponent(f); } catch { /* ignore */ }
    return f ? path.resolve(baseDir, f) : null;
  };
  for (const m of body.matchAll(/\b(?:src|href)="([^"]+)"/g)) {
    const file = localFile(m[1]);
    if (file && !fs.existsSync(file)) warnings.push(`fichier introuvable : ${m[1]}`);
  }
  if (cfg.embedImages) {
    let total = 0;
    body = body.replace(/(<img\b[^>]*?\bsrc=")([^"]+)(")/g, (all, a, ref, z) => {
      const file = localFile(ref);
      if (!file || !fs.existsSync(file)) return all;
      total += fs.statSync(file).size;
      return `${a}${dataUri(file)}${z}`;
    });
    console.log(`  images incorporées : ${(total / 1048576).toFixed(1)} Mo`);
  }

  const title = cfg.title || (headings.find((h) => h.level === 1) || { text: 'Rapport' }).text;
  const [minLvl, maxLvl] = cfg.layout.tocLevels;
  const sidebar = renderSidebar(headings.filter((h) => h.level >= minLvl && h.level <= maxLvl), minLvl);

  const { style, layout } = cfg;
  const clientCfg = {
    key: `rapport:${path.parse(cfg.sourcePath).name}`,
    sidebarWidth: layout.sidebarWidth, sidebarMin: layout.sidebarMinWidth, sidebarMax: layout.sidebarMaxWidth,
    mermaidTheme: style.mermaidTheme,
  };
  const json = (o) => JSON.stringify(o).replace(/</g, '\\u003c');

  const css = [
    readAsset(ROOT, 'template', 'style.css'),
    hasMath ? katexCss() : '',
    scopedHljsCss(style.highlight.light, ':root[data-theme="light"]'),
    scopedHljsCss(style.highlight.dark, ':root[data-theme="dark"]'),
    `:root{--sidebar-w:${layout.sidebarWidth}px;--content-pct:${layout.contentWidthPercent};--font:${style.fontFamily};--code-font:${style.codeFontFamily};--font-size:${style.fontSize};--code-font-size:${style.codeFontSize}}\n`,
    colorRules(style.colors.light, ':root'),
    colorRules(style.colors.dark, ':root[data-theme="dark"]'),
    style.customCss ? readAsset(path.resolve(ROOT, style.customCss)) : '',
  ].join('\n');

  const elkCode = usesElk ? await elkBundle() : '';
  const html = `<!DOCTYPE html>
<html lang="${esc(cfg.language)}" data-theme="light" data-mode="${cfg.theme}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(title)}</title>
<script>(function(){var c=${json(clientCfg)},r=document.documentElement,m=${json(cfg.theme)},w;try{m=localStorage.getItem(c.key+':theme')||m;w=localStorage.getItem(c.key+':sidebar')}catch(e){}
r.dataset.mode=m;r.dataset.theme=(m==='dark'||(m==='auto'&&matchMedia('(prefers-color-scheme: dark)').matches))?'dark':'light';
if(w)r.style.setProperty('--sidebar-w',Math.max(c.sidebarMin,Math.min(c.sidebarMax,+w))+'px')})()</script>
<style>
${css}
</style>
</head>
<body>
<button id="sidebar-toggle" type="button" aria-label="Afficher / masquer le sommaire">&#9776;</button>
<aside id="sidebar">
  <div class="sidebar-head"><div class="sidebar-title">${esc(title)}</div><button id="theme-toggle" type="button" title="Changer le thème"></button></div>
  <input id="toc-filter" type="search" placeholder="Filtrer les titres…" autocomplete="off">
  <nav id="toc">${sidebar}</nav>
</aside>
<div id="resizer" role="separator" aria-orientation="vertical" title="Glisser pour redimensionner (double-clic : largeur initiale)"></div>
<main id="content"><article class="markdown-body">
${body}
</article></main>
<script>window.__REPORT__=${json(clientCfg)}</script>
${hasMermaid ? `<script>${safeScript(readAsset(modDir('mermaid'), 'dist', 'mermaid.min.js'))}</script>` : ''}
${usesElk ? `<script>${safeScript(elkCode)}</script>` : ''}
<script>${safeScript(readAsset(ROOT, 'template', 'app.js'))}</script>
${WATCH ? '<script>let v;setInterval(async()=>{try{const n=await (await fetch("/__build",{cache:"no-store"})).text();if(v&&v!==n)location.reload();v=n}catch{}},800)</script>' : ''}
</body>
</html>
`;
  return { html, headings: headings.length, warnings: [...new Set(warnings)] };
}

function renderSidebar(items, minLvl) {
  let out = '';
  let i = 0;
  const link = (h) => `<a class="toc-l${h.level}" href="#${h.id}" data-id="${h.id}">${esc(h.text)}</a>`;
  while (i < items.length) {
    const h = items[i++];
    let sub = '';
    while (i < items.length && items[i].level > minLvl) sub += link(items[i++]);
    out += `<div class="toc-group">${link(h)}${sub ? `<div class="toc-sub">${sub}</div>` : ''}</div>`;
  }
  return out;
}

async function generate() {
  const started = Date.now();
  const cfg = loadConfig();
  const result = await render(cfg);
  result.warnings.forEach((w) => console.warn('  ⚠', w));
  console.log(`✔ ${result.headings} titres, ${result.warnings.length} avertissement(s), ${Date.now() - started} ms`);
  return { cfg, ...result };
}

// ---------- exécution ----------
if (!WATCH) {
  try {
    const { cfg, html } = await generate();
    fs.mkdirSync(path.dirname(cfg.outputFile), { recursive: true });
    fs.writeFileSync(cfg.outputFile, html);
    console.log(`→ ${path.relative(ROOT, cfg.outputFile)} (${(Buffer.byteLength(html) / 1048576).toFixed(1)} Mo)`);
  } catch (e) {
    if (!(e instanceof ConfigError)) throw e;
    console.error('✖', e.message);
    process.exit(1);
  }
} else {
  let state = { html: '<p>Génération en cours…</p>', cfg: null };
  let buildId = String(Date.now());
  const rebuild = async () => {
    try { state = await generate(); }
    catch (e) {
      console.error('✖', e.message);
      state = { ...state, html: `<pre style="color:#c00;padding:2em">${esc(e.message)}</pre>` };
    }
    buildId = String(Date.now());
  };
  await rebuild();

  http.createServer((req, res) => {
    let url;
    try { url = decodeURIComponent(req.url.split('?')[0]); } catch { res.statusCode = 400; return res.end(); }
    if (url === '/__build') { res.setHeader('Content-Type', 'text/plain'); return res.end(buildId); }
    if (url === '/' || url === '/index.html') {
      res.setHeader('Content-Type', 'text/html; charset=utf-8');
      return res.end(state.html);
    }
    // Les fichiers ne sont servis que depuis le dossier du .md (images, pdf…)
    const baseDir = state.cfg ? path.dirname(state.cfg.sourcePath) : ROOT;
    const file = path.resolve(baseDir, '.' + url);
    const okDir = [...(state.cfg ? state.cfg.mediaPaths : []), baseDir];
    const inside = okDir.some((d) => file === d || file.startsWith(d + path.sep));
    const isPlain = fs.existsSync(file) && fs.statSync(file).isFile() && MIME[path.extname(file).toLowerCase()];
    if (!inside || !isPlain || /node_modules|(^|[\\/])\./.test(path.relative(baseDir, file))) { res.statusCode = 404; return res.end('404'); }
    res.setHeader('Content-Type', MIME[path.extname(file).toLowerCase()] || 'application/octet-stream');
    fs.createReadStream(file).pipe(res);
  }).listen(PORT, '127.0.0.1', () => console.log(`→ http://localhost:${PORT}  (Ctrl+C pour arrêter)`));

  let timer;
  const trigger = () => { clearTimeout(timer); timer = setTimeout(rebuild, 250); };
  const watchDir = (d, filter) => {
    if (fs.existsSync(d)) fs.watch(d, { recursive: true }, (_, f) => { if (!filter || (f && filter(f))) trigger(); });
  };
  const srcDir = path.dirname(state.cfg ? state.cfg.sourcePath : path.join(ROOT, DEFAULTS.source));
  const srcName = path.basename(state.cfg ? state.cfg.sourcePath : DEFAULTS.source).toLowerCase();
  // On surveille les dossiers : certains éditeurs remplacent le fichier à la sauvegarde.
  fs.watch(ROOT, (_, f) => { if (f && ['rapport.config.jsonc', srcName].includes(f.toLowerCase())) trigger(); });
  if (srcDir !== ROOT) fs.watch(srcDir, (_, f) => { if (f && f.toLowerCase() === srcName) trigger(); });
  watchDir(path.join(ROOT, 'template'));
  (state.cfg ? state.cfg.mediaPaths : []).forEach((d) => watchDir(d));
}
