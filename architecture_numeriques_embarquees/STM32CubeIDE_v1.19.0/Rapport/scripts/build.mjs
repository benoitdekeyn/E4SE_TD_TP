// Convertit les documents Markdown de doc/ en pages HTML (web/) ou en PDF (pdf/).
// Chaque doc/<nom>.md est traité si doc/<nom>.jsonc existe et est valide.
// Usage : node scripts/build.mjs [nom…]           -> web/<nom>/<htmlName>.html + médias utilisés
//         node scripts/build.mjs --pdf [nom…]     -> pdf/<nom>.pdf
//         node scripts/build.mjs --watch [nom…]   -> sert les pages sur http://localhost:4173 avec rechargement automatique
// Option : --no-open (n'ouvre ni l'explorateur, ni le PDF, ni le navigateur)
import fs from 'node:fs';
import path from 'node:path';
import http from 'node:http';
import { spawn } from 'node:child_process';
import { createRequire } from 'node:module';
import MarkdownIt from 'markdown-it';
import texmath from 'markdown-it-texmath';
import katex from 'katex';
import hljs from 'highlight.js';
import GithubSlugger from 'github-slugger';
import { exportPdf, pdfGeometry, validatePdfConfig } from './pdf.mjs';
import { isObj, merge } from './jsonc.mjs';
import { DOC_DIR, TEMPLATE_DIR, WEB_DIR, PDF_DIR, CONFIG_SUFFIX, ConfigError, discoverDocs, rel } from './workspace.mjs';

const require = createRequire(import.meta.url);
const WATCH = process.argv.includes('--watch');
const PDF = process.argv.includes('--pdf');
const NAMES = process.argv.slice(2).filter((a) => !a.startsWith('--'));
const PORT = Number(process.env.PORT) || 4173;

const DEFAULTS = {
  htmlName: 'index',
  embedImages: false,
  icon: null,
  title: null,
  language: 'fr',
  theme: 'auto',
  pdf: {
    name: null,
    theme: 'light',
    format: 'A4',
    landscape: false,
    zoom: 100,
    margins: { top: '20mm', right: '18mm', bottom: '20mm', left: '18mm' },
    pageBreakBefore: [],
    hideToc: false,
    keepSectionsTogether: true,
    keepBlocksTogether: true,
    browser: null,
    header: null,
    footer: { center: '{page} / {pages}' },
  },
  layout: { sidebarWidth: 320, sidebarMinWidth: 200, sidebarMaxWidth: 700, contentWidthPercent: 90 },
  style: {
    fontFamily: "-apple-system, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif",
    codeFontFamily: "ui-monospace, 'Cascadia Code', Consolas, 'SFMono-Regular', monospace",
    fontSize: '16px',
    codeFontSize: '85%',
    highlight: { light: 'github', dark: 'github-dark' },
    mermaidTheme: 'auto',
    mermaid: { layout: 'dagre', direction: null },
    // Unité : em (multiple de la taille de base ; 1 = taille du texte).
    headings: {
      1: { size: 2, indent: 0, color: null }, 2: { size: 1.5, indent: 0, color: null }, 3: { size: 1.25, indent: 0, color: null },
      4: { size: 1, indent: 0, color: null }, 5: { size: 0.875, indent: 0, color: null }, 6: { size: 0.85, indent: 0, color: null },
    },
    toc: {
      1: { size: 1.1, indent: 0, color: null }, 2: { size: 1, indent: 0, color: null }, 3: { size: 1, indent: 1.5, color: null },
      4: { size: 0.93, indent: 2.5, color: null }, 5: { size: 0.93, indent: 3.5, color: null }, 6: { size: 0.93, indent: 4.5, color: null },
    },
    colors: { light: {}, dark: {} },
    customCss: null,
  },
};

