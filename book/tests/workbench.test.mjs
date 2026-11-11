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
    'close-emulator', 'expand-emulator', 'audio', 'load-all', 'fresh-lesson', 'export-work',
    'import-work', 'restore-eeprom', 'lesson-path', 'workspace-folders', 'disk-store-path'];
  const elements = Object.fromEntries(ids.map(id => [id, new Element()]));
  const open = new Element();
  elements['emulator-panel'].querySelectorAll = () => [elements['start-emulator']];
  elements['emulator-panel'].hidden = true;
  elements['emulator-panel'].dataset = {base: '/zeal-pascal/', emulatorVersion: 'test-version'};
  const origin = new Directory();
  const h = await origin.getDirectoryHandle('zeal-pascal-book', {create: true});
  const personal = await h.getFileHandle('sl.pas', {create: true});
  personal.bytes = new TextEncoder().encode('existing reader program');
  const stores = [], modules = [];
  class Store {
    namespace = 'project-hash';
    constructor(options) { stores.push(options); this.images = options.images.filter(image => image.source != null); }
    async loadAll() { return {}; }
    async checkUpdates() { return []; }
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
      modules.push(options);
      const module = {FS: {mkdirTree() {}}};
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
  globals.window = {...globals, addEventListener() {}};
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
    modules[0].hostfsBackend.onError('operation 1', new DOMException('Workspace deleted', 'NotFoundError'));
    assert.match(elements['emulator-status'].textContent, /File operation failed: Workspace deleted/);
    modules[0].hostfsBackend.onError('operation 4', new DOMException('Disk full', 'QuotaExceededError'));
    assert.match(elements['emulator-status'].textContent, /File operation failed: Disk full/);
    open.listeners.click(); // Hide, then reopen without restarting the guest.
    open.listeners.click();
    assert.equal(modules.length, 1);
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
