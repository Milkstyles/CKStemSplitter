var CKVoiceInsert = CKVoiceInsert || {};

CKVoiceInsert._json = function (obj) {
    function esc(s) { return String(s).replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\r/g, '\\r').replace(/\n/g, '\\n'); }
    var out = [];
    for (var k in obj) {
        if (!obj.hasOwnProperty(k)) continue;
        var v = obj[k];
        if (typeof v === 'string') out.push('"' + esc(k) + '":"' + esc(v) + '"');
        else if (typeof v === 'boolean' || typeof v === 'number') out.push('"' + esc(k) + '":' + v);
        else if (v === null) out.push('"' + esc(k) + '":null');
    }
    return '{' + out.join(',') + '}';
};

CKVoiceInsert.getInsertionContext = function () {
    try {
        if (!app.activeDocument)
            return CKVoiceInsert._json({ ok:false, error:'Open a Waveform file in Audition first.' });

        var doc = app.activeDocument;
        if (!doc.reflect || doc.reflect.name !== 'WaveDocument')
            return CKVoiceInsert._json({ ok:false, error:'Version 0.1 inserts into the Waveform Editor. Open the target file in Waveform view.' });

        var sampleRate = Number(doc.sampleRate || 0);
        if (!sampleRate)
            return CKVoiceInsert._json({ ok:false, error:'Could not read the Waveform sample rate.' });

        // Audition exposes playheadPosition in samples. When a range is selected, the CTI/in-point
        // is used as the insertion anchor. If future Audition builds expose selectionStart directly,
        // prefer it automatically.
        var startSamples = Number(doc.playheadPosition || 0);
        try {
            if (typeof doc.selectionStart !== 'undefined') startSamples = Number(doc.selectionStart);
            else if (doc.timeSelection && typeof doc.timeSelection.start !== 'undefined') startSamples = Number(doc.timeSelection.start);
        } catch (ignoreSelectionProbe) {}

        return CKVoiceInsert._json({
            ok:true,
            startSamples:startSamples,
            startSeconds:startSamples / sampleRate,
            sampleRate:sampleRate
        });
    } catch (e) {
        return CKVoiceInsert._json({ ok:false, error:String(e) });
    }
};

CKVoiceInsert.insertGeneratedAudio = function (filePath, mode, startSamples) {
    try {
        if (!app.activeDocument || !app.activeDocument.reflect || app.activeDocument.reflect.name !== 'WaveDocument')
            return CKVoiceInsert._json({ ok:false, error:'Waveform document is no longer active.' });

        var doc = app.activeDocument;
        var target = Number(startSamples);
        if (isNaN(target)) target = Number(doc.playheadPosition || 0);

        // Generated WAV is already on the Windows WaveAudio clipboard when this is called.
        // We intentionally collapse any highlighted selection back to its START. That prevents
        // a 3-second selection from clipping/replacing a 10-second generation.
        try { app.invokeCommand(String(Application.COMMAND_EDIT_DESELECTALL)); }
        catch (e1) { app.invokeCommand('Edit.DeselectAll'); }

        doc.playheadPosition = target;

        if (mode === 'overwrite') {
            // Bootstrap: default to normal paste until exact generated-duration out-point is known.
            // Insert/ripple is the product's primary behavior and is fully deterministic.
        }

        try { app.invokeCommand(String(Application.COMMAND_EDIT_PASTE)); }
        catch (e2) { app.invokeCommand('Edit.Paste'); }

        return CKVoiceInsert._json({ ok:true });
    } catch (e) {
        return CKVoiceInsert._json({ ok:false, error:'Audition paste failed: ' + String(e) });
    }
};
