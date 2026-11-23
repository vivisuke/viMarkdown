#include "MarkdownPreview.h"
#include "TextDocument.h"

#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QSignalBlocker>
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
constexpr int kBlockCode = 2;
constexpr int kBlockCheckboxBase = 100;	//	kBlockCheckboxBase + 論理行番号（0〜）

QFont codeBlockFont(qreal basePt)
{
	static const QStringList candidates = {
		QStringLiteral("BIZ UDGothic"),
		QStringLiteral("Consolas"),
		QStringLiteral("MS Gothic"),
	};
	for( const QString &name : candidates ) {
		if( QFontDatabase::hasFamily(name) ) {
			QFont f(name);
			f.setPointSizeF(basePt);
			f.setStyleHint(QFont::Monospace);
			return f;
		}
	}
	QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	f.setPointSizeF(basePt);
	return f;
}

//	コードフェンス行（先頭 0〜3 個の空白 + "```"）を判定
bool isCodeFence(const QString &line)
{
	qsizetype i = 0;
	while( i < line.size() && i < 3 && line[i] == QChar(' ') )
		++i;
	return QStringView(line).mid(i).startsWith(u"```");
}
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

//	チェックボックス行（[空白]* (-|*|+ ) \[[ xX]\]  テキスト）を判定し、インデント階層・チェック状態・本文を返す
bool parseCheckbox(const QString &line, int &outIndent, bool &outChecked, QString &outText)
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
	if( i + 6 > line.size() )
		return false;
	const QChar marker = line[i];
	if( marker != QChar('-') && marker != QChar('*') && marker != QChar('+') )
		return false;
	if( line[i + 1] != QChar(' ') && line[i + 1] != QChar('\t') )
		return false;
	if( line[i + 2] != QChar('[') || line[i + 4] != QChar(']') )
		return false;
	const QChar chk = line[i + 3];
	if( chk != QChar(' ') && chk != QChar('x') && chk != QChar('X') )
		return false;
	if( line[i + 5] != QChar(' ') && line[i + 5] != QChar('\t') )
		return false;
	outIndent = spaces / 2;
	outChecked = (chk == QChar('x') || chk == QChar('X'));
	outText = line.mid(i + 6);
	return true;
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

struct ParseState {
	int activeListIndent = 0;
	bool inQuote = false;
	bool inCodeBlock = false;
	bool codeHasLines = false;
	bool hasEmittedBlock = false;
	qreal basePt = 10.0;
};

void finishBlockMarginIfNeeded(QTextCursor &cursor, int expectedState)
{
	QTextBlock b = cursor.block();
	if( b.isValid() && b.userState() == expectedState ) {
		QTextCursor bc(b);
		QTextBlockFormat bf = b.blockFormat();
		bf.setBottomMargin(8.0);
		bc.setBlockFormat(bf);
	}
}

