# CK Stem Splitter v0.9.1

Self-contained Adobe Audition VST3 for playback-free vocal/instrumental separation of the highlighted waveform selection.

## Audition workflow

1. Highlight a range in Audition's Waveform Editor.
2. Open **Effects > VST3 > Commercial Kings > CK Stem Splitter**.
3. Click **Scan Selection**, then click Audition's **Apply** once. Audition renders the highlighted range through the effect offline; CK captures it while passing the original through unchanged. Playback is not required.
4. Wait for **Stems ready**, then choose **Acapella** or **Instrumental**.
5. Keep the same range highlighted and click Audition's **Apply** again. The selected stem replaces that range in the current file.
6. Use Audition Undo to restore the original if needed.

The two Apply passes are required because the full selection must be scanned before the AI model can calculate a stem. The temporary capture and stem cache are private implementation details; there is no Save window, file browser, manually opened file, upload, Python installation, or model download.

## v0.9 changes

- Restores the Effects-panel VST3 based on the working capture/apply build.
- Automatically finishes capture when Audition's offline Apply pass becomes idle or releases the effect.
- Restores the completed scan when Audition recreates the effect between Apply passes.
- Keeps the first pass transparent so it does not alter the current waveform.
- Renames Vocals to Acapella and preserves Instrumental as the second choice.
- Removes the discontinued CEP selection-export panel during installation.
- Bundles the existing `htdemucs_ft_vocals` FP16 model and frozen local engine.
- v0.9.1 defers cross-Apply cache restoration to Audition's UI thread to avoid host crashes during audio initialization.

## Installed locations

- VST3: `C:\Program Files\Common Files\VST3\CK Stem Splitter.vst3`
- Engine/model: `C:\ProgramData\Commercial Kings\CK Stem Splitter\engine\`
- Private capture/cache: `%TEMP%\Commercial Kings\CK Stem Splitter\` and `%APPDATA%\Commercial Kings\CK Stem Splitter\Cache\`

## Automated Windows build

`.github/workflows/windows-ci.yml` compiles the VST3, checks the offline-scan implementation, freezes the stem engine, bundles the model, runs a real separation smoke test, packages the installer, generates a SHA-256 checksum, and uploads the Windows artifact.
