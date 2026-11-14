import {directoryAt, writeFile, seedFiles, listFiles, validateBackup, encodeBytes} from './workspace.mjs';

const panel = document.querySelector('#emulator-panel');
const status = document.querySelector('#emulator-status');
const canvas = document.querySelector('#canvas');
const base = new URL(panel.dataset.base, location.origin);
const asset = path => new URL(path, base).href;
const startButton = document.querySelector('#start-emulator');
const storageStatus = document.querySelector('#storage-status');
const emulatorAsset = path => {
  const url = new URL(path, base);
  url.searchParams.set('v', panel.dataset.emulatorVersion);
  return url.href;
};
let emulator, root, running = false, busy = false, releaseLock;
const setStatus = message => { status.textContent = message; };

function showEmulator(show) {
  panel.hidden = !show;
  document.body.classList.toggle('workbench-open', show);
  document.querySelectorAll('[data-open-emulator]').forEach(button => button.setAttribute('aria-expanded', String(show)));
  if (show) {
    if (running) canvas.focus();
    else { panel.focus(); if (!busy) startEmulator(); }
  } else document.querySelector('[data-open-emulator]').focus();
}
document.querySelectorAll('[data-open-emulator]').forEach(button => button.addEventListener('click', () => showEmulator(panel.hidden)));
document.querySelector('#close-emulator').addEventListener('click', () => showEmulator(false));
document.querySelector('#expand-emulator').addEventListener('click', event => {
  const expanded = panel.classList.toggle('expanded');
  event.currentTarget.setAttribute('aria-pressed', String(expanded));
  event.currentTarget.textContent = expanded ? 'Reduce' : 'Expand';
});
canvas.addEventListener('click', () => canvas.focus());
// Book shortcuts are scoped to the emulated keyboard; page navigation keeps native behavior.
canvas.addEventListener('keydown', event => {
  if (event.key === 'F8') {
    event.preventDefault(); event.stopImmediatePropagation();
    document.querySelector('#close-emulator').focus();
  } else if (event.ctrlKey || event.metaKey || event.key === 'Tab' || event.key.startsWith('Arrow') || event.key === ' ') event.preventDefault();
}, true);
canvas.addEventListener('contextmenu', event => event.preventDefault());

