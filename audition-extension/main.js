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

    function bridgePath() {
        return path.join(extensionPath(), "bin", "CKStemBridge.exe");
    }

    function evalHost(command, callback) {
        window.__adobe_cep__.evalScript("ckInvokeCommand('" + command + "')", callback || function () {});
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

    function invokeWithAutomatedDialog(mode, file, command, callback) {
        var finished = false;
        var hostError = "";
        var helper = childProcess.execFile(bridgePath(), ["dialog", mode, file], { windowsHide: true }, function (error) {
            if (finished) return;
            finished = true;
            if (error) {
                callback(new Error("Audition's file window could not be completed automatically."));
                return;
            }
            if (hostError) {
                callback(new Error(hostError));
                return;
            }
            callback(null);
        });

        setTimeout(function () {
            evalHost(command, function (result) {
                if (String(result).indexOf("ERROR:") === 0) {
                    hostError = String(result).substring(6);
                    if (!finished) {
                        finished = true;
                        helper.kill();
                        callback(new Error(hostError));
                    }
                }
            });
        }, 250);
    }

    function splitHighlightedSelection() {
        var programData = process.env.ProgramData || "C:\\ProgramData";
        var engineRoot = path.join(programData, "Commercial Kings", "CK Stem Splitter", "engine");
        var engine = path.join(engineRoot, "ckstem-engine", "ckstem-engine.exe");
        var modelDirectory = path.join(engineRoot, "models");
        var base = path.join(process.env.APPDATA || os.tmpdir(), "Commercial Kings", "CK Stem Splitter", "Jobs");
        var job = path.join(base, String(Date.now()));
        var input = path.join(job, "selection.wav");
        outputDirectory = path.join(job, "stems");

        fs.mkdirSync(outputDirectory, { recursive: true });
        setBusy("Exporting the highlighted selection from Audition…");

        invokeWithAutomatedDialog("save", input, "saveselectionas", function (exportError) {
            if (exportError || !fs.existsSync(input)) {
                outputDirectory = "";
                setError((exportError && exportError.message) || "Audition did not export the highlighted selection.", true);
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
                status.textContent = "Stems ready. Choose one and replace the original highlighted range.";
            });
        });
    }

    function applySelectedStem() {
        if (!outputDirectory) return;
        var fileName = selectedStem === "vocals" ? "vocals.wav" : "karaoke.wav";
        var stemPath = path.join(outputDirectory, fileName);
        setBusy("Applying " + selectedStem + " to the highlighted selection…");

        invokeWithAutomatedDialog("open", stemPath, "open", function (openError) {
            if (openError) {
                setError(openError.message, false);
                return;
            }

            setTimeout(function () {
                evalHost("selectall", function (selectResult) {
                    if (String(selectResult).indexOf("ERROR:") === 0) {
                        setError("Audition could not select the generated stem.", false);
                        return;
                    }
                    evalHost("copy", function (copyResult) {
                        if (String(copyResult).indexOf("ERROR:") === 0) {
                            setError("Audition could not copy the generated stem.", false);
                            return;
                        }
                        evalHost("close", function (closeResult) {
                            if (String(closeResult).indexOf("ERROR:") === 0) {
                                setError("Audition could not return to the original waveform.", false);
                                return;
                            }
                            setTimeout(function () {
                                evalHost("paste", function (pasteResult) {
                                    progress.classList.add("hidden");
                                    splitButton.disabled = false;
                                    applyButton.disabled = false;
                                    if (String(pasteResult).indexOf("ERROR:") === 0) {
                                        setError("Re-highlight the original range and click Apply again.", false);
                                        return;
                                    }
                                    status.textContent = "Applied " + selectedStem + ". Use Audition Undo to restore the original.";
                                });
                            }, 350);
                        });
                    });
                });
            }, 500);
        });
    }

    splitButton.addEventListener("click", splitHighlightedSelection);
    retryButton.addEventListener("click", splitHighlightedSelection);
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
