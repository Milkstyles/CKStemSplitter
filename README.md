# CK Stem Splitter v1.2.0

A self-contained Windows VST3 for playback-free vocal and instrumental separation in Adobe Audition.

## Audition workflow

1. Highlight audio in Audition's Waveform Editor.
2. Open **Effects > VST3 > Effect > Commercial Kings > CK Stem Splitter**.
3. Choose **Make Acapella** or **Make Instrumental**.
4. Click Audition's **Apply** button.

Audition renders the highlighted range offline, so the song does not have to play in real time. The selected stem replaces that range in the current file and can be reversed with Audition Undo. There is no Save As window, file browser, extension panel, upload, Python installation, or model download.

## Stable v1.2 renderer

- Runs the AI stem engine outside Audition so an engine failure cannot take down the editor.
- Keeps one engine process ready while the effect window is open, avoiding repeated model startup.
- Performs one model pass for each VST processing window; the plug-in handles overlap and blending.
- Uses the dependable CPU provider on Windows 11 and avoids experimental GPU dependencies.
- Preserves exact input length and supports both Acapella and Instrumental output.
- Includes a one-shot CPU fallback if the persistent engine cannot start.
- Bundles the frozen engine and `htdemucs_ft_vocals` FP16 model.

## Installed locations

- VST3: `C:\Program Files\Common Files\VST3\CK Stem Splitter.vst3`
- Engine and model: `C:\ProgramData\Commercial Kings\CK Stem Splitter\engine\`

The installer removes the retired CK Stem Splitter VST3, CEP extension, and automation companion so they cannot conflict with this build.

## Automated Windows build

`.github/workflows/windows-ci.yml` builds and host-loads the VST3, freezes the pinned CPU engine, bundles the model, runs a real end-to-end offline render through the plug-in processor, packages the installer, generates a SHA-256 checksum, and uploads the Windows artifact.
