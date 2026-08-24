# Building CK Stem Splitter Setup

The Windows installer is built by `.github/workflows/windows-ci.yml`.

The CI job:

1. Builds the native Audition selection/file-dialog bridge with Visual Studio.
2. Stages and validates the Audition CEP offline-selection panel.
3. Installs `demucs-onnx==0.3.0` only on the CI runner.
4. Prewarms the `htdemucs_ft_vocals` FP16-weights model into the staging cache.
5. Freezes `demucs_onnx.cli` with PyInstaller into a portable `ckstem-engine.exe` folder.
6. Packages the Audition panel, bridge, frozen engine, ONNX Runtime dependencies, and model cache with Inno Setup.
7. Uploads `CK-Stem-Splitter-Setup.exe` as the GitHub Actions artifact.

No Python or developer tools are required on the end user's PC.
