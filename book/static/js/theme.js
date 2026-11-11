// Run before styles load so a saved preference applies on the first paint.
(() => {
  const key = 'zeal-pascal-book-theme';
  const root = document.documentElement;
  const system = window.matchMedia('(prefers-color-scheme: dark)');
  let choice = null;
  try { choice = localStorage.getItem(key); } catch { /* System theme still works without storage. */ }
  const valid = value => value === 'light' || value === 'dark';
  if (valid(choice)) root.dataset.theme = choice;
  else choice = null;

  const isDark = () => choice ? choice === 'dark' : system.matches;
  let button;
  function updateButton() {
    if (!button) return;
    button.setAttribute('aria-pressed', String(isDark()));
    button.title = isDark() ? 'Switch to light theme' : 'Switch to dark theme';
  }
  system.addEventListener('change', updateButton);
  window.addEventListener('storage', event => {
    if (event.key !== key && event.key !== null) return;
    choice = valid(event.newValue) ? event.newValue : null;
    if (choice) root.dataset.theme = choice;
    else delete root.dataset.theme;
    updateButton();
  });
  document.addEventListener('DOMContentLoaded', () => {
    button = document.querySelector('#theme-toggle');
    button.hidden = false;
    updateButton();
    button.addEventListener('click', () => {
      choice = isDark() ? 'light' : 'dark';
      root.dataset.theme = choice;
      try { localStorage.setItem(key, choice); } catch { /* Keep the choice for this page. */ }
      updateButton();
    });
  });
})();
