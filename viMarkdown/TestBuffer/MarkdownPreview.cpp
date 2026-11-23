#include "MarkdownPreview.h"
#include "TextDocument.h"

#include <QFont>
#include <QScrollBar>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextList>
#include <QTextListFormat>

namespace {
//	見出し行（#〜###### + 空白または行末）を判定し、レベル [1..6] と見出し本文を返す
int parseHeading(const QString &line, QString &outText)
{
	qsizetype i = 0;
	while( i < line.size() && i < 6 && line[i] == QChar('#') )
		++i;
	if( i == 0 )
		return 0;
	if( i < line.size() && line[i] != QChar(' ') && line[i] != QChar('\t') )
		return 0;
	qsizetype textStart = i;
	while( textStart < line.size() && (line[textStart] == QChar(' ') || line[textStart] == QChar('\t')) )
		++textStart;
	outText = line.mid(textStart);
	return static_cast<int>(i);
}

//	箇条書きリスト行（[空白]* (-|*|+ ) テキスト）を判定し、インデント階層 [0..] と本文を返す
bool parseList(const QString &line, int &outIndent, QString &outText)
{
	qsizetype i = 0;
	int spaces = 0;
	while( i < line.size() ) {
		if( line[i] == QChar(' ') ) {
			++spaces;
			++i;
		} else if( line[i] == QChar('\t') ) {
			spaces += 2;
			++i;
		} else {
			break;
		}
	}
	if( i + 1 >= line.size() )
		return false;
	const QChar marker = line[i];
	if( marker != QChar('-') && marker != QChar('*') && marker != QChar('+') )
		return false;
	if( line[i + 1] != QChar(' ') && line[i + 1] != QChar('\t') )
		return false;
	outIndent = spaces / 2;
	outText = line.mid(i + 2);
	return true;
}

LineMgr::LineType parseAndAppendLine(QTextCursor &cursor, const QString &line)
{
	static const qreal kHeadingPtSize[7] = { 10.0, 22.0, 18.0, 15.0, 13.0, 11.0, 10.0 };
	static const QTextListFormat::Style kListStyles[3] = {
		QTextListFormat::ListDisc,
		QTextListFormat::ListCircle,
		QTextListFormat::ListSquare,
	};
	static const QTextBlockFormat bodyBlockFmt;
	static const QTextCharFormat bodyCharFmt;

	QString parsedText;
	if( const int level = parseHeading(line, parsedText); level > 0 ) {
		if( QTextList *curList = cursor.currentList() )
			curList->remove(cursor.block());
		QTextBlockFormat bf;
		bf.setHeadingLevel(level);
		if( level == 1 )
			bf.setAlignment(Qt::AlignCenter);
		bf.setTopMargin(level == 1 ? 12.0 : 10.0);
		bf.setBottomMargin(level == 1 ? 10.0 : 6.0);
		QTextCharFormat cf;
		cf.setFontWeight(QFont::Bold);
		cf.setFontPointSize(kHeadingPtSize[level]);
		cursor.setBlockFormat(bf);
		cursor.insertText(parsedText, cf);
		return LineMgr::LT_HEADING;
	}

	int indent = 0;
	if( parseList(line, indent, parsedText) ) {
		const QTextListFormat::Style style = kListStyles[indent % 3];
		const int listIndent = indent + 1;
		cursor.setBlockCharFormat(bodyCharFmt);
		QTextList *curList = cursor.currentList();
		if( !curList || curList->format().indent() != listIndent || curList->format().style() != style ) {
			cursor.setBlockFormat(bodyBlockFmt);
			QTextListFormat lf;
			lf.setStyle(style);
			lf.setIndent(listIndent);
			cursor.createList(lf);
		}
		cursor.insertText(parsedText, bodyCharFmt);
		return LineMgr::LT_LIST;
	}

	if( QTextList *curList = cursor.currentList() )
		curList->remove(cursor.block());
	cursor.setBlockFormat(bodyBlockFmt);
	cursor.setBlockCharFormat(bodyCharFmt);
	cursor.insertText(line, bodyCharFmt);
	return LineMgr::LT_BODY;
}
}

MarkdownPreview::MarkdownPreview(QWidget *parent)
	: QTextEdit(parent)
{
}

void MarkdownPreview::setMarkdown(const QString &markdown)
{
	QTextDocument *doc = document();
	doc->clear();

	QTextCursor cursor(doc);
	cursor.beginEditBlock();

	const QStringList lines = markdown.split(QChar('\n'));
	for( qsizetype idx = 0; idx < lines.size(); ++idx ) {
		if( idx > 0 )
			cursor.insertBlock();

		QString line = lines[idx];
		if( line.endsWith(QChar('\r')) )
			line.chop(1);

		parseAndAppendLine(cursor, line);
	}

	cursor.endEditBlock();
	doc->setModified(false);
	moveCursor(QTextCursor::Start);
}

void MarkdownPreview::setMarkdown(TextDocument *srcDoc)
{
	if( !srcDoc ) {
		clear();
		return;
	}

	const int prevScroll = verticalScrollBar() ? verticalScrollBar()->value() : 0;

	QTextDocument *doc = document();
	doc->clear();

	QTextCursor cursor(doc);
	cursor.beginEditBlock();

	ViewLineMgr &mgr = srcDoc->lineMgr();
	const int nLines = mgr.lineCount();
	for( int ln = 0; ln < nLines; ++ln ) {
		if( ln > 0 )
			cursor.insertBlock();

		QString line = srcDoc->findBlockByNumber(ln).text();
		if( line.endsWith(QChar('\n')) ) line.chop(1);
		if( line.endsWith(QChar('\r')) ) line.chop(1);

		const LineMgr::LineType type = parseAndAppendLine(cursor, line);
		mgr.setLineType(ln, type);
	}

	cursor.endEditBlock();
	doc->setModified(false);
	if( QScrollBar *sb = verticalScrollBar() )
		sb->setValue(qMin(prevScroll, sb->maximum()));
}
