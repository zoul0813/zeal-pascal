// Book-owned file preparation around the emulator's supported directory-handle HostFS.
export async function directoryAt(root, path, create = false) {
  let directory = root;
  for (const part of path.split('/').filter(Boolean)) {
    if (!/^[A-Za-z0-9_.-]{1,16}$/.test(part) || part === '.' || part === '..') throw new Error('Invalid workspace path');
    directory = await directory.getDirectoryHandle(part, {create});
  }
  return directory;
}
export async function writeFile(root, path, bytes, {overwrite = false} = {}) {
  const parts = path.split('/');
  const name = parts.pop();
  if (!/^[A-Za-z0-9_.-]{1,16}$/.test(name) || name === '.' || name === '..') throw new Error('Invalid filename');
  const parent = await directoryAt(root, parts.join('/'), true);
  if (!overwrite) {
    try { await parent.getFileHandle(name); return false; }
    catch (error) { if (error.name !== 'NotFoundError') throw error; }
  }
  const handle = await parent.getFileHandle(name, {create: true});
  const writer = await handle.createWritable();
  try { await writer.write(bytes); await writer.close(); }
  catch (error) { await writer.abort().catch(() => {}); throw error; }
  return true;
}
export async function seedFiles(root, files, fetchBytes) {
  // Fetch every input first: a failed request must not leave an apparently complete starter.
  const inputs = await Promise.all(files.map(async file => ({...file, bytes: await fetchBytes(file.url)})));
  for (const file of inputs) await writeFile(root, file.path, file.bytes);
}
export async function listFiles(root, prefix = '') {
  const files = [];
  for await (const entry of root.values()) {
    const path = prefix + entry.name;
    if (entry.kind === 'directory') files.push(...await listFiles(entry, path + '/'));
    else files.push({path, handle: entry});
  }
  return files.sort((a, b) => a.path.localeCompare(b.path));
}
export function validateBackup(value) {
  if (value?.format !== 'zeal-pascal-book-v1' || !Array.isArray(value.files) || value.files.length > 500) throw new Error('Not a supported workspace backup');
  let total = 0;
  const paths = new Set();
  const files = value.files.map(file => {
    if (typeof file.path !== 'string' || file.path.length > 120 || !/^(book|work)\//.test(file.path)) throw new Error('Backup has an invalid path');
    const parts = file.path.split('/');
    if (parts.some(part => !/^[A-Za-z0-9_.-]{1,16}$/.test(part) || part === '.' || part === '..') || paths.has(file.path)) throw new Error('Backup has an unsafe or duplicate path');
    paths.add(file.path);
    if (typeof file.base64 !== 'string' || file.base64.length > 12_000_000 || !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(file.base64)) throw new Error('Backup contains invalid file data');
    const binary = atob(file.base64);
    total += binary.length;
    if (total > 32_000_000) throw new Error('Backup exceeds the 32 MB workspace limit');
    return {path: file.path, bytes: Uint8Array.from(binary, c => c.charCodeAt(0))};
  });
  // A file may not also be the parent directory of another file.
  for (const path of paths) {
    const parts = path.split('/');
    parts.pop();
    while (parts.length) {
      if (paths.has(parts.join('/'))) throw new Error('Backup has conflicting file and directory paths');
      parts.pop();
    }
  }
  return files;
}
export function encodeBytes(bytes) {
  let binary = '';
  for (let at = 0; at < bytes.length; at += 8192) binary += String.fromCharCode(...bytes.subarray(at, at + 8192));
  return btoa(binary);
}
