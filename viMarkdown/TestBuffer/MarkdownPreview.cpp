#include "MarkdownPreview.h"
#include "TextDocument.h"

#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextList>
#include <QTextListFormat>
#include <QWheelEvent>

namespace {
constexpr int kBlockNormal = 0;
constexpr int kBlockQuote = 1;
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

//	引用行（"> " または単独の ">"）を判定し、引用本文を返す
bool parseQuote(const QString &line, QString &outText)
{
	if( line.startsWith(QStringLiteral("> ")) ) {
		outText = line.mid(2);
		return true;
	}
	if( line == QStringLiteral(">") ) {
		outText.clear();
		return true;
	}
	return false;
}

LineMgr::LineType parseAndAppendLine(QTextCursor &cursor, const QString &line,
                                     int &activeListIndent, bool &inQuote, qreal basePt)
{
	static const qreal kHeadingScale[7] = { 1.0, 2.2, 1.8, 1.5, 1.3, 1.1, 1.0 };
	static const QTextListFormat::Style kListStyles[3] = {
		QTextListFormat::ListDisc,
		QTextListFormat::ListCircle,
		QTextListFormat::ListSquare,
	};
	static const QTextBlockFormat bodyBlockFmt;
	static const QTextCharFormat bodyCharFmt;

	auto finishQuoteIfNeeded = [&]() {
		if( !inQuote ) return;
		inQuote = false;
		QTextBlock prev = cursor.block().previous();
		if( prev.isValid() && prev.userState() == kBlockQuote ) {
			QTextCursor prevCur(prev);
			QTextBlockFormat pbf = prev.blockFormat();
			pbf.setBottomMargin(8.0);
			prevCur.setBlockFormat(pbf);
		}
	};

	QString parsedText;
	if( const int level = parseHeading(line, parsedText); level > 0 ) {
		finishQuoteIfNeeded();
		activeListIndent = 0;
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
		cf.setFontPointSize(basePt * kHeadingScale[level]);
		cursor.setBlockFormat(bf);
		cursor.block().setUserState(kBlockNormal);
		cursor.insertText(parsedText, cf);
		return LineMgr::LT_HEADING;
	}

	int indent = 0;
	if( parseList(line, indent, parsedText) ) {
		finishQuoteIfNeeded();
		const QTextListFormat::Style style = kListStyles[indent % 3];
		const int listIndent = indent + 1;
		activeListIndent = listIndent;
		cursor.setBlockCharFormat(bodyCharFmt);
		QTextList *curList = cursor.currentList();
		if( !curList || curList->format().indent() != listIndent || curList->format().style() != style ) {
			cursor.setBlockFormat(bodyBlockFmt);
			QTextListFormat lf;
			lf.setStyle(style);
			lf.setIndent(listIndent);
			cursor.createList(lf);
		}
		cursor.block().setUserState(kBlockNormal);
		cursor.insertText(parsedText, bodyCharFmt);
		return LineMgr::LT_LIST;
	}

	if( parseQuote(line, parsedText) ) {
		activeListIndent = 0;
		if( QTextList *curList = cursor.currentList() )
			curList->remove(cursor.block());
		QTextBlockFormat bf;
		bf.setLeftMargin(24.0);
		bf.setRightMargin(14.0);
		bf.setTopMargin(inQuote ? 0.0 : 8.0);
		bf.setBottomMargin(0.0);
		cursor.setBlockFormat(bf);
		cursor.setBlockCharFormat(bodyCharFmt);
		cursor.block().setUserState(kBlockQuote);
		cursor.insertText(parsedText, bodyCharFmt);
		inQuote = true;
		return LineMgr::LT_QUOTE;
	}

	finishQuoteIfNeeded();
	if( QTextList *curList = cursor.currentList() )
		curList->remove(cursor.block());
	cursor.setBlockCharFormat(bodyCharFmt);
	cursor.block().setUserState(kBlockNormal);

	if( line.trimmed().isEmpty() ) {
		activeListIndent = 0;
		cursor.setBlockFormat(bodyBlockFmt);
		cursor.insertText(line, bodyCharFmt);
		return LineMgr::LT_BODY;
	}

	if( activeListIndent > 0 ) {
		//	リスト直後の非空行は継続行としてリストと同じインデントにする
		qsizetype textStart = 0;
		while( textStart < line.size() && (line[textStart] == QChar(' ') || line[textStart] == QChar('\t')) )
			++textStart;
		QTextBlockFormat bf;
		bf.setIndent(activeListIndent);
		cursor.setBlockFormat(bf);
		cursor.insertText(line.mid(textStart), bodyCharFmt);
		return LineMgr::LT_BODY;
	}

	cursor.setBlockFormat(bodyBlockFmt);
	cursor.insertText(line, bodyCharFmt);
	return LineMgr::LT_BODY;
}
}