LineMgr::LineType parseAndAppendLine(QTextCursor &cursor, const QString &line, ParseState &st, int srcLine = -1)
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
		if( !st.inQuote ) return;
		st.inQuote = false;
		finishBlockMarginIfNeeded(cursor, kBlockQuote);
	};
	auto beginEmittedBlock = [&]() {
		if( st.hasEmittedBlock )
			cursor.insertBlock();
		st.hasEmittedBlock = true;
	};

	if( isCodeFence(line) ) {
		finishQuoteIfNeeded();
		st.activeListIndent = 0;
		if( !st.inCodeBlock ) {
			st.inCodeBlock = true;
			st.codeHasLines = false;
		} else {
			st.inCodeBlock = false;
			if( st.codeHasLines )
				finishBlockMarginIfNeeded(cursor, kBlockCode);
			st.codeHasLines = false;
		}
		return LineMgr::LT_CODE_FENCE;
	}

	if( st.inCodeBlock ) {
		beginEmittedBlock();
		if( QTextList *curList = cursor.currentList() )
			curList->remove(cursor.block());
		QTextBlockFormat bf;
		bf.setLeftMargin(16.0);
		bf.setRightMargin(14.0);
		bf.setTopMargin(st.codeHasLines ? 0.0 : 8.0);
		bf.setBottomMargin(0.0);
		QTextCharFormat cf;
		cf.setFont(codeBlockFont(st.basePt));
		cursor.setBlockFormat(bf);
		cursor.setBlockCharFormat(cf);
		cursor.block().setUserState(kBlockCode);
		cursor.insertText(line, cf);
		st.codeHasLines = true;
		return LineMgr::LT_CODE_BLOCK;
	}

	QString parsedText;
	if( const int level = parseHeading(line, parsedText); level > 0 ) {
		finishQuoteIfNeeded();
		st.activeListIndent = 0;
		beginEmittedBlock();
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
		cf.setFontPointSize(st.basePt * kHeadingScale[level]);
		cursor.setBlockFormat(bf);
		cursor.block().setUserState(kBlockNormal);
		cursor.insertText(parsedText, cf);
		return LineMgr::LT_HEADING;
	}

	int indent = 0;
	bool checked = false;
	if( parseCheckbox(line, indent, checked, parsedText) ) {
		finishQuoteIfNeeded();
		beginEmittedBlock();
		const int listIndent = indent + 1;
		st.activeListIndent = listIndent;
		cursor.setBlockCharFormat(bodyCharFmt);
		QTextList *curList = cursor.currentList();
		if( !curList || curList->format().indent() != listIndent || curList->format().style() != QTextListFormat::ListDisc ) {
			cursor.setBlockFormat(bodyBlockFmt);
			QTextListFormat lf;
			lf.setStyle(QTextListFormat::ListDisc);
			lf.setIndent(listIndent);
			cursor.createList(lf);
		}
		QTextBlockFormat bf = cursor.blockFormat();
		bf.setMarker(checked ? QTextBlockFormat::MarkerType::Checked : QTextBlockFormat::MarkerType::Unchecked);
		cursor.setBlockFormat(bf);
		cursor.block().setUserState(srcLine >= 0 ? (kBlockCheckboxBase + srcLine) : kBlockNormal);
		cursor.insertText(parsedText, bodyCharFmt);
		return LineMgr::LT_CHECKBOX;
	}

	if( parseList(line, indent, parsedText) ) {
		finishQuoteIfNeeded();
		beginEmittedBlock();
		const QTextListFormat::Style style = kListStyles[indent % 3];
		const int listIndent = indent + 1;
		st.activeListIndent = listIndent;
		cursor.setBlockCharFormat(bodyCharFmt);
		QTextList *curList = cursor.currentList();
		if( !curList || curList->format().indent() != listIndent || curList->format().style() != style ) {
			cursor.setBlockFormat(bodyBlockFmt);
			QTextListFormat lf;
			lf.setStyle(style);
			lf.setIndent(listIndent);
			cursor.createList(lf);
		} else if( cursor.blockFormat().marker() != QTextBlockFormat::MarkerType::NoMarker ) {
			QTextBlockFormat bf = cursor.blockFormat();
			bf.setMarker(QTextBlockFormat::MarkerType::NoMarker);
			cursor.setBlockFormat(bf);
		}
		cursor.block().setUserState(kBlockNormal);
		cursor.insertText(parsedText, bodyCharFmt);
		return LineMgr::LT_LIST;
	}

	if( parseQuote(line, parsedText) ) {
		st.activeListIndent = 0;
		beginEmittedBlock();
		if( QTextList *curList = cursor.currentList() )
			curList->remove(cursor.block());
		QTextBlockFormat bf;
		bf.setLeftMargin(24.0);
		bf.setRightMargin(14.0);
		bf.setTopMargin(st.inQuote ? 0.0 : 8.0);
		bf.setBottomMargin(0.0);
		cursor.setBlockFormat(bf);
		cursor.setBlockCharFormat(bodyCharFmt);
		cursor.block().setUserState(kBlockQuote);
		cursor.insertText(parsedText, bodyCharFmt);
		st.inQuote = true;
		return LineMgr::LT_QUOTE;
	}

	finishQuoteIfNeeded();
	beginEmittedBlock();
	if( QTextList *curList = cursor.currentList() )
		curList->remove(cursor.block());
	cursor.setBlockCharFormat(bodyCharFmt);
	cursor.block().setUserState(kBlockNormal);

	if( line.trimmed().isEmpty() ) {
		st.activeListIndent = 0;
		cursor.setBlockFormat(bodyBlockFmt);
		cursor.insertText(line, bodyCharFmt);
		return LineMgr::LT_BODY;
	}

	if( st.activeListIndent > 0 ) {
		//	リスト直後の非空行は継続行としてリストと同じインデントにする
		qsizetype textStart = 0;
		while( textStart < line.size() && (line[textStart] == QChar(' ') || line[textStart] == QChar('\t')) )
			++textStart;
		QTextBlockFormat bf;
		bf.setIndent(st.activeListIndent);
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

void MarkdownPreview::mousePressEvent(QMouseEvent *event)
{
	if( event->button() == Qt::LeftButton ) {
		QTextCursor cur = cursorForPosition(event->pos());
		QTextBlock block = cur.block();
		if( block.isValid() && block.userState() >= kBlockCheckboxBase ) {
			QTextCursor startCur(block);
			const QRect startRect = cursorRect(startCur);
			const int x = event->pos().x();
			const int y = event->pos().y();
			if( y >= startRect.top() && y <= startRect.bottom()
				&& x >= startRect.left() - 32 && x <= startRect.left() + 2 ) {
				const int srcLine = block.userState() - kBlockCheckboxBase;
				const bool newChecked = (block.blockFormat().marker() != QTextBlockFormat::MarkerType::Checked);
				if( m_onCheckboxToggled )
					m_onCheckboxToggled(srcLine, newChecked);
				event->accept();
				return;
			}
		}
	}
	QTextEdit::mousePressEvent(event);
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
			const QColor codeBg(0xF2, 0xF2, 0xF2);
			const QColor codeBorder(0xD4, 0xD4, 0xD4);
			constexpr qreal kCodePadX = 8.0;

			QTextBlock b = doc->begin();
			while( b.isValid() ) {
				const int state = b.userState();
				if( state != kBlockQuote && state != kBlockCode ) {
					b = b.next();
					continue;
				}
				const qreal leftMargin = b.blockFormat().leftMargin();
				QRectF groupRect = layout->blockBoundingRect(b).translated(offset);
				b = b.next();
				while( b.isValid() && b.userState() == state ) {
					groupRect = groupRect.united(layout->blockBoundingRect(b).translated(offset));
					b = b.next();
				}
				if( state == kBlockQuote ) {
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
				} else {
					QRectF bgRect(
						groupRect.left() + leftMargin - kCodePadX,
						groupRect.top() - kPadY,
						groupRect.width() - leftMargin + kCodePadX + kPadRight,
						groupRect.height() + kPadY * 2.0
					);
					if( bgRect.intersects(viewRect) ) {
						p.fillRect(bgRect, codeBg);
						p.setPen(codeBorder);
						p.drawRect(bgRect);
					}
				}
			}
		}
	}
	QTextEdit::paintEvent(event);
}

