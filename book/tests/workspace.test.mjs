import test from 'node:test';
import assert from 'node:assert/strict';
import {directoryAt, writeFile, seedFiles, validateBackup, encodeBytes} from '../static/js/workspace.mjs';
class Directory {
  entries = new Map();
  async getDirectoryHandle(name, {create = false} = {}) {
    if (!this.entries.has(name) && create) this.entries.set(name, new Directory());
    const value = this.entries.get(name);
    if (!value) throw new DOMException('Missing', 'NotFoundError');
    if (!(value instanceof Directory)) throw new DOMException('File', 'TypeMismatchError');
    return value;
  }
  async getFileHandle(name, {create = false} = {}) {
    if (!this.entries.has(name) && create) this.entries.set(name, {bytes: new Uint8Array(), async createWritable() { return {write: async bytes => { this.bytes = bytes; }, close: async () => {}, abort: async () => {}}; }});
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