const MIME = {
  '.html': 'text/html; charset=utf-8', '.css': 'text/css', '.js': 'text/javascript',
  '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.gif': 'image/gif',
  '.svg': 'image/svg+xml', '.webp': 'image/webp', '.pdf': 'application/pdf', '.txt': 'text/plain; charset=utf-8',
};

const REMOVED_KEYS = {
  source: 'le document est désormais déduit du nom : doc/<nom>.jsonc ↔ doc/<nom>.md',
  mediaDirs: 'les médias sont lus à côté du .md (doc/) et seuls ceux utilisés sont copiés dans web/<nom>/',
  outputDir: 'les sorties vont dans web/<nom>/ (HTML) et pdf/ (PDF)',
};

// Nom de fichier sans extension (htmlName, pdf.name) : pas de dossier ni de caractère interdit sous Windows.
function fileStem(value, label, ext) {
  const v = String(value).replace(new RegExp(`\\${ext}$`, 'i'), '');
  if (!v || /[\\/:*?"<>|]/.test(v) || /^\.|\.$/.test(v)) throw new ConfigError(`${label} : nom de fichier invalide "${value}" (sans dossier ni \\ / : * ? " < > |)`);
  return v;
}

// ---------- configuration ----------
function loadConfig(doc) {
  const user = doc.user;
  for (const [k, hint] of Object.entries(REMOVED_KEYS)) if (k in user) throw new ConfigError(`"${k}" n'existe plus : ${hint}`);
  if (isObj(user.pdf) && 'output' in user.pdf) throw new ConfigError('"pdf.output" n\'existe plus : utilisez "pdf.name" (nom du fichier, le PDF est écrit dans pdf/)');
  const cfg = merge(DEFAULTS, user);
  cfg.docName = doc.name;
  cfg.sourcePath = doc.mdPath;
  cfg.configPath = doc.configPath;
  if (!['auto', 'light', 'dark'].includes(cfg.theme)) throw new ConfigError(`theme doit valoir auto, light ou dark (reçu "${cfg.theme}")`);
  cfg.outputDir = path.join(WEB_DIR, doc.name);
  cfg.outputFile = path.join(cfg.outputDir, `${fileStem(cfg.htmlName, 'htmlName', '.html')}.html`);
  try { validatePdfConfig(cfg.pdf); } catch (e) { throw new ConfigError(e.message); }
  cfg.pdfFile = path.join(PDF_DIR, `${cfg.pdf.name == null ? doc.name : fileStem(cfg.pdf.name, 'pdf.name', '.pdf')}.pdf`);
  const bad = (m) => { throw new ConfigError(`style.mermaid : ${m}`); };
  cfg.style.mermaid = normalizeMermaidOptions(cfg.style.mermaid, bad);
  for (const part of ['headings', 'toc']) {
    for (const [lvl, o] of Object.entries(cfg.style[part])) {
      if (!/^[1-6]$/.test(lvl)) throw new ConfigError(`style.${part} : niveau "${lvl}" invalide (1 à 6)`);
      for (const k of ['size', 'indent']) {
        if (!(typeof o[k] === 'number' && o[k] >= 0 && o[k] <= 20)) throw new ConfigError(`style.${part}.${lvl}.${k} doit être un nombre en em (0 à 20)`);
      }
      const c = o.color;
      const okColor = (v) => v == null || typeof v === 'string';
      if (!(okColor(c) || (isObj(c) && okColor(c.light) && okColor(c.dark)))) throw new ConfigError(`style.${part}.${lvl}.color : chaîne CSS ou { light, dark }`);
    }
  }
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

// Règles CSS par niveau de titre. page : titres du document ; nav : liens du sommaire (base = --toc-size).
function levelRules(conf, kind) {
  const colorOf = (c) => (isObj(c) ? c : { light: c, dark: c });
  let css = '';
  for (const [lvl, o] of Object.entries(conf)) {
    const { light, dark } = colorOf(o.color);
    if (kind === 'page') {
      const sel = `.markdown-body h${lvl}`;
      css += `${sel}{font-size:${o.size}em;margin-left:${o.indent}em}\n`;
      if (light) css += `:root ${sel}{color:${light}}\n`;
      if (dark) css += `:root[data-theme="dark"] ${sel}{color:${dark}}\n`;
    } else {
      const link = `#toc a.toc-l${lvl}`;
      css += `${link}{font-size:calc(var(--toc-size)*${o.size})}\n#toc .row-l${lvl}{padding-left:calc(var(--toc-size)*${o.indent})}\n`;
      if (light) css += `:root ${link}:not(.active){color:${light}}\n`;
      if (dark) css += `:root[data-theme="dark"] ${link}:not(.active){color:${dark}}\n`;
    }
  }
  return css;
}

// template/mermaid.css est injecté dans chaque diagramme (option Mermaid « themeCSS »).
// Les marqueurs /* @light */ et /* @dark */ réservent les règles qui suivent à un thème de la page.
function mermaidCssSections() {
  const file = path.join(TEMPLATE_DIR, 'mermaid.css');
  const out = { common: '', light: '', dark: '' };
  if (!fs.existsSync(file)) return out;
  let cur = 'common';
  for (const part of fs.readFileSync(file, 'utf8').split(/(\/\*\s*@(?:light|dark|all)\s*\*\/)/)) {
    const m = /^\/\*\s*@(light|dark|all)\s*\*\/$/.exec(part);
    if (m) cur = m[1] === 'all' ? 'common' : m[1];
    else out[cur] += part;
  }
  return out;
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
async function render(cfg, { pdf = false } = {}) {
  const warnings = [];
  const source = fs.readFileSync(cfg.sourcePath, 'utf8');

  const md = new MarkdownIt({ html: true, linkify: true, typographer: false });
  md.use(texmath, { engine: katex, delimiters: 'dollars', katexOptions: { throwOnError: false, strict: 'ignore' } });
  // texmath rend « texte $$…$$ texte » avec <section>, élément de bloc qui ferme le <p> en cours dans le HTML :
  // le texte suivant se retrouvait alors hors de tout paragraphe. On garde un <span> (affiché en bloc par le CSS).
  for (const name of ['math_inline_double']) {
    const rule = md.renderer.rules[name];
    if (rule) md.renderer.rules[name] = (...a) => rule(...a).replace(/^<section><eqn>([\s\S]*)<\/eqn><\/section>$/, '<span class="eqn-display">$1</span>');
  }

  // Commandes d'impression : "\newpage" (saut de page) et "\nohf" (page sans en-tête/pied), seules sur leur ligne,
  // ou leur équivalent en commentaire HTML (<!-- newpage -->, <!-- nohf -->) invisible sur GitHub.
  md.core.ruler.push('pagebreak', (state) => {
    const t = state.tokens;
    const markers = (cmds) => cmds.map((c) => {
      const k = new state.Token('html_block', '', 0);
      k.content = `<div class="${c === 'nohf' ? 'nohf' : 'pagebreak'}"></div>\n`;
      return k;
    });
    const kind = (s) => (/^(nohf)$/i.test(s) ? 'nohf' : 'pagebreak');
    for (let i = 0; i < t.length; i++) {
      if (t[i].type === 'html_block') {
        const found = t[i].content.match(/<!--\s*(?:newpage|pagebreak|nohf)\s*-->/gi);
        if (found && t[i].content.replace(/<!--\s*(?:newpage|pagebreak|nohf)\s*-->/gi, '').trim() === '') {
          const ms = markers(found.map((f) => kind(f.replace(/<!--\s*|\s*-->/g, ''))));
          t.splice(i, 1, ...ms);
          i += ms.length - 1;
        }
      } else if (t[i].type === 'paragraph_open' && t[i + 1]?.type === 'inline' && t[i + 2]?.type === 'paragraph_close') {
        const lines = t[i + 1].content.split('\n').map((l) => l.trim());
        if (lines.every((l) => /^\\(newpage|pagebreak|clearpage|nohf)$/i.test(l))) {
          const ms = markers(lines.map((l) => kind(l.slice(1))));
          t.splice(i, 3, ...ms);
          i += ms.length - 1;
        }
      }
    }
  });

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
      return `<div class="mermaid-box"><pre class="mermaid">${esc(applyMermaidOptions(token.content, opts, (m) => warnings.push(`${where} : ${m}`)))}</pre></div>\n`;
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

  // Fichiers locaux restant référencés (images, pièces jointes) : ils doivent exister à côté du HTML dans web/.
  const assets = new Set();
  for (const m of body.matchAll(/\b(?:src|href)="([^"]+)"/g)) {
    const file = localFile(m[1]);
    if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile() || /\.md$/i.test(file)) continue;
    const r = path.relative(baseDir, file);
    if (r.startsWith('..') || path.isAbsolute(r)) warnings.push(`fichier situé hors de doc/, il ne sera pas copié dans web/ : ${m[1]}`);
    else assets.add(r.split(path.sep).join('/'));
  }

  // Icône (favicon + logo du sommaire) : fichier de doc/ (ex. media/logo.png), copié dans web/ comme les autres médias.
  let iconSrc = null;
  if (cfg.icon && !pdf) {
    const file = localFile(String(cfg.icon));
    const r = file && path.relative(baseDir, file);
    if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) warnings.push(`icon : fichier introuvable : ${cfg.icon}`);
    else if (r.startsWith('..') || path.isAbsolute(r)) warnings.push(`icon : le fichier doit se trouver dans doc/ : ${cfg.icon}`);
    else if (cfg.embedImages) iconSrc = dataUri(file);
    else { iconSrc = encodeURI(r.split(path.sep).join('/')); assets.add(r.split(path.sep).join('/')); }
  }

  const title = cfg.title || path.parse(cfg.sourcePath).name;
  const sidebar = pdf ? '' : renderSidebar(headings);

  const { style, layout } = cfg;
  const clientCfg = {
    key: `rapport:${path.parse(cfg.sourcePath).name}`,
    sidebarWidth: layout.sidebarWidth, sidebarMin: layout.sidebarMinWidth, sidebarMax: layout.sidebarMaxWidth,
    mermaidTheme: style.mermaidTheme,
    mermaidCss: mermaidCssSections(),
  };
  const json = (o) => JSON.stringify(o).replace(/</g, '\\u003c');

  const css = [
    readAsset(TEMPLATE_DIR, 'style.css'),
    readAsset(TEMPLATE_DIR, 'print.css'),
    hasMath ? katexCss() : '',
    scopedHljsCss(style.highlight.light, ':root[data-theme="light"]'),
    scopedHljsCss(style.highlight.dark, ':root[data-theme="dark"]'),
    levelRules(style.headings, 'page'),
    levelRules(style.toc, 'nav'),
    `:root{--sidebar-w:${layout.sidebarWidth}px;--content-pct:${layout.contentWidthPercent};--font:${style.fontFamily};--code-font:${style.codeFontFamily};--font-size:${style.fontSize};--code-font-size:${style.codeFontSize}${pdf ? `;--pdf-fig-h:${pdfGeometry(cfg.pdf).figHeight}px` : ''}}\n`,
    colorRules(style.colors.light, ':root'),
    colorRules(style.colors.dark, ':root[data-theme="dark"]'),
    style.customCss ? readAsset(path.resolve(path.dirname(cfg.configPath), style.customCss)) : '',
  ].join('\n');

  const elkCode = usesElk ? await elkBundle() : '';
  const mermaidScripts = hasMermaid ? `<script>${safeScript(readAsset(modDir('mermaid'), 'dist', 'mermaid.min.js'))}</script>` : '';
  const elkScript = usesElk ? `<script>${safeScript(elkCode)}</script>` : '';

  if (pdf) {
    const html = `<!DOCTYPE html>
<html lang="${esc(cfg.language)}" class="pdf" data-theme="${cfg.pdf.theme}" data-mode="${cfg.pdf.theme}">
<head>
<meta charset="utf-8">
<title>${esc(title)}</title>
<style>
${css}
</style>
</head>
<body>
<main id="content"><article class="markdown-body">
${body}
</article></main>
<script>window.__REPORT__=${json(clientCfg)};window.__PDF__=${json({ breakLevels: cfg.pdf.pageBreakBefore, hideToc: cfg.pdf.hideToc, keepSections: cfg.pdf.keepSectionsTogether, keepBlocks: cfg.pdf.keepBlocksTogether })}</script>
${mermaidScripts}
${elkScript}
<script>${safeScript(readAsset(TEMPLATE_DIR, 'print.js'))}</script>
</body>
</html>
`;
    return { html, headings: headings.length, warnings: [...new Set(warnings)] };
  }

  const html = `<!DOCTYPE html>
<html lang="${esc(cfg.language)}" data-theme="light" data-mode="${cfg.theme}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(title)}</title>
${iconSrc ? `<link rel="icon" href="${esc(iconSrc)}">\n` : ''}<script>(function(){var c=${json(clientCfg)},r=document.documentElement,m=${json(cfg.theme)},w;try{m=localStorage.getItem(c.key+':theme')||m;w=localStorage.getItem(c.key+':sidebar')}catch(e){}
r.dataset.mode=m;r.dataset.theme=(m==='dark'||(m==='auto'&&matchMedia('(prefers-color-scheme: dark)').matches))?'dark':'light';
if(w)r.style.setProperty('--sidebar-w',Math.max(c.sidebarMin,Math.min(c.sidebarMax,+w))+'px')})()</script>
<style>
${css}
</style>
</head>
<body>
<button id="sidebar-toggle" type="button" aria-label="Afficher / masquer le sommaire">&#9776;</button>
<aside id="sidebar">
  <div class="sidebar-head"><div class="sidebar-title">${iconSrc ? `<img class="sidebar-logo" src="${esc(iconSrc)}" alt="">` : ''}<span>${esc(title)}</span></div><button id="theme-toggle" type="button" title="Changer le thème"></button></div>
  <input id="toc-filter" type="search" placeholder="Filtrer les titres…" autocomplete="off">
  <nav id="toc">${sidebar}</nav>
</aside>
<div id="resizer" role="separator" aria-orientation="vertical" title="Glisser pour redimensionner (double-clic : largeur initiale)"></div>
<main id="content"><article class="markdown-body">
${body}
</article></main>
<script>window.__REPORT__=${json(clientCfg)}</script>
${hasMermaid ? `${mermaidScripts}\n<script>${safeScript(readAsset(modDir('svg-pan-zoom'), 'dist', 'svg-pan-zoom.min.js'))}</script>` : ''}
${elkScript}
<script>${safeScript(readAsset(TEMPLATE_DIR, 'app.js'))}</script>
${WATCH ? '<script>let v;setInterval(async()=>{try{const n=await (await fetch("/__build",{cache:"no-store"})).text();if(v&&v!==n)location.reload();v=n}catch{}},800)</script>' : ''}
</body>
</html>
`;
  return { html, headings: headings.length, warnings: [...new Set(warnings)], assets: [...assets] };
}

