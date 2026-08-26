function ckFindCommand(keyword)
{
    var wanted = String(keyword).toLowerCase();
    var exact = {
        copy: "COMMAND_EDIT_COPY",
        paste: "COMMAND_EDIT_PASTE",
        selectall: "COMMAND_EDIT_SELECTALL",
        open: "COMMAND_FILE_OPEN",
        close: "COMMAND_FILE_CLOSE",
        saveselectionas: "COMMAND_FILE_SAVESELECTIONAS"
    };
    var properties = Application.reflect.properties;
    var exactName = exact[wanted];
    var fallback = "";
    for (var i = 0; i < properties.length; ++i)
    {
        var name = properties[i].name;
        if (name.indexOf("COMMAND_") !== 0) continue;
        if (exactName && name === exactName) return Application[name];
        if (!fallback && name.toLowerCase().indexOf("_" + wanted) >= 0)
            fallback = Application[name];
    }
    return fallback;
}

function ckInvokeCommand(keyword)
{
    try
    {
        var command = ckFindCommand(keyword);
        if (!command) return "ERROR:Audition command not found: " + keyword;
        if (!app.isCommandEnabled(command))
            return "ERROR:The Audition command is unavailable. Keep the waveform and selection active.";
        app.invokeCommand(command);
        return "OK";
    }
    catch (error)
    {
        return "ERROR:" + error.toString();
    }
}
