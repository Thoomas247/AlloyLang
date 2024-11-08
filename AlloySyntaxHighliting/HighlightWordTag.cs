using Microsoft.VisualStudio.LanguageServer.Client;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Threading;
using Microsoft.VisualStudio.Utilities;
using StreamJsonRpc;
using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;

namespace AlloySyntaxHighliting
{
#if USE_LANGUAGE_SERVER
    [ContentType("alloy")]
    [Export(typeof(ILanguageClient))]
    [RunOnContext(RunningContext.RunOnHost)]
    public class AlloyLanguageClient : ILanguageClient
    {
        public AlloyLanguageClient()
        {
            Instance = this;
        }

        internal static AlloyLanguageClient Instance
        {
            get;
            set;
        }

        internal JsonRpc Rpc
        {
            get;
            set;
        }

        public event AsyncEventHandler<EventArgs> StartAsync;
        public event AsyncEventHandler<EventArgs> StopAsync;

        public string Name => "Alloy Language Extension";

        public IEnumerable<string> ConfigurationSections
        {
            get
            {
                yield return "alloy";
            }
        }

        public object InitializationOptions => null;

        public IEnumerable<string> FilesToWatch => null;

        public object CustomMessageTarget => null;

        public bool ShowNotificationOnInitializeFailed => true;

        public async Task<Connection> ActivateAsync(CancellationToken token)
        {
            // Debugger.Launch();

            var stdInPipeName = "AlloyLanguageServer-" + Guid.NewGuid().ToString();
            var stdOutPipeName = "AlloyLanguageServer-" + Guid.NewGuid().ToString();

            var pipeAccessRule = new PipeAccessRule("Everyone", PipeAccessRights.ReadWrite, System.Security.AccessControl.AccessControlType.Allow);
            var pipeSecurity = new PipeSecurity();
            pipeSecurity.AddAccessRule(pipeAccessRule);

            var readerPipe = new NamedPipeClientStream(stdInPipeName);
            var writerPipe = new NamedPipeClientStream(stdOutPipeName);

            ProcessStartInfo info = new ProcessStartInfo();
            // var programPath = Path.Combine("X:\\Projects\\AlloyLang\\AlloyLanguageServer\\bin\\x64\\Debug", @"AlloyLanguageServer.exe");
            var programPath = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "Server", @"AlloyLanguageServer.exe");
            info.FileName = programPath;
            info.WorkingDirectory = Path.GetDirectoryName(programPath);
            info.Arguments = stdOutPipeName + " " + stdInPipeName;

            System.Diagnostics.Process process = new System.Diagnostics.Process();
            process.StartInfo = info;

            if (process.Start())
            {
                await readerPipe.ConnectAsync();
                await writerPipe.ConnectAsync();

                return new Connection(readerPipe, writerPipe);
            }

            return null;
        }

        public async Task OnLoadedAsync()
        {
            if (StartAsync != null)
            {
                await StartAsync.InvokeAsync(this, EventArgs.Empty);
            }
        }

        public async Task StopServerAsync()
        {
            if (StopAsync != null)
            {
                await StopAsync.InvokeAsync(this, EventArgs.Empty);
            }
        }

        public Task OnServerInitializedAsync()
        {
            return Task.CompletedTask;
        }

        public Task AttachForCustomMessageAsync(JsonRpc rpc)
        {
            this.Rpc = rpc;

            return Task.CompletedTask;
        }

        public Task<InitializationFailureContext> OnServerInitializeFailedAsync(ILanguageClientInitializationInfo initializationState)
        {
            string message = "Alloy Language Client failed to activate! :(";
            string exception = initializationState.InitializationException?.ToString() ?? string.Empty;
            message = $"{message}\n {exception}";

            var failureContext = new InitializationFailureContext()
            {
                FailureMessage = message,
            };

            return Task.FromResult(failureContext);
        }
    }

#pragma warning disable 649
    public class AlloyContentDefinition
    {
        [Export]
        [Name("alloy")]
        [BaseDefinition(CodeRemoteContentDefinition.CodeRemoteBaseTypeName)]
        internal static ContentTypeDefinition AlloyContentTypeDefinition;


        [Export]
        [FileExtension(".alloy")]
        [ContentType("alloy")]
        internal static FileExtensionToContentTypeDefinition AlloyFileExtensionDefinition;
    }
#pragma warning restore 649

#endif  // USE_LANGUAGE_SERVER

