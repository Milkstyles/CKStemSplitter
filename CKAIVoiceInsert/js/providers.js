(function (global) {
  function normalizeElevenVoice(v) {
    return {
      id: v.voice_id,
      name: v.name || 'Unnamed voice',
      provider: 'ElevenLabs',
      providerKey: 'eleven',
      description: v.description || '',
      labels: v.labels || {},
      previewUrl: v.preview_url || '',
      raw: v
    };
  }

  function normalizeFishVoice(v, owned) {
    return {
      id: v.id || v._id || v.reference_id,
      name: v.title || v.name || v.nickname || 'Unnamed voice',
      provider: 'Fish Audio',
      providerKey: 'fish',
      owned: !!owned,
      description: v.description || v.text || '',
      labels: {
        account: owned ? 'My voice' : '',
        language: v.language || '',
        gender: v.gender || '',
        author: (v.author && (v.author.nickname || v.author.name)) || ''
      },
      previewUrl: v.cover_image || v.preview_url || '',
      raw: v
    };
  }

  async function fetchJson(url, options) {
    const response = await fetch(url, options || {});
    if (!response.ok) {
      const body = await response.text();
      throw new Error(response.status + ' ' + response.statusText + ': ' + body.slice(0, 300));
    }
    return response.json();
  }

  async function listElevenVoices(apiKey) {
    if (!apiKey) return [];
    let nextPageToken = null;
    const voices = [];
    do {
      const params = new URLSearchParams({ page_size: '100' });
      if (nextPageToken) params.set('next_page_token', nextPageToken);
      const data = await fetchJson('https://api.elevenlabs.io/v2/voices?' + params.toString(), {
        headers: { 'xi-api-key': apiKey }
      });
      (data.voices || []).forEach(function (v) { voices.push(normalizeElevenVoice(v)); });
      nextPageToken = data.has_more ? data.next_page_token : null;
    } while (nextPageToken);
    return voices;
  }

  async function listFishModelPages(apiKey, selfOnly) {
    const models = [];
    let pageNumber = 1;
    let hasMore = false;
    do {
      const params = new URLSearchParams({
        page_size: '100',
        page_number: String(pageNumber),
        sort_by: selfOnly ? 'created_at' : 'task_count'
      });
      if (selfOnly) params.set('self', 'true');
      const data = await fetchJson('https://api.fish.audio/model?' + params.toString(), {
        headers: { Authorization: 'Bearer ' + apiKey }
      });
      const items = data.items || data.models || data.data || data.results || [];
      if (!Array.isArray(items)) throw new Error('Fish Audio returned an invalid voice list.');
      items.forEach(function (v) { models.push(normalizeFishVoice(v, selfOnly)); });
      hasMore = !!data.has_more;
      pageNumber += 1;
    } while (hasMore && pageNumber <= 20);
    return models.filter(function (v) { return !!v.id; });
  }

  async function listFishVoices(apiKey) {
    if (!apiKey) return [];

    // Fish requires self=true to include private/unlisted models created by the
    // authenticated account. Load those first so they win duplicate IDs.
    const owned = await listFishModelPages(apiKey, true);
    let publicVoices = [];
    try {
      publicVoices = await listFishModelPages(apiKey, false);
    } catch (_) {
      // Account voices are the essential result; keep them usable even if the
      // public discovery catalog is temporarily unavailable.
    }

    const seen = {};
    return owned.concat(publicVoices).filter(function (voice) {
      if (seen[voice.id]) return false;
      seen[voice.id] = true;
      return true;
    });
  }

  async function generateEleven(apiKey, voice, text) {
    const response = await fetch(
      'https://api.elevenlabs.io/v1/text-to-speech/' + encodeURIComponent(voice.id) + '?output_format=pcm_44100',
      {
        method: 'POST',
        headers: { 'xi-api-key': apiKey, 'Content-Type': 'application/json' },
        body: JSON.stringify({ text: text, model_id: 'eleven_multilingual_v2' })
      }
    );
    if (!response.ok) throw new Error('ElevenLabs generation failed: ' + response.status + ' ' + await response.text());
    return { arrayBuffer: await response.arrayBuffer(), audioKind: 'pcm16', sampleRate: 44100, channels: 1 };
  }

  async function generateFish(apiKey, voice, text) {
    const response = await fetch('https://api.fish.audio/v1/tts', {
      method: 'POST',
      headers: {
        Authorization: 'Bearer ' + apiKey,
        'Content-Type': 'application/json',
        model: 's2-pro'
      },
      body: JSON.stringify({ text: text, reference_id: voice.id, format: 'wav' })
    });
    if (!response.ok) throw new Error('Fish Audio generation failed: ' + response.status + ' ' + await response.text());
    return { arrayBuffer: await response.arrayBuffer(), audioKind: 'wav' };
  }

  global.CKProviders = {
    listAllVoices: async function (keys) {
      const results = await Promise.allSettled([
        listElevenVoices(keys.eleven),
        listFishVoices(keys.fish)
      ]);
      const voices = [];
      results.forEach(function (r) { if (r.status === 'fulfilled') voices.push.apply(voices, r.value); });
      const errors = results.filter(function (r) { return r.status === 'rejected'; }).map(function (r) { return r.reason.message; });
      voices.sort(function (a, b) {
        if (!!a.owned !== !!b.owned) return a.owned ? -1 : 1;
        return a.name.localeCompare(b.name);
      });
      return { voices: voices, errors: errors };
    },
    generate: function (keys, voice, text) {
      if (voice.providerKey === 'eleven') return generateEleven(keys.eleven, voice, text);
      if (voice.providerKey === 'fish') return generateFish(keys.fish, voice, text);
      throw new Error('Unknown voice provider');
    }
  };
})(window);
