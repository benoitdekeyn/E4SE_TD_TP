(() => {
  const CFG = window.__REPORT__;
  const root = document.documentElement;
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => [...r.querySelectorAll(s)];
  const store = {
    get: (k) => { try { return localStorage.getItem(`${CFG.key}:${k}`); } catch (_) { return null; } },
    set: (k, v) => { try { localStorage.setItem(`${CFG.key}:${k}`, v); } catch (_) {} },
  };

  // --- Navigation interne (comme GitHub : fragments encodés ou non, insensibles à la casse) ---
  function findTarget(hash) {
    const id = hash.replace(/^#/, '');
    if (!id) return null;
    const candidates = [id];
    try { candidates.push(decodeURIComponent(id)); } catch (_) {}
    for (const c of candidates) {
      const el = document.getElementById(c) || document.getElementById(c.toLowerCase());
      if (el) return el;
    }
    return null;
  }
  function goTo(hash, push) {
    const el = findTarget(hash);
    if (!el) return false;
    el.scrollIntoView();
    if (push) history.pushState(null, '', hash);
    document.body.classList.remove('sidebar-open');
    return true;
  }
  document.addEventListener('click', (e) => {
    const a = e.target.closest('a[href^="#"]');
    if (!a || e.defaultPrevented || e.ctrlKey || e.metaKey || e.shiftKey) return;
    if (goTo(a.getAttribute('href'), true)) e.preventDefault();
  });
  window.addEventListener('hashchange', () => goTo(location.hash, false));

  // --- Boutons "Copier" ---
  $$('.copy-btn').forEach((btn) => {
    btn.addEventListener('click', async () => {
      const code = $('code', btn.parentElement).innerText;
      try {
        await navigator.clipboard.writeText(code);
        btn.textContent = 'Copié ✓';
      } catch (_) {
        btn.textContent = 'Échec';
      }
      setTimeout(() => (btn.textContent = 'Copier'), 1500);
    });
  });

  // --- Sommaire : suivi de la section visible ---
  const toc = $('#toc');
  const links = $$('a[data-id]', toc);
  const byId = new Map(links.map((a) => [a.dataset.id, a]));
  const headings = links.map((a) => document.getElementById(a.dataset.id)).filter(Boolean);
  let current = null;

  // Le sommaire défile seul (jamais la page) et on ne le déplace pas pendant que l'utilisateur le manipule.
  let lastTocUse = 0;
  ['wheel', 'touchstart', 'pointerdown', 'keydown'].forEach((ev) =>
    toc.addEventListener(ev, () => { lastTocUse = Date.now(); }, { passive: true }));
  function revealInToc(a) {
    if (Date.now() - lastTocUse < 2500) return;
    const box = toc.getBoundingClientRect();
    const r = a.getBoundingClientRect();
    if (r.top >= box.top + 8 && r.bottom <= box.bottom - 8) return;
    toc.scrollTop += r.top - box.top - box.height / 2 + r.height / 2;
  }
  function setActive(id) {
    if (id === current) return;
    current = id;
    links.forEach((a) => a.classList.remove('active'));
    $$('.toc-group.open', toc).forEach((g) => g.classList.remove('open'));
    const a = byId.get(id);
    if (!a) return;
    a.classList.add('active');
    const group = a.closest('.toc-group');
    if (group) group.classList.add('open');
    revealInToc(a);
  }
  function onScroll() {
    let active = headings[0];
    for (const h of headings) {
      if (h.getBoundingClientRect().top <= 80) active = h;
      else break;
    }
    if (active) setActive(active.id);
  }
  let ticking = false;
  window.addEventListener('scroll', () => {
    if (ticking) return;
    ticking = true;
    requestAnimationFrame(() => { onScroll(); ticking = false; });
  }, { passive: true });

  // --- Filtre du sommaire ---
  $('#toc-filter').addEventListener('input', (e) => {
    const q = e.target.value.trim().toLowerCase();
    toc.classList.toggle('filtering', !!q);
    $$('.toc-group', toc).forEach((g) => {
      let any = false;
      $$('a', g).forEach((a) => {
        const hit = !q || a.textContent.toLowerCase().includes(q);
        a.classList.toggle('hidden', !hit);
        any = any || hit;
      });
      g.classList.toggle('hidden', !any);
    });
  });

  $('#sidebar-toggle').addEventListener('click', () => document.body.classList.toggle('sidebar-open'));

  // --- Redimensionnement du sommaire par glisser-déposer de sa bordure ---
  const resizer = $('#resizer');
  const clampW = (w) => Math.max(CFG.sidebarMin, Math.min(CFG.sidebarMax, Math.min(w, window.innerWidth * 0.8)));
  const setW = (w) => root.style.setProperty('--sidebar-w', `${Math.round(clampW(w))}px`);
  resizer.addEventListener('pointerdown', (e) => {
    resizer.setPointerCapture(e.pointerId);
    document.body.classList.add('resizing');
    e.preventDefault();
  });
  resizer.addEventListener('pointermove', (e) => {
    if (resizer.hasPointerCapture(e.pointerId)) setW(e.clientX);
  });
  const endDrag = (e) => {
    if (!resizer.hasPointerCapture(e.pointerId)) return;
    resizer.releasePointerCapture(e.pointerId);
    document.body.classList.remove('resizing');
    store.set('sidebar', parseInt(getComputedStyle(root).getPropertyValue('--sidebar-w'), 10));
  };
  resizer.addEventListener('pointerup', endDrag);
  resizer.addEventListener('pointercancel', endDrag);
  resizer.addEventListener('dblclick', () => { setW(CFG.sidebarWidth); store.set('sidebar', CFG.sidebarWidth); });
  const saved = parseInt(store.get('sidebar'), 10);
  if (saved) setW(saved);

  // --- Thème : auto -> clair -> sombre ---
  const themeBtn = $('#theme-toggle');
  const labels = { auto: '◐ Auto', light: '☀ Clair', dark: '☾ Sombre' };
  const dm = matchMedia('(prefers-color-scheme: dark)');
  let mode = root.dataset.mode;
  function applyTheme() {
    const dark = mode === 'dark' || (mode === 'auto' && dm.matches);
    root.dataset.theme = dark ? 'dark' : 'light';
    root.dataset.mode = mode;
    themeBtn.textContent = labels[mode];
    renderMermaid();
  }
  themeBtn.addEventListener('click', () => {
    mode = { auto: 'light', light: 'dark', dark: 'auto' }[mode];
    store.set('theme', mode);
    applyTheme();
  });
  dm.addEventListener('change', () => { if (mode === 'auto') applyTheme(); });

  // --- Mermaid (re-rendu à chaque changement de thème, une seule exécution à la fois) ---
  $$('pre.mermaid').forEach((n) => { n.dataset.src = n.textContent; });
  let mermaidQueue = Promise.resolve();
  function renderMermaid() {
    if (!window.mermaid) return mermaidQueue;
    mermaidQueue = mermaidQueue.then(async () => {
      const theme = CFG.mermaidTheme === 'auto' ? (root.dataset.theme === 'dark' ? 'dark' : 'default') : CFG.mermaidTheme;
      if (window.__elk && mermaid.registerLayoutLoaders) mermaid.registerLayoutLoaders(window.__elk.default);
      mermaid.initialize({ startOnLoad: false, theme, securityLevel: 'strict' });
      const nodes = $$('pre.mermaid');
      nodes.forEach((n) => { n.textContent = n.dataset.src; n.removeAttribute('data-processed'); });
      try { await mermaid.run({ nodes }); } catch (e) { console.error('Mermaid :', e); }
    });
    return mermaidQueue;
  }

  themeBtn.textContent = labels[mode];
  window.addEventListener('load', async () => {
    await renderMermaid();
    // Images et diagrammes changent la hauteur de page : on se replace sur l'ancre demandée.
    if (location.hash) goTo(location.hash, false);
    onScroll();
  });
  onScroll();
})();