function renderSidebar(items) {
  const roots = [];
  const stack = [];
  for (const h of items) {
    const node = { ...h, kids: [] };
    while (stack.length && stack[stack.length - 1].level >= h.level) stack.pop();
    (stack.length ? stack[stack.length - 1].kids : roots).push(node);
    stack.push(node);
  }
  const caret = '<button class="toc-caret" type="button" tabindex="-1" aria-label="Déplier / replier"></button>';
  const html = (n, root) => `<div class="toc-node${root ? ' root' : ''}${n.kids.length ? ' has-kids' : ''}"><div class="toc-row row-l${n.level}">${n.kids.length ? caret : '<span class="toc-caret"></span>'}<a class="toc-l${n.level}" href="#${n.id}" data-id="${n.id}">${esc(n.text)}</a></div>${n.kids.length ? `<div class="toc-children">${n.kids.map((k) => html(k, false)).join('')}</div>` : ''}</div>`;
  return roots.map((n) => html(n, true)).join('');
}

async function generate(cfg, opts = {}) {
  const started = Date.now();
  const result = await render(cfg, opts);
  result.warnings.forEach((w) => console.warn('  ⚠', w));
  console.log(`✔ ${cfg.docName} : ${result.headings} titres, ${result.warnings.length} avertissement(s), ${Date.now() - started} ms`);
  return { cfg, ...result };
}

