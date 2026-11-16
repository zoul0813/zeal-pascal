import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';

class Element {
  checked = false; hidden = false; disabled = false; textContent = ''; dataset = {}; listeners = {}; children = [];
  classList = {toggle() {}};
  addEventListener(name, action) { this.listeners[name] = action; }
  setAttribute() {}
  isConnected = true;
  canFocus = true;
  click() { this.clicked = true; this.listeners.click?.(); }
  dispatchEvent(event) { this.listeners[event.type]?.(event); }
  focus() {
    if (!this.canFocus || !this.document) return;
    const old = this.document.activeElement;
    this.document.activeElement = this;
    old?.listeners.blur?.();
    this.listeners.focus?.();
    this.document.listeners.focusin?.({target: this});
  }
  replaceChildren() { this.children = []; }
  append(child) { this.children.push(child); }
}
class Directory {
  kind = 'directory'; entries = new Map();
  constructor(name = '') { this.name = name; }
  async getDirectoryHandle(name, {create = false} = {}) {
    if (!this.entries.has(name) && create) this.entries.set(name, new Directory(name));
    const entry = this.entries.get(name);
    if (!entry) throw new DOMException('Missing', 'NotFoundError');
    if (entry.kind !== 'directory') throw new DOMException('Not a directory', 'TypeMismatchError');
    return entry;
  }
  async getFileHandle(name, {create = false} = {}) {
    if (!this.entries.has(name) && create) {
      this.entries.set(name, {name, kind: 'file', bytes: new Uint8Array(),
        async getFile() { return new Blob([this.bytes]); },
        async createWritable() { return {
          write: async bytes => { this.bytes = new Uint8Array(bytes); }, close: async () => {}, abort: async () => {},
        }; },
      });
    }
    const entry = this.entries.get(name);
    if (!entry) throw new DOMException('Missing', 'NotFoundError');
    if (entry.kind !== 'file') throw new DOMException('Not a file', 'TypeMismatchError');
    return entry;
  }
  async removeEntry(name) {
    if (!this.entries.delete(name)) throw new DOMException('Missing', 'NotFoundError');
  }
  async *values() { yield* this.entries.values(); }
}

