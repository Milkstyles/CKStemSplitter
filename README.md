# CK Stem Splitter v0.8

Self-contained Windows stem separator with an Adobe Audition playback-free selection panel.

## User workflow

1. Open a file in Audition's **Waveform Editor** and highlight the range to process.
2. Open **Window > Extensions > CK Stem Splitter**.
3. Click **Split Highlighted Selection**. The panel invokes Audition's native **Save Selection As** command and its bundled helper supplies a private temporary WAV path; the user never browses for a file and audio is not played.
4. Choose **Vocals** or **Instrumental**.
5. Click **Replace Selection**. The panel opens the generated stem, transfers it through Audition's internal audio clipboard, returns to the original waveform, and pastes over the highlighted range.
6. Use Audition **Undo** to restore the original audio if needed.

## v0.8 changes

- Replaces the incompatible Windows audio-clipboard capture path with Audition 26.3's native `File.SaveSelectionAs` command.
- Automates only the Audition file dialogs opened by the user's Split or Apply action.
- Uses Audition's internal clipboard for the final replacement, which is required because Audition 26.3 does not publish waveform audio to the Windows clipboard.
- Removes the optional VST3 from the installer and deletes the old copy during upgrades because it crashed Audition 26.3 on launch.
- Keeps the existing local `htdemucs_ft_vocals` FP16 engine and model; no upload, Python installation, model download, playback, or manual file browsing is required.

## Installed locations

Private AI engine and model:

`C:\ProgramData\Commercial Kings\CK Stem Splitter\engine\`

Per-user temporary jobs:

`%APPDATA%\Commercial Kings\CK Stem Splitter\Jobs\`

Audition panel:

`C:\Program Files (x86)\Common Files\Adobe\CEP\extensions\com.commercialkings.ckstemsplitter\`

## Automated Windows build

The GitHub Actions workflow `.github/workflows/windows-ci.yml` builds the x64 Audition helper, validates the panel and manifest, freezes the private separation engine, prewarms the bundled model, runs a real stem-separation smoke test, packages everything with Inno Setup, and uploads the installer plus its SHA-256 checksum.

The earlier JUCE VST3 source remains in the repository for reference, but v0.8 does not build or install it.
