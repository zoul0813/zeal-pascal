import test from 'node:test';
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';
import {webcrypto} from 'node:crypto';
import {clearBookStorage} from '../static/js/workspace.mjs';

class Directory {
  entries = new Map();
  async getDirectoryHandle(name, {create = false} = {}) {
    if (!this.entries.has(name) && create) this.entries.set(name, new Directory());
    if (!this.entries.has(name)) throw new DOMException('Missing', 'NotFoundError');
    return this.entries.get(name);
  }
  async getFileHandle(name) {
    if (!this.entries.has(name)) throw new DOMException('Missing', 'NotFoundError');
    return this.entries.get(name);
  }
  async removeEntry(name) {
    if (!this.entries.delete(name)) throw new DOMException('Missing', 'NotFoundError');
  }
}
function locks() {
  const held = new Set();
  return {held, async request(name, options, action) {
    if (held.has(name)) return action(null);
    held.add(name);
    try { return await action({name}); }
    finally { held.delete(name); }
  }};
}
test('real native image store namespace and locks match reset, including corrupt-manifest recovery', async () => {
  const origin = new Directory(), lockManager = locks();
  const context = vm.createContext({window: {isSecureContext: true}, console, URL, Uint8Array, ArrayBuffer, TextEncoder,
    crypto: webcrypto, navigator: {storage: {getDirectory: async () => origin}, locks: lockManager}});
  vm.runInContext(readFileSync(new URL('../static/emulator/libnative/disk-images.js', import.meta.url), 'utf8'), context);
  const Store = context.window.DiskImageStore;
  const store = new Store({storage: 'opfs', storageNamespace: '/zeal-pascal/book/'});
  const namespace = await store.hash(`storageNamespace:${JSON.stringify('/zeal-pascal/book/')}`);
  await store.open();
  assert.equal(store.namespace, namespace);
  assert.equal('database' in store, false, 'the upstream OPFS store has no IndexedDB dependency');
  const h = await origin.getDirectoryHandle('zeal-pascal-book', {create: true});
  const parent = await origin.getDirectoryHandle('zeal-native');
  await assert.rejects(clearBookStorage(origin, namespace, lockManager), /in use/);
  assert.equal(await origin.getDirectoryHandle('zeal-pascal-book'), h);
  await store.close();
  assert.equal(lockManager.held.size, 0);
  const diskDirectory = await parent.getDirectoryHandle(namespace);
  diskDirectory.entries.set('manifest.json', {getFile: async () => new Blob(['{broken JSON'])});
  await assert.rejects(store.open(), /JSON/);
  assert.equal(lockManager.held.size, 0, 'failed manifest validation releases native ownership');
  await clearBookStorage(origin, namespace, lockManager);
  assert.equal(origin.entries.has('zeal-pascal-book'), false);
  assert.equal(parent.entries.has(namespace), false);
  await store.open();
  assert.equal(Object.keys(store.manifest.images).length, 0, 'fresh store initializes after complete reset');
  await store.close();
});