test('Open Zeal starts once with project OPFS disks, preserving existing H: files', async () => {
  // Build the stub from the real partial so a dangling #id selector fails here.
  const markup = readFileSync(new URL('../layouts/partials/emulator.html', import.meta.url), 'utf8');
  const ids = [...markup.matchAll(/id="([^"]+)"/g)].map(match => match[1]).concat('main');
  const elements = Object.fromEntries(ids.map(id => [id, new Element()]));
  const open = new Element();
  const screen = new Element();
  const lessonLink = new Element();
  const releasedKeys = [];
  elements.main.contains = target => target === elements.main || target === lessonLink;
  elements['emulator-panel'].querySelectorAll = () => [elements['start-emulator']];
  elements['emulator-panel'].hidden = true;
  elements['emulator-panel'].dataset = {base: '/zeal-pascal/', emulatorVersion: 'test-version'};
  const origin = new Directory();
  const h = await origin.getDirectoryHandle('zeal-pascal-book', {create: true});
  const personal = await h.getFileHandle('sl.pas', {create: true});
  personal.bytes = new TextEncoder().encode('existing reader program');
  const labBeforeStart = await (await h.getDirectoryHandle('book', {create: true})).getDirectoryHandle('lab', {create: true});
  (await labBeforeStart.getFileHandle('START.PAS', {create: true})).bytes = new TextEncoder().encode('earlier example');
  const stores = [], modules = [], updates = [];
  let remoteUpdate = true;
  let typedReset = null, failExit = false, failClose = false, imageLockAvailable = true, workspaceLockAvailable = true, reloaded = 0;
  let accepted = false, failUpdate = false, failCommit = false, failStart = false;
  const confirmations = [], created = [], timers = [];
  const nativeFetch = globalThis.fetch;
  class Store {
    namespace = 'a'.repeat(64);
    async hash(input) { assert.equal(input, 'storageNamespace:"/zeal-pascal/"'); return this.namespace; }
    constructor(options) { this.options = options; stores.push(options); this.images = (options.images || []).filter(image => image.source != null); }
    async loadAll() { return {}; }
    async checkUpdates() {
      if (remoteUpdate) this.options.onUpdateAvailable({name: 'tf'});
      return remoteUpdate ? [{name: 'tf'}] : [];
    }
    async open() {}
    async close() { if (failClose) throw new Error('Close failed'); }
    async stageUpdates(names) { if (failUpdate) throw new Error('Download failed'); return {files: [], names}; }
    async commit(staged) { if (failCommit) throw new Error('Commit failed'); updates.push(Array.from(staged.names)); }
    async persist() {}
    async removeFiles() {}
  }
  const context = vm.createContext({window: {}, DiskImageStore: Store, console, URL, Uint8Array, ArrayBuffer});
  vm.runInContext(readFileSync(new URL('../static/emulator/libnative/zeal-native.js', import.meta.url), 'utf8'), context);
  const globals = {
    document: {
      querySelector: selector => selector === '.screen-wrap' ? screen : selector === '[data-open-emulator]' ? open : elements[selector.slice(1)],
      querySelectorAll: selector => selector === '[data-open-emulator]' ? [open] : [elements['start-emulator']],
      body: new Element(), createElement: () => { const element = new Element(); created.push(element); return element; },
      activeElement: null, hidden: false, hasFocus: () => true, listeners: {},
      addEventListener(name, action) { this.listeners[name] = action; },
    },
    location: {origin: 'https://example.test', pathname: '/zeal-pascal/lessons/the-silent-lab/', reload: () => { reloaded++; }},
    navigator: {
      storage: {getDirectory: async () => origin},
      locks: {request: (name, options, action) => action((name.startsWith('zeal-images:') ? imageLockAvailable : workspaceLockAvailable) ? {name} : null)},
    },
    NativeModule: async options => {
      if (failStart) throw new Error('Restart failed');
      modules.push(options);
      const module = {FS: {mkdirTree() {}},
        _zeal_flush_storage_web: () => { if (failExit) throw new Error('Cannot flush'); options.onStorageComplete(); },
        _zeal_exit_web: () => { options.onStorageComplete(); queueMicrotask(() => options.onExit.call(module, 0)); },
      };
      options.onRuntimeInitialized.call(module);
      options.onEmulatorReady();
      return module;
    },
    setTimeout: callback => { timers.push(callback); return timers.length; },
    KeyboardEvent: class { constructor(type, fields) { this.type = type; Object.assign(this, fields); releasedKeys.push(fields.code); } },
    DiskImageStore: Store, ZealNative: context.window.ZealNative,
    HostFS: class { constructor(directory, options) { this.directory = directory; this.onError = options.onError; } bindModule() {} },
    fetch: async url => {
      const relative = new URL(url).pathname.replace('/zeal-pascal/', '');
      return new Response(readFileSync(new URL('../static/' + relative, import.meta.url)));
    },
  };
  globals.window = {...globals, listeners: {}, addEventListener(name, action) { this.listeners[name] = action; }, prompt: () => typedReset, confirm: message => { confirmations.push(message); return accepted; }};
  for (const element of [...Object.values(elements), open, screen, lessonLink]) element.document = globals.document;
  elements['emulator-panel'].contains = target => Object.entries(elements).some(([id, value]) => id !== 'main' && value === target);
  globals.document.activeElement = elements.main;
  const previous = Object.fromEntries(Object.keys(globals).map(name => [name, Object.getOwnPropertyDescriptor(globalThis, name)]));
  try {
    for (const [name, value] of Object.entries(globals)) Object.defineProperty(globalThis, name, {value, configurable: true});
    await import('../static/js/book.js?workbench-test');
    open.listeners.click(); // No separate Start Zeal click.
    for (let i = 0; i < 100 && elements['start-emulator'].disabled; i++) await new Promise(resolve => setImmediate(resolve));
    assert.equal(modules.length, 1, elements['emulator-status'].textContent);
    assert.equal(elements['focus-emulator'].hidden, true, 'focused canvas has no overlay');
    const toggleFocus = async (extra = {}) => {
      let prevented = false;
      globals.document.listeners.keydown({key: ' ', code: 'Space', ctrlKey: true, shiftKey: true, preventDefault() { prevented = true; }, stopImmediatePropagation() {}, ...extra});
      globals.document.listeners.keyup({key: ' ', code: 'Space'});
      await Promise.resolve();
      return prevented;
    };
    elements.canvas.listeners.keydown({key: 'Control', code: 'ControlLeft', keyCode: 17, ctrlKey: true, preventDefault() {}});
    elements.canvas.listeners.keydown({key: 'Shift', code: 'ShiftLeft', keyCode: 16, ctrlKey: true, preventDefault() {}});
    assert.equal(await toggleFocus(), true);
    assert.deepEqual(releasedKeys, ['ControlLeft', 'ShiftLeft'], 'toggle releases native modifier keys before leaving canvas');
    assert.equal(globals.document.activeElement, elements.main, 'Ctrl+Shift+Space returns to the lesson');
    assert.equal(elements['focus-emulator'].hidden, false);
    assert.match(elements['keyboard-status'].textContent, /inactive/);
    assert.equal(await toggleFocus({repeat: true}), true);
    assert.equal(globals.document.activeElement, elements.main, 'held Ctrl+Shift+Space does not oscillate');
    await toggleFocus();
    assert.equal(globals.document.activeElement, elements.canvas);
    assert.match(elements['keyboard-status'].textContent, /Keyboard active/);
    globals.window.listeners.blur();
    assert.equal(elements['focus-emulator'].hidden, false, 'browser window blur shows overlay');
    globals.window.listeners.focus();
    assert.equal(elements['focus-emulator'].hidden, true);
    globals.document.hidden = true;
    globals.document.listeners.visibilitychange();
    assert.equal(elements['focus-emulator'].hidden, false);
    globals.document.hidden = false;
    globals.document.listeners.visibilitychange();
    elements.main.focus();
    elements['focus-emulator'].listeners.click();
    assert.equal(globals.document.activeElement, elements.canvas, 'overlay focuses canvas');
    assert.equal(await toggleFocus({altKey: true}), false, 'modified browser shortcuts are not captured');
    open.focus();
    await toggleFocus(); // Book button → canvas, but not a new return target.
    await toggleFocus();
    assert.equal(globals.document.activeElement, elements.main, 'return does not focus Open Zeal');
    lessonLink.focus();
    await toggleFocus();
    lessonLink.canFocus = false;
    await toggleFocus();
    assert.equal(globals.document.activeElement, elements.main, 'hidden lesson target falls back to main');
    assert.equal(elements.canvas.dataset.mouseEnabled, 'false');
    screen.listeners.click();
    assert.equal(globals.document.activeElement, elements.canvas, 'screen click focuses keyboard with mouse disabled');
    elements['enable-mouse'].checked = true;
    elements['enable-mouse'].listeners.change();
    assert.equal(elements.canvas.dataset.mouseEnabled, 'true');
    elements['enable-mouse'].checked = false;
    elements['enable-mouse'].listeners.change();
    assert.equal(elements.canvas.dataset.mouseEnabled, 'false');
    assert.equal(stores[0].storage, 'opfs');
    assert.equal(stores[0].storageNamespace, '/zeal-pascal/');
    assert.deepEqual(Array.from(stores[0].images, image => image.key), ['book-rom', 'book-eeprom', 'book-tf']);
    assert.equal(modules[0].hostfsBackend.directory, h);
    assert.equal(modules[0].arguments.filter(arg => arg === '-e').length, 1);
    assert.equal(modules[0].arguments.filter(arg => arg === '-t').length, 1);
    assert.equal(modules[0].locateFile('zeal.elf.wasm'), 'https://example.test/zeal-pascal/emulator/zeal.elf.wasm?v=test-version');
    assert.equal(await (await personal.getFile()).text(), 'existing reader program');
    assert.match(elements['emulator-status'].textContent, /Zeal is ready/);
    assert.equal(elements['start-emulator'].hidden, true);
    assert.equal(h.entries.has('bin'), false, 'no compiler installed on H:');
    assert.equal(h.entries.has('lib'), false, 'no library installed on H:');
    assert.match(elements['storage-status'].textContent, /A Zeal update is available/);
    assert.notEqual(elements['workspace-options'].open, true, 'update notices do not expand More options');
    await elements['update-images'].listeners.click();
    assert.deepEqual(updates, [], 'cancel keeps disks and runtime');
    assert.equal(modules.length, 1);
    assert.match(confirmations.at(-1), /Any files saved on T: will be lost/);
    assert.match(confirmations.at(-1), /Your H: files are kept/);
    accepted = true; remoteUpdate = false;
    await elements['update-images'].listeners.click();
    assert.deepEqual(updates, [['romdisk', 'eeprom', 'tf']], 'one update action refreshes the complete tool/system set');
    assert.equal(modules.length, 2, 'successful update restarts runtime');
    assert.equal(elements['storage-status'].textContent, '', 'no update leaves an empty, rendered live region');
    assert.equal(elements['storage-status'].hidden, false);
    assert.match(elements['starter-status'].textContent, /examples differ/);
    assert.match(elements['starter-status'].textContent, /Save As/);
    failUpdate = true;
    await elements['update-images'].listeners.click();
    assert.equal(modules.length, 2, 'failed staging does not stop runtime');
    assert.match(elements['emulator-status'].textContent, /Download failed/);
    assert.doesNotMatch(elements['storage-status'].textContent, /backup|problem/);
    assert.equal(await (await personal.getFile()).text(), 'existing reader program');
    failUpdate = false;
    const lab = await (await h.getDirectoryHandle('book')).getDirectoryHandle('lab');
    const starter = await lab.getFileHandle('START.PAS');
    starter.bytes = new TextEncoder().encode('old modified starter');
    const myFile = await lab.getFileHandle('MINE.PAS', {create: true});
    myFile.bytes = new TextEncoder().encode('personal file');
    accepted = false;
    await elements['update-starters'].listeners.click();
    assert.equal(await (await starter.getFile()).text(), 'old modified starter');
    accepted = true;
    await elements['update-starters'].listeners.click();
    assert.equal(await (await starter.getFile()).text(), readFileSync(new URL('../static/code/lab/START.PAS', import.meta.url), 'utf8'));
    assert.equal(elements['starter-status'].textContent, '', 'refresh clears the divergence message');
    assert.equal(await (await myFile.getFile()).text(), 'personal file');
    assert.equal(await (await personal.getFile()).text(), 'existing reader program');
    const oldBin = await (await h.getDirectoryHandle('bin', {create: true})).getFileHandle('zpascal.bin', {create: true});
    oldBin.bytes = new Uint8Array([8]);
    const oldLib = await (await h.getDirectoryHandle('lib', {create: true})).getFileHandle('zpascal.zpl', {create: true});
    oldLib.bytes = new Uint8Array([9]);
    failCommit = true; failStart = true;
    await elements['update-images'].listeners.click();
    assert.equal(elements['start-emulator'].hidden, false);
    assert.equal(elements['start-emulator'].disabled, false);
    assert.match(elements['emulator-status'].textContent, /Restart failed/);
    failCommit = false; failStart = false;
    await elements['start-emulator'].listeners.click();
    assert.equal(stores.length, 1, 'retry reuses the image owner instead of taking another lock');
    assert.equal(modules.length, 3);
    assert.deepEqual(oldBin.bytes, new Uint8Array([8]));
    assert.deepEqual(oldLib.bytes, new Uint8Array([9]));
    assert.equal(await (await personal.getFile()).text(), 'existing reader program');
    modules[0].hostfsBackend.onError('operation 1', new DOMException('Workspace deleted', 'NotFoundError'));
    assert.match(elements['emulator-status'].textContent, /File operation failed: Workspace deleted/);
    modules[0].hostfsBackend.onError('operation 4', new DOMException('Disk full', 'QuotaExceededError'));
    assert.match(elements['emulator-status'].textContent, /File operation failed: Disk full/);
    open.listeners.click(); // Hide, then reopen without restarting the guest.
    open.listeners.click();
    assert.equal(modules.length, 3);
    await elements['export-work'].listeners.click();
    const download = created.find(element => element.download === 'zeal-pascal-workspace.json');
    assert.equal(download.clicked, true);
    const backup = await (await nativeFetch(download.href)).json();
    assert.equal(backup.format, 'zeal-pascal-book-v1');
    assert.equal(backup.files.find(file => file.path === 'sl.pas').base64, btoa('existing reader program'));
    assert.equal(backup.files.some(file => file.path.startsWith('bin/') || file.path.startsWith('lib/')), false);
    assert.match(elements['emulator-status'].textContent, /Backup downloaded/);
    timers.forEach(callback => callback());
    const restore = {files: [{size: 1024, text: async () => JSON.stringify(backup)}], value: 'backup.json'};
    await elements['import-work'].listeners.change({target: restore});
    assert.equal(restore.value, '');
    const result = elements['emulator-status'].textContent;
    assert.match(result, /Restored into H:\/work\/in-/);
    assert.match(result, /Your existing files were not changed/);
    const restoredPath = result.match(/Restored into H:\/(.*?)\/\./)[1];
    let restored = h;
    for (const part of restoredPath.split('/')) restored = await restored.getDirectoryHandle(part);
    assert.equal(await (await (await restored.getFileHandle('sl.pas')).getFile()).text(), 'existing reader program');
    assert.equal(await (await personal.getFile()).text(), 'existing reader program');
    const images = await origin.getDirectoryHandle('zeal-native', {create: true});
    const bookImages = await images.getDirectoryHandle('a'.repeat(64), {create: true});
    await bookImages.getFileHandle('orphan.img', {create: true});
    await images.getDirectoryHandle('other-project', {create: true});
    await origin.getDirectoryHandle('other-app', {create: true});
    accepted = false;
    await elements['reset-workspace'].listeners.click();
    assert.equal(reloaded, 0);
    assert.equal(origin.entries.has('zeal-pascal-book'), true);
    accepted = true; typedReset = 'wrong';
    await elements['reset-workspace'].listeners.click();
    assert.equal(origin.entries.has('zeal-pascal-book'), true);
    typedReset = 'RESET'; failExit = true;
    await elements['reset-workspace'].listeners.click();
    assert.equal(origin.entries.has('zeal-pascal-book'), true, 'failed teardown never deletes storage');
    assert.equal(images.entries.has('a'.repeat(64)), true);
    assert.equal(reloaded, 0);
    failExit = false; failClose = true;
    await elements['reset-workspace'].listeners.click();
    assert.equal(origin.entries.has('zeal-pascal-book'), true, 'store close failure never deletes storage');
    assert.equal(elements['start-emulator'].hidden, false, 'stopped runtime is not reported as running');
    assert.equal(elements['audio'].disabled, true);
    assert.match(elements['keyboard-status'].textContent, /inactive/);
    failClose = false; imageLockAvailable = false;
    await elements['reset-workspace'].listeners.click();
    assert.equal(origin.entries.has('zeal-pascal-book'), true, 'active image owner prevents all deletion');
    assert.equal(elements['reset-workspace'].disabled, false, 'reset is available after failure');
    assert.equal(reloaded, 0);
    imageLockAvailable = true; workspaceLockAvailable = false;
    // The existing book lock is still owned, so reset may safely reuse it.
    await elements['reset-workspace'].listeners.click();
    assert.equal(origin.entries.has('zeal-pascal-book'), false);
    assert.equal(images.entries.has('a'.repeat(64)), false, 'all image files, including orphans, removed');
    assert.equal(images.entries.has('other-project'), true);
    assert.equal(origin.entries.has('other-app'), true);
    assert.equal(reloaded, 1, 'successful reset reloads without restarting stale handles');
    assert.match(confirmations.at(-1), /ALL.*H: files/);
  } finally {
    for (const [name, descriptor] of Object.entries(previous)) {
      if (descriptor) Object.defineProperty(globalThis, name, descriptor);
      else delete globalThis[name];
    }
  }
});

