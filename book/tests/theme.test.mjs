import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';

test('theme switch follows system until chosen and synchronizes icon state, tooltip and other tabs', () => {
  const events = {}, attrs = {}, root = {dataset: {}}, button = {hidden: true,
    setAttribute: (key, value) => { attrs[key] = value; },
    addEventListener: (key, action) => { events[key] = action; }};
  const system = {matches: true, addEventListener: (key, action) => { events.system = action; }};
  const saved = new Map();
  const context = vm.createContext({document: {documentElement: root,
    querySelector: selector => { assert.equal(selector, '#theme-toggle'); return button; },
    addEventListener: (key, action) => { events[key] = action; }},
    window: {matchMedia: () => system, addEventListener: (key, action) => { events[key] = action; }},
    localStorage: {getItem: key => saved.get(key), setItem: (key, value) => saved.set(key, value)}});
  vm.runInContext(readFileSync(new URL('../static/js/theme.js', import.meta.url), 'utf8'), context);
  events.DOMContentLoaded();
  assert.equal(button.hidden, false);
  assert.equal(root.dataset.theme, undefined);
  assert.equal(attrs['aria-checked'], 'true');
  assert.equal(button.title, 'Switch to light theme');
  system.matches = false; events.system();
  assert.equal(attrs['aria-checked'], 'false');
  assert.equal(button.title, 'Switch to dark theme');
  events.click();
  assert.equal(root.dataset.theme, 'dark');
  assert.equal(saved.get('zeal-pascal-book-theme'), 'dark');
  assert.equal(attrs['aria-checked'], 'true');
  events.storage({key: 'zeal-pascal-book-theme', newValue: 'light'});
  assert.equal(root.dataset.theme, 'light');
  assert.equal(attrs['aria-checked'], 'false');
  events.storage({key: 'zeal-pascal-book-theme', newValue: null});
  assert.equal(root.dataset.theme, undefined);
});
