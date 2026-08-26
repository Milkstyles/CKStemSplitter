# Building the CK Stem Splitter Windows installer

The supported installer is built by `.github/workflows/windows-ci.yml`.

The job:

1. Builds the known-good **CK Stem Splitter** VST3 and its tests with Visual Studio.
2. Verifies that a standalone VST3 host can load and create the plug-in.
3. Installs pinned CPU-only engine dependencies and prewarms the bundled `htdemucs_ft_vocals` FP16 model.
4. Freezes the local Python engine into a self-contained Windows executable.
5. Runs an end-to-end offline Acapella render through the actual plug-in processor.
6. Packages the VST3, engine, and model with Inno Setup.
7. Checks the installer size, generates a SHA-256 checksum, and uploads both files.

The finished installer does not require Python, developer tools, a model download, or the retired Adobe extension/automation companion on the user's computer.
