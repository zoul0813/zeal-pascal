import test from 'node:test';
import assert from 'node:assert/strict';
import {directoryAt, writeFile, seedFiles, validateBackup, encodeBytes, clearBookStorage} from '../static/js/workspace.mjs';
class Directory {
  entries = new Map();
  async getDirectoryHandle(name, {create = false} = {}) {
    if (!this.entries.has(name) && create) this.entries.set(name, new Directory());
    const value = this.entries.get(name);
    if (!value) throw new DOMException('Missing', 'NotFoundError');
    if (!(value instanceof Directory)) throw new DOMException('File', 'TypeMismatchError');
    return value;
  }
  async removeEntry(name) {
    if (!this.entries.delete(name)) throw new DOMException('Missing', 'NotFoundError');
  }
  async getFileHandle(name, {create = false} = {}) {
    if (!this.entries.has(name) && create) this.entries.set(name, {bytes: new Uint8Array(), async getFile() { return new Blob([this.bytes]); }, async createWritable() { return {write: async bytes => { this.bytes = bytes; }, close: async () => {}, abort: async () => {}}; }});
    const value = this.entries.get(name);
    if (!value) throw new DOMException('Missing', 'NotFoundError');
    if (value instanceof Directory) throw new DOMException('Directory', 'TypeMismatchError');
    return value;
  }
}
test('lesson loading preserves edited files and fresh copies use separate folders', async () => {
  const root = new Directory();
  await writeFile(root, 'book/lab/START.PAS', new Uint8Array([9]));
  const files = [{path: 'book/lab/START.PAS', url: 'starter'}, {path: 'book/lab/HELLO.PAS', url: 'hello'}];
  await seedFiles(root, files, async () => new Uint8Array([1]));
  const original = await directoryAt(root, 'book/lab');
  assert.deepEqual((await original.getFileHandle('START.PAS')).bytes, new Uint8Array([9]));
  await seedFiles(root, [{path: 'book/lab-new/START.PAS', url: 'starter'}], async () => new Uint8Array([1]));
  assert.deepEqual((await original.getFileHandle('START.PAS')).bytes, new Uint8Array([9]));
});
test('a failed starter download writes no partial starter files', async () => {
  const root = new Directory();
  await assert.rejects(seedFiles(root, [{path: 'book/lab/A.PAS', url: 'good'}, {path: 'book/lab/B.PAS', url: 'bad'}], async url => {
    if (url === 'bad') throw new Error('Network'); return new Uint8Array([1]);
  }));
  assert.equal(root.entries.size, 0);
});
test('backup validates binary data and rejects traversal, duplicates, conflicts and system paths', () => {
  const bytes = new Uint8Array([0, 127, 128, 255]);
  const valid = {format: 'zeal-pascal-book-v1', files: [{path: 'work/LAB.COM', base64: encodeBytes(bytes)}]};
  assert.deepEqual(validateBackup(valid)[0].bytes, bytes);
  for (const path of ['work/../bin/a', 'work//a', '/work/a', 'bin/a', 'work/a/b/../../c', 'work/this-is-too-long-a-name']) {
    assert.throws(() => validateBackup({...valid, files: [{path, base64: ''}]}));
  }
  assert.throws(() => validateBackup({...valid, files: [...valid.files, ...valid.files]}));
  assert.throws(() => validateBackup({...valid, files: [{path: 'work/a', base64: ''}, {path: 'work/a/b', base64: ''}]}));
  assert.throws(() => validateBackup({...valid, files: [{path: 'work/a', base64: '%bad'}]}));
});

test('backup accepts personal files saved at the H: root without permitting system paths', () => {
  const value = {format: 'zeal-pascal-book-v1', files: [{path: 'sl.pas', base64: encodeBytes(new Uint8Array([65]))}]};
  assert.equal(validateBackup(value)[0].path, 'sl.pas');
  for (const path of ['lib/zpascal.zpl', 'bin/zpascal.bin', '.state.json']) {
    assert.throws(() => validateBackup({...value, files: [{path, base64: ''}]}));
  }
});

