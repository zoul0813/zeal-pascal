import {directoryAt, writeFile, seedFiles, listFiles, validateBackup, encodeBytes, clearBookStorage} from './workspace.mjs';

const panel = document.querySelector('#emulator-panel');
const status = document.querySelector('#emulator-status');
const canvas = document.querySelector('#canvas');
const base = new URL(panel.dataset.base, location.origin);
const asset = path => new URL(path, base).href;
const startButton = document.querySelector('#start-emulator');
const storageStatus = document.querySelector('#storage-status');
const resetButton = document.querySelector('#reset-workspace');
const focusOverlay = document.querySelector('#focus-emulator');
const keyboardStatus = document.querySelector('#keyboard-status');
const mouseInput = document.querySelector('#enable-mouse');
const screen = document.querySelector('.screen-wrap');
const emulatorAsset = path => {
  const url = new URL(path, base);
  url.searchParams.set('v', panel.dataset.emulatorVersion);
  return url.href;
};
let emulator, root, running = false, busy = false, releaseLock;
const setStatus = message => { status.textContent = message; };

let windowFocused = document.hasFocus();
let bookFocus = document.querySelector('#main');
let shortcutDown = false;
const heldKeys = new Map();
function updateKeyboardFocus() {
  const focused = running && windowFocused && !document.hidden && document.activeElement === canvas;
  focusOverlay.hidden = !running || focused;
  keyboardStatus.textContent = !running ? 'Keyboard inactive' : focused
    ? 'Keyboard active · Ctrl+Shift+Space switches to the book' : 'Keyboard inactive · click or press Ctrl+Shift+Space to type';
  keyboardStatus.dataset.active = String(focused);
}
document.addEventListener('focusin', event => {
  if (document.querySelector('#main').contains(event.target)) bookFocus = event.target;
});
function releaseCanvasKeys() {
  // The native keyboard listens on the canvas. Release keys it actually received
  // before moving focus so later keyups on the book cannot leave guest keys held.
  for (const event of [...heldKeys.values()]) {
    canvas.dispatchEvent(new KeyboardEvent('keyup', {key: event.key, code: event.code, keyCode: event.keyCode, which: event.which}));
  }
  heldKeys.clear();
}
document.addEventListener('keydown', event => {
  const space = event.code === 'Space' || event.key === ' ';
  const shortcut = space && event.ctrlKey && event.shiftKey && !event.altKey && !event.metaKey;
  if (!space || panel.hidden || !running || (!shortcut && !shortcutDown)) return;
  event.preventDefault();
  event.stopImmediatePropagation();
  if (event.repeat || shortcutDown) return;
  shortcutDown = true;
  if (document.activeElement === canvas) {
    releaseCanvasKeys();
    (bookFocus?.isConnected ? bookFocus : document.querySelector('#main')).focus({preventScroll: true});
    if (document.activeElement === canvas) document.querySelector('#main').focus({preventScroll: true});
  } else canvas.focus({preventScroll: true});
}, true);
document.addEventListener('keyup', event => {
  if (event.code === 'Space' || event.key === ' ') shortcutDown = false;
}, true);
canvas.addEventListener('keyup', event => heldKeys.delete(event.code || event.key));
canvas.addEventListener('focus', updateKeyboardFocus);
canvas.addEventListener('blur', () => { releaseCanvasKeys(); updateKeyboardFocus(); });
window.addEventListener('focus', () => { windowFocused = true; updateKeyboardFocus(); });
window.addEventListener('blur', () => { windowFocused = false; shortcutDown = false; updateKeyboardFocus(); });
document.addEventListener('visibilitychange', () => { if (document.hidden) shortcutDown = false; updateKeyboardFocus(); });
focusOverlay.addEventListener('click', () => canvas.focus());
updateKeyboardFocus();

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
screen.addEventListener('click', () => { if (!mouseInput.checked) canvas.focus(); });
function updateMouseInput() { canvas.dataset.mouseEnabled = String(mouseInput.checked); }
mouseInput.addEventListener('change', updateMouseInput);
updateMouseInput();
// Book shortcuts are scoped to the emulated keyboard; page navigation keeps native behavior.
canvas.addEventListener('keydown', event => {
  heldKeys.set(event.code || event.key, event);
  if (event.ctrlKey || event.metaKey || event.key === 'Tab' || event.key.startsWith('Arrow') || event.key === ' ') event.preventDefault();
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
const bookImages = ['romdisk', 'eeprom', 'tf'];
const pendingImages = new Set();
function showImageUpdates() {
  storageStatus.textContent = pendingImages.size
    ? 'A Zeal update is available. Open More options to install it.' : '';
}
async function prepareStarters(overwrite = false) {
  const outdated = await seedFiles(root, lessonFiles('book/lab'), fetchBytes, {overwrite});
  const message = document.querySelector('#starter-status');
  message.textContent = outdated.length
    ? 'Your lesson examples differ from the book’s versions. To keep your changes, use A (Save As) in Pascal to save them in H:/work/ before refreshing.'
    : '';
}
async function initializeWorkspace() {
  if (!navigator.storage?.getDirectory) throw new Error('Private browser filesystem is unavailable. Use a current desktop browser over HTTPS or localhost.');
  const originRoot = await navigator.storage.getDirectory();
  root = await originRoot.getDirectoryHandle('zeal-pascal-book', {create: true});
  // H: contains starters and reader files only. Keep all legacy files intact;
  // the startup PATH no longer uses old H:/bin installations.
  await directoryAt(root, 'work', true);
  await prepareStarters();
}
async function exclusive(operation) {
  if (busy) return;
  busy = true;
  const controls = [...panel.querySelectorAll('.workspace-tools button, .workspace-tools input, #start-emulator')];
  controls.forEach(control => { control.disabled = true; });
  try { await operation(); }
  catch (error) { console.error(error); setStatus(`Could not complete: ${error.message}`); }
  finally { busy = false; controls.forEach(control => { control.disabled = !running; }); startButton.disabled = !running && !!emulator?.module; resetButton.disabled = false; updateKeyboardFocus(); }
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
          if (!bookImages.includes(update.name)) return;
          pendingImages.add(update.name);
          showImageUpdates();
        },
        onStorageError(error) {
          setStatus(`Zeal reported a problem: ${error.message}`);
        },
        print: text => console.info('[Zeal]', text),
        printErr: text => { console.error('[Zeal]', text); if (/error|fail/i.test(text)) setStatus(`Emulator: ${text}`); },
      });
      else emulator.setHostFS(hostfs);
      await emulator.start();
      running = true;
      showImageUpdates();
      startButton.hidden = true;
      document.querySelector('#audio').disabled = false;
      setStatus('Zeal is ready.');
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
document.querySelector('#update-starters').addEventListener('click', () => exclusive(async () => {
  if (!window.confirm('Replace HELLO.PAS, START.PAS and SOLUTION.PAS in H:/book/lab/ with the book’s versions? Any edits to these three files will be replaced. Save personal versions as new H:/work/ files first. All other H: files are kept.')) return;
  await prepareStarters(true);
  setStatus('Lesson examples refreshed. Your H:/work/ files were kept. Reopen an example in Pascal to see its new contents.');
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
  setStatus('Backup downloaded. It contains saved H: files only; unsaved editor text and tool disks are not included.');
}));
document.querySelector('#import-work').addEventListener('change', event => exclusive(async () => {
  const file = event.target.files[0];
  event.target.value = '';
  if (!file) return;
  if (file.size > 45_000_000) throw new Error('Backup is too large');
  const files = validateBackup(JSON.parse(await file.text()));
  const folder = `work/in-${Date.now().toString(36)}`;
  for (const entry of files) await writeFile(root, `${folder}/${entry.path}`, entry.bytes);
  setStatus(`Restored into H:/${folder}/. Your existing files were not changed. Use W in Pascal to open a restored file.`);
}));
window.addEventListener('beforeunload', event => {
  if (emulator?.module) { event.preventDefault(); event.returnValue = ''; }
});

