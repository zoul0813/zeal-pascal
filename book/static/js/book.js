import {directoryAt, writeFile, seedFiles, listFiles, validateBackup, encodeBytes} from './workspace.mjs';

const panel = document.querySelector('#emulator-panel');
const status = document.querySelector('#emulator-status');
const canvas = document.querySelector('#canvas');
const base = new URL(panel.dataset.base, location.origin);
const asset = path => new URL(path, base).href;
const startButton = document.querySelector('#start-emulator');
let emulator, root, busy = false, active = 'book/lab', releaseLock;
const setStatus = message => { status.textContent = message; };

function showEmulator(show) {
  panel.hidden = !show;
  document.body.classList.toggle('workbench-open', show);
  document.querySelectorAll('[data-open-emulator]').forEach(button => button.setAttribute('aria-expanded', String(show)));
  if (show) startButton.focus();
  else document.querySelector('[data-open-emulator]').focus();
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
  const response = await fetch(url);
  if (!response.ok) throw new Error(`Cannot load ${new URL(url).pathname} (${response.status}).`);
  return new Uint8Array(await response.arrayBuffer());
}
function loadScript(path) {
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = asset(path);
    script.onload = resolve;
    script.onerror = () => { script.remove(); reject(new Error('Emulator assets could not be loaded. Reload and try again.')); };
    document.head.append(script);
  });
}
async function acquireLock() {
  if (!navigator.locks) throw new Error('This pilot needs a desktop browser with Web Locks and private filesystem support. Try current Chrome or Edge.');
  return new Promise((resolve, reject) => {
    navigator.locks.request('zeal-pascal-book-workspace', {ifAvailable: true}, async lock => {
      if (!lock) { resolve(false); return; }
      await new Promise(release => { releaseLock = release; resolve(true); });
    }).catch(reject);
  });
}
const lessonFiles = folder => ['HELLO.PAS', 'START.PAS', 'SOLUTION.PAS'].map(name => ({path: `${folder}/${name}`, url: asset(`code/lab/${name}`)}));
async function saveState() {
  await writeFile(root, '.state.json', new TextEncoder().encode(JSON.stringify({active})), {overwrite: true});
}
async function updateFolders() {
  document.querySelector('#lesson-path').textContent = `H:/${active}`;
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
  try {
    const state = JSON.parse(await (await (await root.getFileHandle('.state.json')).getFile()).text());
    if (typeof state.active === 'string' && /^book\/lab(?:-[a-z0-9]+)?$/.test(state.active)) active = state.active;
  } catch (error) { if (error.name !== 'NotFoundError') throw new Error('Workspace state could not be read. Your files were kept; export them before clearing browser data.'); }
  const systemFiles = ['zpascal.bin', 'zpascal.zpl', 'SYSFILE.ZPU', 'CRT.ZPU', 'ZVB.ZPU', 'ZOS.ZPU', 'ZVB.LIC', 'ZVB.NTC'].map(name => ({
    path: `${name === 'zpascal.bin' ? 'bin' : 'lib'}/${name}`, url: asset(`media/pascal/${name}`),
  }));
  await seedFiles(root, systemFiles, fetchBytes);
  await directoryAt(root, 'work', true);
  await seedFiles(root, lessonFiles(active), fetchBytes);
  await saveState();
  await updateFolders();
}
async function exclusive(operation) {
  if (busy) return;
  busy = true;
  const controls = [...panel.querySelectorAll('.workspace-tools button, .workspace-tools input, #start-emulator')];
  controls.forEach(control => { control.disabled = true; });
  try { await operation(); }
  catch (error) { console.error(error); setStatus(`Could not complete: ${error.message} Your existing files have not been deleted.`); }
  finally { busy = false; controls.forEach(control => { control.disabled = !emulator?.module; }); if (!emulator?.module) startButton.disabled = false; }
}
startButton.addEventListener('click', () => exclusive(async () => {
  if (emulator?.module) { canvas.focus(); return; }
  if (!await acquireLock()) throw new Error('Another tab has this workspace open. Close that workbench tab and try again.');
  try {
    setStatus('Preparing your browser workspace and community tools…');
    await initializeWorkspace();
    if (!window.NativeModule) await loadScript('emulator/zeal.elf.js');
    if (!window.HostFS) await loadScript('emulator/libnative/hostfs.js');
    if (!window.ZealNative) await loadScript('emulator/libnative/zeal-native.js');
    const hostfs = new HostFS(root, {onError(operation, error) { setStatus(`File operation failed: ${error.message}. Save again or download a backup before leaving.`); }});
    emulator = new ZealNative({
      canvas,
      moduleFactory: options => NativeModule({...options, locateFile: name => asset(`emulator/${name}`)}),
      romdisk: asset('media/zshell.img'), tf: asset('media/tf.img'), hostfs,
      arguments: ['-r', '/roms/default.img', '-t', '/roms/tf.img'],
      print: text => console.info('[Zeal]', text),
      printErr: text => { console.error('[Zeal]', text); if (/error|fail/i.test(text)) setStatus(`Emulator: ${text}`); },
    });
    await emulator.start();
    startButton.textContent = 'Focus the Zeal';
    document.querySelector('#audio').disabled = false;
    setStatus('Zeal is running. At ZShell, launch Pascal with the command below. Save source on H: with S in Pascal.');
  } catch (error) { releaseLock?.(); releaseLock = null; throw error; }
}));
document.querySelector('#audio').addEventListener('click', () => emulator?.resumeAudio().catch(error => setStatus(error.message)));
document.querySelector('#load-all').addEventListener('click', () => exclusive(async () => {
  await seedFiles(root, lessonFiles('book/lab'), fetchBytes);
  await updateFolders();
  setStatus('All pilot book code is available in H:/book/lab. Existing files were kept. The pilot currently contains one lesson.');
}));
document.querySelector('#fresh-lesson').addEventListener('click', () => exclusive(async () => {
  const next = `book/lab-${Date.now().toString(36)}`;
  await seedFiles(root, lessonFiles(next), fetchBytes);
  active = next;
  await saveState(); await updateFolders();
  setStatus(`Fresh files are in H:/${active}. Earlier files and editor RAM are unchanged. Save your current editor buffer before opening another file.`);
}));
document.querySelector('#export-work').addEventListener('click', () => exclusive(async () => {
  const files = [];
  for (const name of ['book', 'work']) {
    for (const file of await listFiles(await directoryAt(root, name, true), name + '/')) {
      const bytes = new Uint8Array(await (await file.handle.getFile()).arrayBuffer());
      files.push({path: file.path, base64: encodeBytes(bytes)});
    }
  }
  const value = {format: 'zeal-pascal-book-v1', files};
  validateBackup(value);
  const url = URL.createObjectURL(new Blob([JSON.stringify(value)], {type: 'application/json'}));
  const link = document.createElement('a'); link.href = url; link.download = 'zeal-pascal-workspace.json'; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 30000);
  setStatus('Downloaded saved files from H:/book and H:/work. Source still in editor RAM is not included; press S in Pascal before making a backup.');
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
