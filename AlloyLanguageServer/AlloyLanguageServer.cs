using AlloyCompiler;
using Microsoft.VisualStudio.LanguageServer.Protocol;
using Newtonsoft.Json.Linq;
using StreamJsonRpc;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace AlloyLanguageServer
{
    public class LanguageServerTarget
    {
        private readonly LanguageServer server;

        public LanguageServerTarget(LanguageServer server)
        {
            this.server = server;
        }

        public event EventHandler OnInitializeCompletion;

        public event EventHandler OnInitialized;

        [JsonRpcMethod(Methods.InitializeName)]
        public object Initialize(JToken arg)
        {
            var capabilities = new VSServerCapabilities();
            capabilities.TextDocumentSync = new TextDocumentSyncOptions();
            capabilities.TextDocumentSync.OpenClose = true;
            capabilities.TextDocumentSync.Change = TextDocumentSyncKind.Full;
            capabilities.RenameProvider = true;
            capabilities.SemanticTokensOptions = new SemanticTokensOptions() { Legend = new SemanticTokensLegend() 
            { TokenTypes = new string[]
                {
                    SemanticTokenTypes.Keyword,     // keyword,
			        SemanticTokenTypes.String,      // basic type
			        SemanticTokenTypes.String,      // string literal
			        SemanticTokenTypes.Modifier,    // var or const
			        SemanticTokenTypes.Comment,     // comment
                    SemanticTokenTypes.Enum,        // enum
                    SemanticTokenTypes.Struct,      // struct
                }

            }, Full = true, Range = true, WorkDoneProgress = false };

            var result = new InitializeResult();
            result.Capabilities = capabilities;

            OnInitializeCompletion?.Invoke(this, new EventArgs());

            return result;
        }

        [JsonRpcMethod(Methods.InitializedName)]
        public void Initialized(JToken arg)
        {
            this.OnInitialized?.Invoke(this, EventArgs.Empty);
        }

        [JsonRpcMethod(Methods.TextDocumentDidOpenName)]
        public void OnTextDocumentOpened(JToken arg)
        {
            var parameter = arg.ToObject<DidOpenTextDocumentParams>();
            server.OnTextDocumentOpened(parameter);
        }

        [JsonRpcMethod(Methods.TextDocumentDidChangeName)]
        public void OnTextDocumentChanged(JToken arg)
        {
            var parameter = arg.ToObject<DidChangeTextDocumentParams>();
            server.OnTextDocumentChanged(parameter);
        }

        [JsonRpcMethod(Methods.TextDocumentSemanticTokensFullName, UseSingleObjectParameterDeserialization = true)]
        public SemanticTokens GetDocumentSemanticTokens(SemanticTokensParams arg, CancellationToken token)
        {
            var result = this.server.GetDocumentSemanticTokens(arg.PartialResultToken, token);
            return result;
        }

        [JsonRpcMethod(Methods.TextDocumentSemanticTokensRangeName, UseSingleObjectParameterDeserialization = true)]
        public SemanticTokens GetDocumentSemanticTokensRange(SemanticTokensRangeParams arg, CancellationToken token)
        {
            var result = this.server.GetDocumentSemanticTokens(arg.PartialResultToken, token);
            return result;
        }
    }

    public class LanguageServer : INotifyPropertyChanged
    {
        private readonly JsonRpc rpc;
        private readonly HeaderDelimitedMessageHandler messageHandler;
        private readonly LanguageServerTarget target;
        private readonly ManualResetEvent disconnectEvent = new ManualResetEvent(false);
        private TextDocumentItem textDocument = null;

        public LanguageServer(Stream sender, Stream reader)
        {
            this.target = new LanguageServerTarget(this);
            this.messageHandler = new HeaderDelimitedMessageHandler(sender, reader);
            this.rpc = new JsonRpc(this.messageHandler, this.target);
            this.rpc.Disconnected += OnRpcDisconnected;

            ((JsonMessageFormatter)this.messageHandler.Formatter).JsonSerializer.Converters.Add(new VSExtensionConverter<TextDocumentIdentifier, VSTextDocumentIdentifier>());

            this.rpc.StartListening();

            this.target.OnInitializeCompletion += OnTargetInitializeCompletion;
            this.target.OnInitialized += OnTargetInitialized;
        }

        public event EventHandler OnInitialized;
        public event EventHandler Disconnected;
        public event PropertyChangedEventHandler PropertyChanged;

        private void OnTargetInitializeCompletion(object sender, EventArgs e)
        {
        }

        private void OnTargetInitialized(object sender, EventArgs e)
        {
            this.OnInitialized?.Invoke(this, EventArgs.Empty);
        }

        public void OnTextDocumentOpened(DidOpenTextDocumentParams messageParams)
        {
            Debugger.Launch();
            this.textDocument = messageParams.TextDocument;
        }

        public void OnTextDocumentChanged(DidChangeTextDocumentParams messageParams)
        {
            this.textDocument.Text = messageParams.ContentChanges[0].Text;
        }

        public void OnTextDocumentClosed(DidCloseTextDocumentParams messageParams)
        {
            this.textDocument = null;
        }

        public JsonRpc Rpc
        {
            get => this.rpc;
        }

        private Range GetHighlightRange(string line, int lineOffset, ref int characterOffset, string wordToMatch)
        {
            if ((characterOffset + wordToMatch.Length) <= line.Length)
            {
                var subString = line.Substring(characterOffset, wordToMatch.Length);
                if (subString.Equals(wordToMatch, StringComparison.OrdinalIgnoreCase))
                {
                    var range = new Range();
                    range.Start = new Position(lineOffset, characterOffset);
                    range.End = new Position(lineOffset, characterOffset + wordToMatch.Length);

                    return range;
                }
            }

            return null;
        }

        public SemanticTokens GetDocumentSemanticTokens(IProgress<SemanticTokensPartialResult> progress, CancellationToken token)
        {
            List<int> tokens = new List<int>();

            if (this.textDocument == null)
            {
                return new SemanticTokens() { Data = tokens.ToArray() };
            }
            //Debugger.Launch();

            //String text = this.textDocument.Text.Replace("\r\n", "\n");
            List<GlobalFunctions.CSharpToken> tokenList = GlobalFunctions.Tokenize(this.textDocument.Text);
            if (tokenList != null)
            {
                int lastLine = 1;
                int lastColumn = 1;
                foreach (GlobalFunctions.CSharpToken tok in tokenList)
                {
                    GlobalFunctions.AlloyTokenTypes tokenType = GlobalFunctions.GetTokenType(tok.Kind);
                    if (tokenType != GlobalFunctions.AlloyTokenTypes.none)
                    {
                        int len = tok.Value.TrimEnd('\r').Length;
                        if (tok.Kind == 49 /*string_literal*/ || tok.Kind == 50 /*character_literal*/) len += 2;  // for strings and single chars we want to cover the quotes

                        tokens.Add(tok.Line - lastLine);           // line
                        tokens.Add(lastLine == tok.Line ? tok.Column - lastColumn : tok.Column-1);         // startChar
                        tokens.Add(len);   // length
                        tokens.Add((int)tokenType-1);    // Token kind
                        tokens.Add(0);                  // Token modifier

                        // Trace.WriteLine("Token " + tok.Value + " at line " + tok.Line + ", column " + tok.Column + ", length " + len + ", kind " + tok.Kind + ", id " + GlobalFunctions.GetTokenType(tok.Kind));

                        lastColumn = tok.Column;
                        lastLine = tok.Line;
                    }

                    token.ThrowIfCancellationRequested();
                }
            }

            return new SemanticTokens() { Data = tokens.ToArray(), ResultId = null };
        }

        private void OnRpcDisconnected(object sender, JsonRpcDisconnectedEventArgs e)
        {
            Exit();
        }

        private void NotifyPropertyChanged(string propertyName)
        {
            this.PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        private Task SendMethodNotificationAsync<TIn>(LspNotification<TIn> method, TIn param)
        {
            return this.rpc.NotifyWithParameterObjectAsync(method.Name, param);
        }

        private Task<TOut> SendMethodRequestAsync<TIn, TOut>(LspRequest<TIn, TOut> method, TIn param)
        {
            return this.rpc.InvokeWithParameterObjectAsync<TOut>(method.Name, param);
        }

        public void WaitForExit()
        {
            this.disconnectEvent.WaitOne();
        }

        public void Exit()
        {
            this.disconnectEvent.Set();

            Disconnected?.Invoke(this, new EventArgs());
        }

    }

}
