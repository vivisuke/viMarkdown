#include "MarkdownPreview.h"
#include "TextDocument.h"

#include <QFont>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>

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

void appendParsedLine(QTextCursor &cursor, const QString &line, int level, const QString &headingText)
{
	static const qreal kHeadingPtSize[7] = { 10.0, 22.0, 18.0, 15.0, 13.0, 11.0, 10.0 };
	static const QTextBlockFormat bodyBlockFmt;
	static const QTextCharFormat bodyCharFmt;

	if( level > 0 ) {
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
		cursor.insertText(headingText, cf);
	} else {
		cursor.setBlockFormat(bodyBlockFmt);
		cursor.insertText(line, bodyCharFmt);
	}
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

		QString headingText;
		const int level = parseHeading(line, headingText);
		appendParsedLine(cursor, line, level, headingText);
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

		QString headingText;
		const int level = parseHeading(line, headingText);
		mgr.setLineType(ln, level > 0 ? LineMgr::LT_HEADING : LineMgr::LT_BODY);
		appendParsedLine(cursor, line, level, headingText);
	}

	cursor.endEditBlock();
	doc->setModified(false);
	moveCursor(QTextCursor::Start);
}