// ---------- ouverture automatique ----------
// Désactivable avec --no-open (ou RAPPORT_NO_OPEN=1).
const OPEN = !process.argv.includes('--no-open') && !process.env.RAPPORT_NO_OPEN;
function openTarget(target, { reveal = false } = {}) {
  if (!OPEN) return;
  let cmd;
  let args;
  let opts = { detached: true, stdio: 'ignore' };
  if (process.platform === 'win32') {
    if (reveal) { cmd = 'explorer.exe'; args = [`/select,"${target}"`]; opts = { ...opts, windowsVerbatimArguments: true }; }
    else { cmd = 'cmd.exe'; args = ['/c', 'start', '""', `"${target}"`]; opts = { ...opts, windowsVerbatimArguments: true }; }
  } else if (process.platform === 'darwin') {
    cmd = 'open'; args = reveal ? ['-R', target] : [target];
  } else {
    cmd = 'xdg-open'; args = [reveal ? path.dirname(target) : target];
  }
  try {
    const child = spawn(cmd, args, opts);
    child.on('error', (e) => console.warn(`  ⚠ ouverture impossible : ${e.message}`));
    child.unref();
  } catch (e) { console.warn(`  ⚠ ouverture impossible : ${e.message}`); }
}

// ---------- exécution ----------
const fail = (msg) => console.error('✖', msg);
const isConfigError = (e) => e instanceof ConfigError;