#if VS2022_SPECIFIC
    internal static class OrdinaryClassificationDefinition
    {
        #region Type definition

        /// <summary>
        /// Defines the "AlloyCustom" classification type.
        /// </summary>
        [Export(typeof(ClassificationTypeDefinition))]
        [Name("alloycustom")]
        internal static ClassificationTypeDefinition AlloyCustom = null;

        #endregion
    }

    [Export(typeof(ITaggerProvider))]
    [ContentType("alloy")]
    [TagType(typeof(AlloyTokenTag))]
    internal sealed class AlloyTokenTagProvider : ITaggerProvider
    {

        public ITagger<T> CreateTagger<T>(ITextBuffer buffer) where T : ITag
        {
            return new AlloyTokenTagger(buffer) as ITagger<T>;
        }
    }

    public class AlloyTokenTag : ITag
    {
        public GlobalFunctions.AlloyTokenTypes type { get; private set; }

        public AlloyTokenTag(GlobalFunctions.AlloyTokenTypes type)
        {
            this.type = type;
        }
    }

    internal sealed class AlloyTokenTagger : ITagger<AlloyTokenTag>
    {

        ITextBuffer _buffer;

        internal AlloyTokenTagger(ITextBuffer buffer)
        {
            _buffer = buffer;
        }

        public event EventHandler<SnapshotSpanEventArgs> TagsChanged
        {
            add { }
            remove { }
        }

    public IEnumerable<ITagSpan<AlloyTokenTag>> GetTags(NormalizedSnapshotSpanCollection spans)
        {
            foreach (SnapshotSpan curSpan in spans)
            {
                ITextSnapshotLine containingLine = curSpan.Start.GetContainingLine();

                List<GlobalFunctions.CSharpToken> tokens = GlobalFunctions.Tokenize(containingLine.GetText());
                if (tokens != null)
                {
                    foreach (GlobalFunctions.CSharpToken token in tokens)
                    {
                        int curLoc = containingLine.Start.Position + token.Column - 1;
                        if (token.Kind != 0
                            && curLoc+ token.Value.Length < curSpan.Snapshot.Length)
                        {
                            int len = token.Value.Length;
                            if (token.Kind == 49 /*string_literal*/ || token.Kind == 50 /*character_literal*/) len += 2;  // for strings and single chars we want to cover the quotes
                            var tokenSpan = new SnapshotSpan(curSpan.Snapshot, new Span(curLoc, len));
                            if (tokenSpan.IntersectsWith(curSpan))
                                yield return new TagSpan<AlloyTokenTag>(tokenSpan,
                                                                      new AlloyTokenTag(GlobalFunctions.GetTokenType(token.Kind)));
                        }
                    }
                }
            }

        }

        private static bool FindMatchingCloseChar(SnapshotPoint startPoint, char open, char close, int maxLines, out SnapshotSpan pairSpan)
        {
            //
            // Helper function to find closing single or double quotes
            //
            pairSpan = new SnapshotSpan(startPoint.Snapshot, 1, 1);
            ITextSnapshotLine line = startPoint.GetContainingLine();
            string lineText = line.GetText();
            int lineNumber = line.LineNumber;
            int offset = startPoint.Position - line.Start.Position + 1;

            int stopLineNumber = startPoint.Snapshot.LineCount - 1;
            if (maxLines > 0)
                stopLineNumber = Math.Min(stopLineNumber, lineNumber + maxLines);

            int openCount = 0;
            while (true)
            {
                //walk the entire line
                while (offset < line.Length)
                {
                    char currentChar = lineText[offset];
                    if (currentChar == close) //found the close character
                    {
                        if (openCount > 0)
                        {
                            openCount--;
                        }
                        else    //found the matching close
                        {
                            pairSpan = new SnapshotSpan(startPoint.Snapshot, line.Start + offset, 1);
                            return true;
                        }
                    }
                    else if (currentChar == open) // this is another open
                    {
                        openCount++;
                    }
                    offset++;
                }

                //move on to the next line
                if (++lineNumber > stopLineNumber)
                    break;

                line = line.Snapshot.GetLineFromLineNumber(lineNumber);
                lineText = line.GetText();
                offset = 0;
            }

            return false;
        }

        private static bool FindMatchingOpenChar(SnapshotPoint startPoint, char open, char close, int maxLines, out SnapshotSpan pairSpan)
        {
            //
            // Helper function to find closing single or double quotes
            //
            pairSpan = new SnapshotSpan(startPoint, startPoint);

            ITextSnapshotLine line = startPoint.GetContainingLine();

            int lineNumber = line.LineNumber;
            int offset = startPoint - line.Start - 1; //move the offset to the character before this one

            //if the offset is negative, move to the previous line
            if (offset < 0)
            {
                line = line.Snapshot.GetLineFromLineNumber(--lineNumber);
                offset = line.Length - 1;
            }

            string lineText = line.GetText();

            int stopLineNumber = 0;
            if (maxLines > 0)
                stopLineNumber = Math.Max(stopLineNumber, lineNumber - maxLines);

            int closeCount = 0;

            while (true)
            {
                // Walk the entire line
                while (offset >= 0)
                {
                    char currentChar = lineText[offset];

                    if (currentChar == open)
                    {
                        if (closeCount > 0)
                        {
                            closeCount--;
                        }
                        else // We've found the open character
                        {
                            pairSpan = new SnapshotSpan(line.Start + offset, 1); //we just want the character itself
                            return true;
                        }
                    }
                    else if (currentChar == close)
                    {
                        closeCount++;
                    }
                    offset--;
                }

                // Move to the previous line
                if (--lineNumber < stopLineNumber)
                    break;

                line = line.Snapshot.GetLineFromLineNumber(lineNumber);
                lineText = line.GetText();
                offset = line.Length - 1;
            }
            return false;
        }
    }

    #region Format definition
    /// <summary>
    /// Defines the editor format for the structure classification type. Text is colored LightBlue
    /// </summary>
    [Export(typeof(EditorFormatDefinition))]
    [ClassificationType(ClassificationTypeNames = "alloycustom")]
    [Name("alloycustom")]
    //this should be visible to the end user
    [UserVisible(false)]
    //set the priority to be after the default classifiers
    [Order(Before = Priority.Default)]
    internal sealed class AlloyCustom : ClassificationFormatDefinition
    {
        /// <summary>
        /// Defines the visual format for the "AlloyCustom" classification type
        /// </summary>
        public AlloyCustom()
        {
            DisplayName = "Custom"; //human readable version of the name
            ForegroundColor = Colors.LightBlue;
        }
    }
    #endregion //Format definition

    [Export(typeof(ITaggerProvider))]
    [ContentType("alloy")]
    [TagType(typeof(ClassificationTag))]
    internal sealed class AlloyClassifierProvider : ITaggerProvider
    {
        [Export]
        [Name("alloy")]
        [BaseDefinition("code")]
        internal static ContentTypeDefinition AlloyContentType = null;

        [Export]
        [FileExtension(".alloy")]
        [ContentType("alloy")]
        internal static FileExtensionToContentTypeDefinition AlloyFileType = null;

        [Import]
        internal IClassificationTypeRegistryService ClassificationTypeRegistry = null;

        [Import]
        internal IBufferTagAggregatorFactoryService aggregatorFactory = null;

        public ITagger<T> CreateTagger<T>(ITextBuffer buffer) where T : ITag
        {

            ITagAggregator<AlloyTokenTag> AlloyTagAggregator =
                                            aggregatorFactory.CreateTagAggregator<AlloyTokenTag>(buffer);

            return new AlloyClassifier(buffer, AlloyTagAggregator, ClassificationTypeRegistry) as ITagger<T>;
        }
    }

    internal sealed class AlloyClassifier : ITagger<ClassificationTag>
    {
        ITextBuffer _buffer;
        ITagAggregator<AlloyTokenTag> _aggregator;
        IDictionary<GlobalFunctions.AlloyTokenTypes, IClassificationType> AlloyTypes;

        /// <summary>
        /// Construct the classifier and define search tokens
        /// </summary>
        internal AlloyClassifier(ITextBuffer buffer,
                               ITagAggregator<AlloyTokenTag> AlloyTagAggregator,
                               IClassificationTypeRegistryService typeService)
        {
            _buffer = buffer;
            _aggregator = AlloyTagAggregator;
            AlloyTypes = new Dictionary<GlobalFunctions.AlloyTokenTypes, IClassificationType>();
            AlloyTypes[GlobalFunctions.AlloyTokenTypes.keyword] = typeService.GetClassificationType(PredefinedClassificationTypeNames.Keyword);
            AlloyTypes[GlobalFunctions.AlloyTokenTypes.basictype] = typeService.GetClassificationType(PredefinedClassificationTypeNames.Type);
            AlloyTypes[GlobalFunctions.AlloyTokenTypes.stringliteral] = typeService.GetClassificationType(PredefinedClassificationTypeNames.String);
            AlloyTypes[GlobalFunctions.AlloyTokenTypes.varorconst] = typeService.GetClassificationType("alloycustom");
            AlloyTypes[GlobalFunctions.AlloyTokenTypes.none] = typeService.GetClassificationType(PredefinedClassificationTypeNames.Text);
            AlloyTypes[GlobalFunctions.AlloyTokenTypes.comment] = typeService.GetClassificationType(PredefinedClassificationTypeNames.Comment);
        }

        public event EventHandler<SnapshotSpanEventArgs> TagsChanged
        {
            add { }
            remove { }

        }

        /// <summary>
        /// Search the given span for any instances of classified tags
        /// </summary>
        public IEnumerable<ITagSpan<ClassificationTag>> GetTags(NormalizedSnapshotSpanCollection spans)
        {
            foreach (var tagSpan in _aggregator.GetTags(spans))
            {

                var tagSpans = tagSpan.Span.GetSpans(spans[0].Snapshot);
                yield return
                    new TagSpan<ClassificationTag>(tagSpans[0],
                                                   new ClassificationTag(AlloyTypes[tagSpan.Tag.type]));
            }
        }
    }
#endif
}
