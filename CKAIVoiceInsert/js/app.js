(function () {
  const state = {
    voices: [],
    filtered: [],
    selectedVoice: null,
    providerFilter: 'all',
    takes: [],
    selectedTakeId: null,
    nextTakeNumber: 1,
    generating: false,
    inserting: false
  };
  const $ = (id) => document.getElementById(id);

  function setStatus(text, error) {
    $('status').textContent = text;
    $('status').classList.toggle('error', !!error);
  }

  function keys() {
    return {
      eleven: localStorage.getItem('ck.eleven.key') || '',
      fish: localStorage.getItem('ck.fish.key') || ''
    };
  }

  function saveKeys() {
    localStorage.setItem('ck.eleven.key', $('elevenKey').value.trim());
    localStorage.setItem('ck.fish.key', $('fishKey').value.trim());
    setStatus('API keys saved locally.');
    refreshVoices();
  }

  function labelText(v) {
    const parts = [];
    Object.keys(v.labels || {}).forEach(function (k) {
      const value = v.labels[k];
      if (value) parts.push(value);
    });
    return parts.slice(0, 4).join(' • ');
  }

  function renderVoices() {
    const list = $('voiceList');
    list.innerHTML = '';
    state.filtered.forEach(function (voice) {
      const row = document.createElement('button');
      row.className = 'voiceRow' + (state.selectedVoice && state.selectedVoice.id === voice.id && state.selectedVoice.providerKey === voice.providerKey ? ' selected' : '');
      row.innerHTML = '<div class="voiceTop"><strong></strong><span class="provider ' + voice.providerKey + '"></span></div><div class="meta"></div>';
      row.querySelector('strong').textContent = voice.name;
      row.querySelector('.provider').textContent = voice.provider;
      row.querySelector('.meta').textContent = labelText(voice) || voice.description || 'AI voice';
      row.onclick = function () {
        state.selectedVoice = voice;
        $('selectedVoice').textContent = voice.name + ' — ' + voice.provider;
        renderVoices();
      };
      list.appendChild(row);
    });
    if (!state.filtered.length) list.innerHTML = '<div class="empty">No voices match this search.</div>';
  }

  function selectedTake() {
    for (let i = 0; i < state.takes.length; i += 1) {
      if (state.takes[i].id === state.selectedTakeId) return state.takes[i];
    }
    return null;
  }

  function renderTakes() {
    const list = $('takeList');
    list.innerHTML = '';
    state.takes.forEach(function (take) {
      const row = document.createElement('div');
      row.className = 'takeRow' + (take.id === state.selectedTakeId ? ' selected' : '');

      const title = document.createElement('div');
      title.className = 'takeTitle';
      const strong = document.createElement('strong');
      strong.textContent = 'Take ' + take.number + ' — ' + take.voice.name;
      const provider = document.createElement('span');
      provider.textContent = take.voice.provider;
      title.appendChild(strong);
      title.appendChild(provider);

      const player = document.createElement('audio');
      player.controls = true;
      player.preload = 'metadata';
      player.src = take.previewUrl;

      const select = document.createElement('button');
      select.className = 'selectTake';
      select.textContent = take.id === state.selectedTakeId ? 'SELECTED FOR INSERT' : 'SELECT THIS TAKE';
      select.onclick = function () {
        state.selectedTakeId = take.id;
        renderTakes();
        setStatus('Take ' + take.number + ' selected. Approve it when ready.');
      };

      row.appendChild(title);
      row.appendChild(player);
      row.appendChild(select);
      list.appendChild(row);
    });

    if (!state.takes.length) list.innerHTML = '<div class="empty">No takes generated yet. After a successful generation, an audio player appears here.</div>';
    $('approveTake').disabled = !selectedTake() || state.inserting;
  }

  function applyFilter() {
    const q = $('voiceSearch').value.trim().toLowerCase();
    state.filtered = state.voices.filter(function (v) {
      if (state.providerFilter !== 'all' && v.provider !== state.providerFilter) return false;
      const haystack = [v.name, v.provider, v.description, labelText(v)].join(' ').toLowerCase();
      return !q || haystack.indexOf(q) !== -1;
    });
    renderVoices();
  }

  async function refreshVoices() {
    setStatus('Loading ElevenLabs and Fish Audio voices...');
    try {
      const result = await CKProviders.listAllVoices(keys());
      state.voices = result.voices;
      applyFilter();
      const suffix = result.errors.length ? ' (' + result.errors.length + ' provider warning)' : '';
      setStatus('Loaded ' + state.voices.length + ' voices' + suffix + '.');
    } catch (err) {
      setStatus(err.message, true);
    }
  }

  function evalHost(script) {
    return new Promise(function (resolve) {
      if (!window.__adobe_cep__ || !window.__adobe_cep__.evalScript) {
        resolve(JSON.stringify({ ok: false, error: 'Audition CEP bridge unavailable' }));
        return;
      }
      window.__adobe_cep__.evalScript(script, resolve);
    });
  }

  function writeWaveFromPCM16(filePath, arrayBuffer, sampleRate, channels) {
    const fs = require('fs');
    const pcm = Buffer.from(arrayBuffer);
    const header = Buffer.alloc(44);
    const byteRate = sampleRate * channels * 2;
    const blockAlign = channels * 2;
    header.write('RIFF', 0);
    header.writeUInt32LE(36 + pcm.length, 4);
    header.write('WAVE', 8);
    header.write('fmt ', 12);
    header.writeUInt32LE(16, 16);
    header.writeUInt16LE(1, 20);
    header.writeUInt16LE(channels, 22);
    header.writeUInt32LE(sampleRate, 24);
    header.writeUInt32LE(byteRate, 28);
    header.writeUInt16LE(blockAlign, 32);
    header.writeUInt16LE(16, 34);
    header.write('data', 36);
    header.writeUInt32LE(pcm.length, 40);
    fs.writeFileSync(filePath, Buffer.concat([header, pcm]));
  }

  function writeGeneratedAudio(result) {
    const fs = require('fs');
    const os = require('os');
    const path = require('path');
    const dir = path.join(os.tmpdir(), 'Commercial Kings', 'CK AI Voice Insert');
    fs.mkdirSync(dir, { recursive: true });
    const filePath = path.join(dir, 'ck-ai-voice-' + Date.now() + '.wav');
    if (result.audioKind === 'pcm16') {
      writeWaveFromPCM16(filePath, result.arrayBuffer, result.sampleRate || 44100, result.channels || 1);
    } else {
      fs.writeFileSync(filePath, Buffer.from(result.arrayBuffer));
    }
    return filePath;
  }

  function previewUrlForFile(filePath) {
    const fs = require('fs');
    return 'data:audio/wav;base64,' + fs.readFileSync(filePath).toString('base64');
  }

  function extensionRoot() {
    const raw = window.__adobe_cep__.getSystemPath('extension');
    return decodeURIComponent(String(raw).replace(/^file:\/\//i, '').replace(/^\/(\w:)/, '$1'));
  }

  function copyWaveToClipboard(filePath) {
    const path = require('path');
    const child = require('child_process');
    const helper = path.join(extensionRoot(), 'bin', 'CKVoiceClipboard.exe');
    const run = child.spawnSync(helper, ['copy', filePath], { windowsHide: true, encoding: 'utf8' });
    if (run.error) throw run.error;
    if (run.status !== 0) throw new Error((run.stderr || 'Audio clipboard helper failed').trim());
  }

  async function generateTake() {
    const voice = state.selectedVoice;
    const text = $('script').value.trim();
    if (!voice) return setStatus('Choose a voice first.', true);
    if (!text) return setStatus('Enter a script first.', true);
    if (state.generating) return;

    const takeNumber = state.nextTakeNumber;
    try {
      state.generating = true;
      $('generateTake').disabled = true;
      $('generateTake').textContent = 'GENERATING TAKE ' + takeNumber + '...';
      setStatus('Generating take ' + takeNumber + ' with ' + voice.name + ' from ' + voice.provider + '...');
      const audio = await CKProviders.generate(keys(), voice, text);
      const filePath = writeGeneratedAudio(audio);
      const take = {
        id: 'take-' + Date.now() + '-' + takeNumber,
        number: takeNumber,
        voice: voice,
        text: text,
        filePath: filePath,
        previewUrl: previewUrlForFile(filePath)
      };
      state.takes.unshift(take);
      state.selectedTakeId = take.id;
      state.nextTakeNumber += 1;
      renderTakes();
      setStatus('Take ' + takeNumber + ' is ready. Preview it, generate another, or approve and insert it.');
    } catch (err) {
      setStatus('Take ' + takeNumber + ' failed: ' + err.message, true);
    } finally {
      state.generating = false;
      $('generateTake').disabled = false;
      $('generateTake').textContent = 'GENERATE NEW TAKE';
    }
  }

  async function approveAndInsert() {
    const take = selectedTake();
    if (!take) return setStatus('Select a generated take first.', true);
    if (state.inserting) return;

    try {
      state.inserting = true;
      $('approveTake').disabled = true;
      $('approveTake').textContent = 'INSERTING TAKE ' + take.number + '...';

      setStatus('Reading the current Audition insertion point...');
      const contextRaw = await evalHost('CKVoiceInsert.getInsertionContext()');
      let context;
      try { context = JSON.parse(contextRaw); } catch (_) { context = { ok: false, error: contextRaw }; }
      if (!context.ok) throw new Error(context.error || 'Could not read Audition insertion point.');

      setStatus('Preparing approved take ' + take.number + ' for Audition...');
      copyWaveToClipboard(take.filePath);

      setStatus('Inserting take ' + take.number + ' at ' + Number(context.startSeconds || 0).toFixed(3) + 's...');
      const mode = $('insertMode').value;
      const insertRaw = await evalHost("CKVoiceInsert.insertGeneratedAudio('', '" + mode + "', " + Number(context.startSamples || 0) + ")");
      let inserted;
      try { inserted = JSON.parse(insertRaw); } catch (_) { inserted = { ok: false, error: insertRaw }; }
      if (!inserted.ok) throw new Error(inserted.error || 'Audition insertion failed.');
      setStatus('Approved take ' + take.number + ' was inserted at the selected start time.');
    } catch (err) {
      setStatus('Could not insert take ' + take.number + ': ' + err.message, true);
    } finally {
      state.inserting = false;
      $('approveTake').textContent = 'APPROVE & INSERT SELECTED TAKE';
      renderTakes();
    }
  }

  $('saveKeys').onclick = saveKeys;
  $('refreshVoices').onclick = refreshVoices;
  $('voiceSearch').oninput = applyFilter;
  $('generateTake').onclick = generateTake;
  $('approveTake').onclick = approveAndInsert;
  document.querySelectorAll('.filter[data-provider]').forEach(function (button) {
    button.onclick = function () {
      document.querySelectorAll('.filter[data-provider]').forEach(function (b) { b.classList.remove('active'); });
      button.classList.add('active');
      state.providerFilter = button.dataset.provider;
      applyFilter();
    };
  });

  $('elevenKey').value = keys().eleven;
  $('fishKey').value = keys().fish;
  renderTakes();
  refreshVoices();
})();
