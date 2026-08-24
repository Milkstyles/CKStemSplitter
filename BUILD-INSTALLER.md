# Building CK Stem Splitter Setup

The Windows installer is built by `.github/workflows/windows-ci.yml`.

The job:

1. Builds the Audition-compatible JUCE VST3 with Visual Studio.
2. Verifies that the offline scan watchdog and Effects-panel UI are present.
3. Prewarms the bundled `htdemucs_ft_vocals` FP16 model.
4. Freezes the local stem engine and its runtime dependencies.
5. Runs the engine against a generated WAV and verifies both stems.
6. Packages the VST3, engine, and model with Inno Setup.
7. Uploads the installer and SHA-256 checksum.

No Python, model download, or developer tools are required on the user's PC.
