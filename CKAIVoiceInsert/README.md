# CK AI Voice Insert

Adobe Audition CEP extension for generating AI speech from ElevenLabs and Fish Audio and inserting the full generated result at the current Waveform playhead / selection start.

## Target workflow

1. Put the Audition playhead at the insertion point or highlight a range.
2. Open **Window > Extensions > CK AI Voice Insert**.
3. Search one unified voice list. Every voice is labeled `ElevenLabs` or `Fish Audio`.
4. Enter script text.
5. Click **Generate & Insert**.
6. The generated audio begins at the selection/playhead start. The generated duration wins: a 3-second selection may receive a 10-second generated voice.

## Architecture

- `CSXS/manifest.xml` — Audition CEP panel registration.
- `index.html`, `css/app.css`, `js/app.js` — panel UI.
- `js/providers.js` — ElevenLabs and Fish Audio REST clients + normalized unified voice model.
- `jsx/host.jsx` — ExtendScript bridge to Audition. Reads WaveDocument context and owns insertion operations.
- `helper/` — Windows audio-clipboard helper used to hand the generated WAV to Audition for insertion.

## API endpoints

ElevenLabs:
- `GET https://api.elevenlabs.io/v2/voices`
- `POST https://api.elevenlabs.io/v1/text-to-speech/{voice_id}`

Fish Audio:
- `POST https://api.fish.audio/v1/tts`
- Fish voices/models are normalized to `{id,name,provider:'Fish Audio',...}` before display.

## Security

API keys are never committed. The bootstrap UI stores them only in the local CEP profile. Before production release this will move to the Windows credential store through a small native helper.

## Current host integration status

The Audition panel, unified voice-generation layer, playhead/selection-start bridge, and Windows audio-clipboard insertion helper are implemented for the first Windows test build.

Build trigger: first packaged Windows test build.