MarkdownPreview::MarkdownPreview(QWidget *parent)
	: QTextEdit(parent)
{
}

void MarkdownPreview::wheelEvent(QWheelEvent *event)
{
	if( (event->modifiers() & Qt::ControlModifier) != 0 ) {
		const int delta = event->angleDelta().y();
		if( delta != 0 && m_onFontSizeChange )
			m_onFontSizeChange(delta > 0 ? 1 : -1);
		event->accept();
		return;
	}
	QTextEdit::wheelEvent(event);
}

void MarkdownPreview::paintEvent(QPaintEvent *event)
{
	//	引用ブロックの背景矩形と左縦線をパディング付きで先描きする
	if( QTextDocument *doc = document() ) {
		if( QAbstractTextDocumentLayout *layout = doc->documentLayout() ) {
			QPainter p(viewport());
			const QPointF offset(
				horizontalScrollBar() ? -horizontalScrollBar()->value() : 0.0,
				verticalScrollBar() ? -verticalScrollBar()->value() : 0.0
			);
			const QRectF viewRect(viewport()->rect());
			constexpr qreal kPadRight = 10.0;
			constexpr qreal kPadY = 5.0;
			constexpr qreal kBarWidth = 3.0;
			constexpr qreal kPadLeft = kPadY + kBarWidth;	//	縦線の内側に上下と同じ余白を確保
			const QColor quoteBg(0xF0, 0xF8, 0xFF);
			const QColor quoteBar(0x90, 0xB8, 0xDC);

			QTextBlock b = doc->begin();
			while( b.isValid() ) {
				if( b.userState() != kBlockQuote ) {
					b = b.next();
					continue;
				}
				const qreal leftMargin = b.blockFormat().leftMargin();
				QRectF groupRect = layout->blockBoundingRect(b).translated(offset);
				b = b.next();
				while( b.isValid() && b.userState() == kBlockQuote ) {
					groupRect = groupRect.united(layout->blockBoundingRect(b).translated(offset));
					b = b.next();
				}
				QRectF bgRect(
					groupRect.left() + leftMargin - kPadLeft,
					groupRect.top() - kPadY,
					groupRect.width() - leftMargin + kPadLeft + kPadRight,
					groupRect.height() + kPadY * 2.0
				);
				if( bgRect.intersects(viewRect) ) {
					p.fillRect(bgRect, quoteBg);
					p.fillRect(QRectF(bgRect.left(), bgRect.top(), kBarWidth, bgRect.height()), quoteBar);
				}
			}
		}
	}
	QTextEdit::paintEvent(event);
}

void MarkdownPreview::setMarkdown(const QString &markdown)
{
	QTextDocument *doc = document();
	doc->clear();

	QTextCursor cursor(doc);
	cursor.beginEditBlock();

	const qreal basePt = doc->defaultFont().pointSizeF() > 0.0 ? doc->defaultFont().pointSizeF() : 10.0;
	int activeListIndent = 0;
	bool inQuote = false;
	const QStringList lines = markdown.split(QChar('\n'));
	for( qsizetype idx = 0; idx < lines.size(); ++idx ) {
		if( idx > 0 )
			cursor.insertBlock();

		QString line = lines[idx];
		if( line.endsWith(QChar('\r')) )
			line.chop(1);

		parseAndAppendLine(cursor, line, activeListIndent, inQuote, basePt);
	}
	if( inQuote ) {
		QTextBlockFormat bf = cursor.blockFormat();
		bf.setBottomMargin(8.0);
		cursor.setBlockFormat(bf);
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

	const qreal basePt = doc->defaultFont().pointSizeF() > 0.0 ? doc->defaultFont().pointSizeF() : 10.0;
	int activeListIndent = 0;
	bool inQuote = false;
	ViewLineMgr &mgr = srcDoc->lineMgr();
	const int nLines = mgr.lineCount();
	for( int ln = 0; ln < nLines; ++ln ) {
		if( ln > 0 )
			cursor.insertBlock();

		QString line = srcDoc->findBlockByNumber(ln).text();
		if( line.endsWith(QChar('\n')) ) line.chop(1);
		if( line.endsWith(QChar('\r')) ) line.chop(1);

		const LineMgr::LineType type = parseAndAppendLine(cursor, line, activeListIndent, inQuote, basePt);
		mgr.setLineType(ln, type);
	}
	if( inQuote ) {
		QTextBlockFormat bf = cursor.blockFormat();
		bf.setBottomMargin(8.0);
		cursor.setBlockFormat(bf);
	}

	cursor.endEditBlock();
	doc->setModified(false);
	if( QScrollBar *sb = verticalScrollBar() )
		sb->setValue(qMin(prevScroll, sb->maximum()));
}