void MarkdownPreview::scrollToSourceLine(int srcLine, qreal subLineFrac)
{
	QScrollBar *sb = verticalScrollBar();
	if( !sb || m_srcLineToBlock.isEmpty() )
		return;
	QTextDocument *doc = document();
	QAbstractTextDocumentLayout *layout = doc ? doc->documentLayout() : nullptr;
	if( !layout )
		return;

	srcLine = qBound(0, srcLine, static_cast<int>(m_srcLineToBlock.size()) - 1);
	int bn = m_srcLineToBlock[srcLine];
	if( bn < 0 ) {
		for( int i = srcLine + 1; i < m_srcLineToBlock.size(); ++i ) {
			if( m_srcLineToBlock[i] >= 0 ) {
				bn = m_srcLineToBlock[i];
				subLineFrac = 0.0;
				break;
			}
		}
	}
	if( bn < 0 ) {
		for( int i = srcLine - 1; i >= 0; --i ) {
			if( m_srcLineToBlock[i] >= 0 ) {
				bn = m_srcLineToBlock[i];
				subLineFrac = 1.0;
				break;
			}
		}
	}
	if( bn < 0 )
		return;

	const QTextBlock b = doc->findBlockByNumber(bn);
	if( !b.isValid() )
		return;
	const QRectF rect = layout->blockBoundingRect(b);
	const int targetY = qRound(rect.top() + rect.height() * qBound(0.0, subLineFrac, 1.0));
	sb->setValue(qBound(sb->minimum(), targetY, sb->maximum()));
}

