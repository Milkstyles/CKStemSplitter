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
    fetch: async (url) => {
      requests.push(url);
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

  const result = await context.window.CKProviders.listAllVoices({ eleven: '', fish: 'test-key' });
  assert.equal(result.errors.length, 0);
  assert.equal(result.voices.filter((voice) => voice.id === 'mine').length, 1);
  assert.equal(result.voices[0].id, 'mine');
  assert.equal(result.voices[0].labels.account, 'My voice');
  assert.ok(result.voices.some((voice) => voice.id === 'second-page'));
  assert.ok(requests.some((url) => url.includes('self=true')));
  assert.ok(requests.some((url) => url.includes('page_number=2')));
});

test('Fish Audio generation uses the supported s2-pro model header', async () => {
  let request;
  const context = {
    URLSearchParams,
    window: {},
    fetch: async (url, options) => {
      request = { url, options };
      return {
        ok: true,
        arrayBuffer: async () => new Uint8Array([82, 73, 70, 70]).buffer
      };
    }
  };

  vm.createContext(context);
  const source = fs.readFileSync(
    path.join(__dirname, '..', 'CKAIVoiceInsert', 'js', 'providers.js'),
    'utf8'
  );
  vm.runInContext(source, context);

  await context.window.CKProviders.generate(
    { eleven: '', fish: 'Bearer test-key' },
    { id: 'owned-voice', providerKey: 'fish' },
    'Test line'
  );

  assert.equal(request.url, 'https://api.fish.audio/v1/tts');
  assert.equal(request.options.headers.model, 's2-pro');
  assert.equal(request.options.headers.Authorization, 'Bearer test-key');
  assert.equal(JSON.parse(request.options.body).reference_id, 'owned-voice');
});

test('Fish Audio 401 responses explain how to replace the API key', async () => {
  const context = {
    URLSearchParams,
    window: {},
    fetch: async () => ({
      ok: false,
      status: 401,
      text: async () => '{"message":"Invalid Token"}'
    })
  };

  vm.createContext(context);
  const source = fs.readFileSync(
    path.join(__dirname, '..', 'CKAIVoiceInsert', 'js', 'providers.js'),
    'utf8'
  );
  vm.runInContext(source, context);

  await assert.rejects(
    context.window.CKProviders.generate(
      { eleven: '', fish: 'invalid' },
      { id: 'owned-voice', providerKey: 'fish' },
      'Test line'
    ),
    /Fish Audio rejected the API key/
  );
});