async function fetchBytes(url) {
  const response = await fetch(url, {cache: 'no-cache'});
  if (!response.ok) throw new Error(`Cannot load ${new URL(url).pathname} (${response.status}).`);
  return new Uint8Array(await response.arrayBuffer());
}
function loadScript(path) {
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = emulatorAsset(path);
    script.onload = resolve;
    script.onerror = () => { script.remove(); reject(new Error('Emulator assets could not be loaded. Reload and try again.')); };
    document.head.append(script);
  });
}
async function acquireLock() {
  if (releaseLock) return true;
  if (!navigator.locks) throw new Error('This pilot needs a desktop browser with Web Locks and private filesystem support. Try current Chrome or Edge.');
  return new Promise((resolve, reject) => {
    navigator.locks.request('zeal-pascal-book-workspace', {ifAvailable: true}, async lock => {
      if (!lock) { resolve(false); return; }
      await new Promise(release => { releaseLock = release; resolve(true); });
    }).catch(reject);
  });
}
const lessonFiles = folder => ['HELLO.PAS', 'START.PAS', 'SOLUTION.PAS'].map(name => ({path: `${folder}/${name}`, url: asset(`code/lab/${name}`)}));
const imageNames = {tf: 'TF tools (T:)', eeprom: 'startup EEPROM (B:)', romdisk: 'boot ROM (A:)'};
const pendingImages = new Set();
let storageFailure = null;
function showImageUpdates() {
  if (storageFailure) { storageStatus.textContent = `Storage error: ${storageFailure.message}. Keep this tab open and back up your work.`; return; }
  storageStatus.textContent = pendingImages.size
    ? `New disk images available: ${[...pendingImages].map(name => imageNames[name]).join(', ')}. Use Disk images & updates below to retrieve them. Your saved disks have been kept.`
    : 'Completed disk writes persist automatically. Save personal programs to H:/work/ so disk updates cannot replace them.';
}
async function prepareStarters(overwrite = false) {
  const outdated = await seedFiles(root, lessonFiles('book/lab'), fetchBytes, {overwrite});
  document.querySelector('#starter-status').textContent = outdated.length
    ? 'Updated or modified starter files detected. Update lesson starters to retrieve the published versions. Save personal copies to H:/work/ first.'
    : 'Lesson starters are current. Save your own versions as new files in H:/work/.';
}
async function updateFolders() {
  document.querySelector('#lesson-path').textContent = 'H:/book/lab';
  const list = document.querySelector('#workspace-folders');
  list.replaceChildren();
  for (const parentName of ['book', 'work']) {
    const parent = await directoryAt(root, parentName, true);
    for await (const entry of parent.values()) {
      if (entry.kind === 'directory') {
        const item = document.createElement('li'); item.textContent = `H:/${parentName}/${entry.name}`; list.append(item);
      }
    }
  }
}
async function initializeWorkspace() {
  if (!navigator.storage?.getDirectory) throw new Error('Private browser filesystem is unavailable. Use a current desktop browser over HTTPS or localhost.');
  const originRoot = await navigator.storage.getDirectory();
  root = await originRoot.getDirectoryHandle('zeal-pascal-book', {create: true});
  // H: contains starters and reader files only. Keep all legacy files intact;
  // the startup PATH no longer uses old H:/bin installations.
  await directoryAt(root, 'work', true);
  await prepareStarters();
  await updateFolders();
}
async function exclusive(operation) {
  if (busy) return;
  busy = true;
  const controls = [...panel.querySelectorAll('.workspace-tools button, .workspace-tools input, #start-emulator')];
  controls.forEach(control => { control.disabled = true; });
  try { await operation(); }
  catch (error) { console.error(error); setStatus(`Could not complete: ${error.message}`); }
  finally { busy = false; controls.forEach(control => { control.disabled = !running; }); startButton.disabled = !running && !!emulator?.module; }
}
function startEmulator() {
  return exclusive(async () => {
    if (running) { canvas.focus(); return; }
    if (!await acquireLock()) throw new Error('Another tab has this workspace open. Close that workbench tab and try again.');
    try {
      setStatus('Preparing your browser workspace and community tools…');
      await initializeWorkspace();
      if (!window.NativeModule) await loadScript('emulator/zeal.elf.js');
      if (!window.IndexedDBStore) await loadScript('emulator/libnative/indexeddb.js');
      if (!window.DiskImageStore) await loadScript('emulator/libnative/disk-images.js');
      if (!window.HostFS) await loadScript('emulator/libnative/hostfs.js');
      if (!window.ZealNative) await loadScript('emulator/libnative/zeal-native.js');
      const hostfs = new HostFS(root, {onError(operation, error) {
        // HostFS returns guest errors through the protocol; this callback reports infrastructure failures.
        setStatus(`File operation failed: ${error.message}. Save again or download a backup before leaving.`);
      }});
      if (!emulator) emulator = new ZealNative({
        canvas,
        moduleFactory: options => NativeModule({...options, locateFile: name => emulatorAsset(`emulator/${name}`)}),
        romdisk: asset('media/zshell.img'), tf: asset('media/tf.img'), eeprom: asset('media/eeprom.img'), hostfs,
        imageStorage: 'opfs',
        storageNamespace: base.pathname,
        diskImages: {romdisk: {key: 'book-rom'}, tf: {key: 'book-tf'}, eeprom: {key: 'book-eeprom'}},
        onImageUpdateAvailable(update) {
          if (!Object.hasOwn(imageNames, update.name)) return;
          pendingImages.add(update.name);
          showImageUpdates();
          document.querySelector('#image-updates').open = true;
        },
        onStorageError(error) {
          storageFailure = error;
          showImageUpdates();
        },
        print: text => console.info('[Zeal]', text),
        printErr: text => { console.error('[Zeal]', text); if (/error|fail/i.test(text)) setStatus(`Emulator: ${text}`); },
      });
      else emulator.setHostFS(hostfs);
      storageFailure = null;
      await emulator.start();
      running = true;
      showImageUpdates();
      document.querySelector('#disk-store-path').textContent = `zeal-native/${emulator.diskImages.namespace}/`;
      startButton.hidden = true;
      document.querySelector('#audio').disabled = false;
      setStatus('Zeal is running. At ZShell, type zpascal to launch Pascal. Use W or Save As to choose a new H:/work/ file before editing starters.');
    } catch (error) {
      // A failed native teardown can retain a live module and its image handles.
      // Keep the matching H: ownership until that runtime is gone.
      if (!emulator?.module) { releaseLock?.(); releaseLock = null; }
      startButton.hidden = false;
      throw error;
    }
  });
}
startButton.addEventListener('click', startEmulator);
document.querySelector('#audio').addEventListener('click', () => emulator?.resumeAudio().catch(error => setStatus(error.message)));
document.querySelector('#load-all').addEventListener('click', () => exclusive(async () => {
  await prepareStarters();
  await updateFolders();
  setStatus('All pilot starters are in H:/book/lab. Use Save As to put your version in H:/work/ before editing.');
}));
document.querySelector('#update-starters').addEventListener('click', () => exclusive(async () => {
  if (!window.confirm('Replace HELLO.PAS, START.PAS and SOLUTION.PAS in H:/book/lab with the latest published starters? Any edits to these three files will be replaced. Save personal versions as new H:/work/ files first. All other H: files are kept.')) return;
  await prepareStarters(true);
  await updateFolders();
  setStatus('Latest starters retrieved in H:/book/lab. Your other H: files were kept. Reopen a starter in Pascal to load its updated text; editor RAM is unchanged.');
}));
document.querySelector('#export-work').addEventListener('click', () => exclusive(async () => {
  const files = [];
  for (const file of await listFiles(root)) {
    if (!['bin', 'lib', '.state.json'].includes(file.path.split('/')[0])) {
      const bytes = new Uint8Array(await (await file.handle.getFile()).arrayBuffer());
      files.push({path: file.path, base64: encodeBytes(bytes)});
    }
  }
  const value = {format: 'zeal-pascal-book-v1', files};
  validateBackup(value);
  const url = URL.createObjectURL(new Blob([JSON.stringify(value)], {type: 'application/json'}));
  const link = document.createElement('a'); link.href = url; link.download = 'zeal-pascal-workspace.json'; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 30000);
  setStatus('Downloaded personal H: files, including files saved at the drive root. Source still in editor RAM is not included; press S in Pascal before making a backup.');
}));
document.querySelector('#import-work').addEventListener('change', event => exclusive(async () => {
  const file = event.target.files[0];
  event.target.value = '';
  if (!file) return;
  if (file.size > 45_000_000) throw new Error('Backup is too large');
  const files = validateBackup(JSON.parse(await file.text()));
  const folder = `work/in-${Date.now().toString(36)}`;
  for (const entry of files) await writeFile(root, `${folder}/${entry.path}`, entry.bytes);
  await updateFolders();
  setStatus(`Imported into H:/${folder}. Existing files were kept. Open the imported source using W in Pascal.`);
}));
window.addEventListener('beforeunload', event => {
  if (emulator?.module) { event.preventDefault(); event.returnValue = ''; }
});