document.querySelector('#update-images').addEventListener('click', () => exclusive(async () => {
  if (!window.confirm('Update Zeal and restart? Unsaved editor text will be lost. The tools and system disks (T:, B:, A:) are replaced completely, including any custom files or settings there. Any files saved on T: will be lost: move them to H:/work/ first. Your H: files are kept. Save in Pascal before continuing.')) return;
  try {
    await emulator.applyImageUpdates(bookImages);
    for (const name of bookImages) pendingImages.delete(name);
    showImageUpdates();
    setStatus('Zeal updated and restarted. Your H: files were kept. Type zpascal, then use W to reopen your program.');
  } finally {
    running = !!emulator.module && !emulator.startupError;
    startButton.hidden = running;
    document.querySelector('#audio').disabled = !running;
  }
}));

resetButton.addEventListener('click', () => exclusive(async () => {
  if (!window.confirm('Erase this book and start over? ALL H: files (including your programs), saved tool disks (T:, B:, A:), and unsaved editor text will be lost. This cannot be undone. Save in Pascal and choose Download backup first if you want to keep your programs; the backup does not include disks. Other projects are not affected.')) return;
  if (window.prompt('Type RESET to erase this book’s saved storage and start fresh.') !== 'RESET') return;
  if (!await acquireLock()) throw new Error('Another tab has this workspace open. Close that workbench tab before resetting.');
  // Do not delete backing files until native shutdown has closed their handles.
  try { await emulator?.exit(); }
  finally {
    running = !!emulator?.module && !emulator.startupError;
    startButton.hidden = running;
    document.querySelector('#audio').disabled = !running;
    updateKeyboardFocus();
  }
  running = false;
  emulator = null;
  root = null;
  startButton.hidden = false;
  document.querySelector('#audio').disabled = true;
  updateKeyboardFocus();
  if (!window.IndexedDBStore) await loadScript('emulator/libnative/indexeddb.js');
  if (!window.DiskImageStore) await loadScript('emulator/libnative/disk-images.js');
  const store = new DiskImageStore({storage: 'opfs', storageNamespace: base.pathname});
  // Reuse the native store’s namespace hashing, including its explicit-name domain.
  const namespace = await store.hash(`storageNamespace:${JSON.stringify(base.pathname)}`);
  try {
    await clearBookStorage(await navigator.storage.getDirectory(), namespace, navigator.locks);
  } catch (error) {
    throw new Error(`Reset failed. ${error.message} Retry reset before starting again.`, {cause: error});
  }
  // A reload discards every old HostFS/module reference. The next Open Zeal seeds
  // current images and starters, rather than reusing handles to deleted files.
  location.reload();
}));