// Documents à traiter : ceux de doc/ ayant un .jsonc valide (ou seulement ceux demandés en argument).
function prepare() {
  const { docs, problems, ignored } = discoverDocs(NAMES);
  problems.forEach((p) => fail(`${p.name}${CONFIG_SUFFIX} : ${p.message} — document ignoré`));
  ignored.forEach((f) => console.log(`  (${f} ignoré : pas de ${f.replace(/\.md$/i, '')}${CONFIG_SUFFIX})`));
  const jobs = [];
  let errors = problems.length;
  for (const doc of docs) {
    try { jobs.push({ doc, cfg: loadConfig(doc) }); }
    catch (e) { if (!isConfigError(e)) throw e; fail(`${doc.name}${CONFIG_SUFFIX} : ${e.message} — document ignoré`); errors++; }
  }
  if (!jobs.length) throw new ConfigError(`aucun document à générer : il faut doc/<nom>.md et doc/<nom>${CONFIG_SUFFIX} valide`);
  return { jobs, errors };
}

// Deux documents ne peuvent pas écrire le même fichier.
function checkCollisions(jobs, pick, option) {
  const seen = new Map();
  for (const { doc, cfg } of jobs) {
    const key = pick(cfg).toLowerCase();
    if (seen.has(key)) throw new ConfigError(`${rel(pick(cfg))} serait écrit par « ${seen.get(key)} » et par « ${doc.name} » : donnez-leur un "${option}" différent dans leur .jsonc`);
    seen.set(key, doc.name);
  }
}