test('explicit starter refresh replaces only supplied files and leaves personal files intact', async () => {
  const root = new Directory();
  await writeFile(root, 'book/lab/START.PAS', new Uint8Array([9]));
  await writeFile(root, 'book/lab/MINE.PAS', new Uint8Array([7]));
  await writeFile(root, 'work/LAB.PAS', new Uint8Array([8]));
  const files = [{path: 'book/lab/START.PAS', url: 'starter'}];
  assert.deepEqual(await seedFiles(root, files, async () => new Uint8Array([1])), ['book/lab/START.PAS']);
  assert.deepEqual(await seedFiles(root, files, async () => new Uint8Array([1]), {overwrite: true}), []);
  assert.deepEqual((await (await directoryAt(root, 'book/lab')).getFileHandle('START.PAS')).bytes, new Uint8Array([1]));
  assert.deepEqual((await (await directoryAt(root, 'book/lab')).getFileHandle('MINE.PAS')).bytes, new Uint8Array([7]));
  assert.deepEqual((await (await directoryAt(root, 'work')).getFileHandle('LAB.PAS')).bytes, new Uint8Array([8]));
});

test('failed refresh download keeps the existing starter bytes', async () => {
  const root = new Directory();
  await writeFile(root, 'book/lab/START.PAS', new Uint8Array([9]));
  await assert.rejects(seedFiles(root, [{path: 'book/lab/START.PAS', url: 'good'}, {path: 'book/lab/HELLO.PAS', url: 'bad'}], async url => {
    if (url === 'bad') throw new Error('Network'); return new Uint8Array([1]);
  }, {overwrite: true}));
  assert.deepEqual((await (await directoryAt(root, 'book/lab')).getFileHandle('START.PAS')).bytes, new Uint8Array([9]));
});


test('reset removes only book storage under the native image lock, including orphan files', async () => {
  const root = new Directory(), namespace = 'b'.repeat(64);
  const h = await root.getDirectoryHandle('zeal-pascal-book', {create: true});
  await h.getFileHandle('personal', {create: true});
  const images = await root.getDirectoryHandle('zeal-native', {create: true});
  await images.getDirectoryHandle(namespace, {create: true});
  await images.getDirectoryHandle('another-project', {create: true});
  await root.getDirectoryHandle('unrelated-app', {create: true});
  const locks = {request: async (name, options, action) => {
    assert.equal(name, `zeal-images:opfs:${namespace}`);
    assert.equal(options.ifAvailable, true);
    return action({});
  }};
  await clearBookStorage(root, namespace, locks);
  assert.equal(root.entries.has('zeal-pascal-book'), false);
  assert.equal(images.entries.has(namespace), false);
  assert.equal(images.entries.has('another-project'), true);
  assert.equal(root.entries.has('unrelated-app'), true);
  await clearBookStorage(root, namespace, locks); // Missing data is already reset.
  await assert.rejects(clearBookStorage(root, '../', locks), /Invalid/);
});

test('reset refuses an occupied image lock and surfaces deletion failures for retry', async () => {
  const root = new Directory(), namespace = 'c'.repeat(64);
  await root.getDirectoryHandle('zeal-pascal-book', {create: true});
  const images = await root.getDirectoryHandle('zeal-native', {create: true});
  await images.getDirectoryHandle(namespace, {create: true});
  await assert.rejects(clearBookStorage(root, namespace, {request: (n, o, action) => action(null)}), /in use/);
  assert.equal(root.entries.has('zeal-pascal-book'), true);
  images.removeEntry = async () => { throw new DOMException('Busy file', 'NoModificationAllowedError'); };
  await assert.rejects(clearBookStorage(root, namespace, {request: (n, o, action) => action({})}), /Busy file/);
  assert.equal(root.entries.has('zeal-pascal-book'), true, 'disk deletion failure keeps H: intact');
  images.removeEntry = Directory.prototype.removeEntry;
  const remove = root.removeEntry;
  root.removeEntry = async () => { throw new DOMException('H: unavailable', 'UnknownError'); };
  await assert.rejects(clearBookStorage(root, namespace, {request: (n, o, action) => action({})}), /H: unavailable/);
  assert.equal(images.entries.has(namespace), false, 'partial reset is reported rather than called success');
  root.removeEntry = remove;
  await clearBookStorage(root, namespace, {request: (n, o, action) => action({})});
  assert.equal(root.entries.has('zeal-pascal-book'), false, 'partial reset can be retried');
});
