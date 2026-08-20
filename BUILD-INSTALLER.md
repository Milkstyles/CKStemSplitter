# Building CK Stem Splitter Setup

The Windows installer is built by `.github/workflows/build-windows-installer.yml`.

The CI job:

1. Builds the JUCE VST3 with Visual Studio 2022.
2. Installs `demucs-onnx==0.3.0` only on the CI runner.
3. Prewarms the `htdemucs_ft_vocals` FP16-weights model into the staging cache.
4. Freezes `demucs_onnx.cli` with PyInstaller into a portable `ckstem-engine.exe` folder.
5. Packages the VST3, frozen engine, ONNX Runtime dependencies, and model cache with Inno Setup.
6. Uploads `CK-Stem-Splitter-Setup.exe` as the GitHub Actions artifact.

No Python or developer tools are required on the end user's PC.