// Fichiers écrits dans web/<nom>/ (page + médias) : on en garde la liste pour supprimer plus tard ceux qui ne sont
// plus utilisés (seulement ceux que l'outil a lui-même écrits, jamais un autre fichier).
function copyAssets(cfg, assets) {
  let copied = 0;
  for (const a of assets) {
    const from = path.join(path.dirname(cfg.sourcePath), a);
    const to = path.join(cfg.outputDir, a);
    const s = fs.statSync(from);
    if (fs.existsSync(to) && fs.statSync(to).size === s.size && fs.statSync(to).mtimeMs >= s.mtimeMs) continue;
    fs.mkdirSync(path.dirname(to), { recursive: true });
    fs.copyFileSync(from, to);
    copied++;
  }
  return copied;
}
function syncManifest(cfg, used) {
  const manifest = path.join(cfg.outputDir, '.rapport-files.json');
  let prev = [];
  try { prev = JSON.parse(fs.readFileSync(manifest, 'utf8')); } catch { /* première génération */ }
  const keep = new Set(used);
  for (const f of prev) {
    const file = path.resolve(cfg.outputDir, f);
    if (keep.has(f) || !file.startsWith(cfg.outputDir + path.sep) || !fs.existsSync(file)) continue;
    fs.rmSync(file);
    console.log(`  - ${rel(file)} (n'est plus utilisé)`);
    for (let d = path.dirname(file); d !== cfg.outputDir && fs.readdirSync(d).length === 0; d = path.dirname(d)) fs.rmdirSync(d);
  }
  fs.writeFileSync(manifest, `${JSON.stringify([...keep].sort(), null, 2)}\n`);
}
async function buildFiles() {
  const { jobs, errors: prepErrors } = prepare();
  let errors = prepErrors;
  if (PDF) checkCollisions(jobs, (c) => c.pdfFile, 'pdf.name');
  const outputs = [];
  for (const { cfg } of jobs) {
    try {
      if (PDF) {
        const { html } = await generate(cfg, { pdf: true });
        const started = Date.now();
        const r = await exportPdf({ cfg, html, ConfigError, log: console.log, warn: (m) => console.warn('  ⚠', m) });
        console.log(`→ ${rel(cfg.pdfFile)} (${r.pages} pages, ${r.bookmarks} signets, ${(r.bytes / 1048576).toFixed(1)} Mo, ${((Date.now() - started) / 1000).toFixed(1)} s)`);
      } else {
        const { html, assets } = await generate(cfg);
        fs.mkdirSync(cfg.outputDir, { recursive: true });
        fs.writeFileSync(cfg.outputFile, html);
        const copied = copyAssets(cfg, assets);
        syncManifest(cfg, [path.basename(cfg.outputFile), ...assets]);
        console.log(`→ ${rel(cfg.outputFile)} (${(Buffer.byteLength(html) / 1048576).toFixed(1)} Mo) + ${assets.length} fichier(s) média, ${copied} copié(s)`);
      }
      outputs.push(PDF ? cfg.pdfFile : cfg.outputFile);
    } catch (e) {
      if (!isConfigError(e)) throw e;
      fail(`${cfg.docName} : ${e.message}`);
      errors++;
    }
  }
  if (PDF) outputs.forEach((f) => openTarget(f));
  else if (outputs.length) openTarget(outputs[0], { reveal: true });
  if (errors) process.exitCode = 1;
}