bool MarkdownPreview::topSourceLine(int &outLine, qreal &outFrac) const
{
	QScrollBar *sb = verticalScrollBar();
	if( !sb || m_blockToSrcLine.isEmpty() )
		return false;
	QTextDocument *doc = document();
	QAbstractTextDocumentLayout *layout = doc ? doc->documentLayout() : nullptr;
	if( !layout )
		return false;

	const qreal scrollY = sb->value();
	const QTextBlock b = cursorForPosition(QPoint(0, 0)).block();
	if( !b.isValid() )
		return false;
	const int bn = b.blockNumber();
	if( bn < 0 || bn >= m_blockToSrcLine.size() )
		return false;

	const QRectF rect = layout->blockBoundingRect(b);
	outLine = m_blockToSrcLine[bn];
	outFrac = (rect.height() > 0.0)
		? qBound(0.0, (scrollY - rect.top()) / rect.height(), 1.0)
		: 0.0;
	return true;
}

void MarkdownPreview::setMarkdown(const QString &markdown)
{
	const QSignalBlocker blocker(verticalScrollBar());
	QTextDocument *doc = document();
	doc->clear();

	QTextCursor cursor(doc);
	cursor.beginEditBlock();

	ParseState st;
	st.basePt = doc->defaultFont().pointSizeF() > 0.0 ? doc->defaultFont().pointSizeF() : 10.0;
	const QStringList lines = markdown.split(QChar('\n'));
	m_srcLineToBlock.fill(-1, lines.size());
	m_blockToSrcLine.clear();
	for( qsizetype idx = 0; idx < lines.size(); ++idx ) {
		QString line = lines[idx];
		if( line.endsWith(QChar('\r')) )
			line.chop(1);

		const LineMgr::LineType type = parseAndAppendLine(cursor, line, st, static_cast<int>(idx));
		if( type != LineMgr::LT_CODE_FENCE ) {
			const int bn = cursor.block().blockNumber();
			m_srcLineToBlock[idx] = bn;
			if( m_blockToSrcLine.size() <= bn )
				m_blockToSrcLine.resize(bn + 1, static_cast<int>(idx));
			m_blockToSrcLine[bn] = static_cast<int>(idx);
		}
	}
	if( st.inQuote )
		finishBlockMarginIfNeeded(cursor, kBlockQuote);
	if( st.inCodeBlock && st.codeHasLines )
		finishBlockMarginIfNeeded(cursor, kBlockCode);

	cursor.endEditBlock();
	doc->setModified(false);
	moveCursor(QTextCursor::Start);
}

void MarkdownPreview::setMarkdown(TextDocument *srcDoc)
{
	if( !srcDoc ) {
		m_srcLineToBlock.clear();
		m_blockToSrcLine.clear();
		clear();
		return;
	}

	const QSignalBlocker blocker(verticalScrollBar());
	const int prevScroll = verticalScrollBar() ? verticalScrollBar()->value() : 0;

	QTextDocument *doc = document();
	doc->clear();

	QTextCursor cursor(doc);
	cursor.beginEditBlock();

	ParseState st;
	st.basePt = doc->defaultFont().pointSizeF() > 0.0 ? doc->defaultFont().pointSizeF() : 10.0;
	ViewLineMgr &mgr = srcDoc->lineMgr();
	const int nLines = mgr.lineCount();
	m_srcLineToBlock.fill(-1, nLines);
	m_blockToSrcLine.clear();
	for( int ln = 0; ln < nLines; ++ln ) {
		QString line = srcDoc->findBlockByNumber(ln).text();
		if( line.endsWith(QChar('\n')) ) line.chop(1);
		if( line.endsWith(QChar('\r')) ) line.chop(1);

		const LineMgr::LineType type = parseAndAppendLine(cursor, line, st, ln);
		mgr.setLineType(ln, type);
		if( type != LineMgr::LT_CODE_FENCE ) {
			const int bn = cursor.block().blockNumber();
			m_srcLineToBlock[ln] = bn;
			if( m_blockToSrcLine.size() <= bn )
				m_blockToSrcLine.resize(bn + 1, ln);
			m_blockToSrcLine[bn] = ln;
		}
	}
	if( st.inQuote )
		finishBlockMarginIfNeeded(cursor, kBlockQuote);
	if( st.inCodeBlock && st.codeHasLines )
		finishBlockMarginIfNeeded(cursor, kBlockCode);

	cursor.endEditBlock();
	doc->setModified(false);
	if( QScrollBar *sb = verticalScrollBar() )
		sb->setValue(qMin(prevScroll, sb->maximum()));
}