test('bundled HostFS keeps guest lookup errors quiet and reports an invalidated workspace', async () => {
  const diagnostics = [], notifications = [];
  const context = vm.createContext({window: {}, DOMException, TextEncoder, Uint8Array,
    console: {error: (...args) => diagnostics.push(args)}});
  vm.runInContext(readFileSync(new URL('../static/emulator/libnative/hostfs.js', import.meta.url), 'utf8'), context);
  const root = new Directory();
  const host = new context.window.HostFS(root, {onError: (...args) => notifications.push(args)});
  const invoke = request => new Promise(resolve => {
    host.bindModule({_hostfs_web_complete: status => resolve(status)});
    host.start({flags: 0, ...request});
  });
  assert.equal(await invoke({operation: 1, path: '/bin/ls'}), 4);
  assert.equal(await invoke({operation: 1, path: '/ls'}), 4);
  assert.equal(await invoke({operation: 6, path: '/missing'}), 4);
  assert.equal(await invoke({operation: 6, path: '/'}), 0);
  assert.equal(diagnostics.length, 0);
  assert.equal(notifications.length, 0);
  root.values = async function* () { throw new DOMException('Workspace deleted', 'NotFoundError'); };
  assert.equal(await invoke({operation: 1, path: '/ls'}), 1);
  assert.equal(diagnostics.length, 1);
  assert.equal(notifications.length, 1);
  assert.equal(notifications[0][1].name, 'NotFoundError');
});