// Serveur local : les pages sont générées en mémoire, les médias sont lus directement dans doc/.
async function serve() {
  const states = new Map();
  let buildId = String(Date.now());
  let running = false;
  let again = false;
  const rebuild = async () => {
    if (running) { again = true; return; }
    running = true;
    try {
      do {
        again = false;
        let jobs = [];
        try { ({ jobs } = prepare()); } catch (e) { if (!isConfigError(e)) throw e; fail(e.message); }
        states.clear();
        for (const { doc, cfg } of jobs) {
          try { states.set(doc.name, await generate(cfg)); }
          catch (e) {
            fail(`${doc.name} : ${e.message}`);
            states.set(doc.name, { html: `<pre style="color:#c00;padding:2em">${esc(e.message)}</pre>` });
          }
        }
        buildId = String(Date.now());
      } while (again);
    } finally { running = false; }
  };
  await rebuild();

  const send = (res, code, type, body) => { res.statusCode = code; res.setHeader('Content-Type', type); res.end(body); };
  const HTML = 'text/html; charset=utf-8';
  const shell = (body) => `<!DOCTYPE html><meta charset="utf-8"><title>Rapport</title><body style="font:16px system-ui;padding:2em">${body}`;
  const server = http.createServer((req, res) => {
    let url;
    try { url = decodeURIComponent(req.url.split('?')[0]); } catch { return send(res, 400, 'text/plain', '400'); }
    if (url === '/__build') return send(res, 200, 'text/plain', buildId);
    if (!states.size) return send(res, 503, HTML, shell('Aucun document valide : voir le terminal.'));
    let name;
    let rest = url;
    if (states.size === 1) name = [...states.keys()][0];
    else {
      if (url === '/') return send(res, 200, HTML, shell(`<h1>Documents</h1><ul>${[...states.keys()].map((n) => `<li><a href="/${encodeURIComponent(n)}/">${esc(n)}</a></li>`).join('')}</ul>`));
      const m = /^\/([^/]+)(\/.*)?$/.exec(url);
      if (!m || !states.has(m[1])) return send(res, 404, 'text/plain', '404');
      if (!m[2]) { res.statusCode = 301; res.setHeader('Location', `/${encodeURIComponent(m[1])}/`); return res.end(); }
      [, name, rest] = m;
    }
    if (rest === '/' || rest === '/index.html') return send(res, 200, HTML, states.get(name).html);
    // Seuls les fichiers de doc/ sont servis (images, pdf…), jamais les fichiers cachés.
    const file = path.resolve(DOC_DIR, '.' + rest);
    const type = MIME[path.extname(file).toLowerCase()];
    const ok = file.startsWith(DOC_DIR + path.sep) && type && fs.existsSync(file) && fs.statSync(file).isFile()
      && !/node_modules|(^|[\\/])\./.test(path.relative(DOC_DIR, file));
    if (!ok) return send(res, 404, 'text/plain', '404');
    res.setHeader('Content-Type', type);
    fs.createReadStream(file).pipe(res);
  });
  // Port occupé (ex. un autre « npm run dev » encore ouvert) : on essaie les ports suivants.
  let port = PORT;
  const MAX_TRIES = 20;
  server.on('error', (e) => {
    if (e.code === 'EADDRINUSE' && !process.env.PORT && port < PORT + MAX_TRIES) {
      console.log(`  ⚠ port ${port} déjà utilisé (un autre « npm run dev » tourne ?), essai du ${port + 1}…`);
      server.listen(++port, '127.0.0.1');
    } else if (e.code === 'EADDRINUSE') {
      fail(`le port ${port} est déjà utilisé. Fermez l'autre « npm run dev » ou choisissez un autre port : $env:PORT=4200; npm run dev`);
      process.exit(1);
    } else {
      fail(`serveur : ${e.message}`);
      process.exit(1);
    }
  });
  server.on('listening', () => {
    console.log(`→ http://localhost:${port}  (Ctrl+C pour arrêter)`);
    openTarget(`http://localhost:${port}`);
  });
  server.listen(port, '127.0.0.1');

  let timer;
  const trigger = () => { clearTimeout(timer); timer = setTimeout(rebuild, 250); };
  // On surveille les dossiers : certains éditeurs remplacent le fichier à la sauvegarde.
  for (const dir of [DOC_DIR, TEMPLATE_DIR]) {
    fs.watch(dir, { recursive: true }, (_, f) => { if (!f || !/(^|[\\/])\./.test(f)) trigger(); });
  }
}

try {
  if (WATCH) await serve();
  else await buildFiles();
} catch (e) {
  if (!isConfigError(e)) throw e;
  fail(e.message);
  process.exit(1);
}
