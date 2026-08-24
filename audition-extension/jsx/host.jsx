function ckFindCommand(keyword)
{
    var wanted = String(keyword).toLowerCase();
    var properties = Application.reflect.properties;
    var fallback = "";

    for (var i = 0; i < properties.length; ++i)
    {
        var name = properties[i].name;
        if (name.indexOf("COMMAND_") !== 0)
            continue;

        var normalized = name.toLowerCase();
        if (normalized === "command_edit_" + wanted || normalized === "command_" + wanted)
            return Application[name];

        if (!fallback && normalized.indexOf("_" + wanted) >= 0
            && normalized.indexOf("copytonew") < 0
            && normalized.indexOf("copy_to_new") < 0)
            fallback = Application[name];
    }

    return fallback;
}

function ckInvokeCommand(keyword)
{
    try
    {
        var command = ckFindCommand(keyword);
        if (!command)
            return "ERROR:Audition command not found: " + keyword;
        if (!app.isCommandEnabled(command))
            return "ERROR:Select audio in the Waveform Editor first";
        app.invokeCommand(command);
        return "OK";
    }
    catch (error)
    {
        return "ERROR:" + error.toString();
    }
}
