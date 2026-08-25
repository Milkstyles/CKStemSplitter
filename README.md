# CK Stem Splitter v1.0.7

Self-contained Adobe Audition VST3 plus Windows companion for one-click, playback-free vocal/instrumental separation of the highlighted waveform selection.

## Audition workflow

1. Highlight a range in Audition's Waveform Editor.
2. Open **Effects > VST3 > Commercial Kings > CK Stem Splitter**.
3. Click **Make Acapella** or **Make Instrumental** once.
4. Keep Audition open while the companion scans and separates the highlighted range offline. It automatically runs both Apply passes and replaces that range in the current file.
5. Use Audition Undo to restore the original if needed.

The companion handles the two internal Apply passes because the full selection must be scanned before the AI model can calculate a stem. The temporary capture and stem cache are private implementation details; there is no playback, Save window, file browser, manually opened file, upload, Python installation, or model download.

## v1.0 changes

- Restores the Effects-panel VST3 based on the working capture/apply build.
- Automatically finishes capture when Audition's offline Apply pass becomes idle or releases the effect.
- Restores the completed scan when Audition recreates the effect between Apply passes.
- Keeps the first pass transparent so it does not alter the current waveform.
- Renames Vocals to Acapella and preserves Instrumental as the second choice.
- Removes the discontinued CEP selection-export panel during installation.
- Bundles the existing `htdemucs_ft_vocals` FP16 model and frozen local engine.
- v1.0 adds a Windows companion that automatically drives Audition's two required offline passes. Highlight audio, open the effect, and click **Make Acapella** or **Make Instrumental** once.
- v1.0.1 runs the AI engine entirely in the external companion and reopens Audition only after both stem WAVs are complete.
- v1.0.2 removes every automatic startup callback from the plugin window. The companion explicitly loads finished stems only during the automated second pass.
- v1.0.3 locates the bundled engine relative to the companion executable and writes an automation result log for host-specific diagnostics.
- v1.0.4 passes Audition's exact native effect-window handle to the companion and avoids host parameter notifications during capture.
- v1.0.5 removes the manual prepared-stem control, loads the requested stem automatically on the second pass, replaces background transport streaming with deterministic in-memory offline rendering, and corrects Audition effect-window Apply detection.
- v1.0.6 delays the first automated Apply until the JUCE button callback has fully returned, removes all exposed VST parameter controls, and rejects stale automation manifests.
- v1.0.7 activates Audition's Apply control using normal mouse input instead of a synchronous cross-process button message.

## Installed locations

- VST3: `C:\Program Files\Common Files\VST3\CK Stem Splitter.vst3`
- Engine/model: `C:\ProgramData\Commercial Kings\CK Stem Splitter\engine\`
- Automation companion: `C:\ProgramData\Commercial Kings\CK Stem Splitter\companion\`
- Private capture/cache: `%TEMP%\Commercial Kings\CK Stem Splitter\` and `%APPDATA%\Commercial Kings\CK Stem Splitter\Cache\`

## Automated Windows build

`.github/workflows/windows-ci.yml` compiles the VST3, checks the offline-scan implementation, freezes the stem engine, bundles the model, runs a real separation smoke test, packages the installer, generates a SHA-256 checksum, and uploads the Windows artifact.

