using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using Microsoft.VisualStudio.Text;
using Microsoft.VisualStudio.Text.Classification;
using Microsoft.VisualStudio.Text.Tagging;
using Microsoft.VisualStudio.Utilities;
using System.Windows.Media;
using Microsoft.VisualStudio.Shell;
using System.Runtime.InteropServices;
using Microsoft.VisualStudio.Language.StandardClassification;
using Microsoft.VisualStudio.Text.Projection;
using AlloyCompiler;

namespace AlloySyntaxHighliting
{
    
    public enum AlloyTokenTypes
    {
        none, keyword, basictype, stringliteral, varorconst, comment
    }

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
        public AlloyTokenTypes type { get; private set; }

        public AlloyTokenTag(AlloyTokenTypes type)
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

        AlloyTokenTypes[] TokenKinds =
        {
            AlloyTokenTypes.none,       // none

		    AlloyTokenTypes.none,       // end_of_file,

		    AlloyTokenTypes.none,    // identifier,
		    AlloyTokenTypes.none,    // long_identifier,

            AlloyTokenTypes.keyword,    // extern_keyword,
		    AlloyTokenTypes.keyword,    // struct_keyword,
		    AlloyTokenTypes.keyword,    // enum_keyword,
		    AlloyTokenTypes.keyword,    // function_keyword,
		    AlloyTokenTypes.keyword,    // macro_keyword,

		    AlloyTokenTypes.keyword,    // import_keyword,
		    AlloyTokenTypes.keyword,    // as_keyword,

		    AlloyTokenTypes.varorconst,    // public_keyword,
		    AlloyTokenTypes.varorconst,    // export_keyword,

		    AlloyTokenTypes.keyword,    // type_keyword,

		    AlloyTokenTypes.varorconst,    // variable_keyword,
		    AlloyTokenTypes.varorconst,    // constant_keyword,

		    AlloyTokenTypes.keyword,    // for_keyword,
		    AlloyTokenTypes.keyword,    // while_keyword,
		    AlloyTokenTypes.keyword,    // if_keyword,
		    AlloyTokenTypes.keyword,    // else_keyword,
		    AlloyTokenTypes.keyword,    // switch_keyword,
		    AlloyTokenTypes.keyword,    // case_keyword,
		    AlloyTokenTypes.keyword,    // return_keyword,

		    AlloyTokenTypes.keyword,    // new_keyword,
		    AlloyTokenTypes.keyword,    // move_keyword,

            AlloyTokenTypes.none,       // pound,
		    AlloyTokenTypes.none,       // at_symbol,

		    AlloyTokenTypes.none,       // reference,

		    AlloyTokenTypes.none,       // comma,
		    AlloyTokenTypes.none,       // colon,
		    AlloyTokenTypes.none,       // semicolon,
		    AlloyTokenTypes.none,       // double_colon,
		    AlloyTokenTypes.none,       // dot,
		    AlloyTokenTypes.none,       // arrow,
		    AlloyTokenTypes.none,       // ellipsis,

		    AlloyTokenTypes.none,       // open_paren,
		    AlloyTokenTypes.none,       // close_paren,
		    AlloyTokenTypes.none,       // open_brace,
		    AlloyTokenTypes.none,       // close_brace,
		    AlloyTokenTypes.none,       // open_bracket,
		    AlloyTokenTypes.none,       // close_bracket,

		    AlloyTokenTypes.none,       // pipe_operator,

		    AlloyTokenTypes.none,       // assignment_operator,

		    AlloyTokenTypes.none,       // unary_operator,
		    AlloyTokenTypes.none,       // binary_operator,

		    AlloyTokenTypes.basictype,    // integer_literal,
		    AlloyTokenTypes.basictype,    // float_literal,
		    AlloyTokenTypes.basictype,    // boolean_literal,
		    AlloyTokenTypes.stringliteral,   // string_literal,
		    AlloyTokenTypes.stringliteral,    // character_literal

            AlloyTokenTypes.comment        // comment
        };

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
                            var tokenSpan = new SnapshotSpan(curSpan.Snapshot, new Span(curLoc, token.Value.Length));
                            if (tokenSpan.IntersectsWith(curSpan))
                                yield return new TagSpan<AlloyTokenTag>(tokenSpan,
                                                                      new AlloyTokenTag(TokenKinds[token.Kind]));
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
        IDictionary<AlloyTokenTypes, IClassificationType> AlloyTypes;

        /// <summary>
        /// Construct the classifier and define search tokens
        /// </summary>
        internal AlloyClassifier(ITextBuffer buffer,
                               ITagAggregator<AlloyTokenTag> AlloyTagAggregator,
                               IClassificationTypeRegistryService typeService)
        {
            _buffer = buffer;
            _aggregator = AlloyTagAggregator;
            AlloyTypes = new Dictionary<AlloyTokenTypes, IClassificationType>();
            AlloyTypes[AlloyTokenTypes.keyword] = typeService.GetClassificationType(PredefinedClassificationTypeNames.Keyword);
            AlloyTypes[AlloyTokenTypes.basictype] = typeService.GetClassificationType(PredefinedClassificationTypeNames.Type);
            AlloyTypes[AlloyTokenTypes.stringliteral] = typeService.GetClassificationType(PredefinedClassificationTypeNames.String);
            AlloyTypes[AlloyTokenTypes.varorconst] = typeService.GetClassificationType("alloycustom");
            AlloyTypes[AlloyTokenTypes.none] = typeService.GetClassificationType(PredefinedClassificationTypeNames.Text);
            AlloyTypes[AlloyTokenTypes.comment] = typeService.GetClassificationType(PredefinedClassificationTypeNames.Comment);
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
}
