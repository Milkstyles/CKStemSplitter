using System;
using System.IO;
using System.Threading;
using System.Windows.Forms;

namespace CKVoiceClipboard;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            if (args.Length != 2 || !args[0].Equals("copy", StringComparison.OrdinalIgnoreCase))
            {
                Console.Error.WriteLine("Usage: CKVoiceClipboard.exe copy <wav-file>");
                return 2;
            }

            var wavPath = Path.GetFullPath(args[1]);
            if (!File.Exists(wavPath))
            {
                Console.Error.WriteLine("WAV file not found: " + wavPath);
                return 3;
            }

            var bytes = File.ReadAllBytes(wavPath);
            if (bytes.Length < 44 || bytes[0] != (byte)'R' || bytes[1] != (byte)'I' || bytes[2] != (byte)'F' || bytes[3] != (byte)'F')
            {
                Console.Error.WriteLine("Input is not a RIFF/WAVE file.");
                return 4;
            }

            var data = new DataObject();
            data.SetData(DataFormats.WaveAudio, false, new MemoryStream(bytes, writable: false));

            // Keep a file-drop representation too. WaveAudio is what Audition's normal Paste path
            // should consume; FileDrop is a harmless fallback for hosts that inspect available formats.
            data.SetData(DataFormats.FileDrop, true, new[] { wavPath });

            Exception? last = null;
            for (var attempt = 0; attempt < 8; attempt++)
            {
                try
                {
                    Clipboard.SetDataObject(data, true, 10, 100);
                    return 0;
                }
                catch (Exception ex)
                {
                    last = ex;
                    Thread.Sleep(80);
                }
            }

            Console.Error.WriteLine("Could not set the Windows audio clipboard: " + last?.Message);
            return 5;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.ToString());
            return 1;
        }
    }
}
