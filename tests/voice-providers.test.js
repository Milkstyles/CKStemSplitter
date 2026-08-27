const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

test('Fish Audio account voices are loaded, labeled, paginated, and deduplicated', async () => {
  const requests = [];
  const context = {
    URLSearchParams,
    window: {},
    fetch: async (url, options) => {
      requests.push({ url, options });
      const parsed = new URL(url);
      const selfOnly = parsed.searchParams.get('self') === 'true';
      const page = Number(parsed.searchParams.get('page_number'));
      let body;

      if (selfOnly) {
        body = {
          items: [{ _id: 'mine', title: 'My Private Voice', visibility: 'private' }],
          has_more: false
        };
      } else if (page === 1) {
        body = {
          items: [
            { _id: 'mine', title: 'My Public Duplicate' },
            { _id: 'popular', title: 'Popular Voice' }
          ],
          has_more: true
        };
      } else {
        body = {
          items: [{ _id: 'second-page', title: 'Second Page Voice' }],
          has_more: false
        };
      }

      return {
        ok: true,
        json: async () => body
      };
    }
  };

  vm.createContext(context);
  const source = fs.readFileSync(
    path.join(__dirname, '..', 'CKAIVoiceInsert', 'js', 'providers.js'),
    'utf8'
  );
  vm.runInContext(source, context);

  const result = await context.window.CKProviders.listAllVoices({ eleven: '', fish: 'Bearer test-key' });
  assert.equal(result.errors.length, 0);
  assert.equal(result.voices.filter((voice) => voice.id === 'mine').length, 1);
  assert.equal(result.voices[0].id, 'mine');
  assert.equal(result.voices[0].labels.account, 'My voice');
  assert.ok(result.voices.some((voice) => voice.id === 'second-page'));
  assert.ok(requests.some((request) => request.url.includes('self=true')));
  assert.ok(requests.some((request) => request.url.includes('page_number=2')));
  assert.ok(requests.every((request) => request.options.headers.Authorization === 'Bearer test-key'));
});
