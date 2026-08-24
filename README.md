# CK Stem Splitter v0.7

Self-contained Windows stem separator with an Adobe Audition offline-selection panel and a VST3 preview effect.

## User workflow

1. Open a file in Audition's **Waveform Editor** and highlight the range to process.
2. Open **Window > Extensions > CK Stem Splitter**.
3. Click **Split Highlighted Selection**. Audition copies the selected waveform to the local companion through the Windows audio clipboard; there is no playback and no file picker.
4. Choose **Vocals** or **Instrumental**.
5. Click **Replace Selection**. The panel asks Audition to paste the selected stem over the still-highlighted range.
6. Use Audition **Undo** if you want the original audio back.

The VST3 is still installed for real-time preview/playback, but it is not the primary Audition selection workflow.

## v0.7 changes

- Adds an Audition CEP panel driven by Audition's reflected `Application.COMMAND_*` API and `app.invokeCommand`.
- Copies and pastes highlighted waveform audio through a small native `CF_WAVE` bridge, avoiding playback and manual file browsing.
- Runs the existing bundled `ckstem-engine.exe` offline and keeps all audio/model processing local.
- Presents direct Vocals/Instrumental selection and destructive replace, with Audition Undo as the safety path.
- Keeps the existing VST3 installed for playback/preview compatibility.

## Existing engine and VST3 features

- Uses the `htdemucs_ft_vocals` FP16 vocal-specialist ONNX model.
- Bundles the AI model and frozen engine into the Windows installer; no Python or model download is required on the user's PC.
- Streams separated WAVs from disk with JUCE read-ahead buffering instead of loading entire stems into RAM.
- Uses JUCE transport sample-rate correction for 44.1/48 kHz session compatibility.
- Starts both cached stem transports after loading so Vocals/Instrumental playback is immediately available.
- Audio callback uses a non-blocking `try_lock`; it clears the stem output rather than waiting on background cache/engine work.
- Plugin state remembers the selected source-file path when the Audition host saves plugin state.
- GitHub Actions smoke-tests the frozen AI engine before packaging.
- Build artifact includes a SHA-256 checksum alongside the installer.

## Installed locations

VST3:

`C:\Program Files\Common Files\VST3\CK Stem Splitter.vst3`

Private AI engine and model:

`C:\ProgramData\Commercial Kings\CK Stem Splitter\engine\`

Per-user stem cache:

`%APPDATA%\Commercial Kings\CK Stem Splitter\Cache\`

Audition panel:

`C:\Program Files (x86)\Common Files\Adobe\CEP\extensions\com.commercialkings.ckstemsplitter\`

## Automated Windows build

The GitHub Actions workflow `.github/workflows/windows-ci.yml` builds the x64 VST3 and native Audition bridge, validates the panel manifest and bridge against a real WAV, freezes the private separation engine, prewarms the bundled model, runs a real stem-separation smoke test, packages everything with Inno Setup, checks installer size, and uploads:

- `CK-Stem-Splitter-Setup.exe`
- `CK-Stem-Splitter-Setup.exe.sha256`

## Development note

The installer can be self-contained for end users, but anyone distributing a closed-source JUCE-based commercial build must make sure their JUCE licensing is appropriate for that distribution model.