document.querySelector('#check-images').addEventListener('click', () => exclusive(async () => {
  pendingImages.clear();
  try { await emulator.checkImageUpdates(); }
  finally { showImageUpdates(); }
}));
document.querySelector('#update-images').addEventListener('click', () => exclusive(async () => {
  const selected = Object.keys(imageNames).filter(name => document.querySelector(`#image-${name}`).checked);
  if (!selected.length) { setStatus('Select at least one disk image to update.'); return; }
  const migrationWarning = selected.includes('tf') ? ' Before updating T:, open T:/src/pascal/UNTITLED.pas (if present), and any other personal T: files, with W in Pascal and use A (Save As) to copy them to H:/work/. ' : selected.includes('eeprom') ? ' If your saved TF is from an older book setup, select TF too so Pascal is installed on T:. ' : '';
  if (!window.confirm(`Retrieve the latest ${selected.map(name => imageNames[name]).join(', ')}? This replaces ALL contents and custom settings on the selected images and restarts Zeal, discarding unsaved editor RAM. H: files are kept. Save personal work to H:/work/ first. H: backups do not contain disk-image changes.${migrationWarning}Continue?`)) return;
  try {
    storageFailure = null;
    await emulator.applyImageUpdates(selected);
    for (const name of selected) pendingImages.delete(name);
    showImageUpdates();
    setStatus('Selected disk images updated and Zeal restarted. Your H: files were kept. Type zpascal, then use W to open your H:/work/ file.');
  } finally {
    running = !!emulator.module && !emulator.startupError;
    startButton.hidden = running;
    document.querySelector('#audio').disabled = !running;
  }
}));
