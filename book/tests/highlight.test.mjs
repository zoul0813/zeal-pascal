import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync, mkdirSync, writeFileSync, readFileSync, copyFileSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {execFileSync} from 'node:child_process';

test('Hugo highlights Pascal and build/shell languages using the shipped render hook', () => {
  const root = mkdtempSync(join(tmpdir(), 'book-highlight-'));
  const cases = new Map([
    ['pascal', "program Example;\nvar n: integer;\nbegin n := 42; WriteLn('hello'); end."],
    ['Makefile', '# Build\nCC = cc\nall: app\n\t$(CC) main.c -o app'],
    ['cmake', '# Build\nproject(Example LANGUAGES C)\nadd_executable(app main.c)'],
    ['CMakeLists.txt', '# Build\nproject(Example LANGUAGES C)'],
    ['bash', '# List\nfor file in *.pas; do echo "$file"; done'],
    ['zsh', '# List\nfor file in *.pas; do print -r -- "$file"; done'],
    ['cli', 'echo "<script>not markup</script>"'],
  ]);
  try {
    mkdirSync(join(root, 'content'));
    mkdirSync(join(root, 'layouts/_default/_markup'), {recursive: true});
    copyFileSync(new URL('../layouts/_default/_markup/render-codeblock.html', import.meta.url), join(root, 'layouts/_default/_markup/render-codeblock.html'));
    copyFileSync(new URL('../hugo.toml', import.meta.url), join(root, 'hugo.toml'));
    writeFileSync(join(root, 'layouts/_default/single.html'), '{{ .Content }}');
    let index = 0;
    for (const [language, source] of cases) {
      writeFileSync(join(root, `content/sample${index++}.md`), `+++\ntitle="Code"\n+++\n\`\`\`${language}\n${source}\n\`\`\`\n`);
    }
    execFileSync('hugo', ['--source', root, '--quiet'], {stdio: 'pipe'});
    index = 0;
    for (const [language] of cases) {
      const html = readFileSync(join(root, `public/sample${index++}/index.html`), 'utf8');
      assert.match(html, /class="chroma"/, language);
      const tokens = new Set([...html.matchAll(/<span class="([^" ]+)"/g)].map(match => match[1]));
      assert.ok(tokens.size >= 3, `${language} must contain lexer tokens, not just plain code`);
      assert.doesNotMatch(html, /<script>/, 'code must remain escaped');
    }
  } finally { rmSync(root, {recursive: true, force: true}); }
});
