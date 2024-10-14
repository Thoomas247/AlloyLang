using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO.Pipes;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using static System.Net.Mime.MediaTypeNames;

namespace AlloyLanguageServer
{
    internal class Program
    {
        private static LanguageServer languageServer;

        static AutoResetEvent disconnectEvent = new AutoResetEvent(false);

        static void Main(string[] args)
        {
            // Debugger.Launch();

            var stdInPipeName = args[0];
            var stdOutPipeName = args[1];

            var pipeAccessRule = new PipeAccessRule("Everyone", PipeAccessRights.ReadWrite, System.Security.AccessControl.AccessControlType.Allow);
            var pipeSecurity = new PipeSecurity();
            pipeSecurity.AddAccessRule(pipeAccessRule);

            var bufferSize = 256;
            var readerPipe = new NamedPipeServerStream(stdInPipeName, PipeDirection.InOut, 4, PipeTransmissionMode.Message, PipeOptions.Asynchronous, bufferSize, bufferSize, pipeSecurity);
            var writerPipe = new NamedPipeServerStream(stdOutPipeName, PipeDirection.InOut, 4, PipeTransmissionMode.Message, PipeOptions.Asynchronous, bufferSize, bufferSize, pipeSecurity);

            readerPipe.WaitForConnection();
            writerPipe.WaitForConnection();

            languageServer = new LanguageServer(writerPipe, readerPipe);

            languageServer.OnInitialized += OnInitialized;
            languageServer.Disconnected += OnDisconnected;
            languageServer.PropertyChanged += OnLanguageServerPropertyChanged;

            // Wait for work method to signal.
            disconnectEvent.WaitOne();
        }

        private static void OnInitialized(object sender, EventArgs e)
        {
        }

        private static void OnLanguageServerPropertyChanged(object sender, PropertyChangedEventArgs e)
        {
        }

        private static void OnDisconnected(object sender, System.EventArgs e)
        {
            disconnectEvent.Set();
        }
    }
}
