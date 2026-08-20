# CK Stem Splitter v0.6

Windows VST3 prototype for Adobe Audition that performs local vocal/instrumental separation.

## User workflow

1. Insert **CK Stem Splitter** as a VST3 effect in Adobe Audition.
2. Click **Load Audio** and choose the song file.
3. Click **Split Stems**. The full file is processed directly; the song does not need to play in real time first.
4. Choose **Vocals** or **Instrumental** in the output selector.
5. Previously generated stems are cached per source file.

## v0.6 changes

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

## Automated Windows build

The GitHub Actions workflow `.github/workflows/build-windows-installer.yml` builds the x64 VST3, freezes the private separation engine, prewarms the bundled model, runs a real WAV separation smoke test, packages everything with Inno Setup, checks installer size, and uploads:

- `CK-Stem-Splitter-Setup.exe`
- `CK-Stem-Splitter-Setup.exe.sha256`

## Development note

The installer can be self-contained for end users, but anyone distributing a closed-source JUCE-based commercial build must make sure their JUCE licensing is appropriate for that distribution model.
