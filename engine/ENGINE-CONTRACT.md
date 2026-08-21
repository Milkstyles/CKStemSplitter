# CK Stem Engine contract — v0.6

The VST3 uses a private frozen engine installed at:

`C:\ProgramData\Commercial Kings\CK Stem Splitter\engine\ckstem-engine\ckstem-engine.exe`

The model cache is installed at:

`C:\ProgramData\Commercial Kings\CK Stem Splitter\engine\models`

The customer does not install Python, pip, ONNX Runtime, CMake, JUCE, or Visual Studio.

Invocation used by the VST3:

`ckstem-engine.exe separate "INPUT" "OUTPUT_DIR" --model htdemucs_ft_vocals --small --providers auto --cache-dir "MODEL_CACHE" --karaoke --verbose`

Expected engine output in `OUTPUT_DIR`:

- `vocals.wav`
- `karaoke.wav`

The VST3 renames `karaoke.wav` to its stable cache name `instrumental.wav`. Both stems are then streamed from disk by the plugin.
