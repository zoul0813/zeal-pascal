import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';

class Element {
  hidden = false; disabled = false; textContent = ''; dataset = {}; listeners = {}; children = [];
  classList = {toggle() {}};
  addEventListener(name, action) { this.listeners[name] = action; }
  setAttribute() {}
  focus() {}
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
  async *values() { yield* this.entries.values(); }
}

test('Open Zeal starts once with project OPFS disks, preserving existing H: files', async () => {
  const ids = ['emulator-panel', 'emulator-status', 'canvas', 'start-emulator', 'storage-status',
    'close-emulator', 'expand-emulator', 'audio', 'load-all', 'update-starters', 'starter-status', 'export-work',
    'import-work', 'image-updates', 'check-images', 'update-images', 'image-tf', 'image-eeprom', 'image-romdisk', 'lesson-path', 'workspace-folders', 'disk-store-path'];
  const elements = Object.fromEntries(ids.map(id => [id, new Element()]));
  const open = new Element();
  elements['emulator-panel'].querySelectorAll = () => [elements['start-emulator']];
  elements['emulator-panel'].hidden = true;
  elements['emulator-panel'].dataset = {base: '/zeal-pascal/', emulatorVersion: 'test-version'};
  const origin = new Directory();
  const h = await origin.getDirectoryHandle('zeal-pascal-book', {create: true});
  const personal = await h.getFileHandle('sl.pas', {create: true});
  personal.bytes = new TextEncoder().encode('existing reader program');
  const stores = [], modules = [], updates = [];
  let accepted = false, failUpdate = false, failCommit = false, failStart = false, failCheck = false;
  const confirmations = [];
  elements["image-tf"].checked = true;
  elements["image-eeprom"].checked = true;
  elements["image-romdisk"].checked = false;
  class Store {
    namespace = 'project-hash';
    constructor(options) { this.options = options; stores.push(options); this.images = options.images.filter(image => image.source != null); }
    async loadAll() { return {}; }
    async checkUpdates() {
      if (failCheck) throw new Error('Check failed');
      this.options.onUpdateAvailable({name: 'tf'});
      return [{name: 'tf'}];
    }
    async open() {}
    async close() {}
    async stageUpdates(names) { if (failUpdate) throw new Error('Download failed'); return {files: [], names}; }
    async commit(staged) { if (failCommit) throw new Error('Commit failed'); updates.push(Array.from(staged.names)); }
    async persist() {}
    async removeFiles() {}
  }
  const context = vm.createContext({window: {}, DiskImageStore: Store, console, URL, Uint8Array, ArrayBuffer});
  vm.runInContext(readFileSync(new URL('../static/emulator/libnative/zeal-native.js', import.meta.url), 'utf8'), context);
  const globals = {
    document: {
      querySelector: selector => selector === '[data-open-emulator]' ? open : elements[selector.slice(1)],
      querySelectorAll: selector => selector === '[data-open-emulator]' ? [open] : [elements['start-emulator']],
      body: new Element(), createElement: () => new Element(),
    },
    location: {origin: 'https://example.test', pathname: '/zeal-pascal/lessons/the-silent-lab/'},
    navigator: {
      storage: {getDirectory: async () => origin},
      locks: {request: (name, options, action) => action({name})},
    },
    NativeModule: async options => {
      if (failStart) throw new Error('Restart failed');
      modules.push(options);
      const module = {FS: {mkdirTree() {}},
        _zeal_flush_storage_web: () => options.onStorageComplete(),
        _zeal_exit_web: () => { options.onStorageComplete(); queueMicrotask(() => options.onExit.call(module, 0)); },
      };
      options.onRuntimeInitialized.call(module);
      options.onEmulatorReady();
      return module;
    },
    IndexedDBStore: class {}, DiskImageStore: Store, ZealNative: context.window.ZealNative,
    HostFS: class { constructor(directory, options) { this.directory = directory; this.onError = options.onError; } bindModule() {} },
    fetch: async url => {
      const relative = new URL(url).pathname.replace('/zeal-pascal/', '');
      return new Response(readFileSync(new URL('../static/' + relative, import.meta.url)));
    },
  };
  globals.window = {...globals, addEventListener() {}, confirm: message => { confirmations.push(message); return accepted; }};
  const previous = Object.fromEntries(Object.keys(globals).map(name => [name, Object.getOwnPropertyDescriptor(globalThis, name)]));
  try {
    for (const [name, value] of Object.entries(globals)) Object.defineProperty(globalThis, name, {value, configurable: true});
    await import('../static/js/book.js?workbench-test');
    open.listeners.click(); // No separate Start Zeal click.
    for (let i = 0; i < 100 && elements['start-emulator'].disabled; i++) await new Promise(resolve => setImmediate(resolve));
    assert.equal(modules.length, 1, elements['emulator-status'].textContent);
    assert.equal(stores[0].storage, 'opfs');
    assert.equal(stores[0].storageNamespace, '/zeal-pascal/');
    assert.deepEqual(Array.from(stores[0].images, image => image.key), ['book-rom', 'book-eeprom', 'book-tf']);
    assert.equal(modules[0].hostfsBackend.directory, h);
    assert.equal(modules[0].arguments.filter(arg => arg === '-e').length, 1);
    assert.equal(modules[0].arguments.filter(arg => arg === '-t').length, 1);
    assert.equal(modules[0].locateFile('zeal.elf.wasm'), 'https://example.test/zeal-pascal/emulator/zeal.elf.wasm?v=test-version');
    assert.equal(await (await personal.getFile()).text(), 'existing reader program');
    assert.equal(elements['disk-store-path'].textContent, 'zeal-native/project-hash/');
    assert.match(elements['emulator-status'].textContent, /type zpascal/);
    assert.equal(elements['start-emulator'].hidden, true);
    assert.equal(h.entries.has('bin'), false, 'no compiler installed on H:');
    assert.equal(h.entries.has('lib'), false, 'no library installed on H:');
    assert.match(elements['storage-status'].textContent, /New disk images available/);
    assert.equal(elements['image-updates'].open, true);
    await elements['update-images'].listeners.click();
    assert.deepEqual(updates, [], 'cancel keeps disks and runtime');
    assert.equal(modules.length, 1);
    assert.match(confirmations.at(-1), /T:\/src\/pascal\/UNTITLED.pas/);
    accepted = true;
    await elements['update-images'].listeners.click();
    assert.deepEqual(updates, [['tf', 'eeprom']], 'only selected images replaced');
    assert.equal(modules.length, 2, 'successful update restarts runtime');
    failUpdate = true;
    await elements['update-images'].listeners.click();
    assert.equal(modules.length, 2, 'failed staging does not stop runtime');
    assert.match(elements['emulator-status'].textContent, /Download failed/);
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
    await elements['check-images'].listeners.click();
    assert.match(elements['storage-status'].textContent, /New disk images available/);
    failCheck = true;
    await elements['check-images'].listeners.click();
    assert.match(elements['emulator-status'].textContent, /Check failed/);
    assert.doesNotMatch(elements['storage-status'].textContent, /New disk images available/);
    modules[0].hostfsBackend.onError('operation 1', new DOMException('Workspace deleted', 'NotFoundError'));
    assert.match(elements['emulator-status'].textContent, /File operation failed: Workspace deleted/);
    modules[0].hostfsBackend.onError('operation 4', new DOMException('Disk full', 'QuotaExceededError'));
    assert.match(elements['emulator-status'].textContent, /File operation failed: Disk full/);
    open.listeners.click(); // Hide, then reopen without restarting the guest.
    open.listeners.click();
    assert.equal(modules.length, 3);
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
