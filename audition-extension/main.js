(function () {
    "use strict";

    var childProcess = require("child_process");
    var fs = require("fs");
    var os = require("os");
    var path = require("path");

    var splitButton = document.getElementById("splitButton");
    var applyButton = document.getElementById("applyButton");
    var stemSection = document.getElementById("stemSection");
    var status = document.getElementById("status");
    var progress = document.getElementById("progress");
    var retryButton = document.getElementById("retryButton");
    var selectedStem = "vocals";
    var outputDirectory = "";

    function extensionPath() {
        var url = window.__adobe_cep__.getSystemPath("extension");
        url = decodeURIComponent(url).replace(/^file:\/\//i, "");
        if (/^\/[A-Za-z]:/.test(url)) url = url.substring(1);
        return url.replace(/\//g, path.sep);
    }

    function evalHost(script, callback) {
        window.__adobe_cep__.evalScript(script, callback || function () {});
    }

    function setBusy(message) {
        status.textContent = message;
        progress.classList.remove("hidden");
        retryButton.classList.add("hidden");
        splitButton.disabled = true;
        applyButton.disabled = true;
    }

    function setError(message, retry) {
        status.textContent = message;
        progress.classList.add("hidden");
        splitButton.disabled = false;
        applyButton.disabled = !outputDirectory;
        retryButton.classList.toggle("hidden", !retry);
    }

    function run(executable, args, callback) {
        childProcess.execFile(executable, args, { windowsHide: true }, function (error, stdout, stderr) {
            callback(error, String(stderr || stdout || "").trim());
        });
    }

    function splitClipboardSelection() {
        var root = extensionPath();
        var bridge = path.join(root, "bin", "CKStemBridge.exe");
        var programData = process.env.ProgramData || "C:\\ProgramData";
        var engineRoot = path.join(programData, "Commercial Kings", "CK Stem Splitter", "engine");
        var engine = path.join(engineRoot, "ckstem-engine", "ckstem-engine.exe");
        var modelDirectory = path.join(engineRoot, "models");
        var base = path.join(process.env.APPDATA || os.tmpdir(), "Commercial Kings", "CK Stem Splitter", "Bridge");
        var job = path.join(base, String(Date.now()));
        var input = path.join(job, "selection.wav");
        outputDirectory = path.join(job, "stems");

        fs.mkdirSync(outputDirectory, { recursive: true });
        setBusy("Reading the highlighted audio from Audition…");

        evalHost("ckInvokeCommand('copy')", function (result) {
            if (String(result).indexOf("ERROR:") === 0) {
                outputDirectory = "";
                setError(String(result).substring(6), false);
                return;
            }

            setTimeout(function () {
                run(bridge, ["capture", input], function (captureError) {
                    if (captureError) {
                        outputDirectory = "";
                        setError("Audition did not place standard waveform audio on the clipboard. Keep the selection highlighted and try again.", true);
                        return;
                    }

                    status.textContent = "Separating vocals and instrumental offline…";
                    var args = [
                        "separate", input, outputDirectory,
                        "--model", "htdemucs_ft_vocals",
                        "--small", "--providers", "auto",
                        "--cache-dir", modelDirectory,
                        "--karaoke", "--quiet"
                    ];

                    run(engine, args, function (engineError, details) {
                        var vocalFile = path.join(outputDirectory, "vocals.wav");
                        var instrumentalFile = path.join(outputDirectory, "karaoke.wav");
                        if (engineError || !fs.existsSync(vocalFile) || !fs.existsSync(instrumentalFile)) {
                            outputDirectory = "";
                            setError("Stem separation failed. " + (details || "Reinstall CK Stem Splitter and try again."), false);
                            return;
                        }

                        progress.classList.add("hidden");
                        splitButton.disabled = false;
                        stemSection.classList.remove("disabled");
                        applyButton.disabled = false;
                        status.textContent = "Stems ready. Choose a stem and replace the highlighted selection.";
                    });
                });
            }, 300);
        });
    }

    function applySelectedStem() {
        if (!outputDirectory) return;

        var root = extensionPath();
        var bridge = path.join(root, "bin", "CKStemBridge.exe");
        var fileName = selectedStem === "vocals" ? "vocals.wav" : "karaoke.wav";
        var stemPath = path.join(outputDirectory, fileName);
        setBusy("Applying " + selectedStem + " to the highlighted selection…");

        run(bridge, ["publish", stemPath], function (publishError) {
            if (publishError) {
                setError("Could not prepare the separated stem for Audition.", false);
                return;
            }

            evalHost("ckInvokeCommand('paste')", function (result) {
                progress.classList.add("hidden");
                splitButton.disabled = false;
                applyButton.disabled = false;
                if (String(result).indexOf("ERROR:") === 0) {
                    setError(String(result).substring(6) + ". Re-highlight the original range and click Apply.", false);
                    return;
                }
                status.textContent = "Applied " + selectedStem + ". Use Audition Undo to restore the original.";
            });
        });
    }

    splitButton.addEventListener("click", splitClipboardSelection);
    retryButton.addEventListener("click", splitClipboardSelection);
    applyButton.addEventListener("click", applySelectedStem);

    Array.prototype.forEach.call(document.querySelectorAll('input[name="stem"]'), function (radio) {
        radio.addEventListener("change", function () {
            selectedStem = radio.value;
            Array.prototype.forEach.call(document.querySelectorAll(".stem"), function (label) {
                label.classList.toggle("selected", label.contains(radio));
            });
            applyButton.textContent = "REPLACE SELECTION WITH " + selectedStem.toUpperCase();
        });
    });
}());
