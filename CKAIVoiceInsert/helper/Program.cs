using System;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace CKVoiceClipboard;

internal static class Program
{
    [STAThread]
    private static async Task<int> Main(string[] args)
    {
        try
        {
            if (args.Length == 2 && args[0].Equals("copy", StringComparison.OrdinalIgnoreCase))
                return CopyWaveToClipboard(args[1]);

            if (args.Length == 3 && args[0].Equals("fish-tts", StringComparison.OrdinalIgnoreCase))
                return await GenerateFishAudio(args[1], args[2]);

            Console.Error.WriteLine("Usage:");
            Console.Error.WriteLine("  CKVoiceClipboard.exe copy <wav-file>");
            Console.Error.WriteLine("  CKVoiceClipboard.exe fish-tts <reference-id> <output-wav>");
            return 2;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.ToString());
            return 1;
        }
    }

    private static int CopyWaveToClipboard(string inputPath)
    {
        var wavPath = Path.GetFullPath(inputPath);
        if (!File.Exists(wavPath))
        {
            Console.Error.WriteLine("WAV file not found: " + wavPath);
            return 3;
        }

        var bytes = File.ReadAllBytes(wavPath);
        if (!IsWave(bytes))
        {
            Console.Error.WriteLine("Input is not a RIFF/WAVE file.");
            return 4;
        }

        var data = new DataObject();
        data.SetData(DataFormats.WaveAudio, false, new MemoryStream(bytes, writable: false));
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

    private static async Task<int> GenerateFishAudio(string referenceId, string outputPath)
    {
        var input = await Console.In.ReadToEndAsync();
        var requestInput = JsonSerializer.Deserialize<FishInput>(input, new JsonSerializerOptions
        {
            PropertyNameCaseInsensitive = true
        });

        if (requestInput is null || string.IsNullOrWhiteSpace(requestInput.ApiKey))
        {
            Console.Error.WriteLine("Fish Audio API key was not provided.");
            return 10;
        }

        if (string.IsNullOrWhiteSpace(requestInput.Text))
        {
            Console.Error.WriteLine("Fish Audio text was not provided.");
            return 11;
        }

        var token = requestInput.ApiKey.Trim();
        if (token.StartsWith("Bearer ", StringComparison.OrdinalIgnoreCase))
            token = token.Substring(7).Trim();

        using var client = new HttpClient { Timeout = TimeSpan.FromMinutes(5) };
        client.DefaultRequestHeaders.UserAgent.ParseAdd("CK-AI-Voice-Insert/0.1.0");

        using var request = new HttpRequestMessage(HttpMethod.Post, "https://api.fish.audio/v1/tts");
        request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", token);
        request.Headers.Add("model", string.IsNullOrWhiteSpace(requestInput.Model) ? "s2.1-pro" : requestInput.Model);
        request.Content = new StringContent(
            JsonSerializer.Serialize(new
            {
                text = requestInput.Text,
                reference_id = referenceId,
                format = "wav"
            }),
            Encoding.UTF8,
            "application/json"
        );

        using var response = await client.SendAsync(request, HttpCompletionOption.ResponseHeadersRead);
        var responseBytes = await response.Content.ReadAsByteArrayAsync();
        if (!response.IsSuccessStatusCode)
        {
            var message = Encoding.UTF8.GetString(responseBytes);
            Console.Error.WriteLine($"Fish Audio TTS failed: {(int)response.StatusCode} {response.ReasonPhrase}: {message}");
            return 12;
        }

        if (!IsWave(responseBytes))
        {
            Console.Error.WriteLine("Fish Audio returned data that was not a RIFF/WAVE file.");
            return 13;
        }

        var fullOutputPath = Path.GetFullPath(outputPath);
        Directory.CreateDirectory(Path.GetDirectoryName(fullOutputPath)!);
        await File.WriteAllBytesAsync(fullOutputPath, responseBytes);
        return 0;
    }

    private static bool IsWave(byte[] bytes)
    {
        return bytes.Length >= 44 &&
               bytes[0] == (byte)'R' &&
               bytes[1] == (byte)'I' &&
               bytes[2] == (byte)'F' &&
               bytes[3] == (byte)'F';
    }

    private sealed class FishInput
    {
        public string ApiKey { get; set; } = "";
        public string Text { get; set; } = "";
        public string Model { get; set; } = "";
    }
}
