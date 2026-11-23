#include "TextEdit.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

namespace {
const int LEFT_MARGIN = 2;

struct HighlightSpan {
	qsizetype start;
	qsizetype length;
	QColor color;
};

//	論理行 1 行を解析して Markdown の強調スパンを返す（オンデマンド解析）
QVector<HighlightSpan> highlightMarkdownLine(const QString &lineText, LineMgr::LineType lineType,
                                             const QColor &headingColor, const QColor &emphColor)
{
	QVector<HighlightSpan> spans;
	if( lineText.isEmpty() )
		return spans;

	if( lineType == LineMgr::LT_HEADING ) {
		spans.push_back({0, lineText.size(), headingColor});
		return spans;
	}

	qsizetype inlineStart = 0;
	if( lineType == LineMgr::LT_LIST ) {
		while( inlineStart < lineText.size()
			&& (lineText[inlineStart] == QChar(' ') || lineText[inlineStart] == QChar('\t')) )
			++inlineStart;
		if( inlineStart + 2 <= lineText.size() )
			inlineStart += 2;	//	リストマーカー（"- " / "* " / "+ "）をスキップ
	}

	static const QRegularExpression reBoldItalic(QStringLiteral(R"(\*\*\*([^\*]+)\*\*\*)"));
	static const QRegularExpression reBold(QStringLiteral(R"((?<![\*\\])\*\*([^\\\*]+)\*\*)"));
	static const QRegularExpression reItalic(QStringLiteral(R"((?<![\*\\])\*([^\\\*]+)\*)"));
	static const QRegularExpression reStrike(QStringLiteral(R"((?<![\*\\])~~([^\\~]+)~~)"));

	auto collect = [&](const QRegularExpression &re) {
		auto it = re.globalMatch(lineText, inlineStart);
		while( it.hasNext() ) {
			auto m = it.next();
			spans.push_back({m.capturedStart(), m.capturedLength(), emphColor});
		}
	};
	collect(reBoldItalic);
	collect(reBold);
	collect(reItalic);
	collect(reStrike);
	return spans;
}

QFont defaultEditorFont()
{
	const QStringList candidates = {
		QStringLiteral("BIZ UDGothic"),
		QStringLiteral("MS Gothic"),
	};
	for( const QString &name : candidates ) {
		if( QFontDatabase::hasFamily(name) ) {
			QFont f(name, 10);
			f.setStyleHint(QFont::Monospace);
			return f;
		}
	}
	return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}
}

TextEdit::TextEdit(QWidget *parent)
	: QAbstractScrollArea(parent), m_cursor(&m_doc)
{
	setFont(defaultEditorFont());
	setFocusPolicy(Qt::StrongFocus);
	setAttribute(Qt::WA_InputMethodEnabled, true);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	viewport()->setCursor(Qt::IBeamCursor);
	m_lineNumberArea = new LineNumberArea(this);
	setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
	updateLineNumberAreaGeometry();
	connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
			[this](int) { viewport()->update(); m_lineNumberArea->update(); });
}

void TextEdit::setPlainText(const QString &text)
{
	m_preedit.clear();
	m_preeditCursor = 0;
	m_doc.setPlainText(text);
	m_cursor.setPosition(0);
	verticalScrollBar()->setValue(0);
	updateWrapWidth();
	updateScrollBars();
	viewport()->update();
	m_lineNumberArea->update();
}

void TextEdit::setTextCursor(const TextCursor &cursor)
{
	if( cursor.document() == &m_doc )
		m_cursor = cursor;
	else
		m_cursor.setPosition(cursor.position());
	afterChange();
}

void TextEdit::setLineSpacing(int extraPx)
{
	m_lineSpacing = extraPx < 0 ? 0 : extraPx;
	updateScrollBars();
	viewport()->update();
	m_lineNumberArea->update();
}

void TextEdit::setEditorFont(const QFont &f)
{
	QFont font = f;
	font.setStyleHint(QFont::Monospace);
	setFont(font);
	setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
	updateLineNumberAreaGeometry();
	updateWrapWidth();
	updateScrollBars();
	viewport()->update();
	m_lineNumberArea->update();
}

void TextEdit::setMarkdownHighlight(bool enabled)
{
	if( m_markdownHighlight == enabled ) return;
	m_markdownHighlight = enabled;
	viewport()->update();
}

void TextEdit::setHeadingColor(const QColor &color)
{
	if( !color.isValid() || m_headingColor == color ) return;
	m_headingColor = color;
	viewport()->update();
}

void TextEdit::setEmphasisColor(const QColor &color)
{
	if( !color.isValid() || m_emphasisColor == color ) return;
	m_emphasisColor = color;
	viewport()->update();
}

void TextEdit::setSelectionColor(const QColor &color)
{
	if( !color.isValid() || m_selectionColor == color ) return;
	m_selectionColor = color;
	viewport()->update();
}

void TextEdit::setTabWidth(int width)
{
	m_doc.lineMgr().setTabWidth(width);
	updateScrollBars();
	viewport()->update();
	m_lineNumberArea->update();
}

void TextEdit::undo()
{
	if( !m_doc.canUndo() ) return;
	int pos = m_doc.undo();
	m_cursor.setPosition(pos);
	afterChange(true);
}

void TextEdit::redo()
{
	if( !m_doc.canRedo() ) return;
	int pos = m_doc.redo();
	m_cursor.setPosition(pos);
	afterChange(true);
}

void TextEdit::onSaved()
{
	m_doc.onSaved();
	m_lineNumberArea->update();
}

int TextEdit::lineHeight() const
{
	return fontMetrics().height() + m_lineSpacing;
}

int TextEdit::pageLines() const
{
	int n = viewport()->height() / lineHeight();
	return n < 1 ? 1 : n;
}

int TextEdit::tabStopPixels(const QFontMetrics &fm) const
{
	int sp = fm.horizontalAdvance(QChar(' '));
	if( sp <= 0 ) sp = 1;
	int tw = m_doc.lineMgr().tabWidth();
	if( tw < 1 ) tw = 1;
	return sp * tw;
}

int TextEdit::drawLineText(QPainter &painter, int x, int yBaseline, const QString &text, const QFontMetrics &fm, int tabPx) const
{
	qsizetype pos = 0;
	while( pos < text.size() ) {
		qsizetype tab = text.indexOf(QChar('\t'), pos);
		if( tab < 0 ) {
			QString chunk = text.mid(pos);
			painter.drawText(x, yBaseline, chunk);
			x += fm.horizontalAdvance(chunk);
			break;
		}
		if( tab > pos ) {
			QString chunk = text.mid(pos, tab - pos);
			painter.drawText(x, yBaseline, chunk);
			x += fm.horizontalAdvance(chunk);
		}
		int rel = x - LEFT_MARGIN;
		x += tabPx - (rel % tabPx);
		pos = tab + 1;
	}
	return x;
}

int TextEdit::xOfPosInViewLine(int viewLine, TextDocument::pos_t pos) const
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	TextDocument::pos_t s = mgr.viewLineStartPos(viewLine);
	const QFontMetrics fm = fontMetrics();
	const int tabPx = tabStopPixels(fm);
	int x = LEFT_MARGIN;
	for( TextDocument::pos_t i = s; i < pos; ++i ) {
		QChar ch = m_doc.characterAt(i);
		if( ch == QChar('\t') ) {
			int rel = x - LEFT_MARGIN;
			x += tabPx - (rel % tabPx);
		} else {
			x += fm.horizontalAdvance(ch);
		}
	}
	return x;
}

int TextEdit::xOfPos(TextDocument::pos_t pos) const
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	return xOfPosInViewLine(mgr.viewLineAtPos(pos), pos);
}

TextDocument::pos_t TextEdit::posFromPoint(const QPoint &pt) const
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	if( mgr.viewLineCount() == 0 ) return 0;
	int viewLine = verticalScrollBar()->value() + pt.y() / lineHeight();
	if( viewLine < 0 ) viewLine = 0;
	if( viewLine >= mgr.viewLineCount() ) viewLine = mgr.viewLineCount() - 1;
	TextDocument::pos_t s = mgr.viewLineStartPos(viewLine);
	TextDocument::pos_t len = mgr.viewLineLength(viewLine);
	const QFontMetrics fm = fontMetrics();
	const int tabPx = tabStopPixels(fm);
	int x = LEFT_MARGIN;
	for( TextDocument::pos_t i = 0; i < len; ++i ) {
		QChar ch = m_doc.characterAt(s + i);
		int w;
		if( ch == QChar('\t') ) {
			int rel = x - LEFT_MARGIN;
			w = tabPx - (rel % tabPx);
		} else {
			w = fm.horizontalAdvance(ch);
		}
		if( pt.x() < x + w / 2 ) return s + i;
		x += w;
	}
	return s + len;
}

void TextEdit::updateScrollBars()
{
	int maxTop = m_doc.lineMgr().viewLineCount() - pageLines();
	if( maxTop < 0 ) maxTop = 0;
	QScrollBar *bar = verticalScrollBar();
	bar->setRange(0, maxTop);
	bar->setPageStep(pageLines());
	bar->setSingleStep(1);
}

void TextEdit::updateWrapWidth()
{
	const QFontMetricsF fmF(font());
	qreal unit = fmF.horizontalAdvance(QChar(' '));
	if( unit <= 0.0 ) unit = 1.0;
	int avail = viewport()->width() - LEFT_MARGIN * 2;
	int units = (int)(avail / unit);
	if( units < 1 ) units = 1;
	if( m_doc.lineMgr().lineWidth() != units ) {
		m_doc.lineMgr().setLineWidth(units);
		updateScrollBars();
	}
}

void TextEdit::ensureCursorVisible()
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	int cv = mgr.viewLineAtPos(m_cursor.position());
	QScrollBar *bar = verticalScrollBar();
	int top = bar->value();
	if( cv < top )
		bar->setValue(cv);
	else if( cv >= top + pageLines() )
		bar->setValue(cv - pageLines() + 1);
}

void TextEdit::afterChange(bool textChanged)
{
	if( textChanged && m_onTextChanged )
		m_onTextChanged();
	updateScrollBars();
	ensureCursorVisible();
	viewport()->update();
	m_lineNumberArea->update();
	QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);
}

int TextEdit::lineNumberAreaWidth() const
{
	return fontMetrics().horizontalAdvance(QChar(' ')) * 8;	//	半角8文字分
}

void TextEdit::updateLineNumberAreaGeometry()
{
	QRect cr = contentsRect();
	m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void TextEdit::lineNumberAreaPaintEvent(QPaintEvent *event)
{
	static const QColor kColorUnmodified(0xDC, 0xDC, 0xDC);	//	未編集（明グレー）
	static const QColor kColorModified(0xFF, 0xFF, 0xFF);	//	編集＆未保存（白）
	static const QColor kColorSaved(0xC0, 0xE0, 0xC0);		//	編集＆保存済み（淡い緑）

	QPainter painter(m_lineNumberArea);
	painter.fillRect(event->rect(), kColorUnmodified);
	const QFontMetrics fm = fontMetrics();
	const int lh = lineHeight();
	const int top = verticalScrollBar()->value();
	const ViewLineMgr &mgr = m_doc.lineMgr();
	const int rows = m_lineNumberArea->height() / lh + 1;
	const int areaW = m_lineNumberArea->width();
	const int right = areaW - fm.horizontalAdvance(QChar(' ')) * 2;	//	右端に半角2文字分の余白を空ける
	painter.setPen(palette().color(QPalette::Text));
	for( int i = 0; i < rows; ++i ) {
		int viewLine = top + i;
		if( viewLine >= mgr.viewLineCount() ) break;
		int ln = mgr.viewLineToLine(viewLine);
		const LineMgr::LineState st = mgr.lineState(ln);
		if( st != LineMgr::LS_UNMODIFIED ) {
			const QColor &bg = (st == LineMgr::LS_SAVED) ? kColorSaved : kColorModified;
			painter.fillRect(0, i * lh, areaW, lh, bg);
		}
		if( viewLine != mgr.lineToViewLine(ln) ) continue;	//	折り返しの継続行には番号を付けない
		QString num = QString::number(ln + 1);
		painter.drawText(right - fm.horizontalAdvance(num), i * lh + fm.ascent() + 2, num);
	}
}

void TextEdit::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter painter(viewport());
	const QFontMetrics fm = fontMetrics();
	const int tabPx = tabStopPixels(fm);
	const int lh = lineHeight();
	const int top = verticalScrollBar()->value();
	const ViewLineMgr &mgr = m_doc.lineMgr();
	const int rows = viewport()->height() / lh + 1;
	const TextDocument::pos_t selS = m_cursor.selectionStart();
	const TextDocument::pos_t selE = m_cursor.selectionEnd();
	const bool hasSel = m_cursor.hasSelection();

	const QColor defaultTextColor = palette().color(QPalette::Text);
	int cachedLine = -1;
	QVector<HighlightSpan> cachedSpans;

	//	表示行の一部 [subOff, subOff + subText.size()) をスパン色で塗り分けながら描画する
	auto drawColoredSegment = [&](int x, int yBaseline, const QString &subText, qsizetype segOffInLine) -> int {
		if( subText.isEmpty() ) return x;
		if( cachedSpans.isEmpty() ) {
			painter.setPen(defaultTextColor);
			return drawLineText(painter, x, yBaseline, subText, fm, tabPx);
		}
		auto colorAt = [&](qsizetype logicalPos) -> QColor {
			for( const HighlightSpan &sp : cachedSpans ) {
				if( logicalPos >= sp.start && logicalPos < sp.start + sp.length )
					return sp.color;
			}
			return defaultTextColor;
		};
		qsizetype pos = 0;
		const qsizetype n = subText.size();
		while( pos < n ) {
			QColor col = colorAt(segOffInLine + pos);
			qsizetype end = pos + 1;
			while( end < n && colorAt(segOffInLine + end) == col )
				++end;
			painter.setPen(col);
			x = drawLineText(painter, x, yBaseline, subText.mid(pos, end - pos), fm, tabPx);
			pos = end;
		}
		return x;
	};

	painter.fillRect(viewport()->rect(), palette().base());
	for( int i = 0; i < rows; ++i ) {
		int viewLine = top + i;
		if( viewLine >= mgr.viewLineCount() ) break;
		ViewLineBlock vb = m_doc.findViewLineByNumber(viewLine);
		TextDocument::pos_t linePos = vb.position();
		const int y = i * lh;
		if( hasSel && linePos + vb.length() > selS && linePos < selE ) {
			const TextDocument::pos_t visEnd = linePos + mgr.viewLineLength(viewLine);
			const TextDocument::pos_t s = selS > linePos ? selS : linePos;
			const TextDocument::pos_t e = selE < visEnd ? selE : visEnd;
			int xs = xOfPosInViewLine(viewLine, s);
			int xe = xOfPosInViewLine(viewLine, e);
			if( selE > visEnd )
				xe += fm.horizontalAdvance(QChar(' '));
			if( xe > xs )
				painter.fillRect(xs, y, xe - xs, lh, m_selectionColor);
		}
		QString text = vb.text();
		if( text.endsWith(QChar('\n')) ) text.chop(1);

		const int ln = mgr.viewLineToLine(viewLine);
		const qsizetype segOff = static_cast<qsizetype>(linePos - mgr.lineStartPos(ln));
		if( m_markdownHighlight ) {
			if( ln != cachedLine ) {
				cachedLine = ln;
				QString logicalText = m_doc.findBlockByNumber(ln).text();
				if( logicalText.endsWith(QChar('\n')) ) logicalText.chop(1);
				cachedSpans = highlightMarkdownLine(logicalText, mgr.lineType(ln), m_headingColor, m_emphasisColor);
			}
		} else {
			cachedSpans.clear();
		}

		if( !m_preedit.isEmpty() && viewLine == mgr.viewLineAtPos(m_cursor.position()) ) {
			int curOff = (int)(m_cursor.position() - linePos);
			if( curOff < 0 ) curOff = 0;
			if( curOff > text.size() ) curOff = (int)text.size();
			QString before = text.left(curOff);
			QString after = text.mid(curOff);
			int xPre = drawColoredSegment(LEFT_MARGIN, y + fm.ascent(), before, segOff);
			int wPre = fm.horizontalAdvance(m_preedit);
			painter.setPen(defaultTextColor);
			painter.drawText(xPre, y + fm.ascent(), m_preedit);
			painter.drawLine(xPre, y + fm.ascent() + 1, xPre + wPre, y + fm.ascent() + 1);
			drawColoredSegment(xPre + wPre, y + fm.ascent(), after, segOff + curOff);
		} else {
			drawColoredSegment(LEFT_MARGIN, y + fm.ascent(), text, segOff);
		}
	}
	//	カーソル（フォーカス時のみ）
	if( hasFocus() && !hasSel ) {
		int cv = mgr.viewLineAtPos(m_cursor.position());
		if( cv >= top && cv < top + rows ) {
			int x = xOfPos(m_cursor.position());
			if( !m_preedit.isEmpty() )
				x += fm.horizontalAdvance(m_preedit.left(m_preeditCursor));
			const int caretH = fm.ascent() + 2;
			painter.fillRect(x, (cv - top) * lh, 2, caretH, Qt::red);
		}
	}
}

void TextEdit::keyPressEvent(QKeyEvent *event)
{
	const bool shift = (event->modifiers() & Qt::ShiftModifier) != 0;
	const bool ctrl = (event->modifiers() & Qt::ControlModifier) != 0;
	const TextCursor::MoveMode mode = shift ? TextCursor::KeepAnchor : TextCursor::MoveAnchor;
	bool handled = true;
	bool textChanged = false;
	switch( event->key() ) {
	case Qt::Key_Left:	m_cursor.movePosition(ctrl ? TextCursor::PreviousWord : TextCursor::Left, mode); break;
	case Qt::Key_Right:	m_cursor.movePosition(ctrl ? TextCursor::NextWord : TextCursor::Right, mode); break;
	case Qt::Key_Up:	m_cursor.movePosition(TextCursor::Up, mode); break;
	case Qt::Key_Down:	m_cursor.movePosition(TextCursor::Down, mode); break;
	case Qt::Key_Home:
		m_cursor.movePosition(ctrl ? TextCursor::Start : TextCursor::StartOfLine, mode);
		break;
	case Qt::Key_End:
		m_cursor.movePosition(ctrl ? TextCursor::End : TextCursor::EndOfLine, mode);
		break;
	case Qt::Key_PageUp:
	case Qt::Key_PageDown: {
		const ViewLineMgr &mgr = m_doc.lineMgr();
		int cv = mgr.viewLineAtPos(m_cursor.position());
		int target = cv + (event->key() == Qt::Key_PageDown ? pageLines() : -pageLines());
		if( target < 0 ) target = 0;
		if( target >= mgr.viewLineCount() ) target = mgr.viewLineCount() - 1;
		//	表示カラムを保って目標表示行へ
		TextDocument::pos_t s = mgr.viewLineStartPos(cv);
		int col = 0;
		for( TextDocument::pos_t i = s; i < m_cursor.position(); ++i )
			col += mgr.charWidth(m_doc.characterAt(i), col);
		TextDocument::pos_t ts = mgr.viewLineStartPos(target);
		TextDocument::pos_t tlen = mgr.viewLineLength(target);
		int acc = 0;
		TextDocument::pos_t ti = 0;
		while( ti < tlen && acc < col ) {
			acc += mgr.charWidth(m_doc.characterAt(ts + ti), acc);
			++ti;
		}
		m_cursor.setPosition(ts + ti, mode);
		break;
	}
	case Qt::Key_Backspace:	m_cursor.deletePreviousChar(); textChanged = true; break;
	case Qt::Key_Delete:	m_cursor.deleteChar(); textChanged = true; break;
	case Qt::Key_Return:
	case Qt::Key_Enter:	handleReturnKey(); textChanged = true; break;
	case Qt::Key_Tab:		textChanged = indentSelection(shift); break;
	case Qt::Key_Backtab:	textChanged = indentSelection(true); break;
	case Qt::Key_Z:	if( ctrl ) { if( shift ) redo(); else undo(); } else handled = false; break;
	case Qt::Key_Y:	if( ctrl ) redo(); else handled = false; break;
	case Qt::Key_A:
		if( ctrl ) {
			m_cursor.setPosition(0);
			m_cursor.setPosition(m_doc.characterCount(), TextCursor::KeepAnchor);
		} else handled = false;
		break;
	case Qt::Key_C:
		if( ctrl && m_cursor.hasSelection() )
			QApplication::clipboard()->setText(m_cursor.selectedText());
		else handled = false;
		break;
	case Qt::Key_X:
		if( ctrl && m_cursor.hasSelection() ) {
			QApplication::clipboard()->setText(m_cursor.selectedText());
			m_cursor.removeSelectedText();
			textChanged = true;
		} else handled = false;
		break;
	case Qt::Key_V:
		if( ctrl ) {
			QString t = QApplication::clipboard()->text();
			if( !t.isEmpty() ) {
				m_cursor.insertText(t);
				textChanged = true;
			}
		} else handled = false;
		break;
	case Qt::Key_Escape:
		m_cursor.clearSelection();
		break;
	default:
		if( !ctrl && !event->text().isEmpty() && event->text()[0] >= QChar(0x20) && event->text()[0] != QChar(0x7f) ) {
			m_cursor.insertText(event->text());
			textChanged = true;
		} else
			handled = false;
		break;
	}
	if( handled )
		afterChange(textChanged);
	else if( !ctrl && !event->text().isEmpty() && event->text()[0] >= QChar(0x20) && event->text()[0] != QChar(0x7f) ) {
		//	専用 case が Ctrl なしで拾った文字キー（A/C/V/X/Y/Z 等）はテキストとして挿入する
		m_cursor.insertText(event->text());
		afterChange(true);
	} else
		QAbstractScrollArea::keyPressEvent(event);
}

void TextEdit::mousePressEvent(QMouseEvent *event)
{
	if( event->button() == Qt::LeftButton ) {
		m_dragging = true;
		bool keep = (event->modifiers() & Qt::ShiftModifier) != 0;
		m_cursor.setPosition(posFromPoint(event->pos()),
							 keep ? TextCursor::KeepAnchor : TextCursor::MoveAnchor);
		viewport()->update();
	} else {
		QAbstractScrollArea::mousePressEvent(event);
	}
}

void TextEdit::mouseMoveEvent(QMouseEvent *event)
{
	if( m_dragging ) {
		m_cursor.setPosition(posFromPoint(event->pos()), TextCursor::KeepAnchor);
		ensureCursorVisible();
		viewport()->update();
	} else {
		QAbstractScrollArea::mouseMoveEvent(event);
	}
}

void TextEdit::mouseReleaseEvent(QMouseEvent *event)
{
	if( event->button() == Qt::LeftButton )
		m_dragging = false;
	QAbstractScrollArea::mouseReleaseEvent(event);
}

void TextEdit::wheelEvent(QWheelEvent *event)
{
	if( (event->modifiers() & Qt::ControlModifier) != 0 ) {
		const int delta = event->angleDelta().y();
		if( delta != 0 && m_onFontSizeChange )
			m_onFontSizeChange(delta > 0 ? 1 : -1);
		event->accept();
		return;
	}
	QAbstractScrollArea::wheelEvent(event);
}

void TextEdit::resizeEvent(QResizeEvent *event)
{
	QAbstractScrollArea::resizeEvent(event);
	updateLineNumberAreaGeometry();
	updateWrapWidth();
	updateScrollBars();
	viewport()->update();
}

void TextEdit::focusInEvent(QFocusEvent *event)
{
	QAbstractScrollArea::focusInEvent(event);
	viewport()->update();
	QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);
}

void TextEdit::focusOutEvent(QFocusEvent *event)
{
	QAbstractScrollArea::focusOutEvent(event);
	viewport()->update();
}

bool TextEdit::focusNextPrevChild(bool next)
{
	Q_UNUSED(next);
	return false;	//	Tab / Shift+Tab でフォーカス移動させずインデント操作に使う
}

bool TextEdit::indentSelection(bool unindent)
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	const bool hadSelection = m_cursor.hasSelection();
	const TextDocument::pos_t selStart = m_cursor.selectionStart();
	const TextDocument::pos_t selEnd = m_cursor.selectionEnd();
	int startLine = mgr.lineAtPos(selStart);
	int endLine = mgr.lineAtPos(selEnd);
	if( hadSelection && selEnd > selStart && selEnd == mgr.lineStartPos(endLine) )
		--endLine;

	bool changed = false;
	m_doc.undoMgr().openBlock();
	for( int ln = startLine; ln <= endLine; ++ln ) {
		const TextDocument::pos_t lineStart = mgr.lineStartPos(ln);
		if( !unindent ) {
			m_doc.insert(lineStart, QStringLiteral("  "));
			changed = true;
		} else {
			const TextDocument::pos_t len = mgr.lineLength(ln);
			if( len >= 1 && m_doc.characterAt(lineStart) == QChar('\t') ) {
				m_doc.erase(lineStart, lineStart + 1);
				changed = true;
			} else if( len >= 2 && m_doc.characterAt(lineStart) == QChar(' ') && m_doc.characterAt(lineStart + 1) == QChar(' ') ) {
				m_doc.erase(lineStart, lineStart + 2);
				changed = true;
			} else if( len >= 1 && m_doc.characterAt(lineStart) == QChar(' ') ) {
				m_doc.erase(lineStart, lineStart + 1);
				changed = true;
			}
		}
	}
	m_doc.undoMgr().closeBlock();

	if( changed && hadSelection && startLine < endLine ) {
		const TextDocument::pos_t newStart = mgr.lineStartPos(startLine);
		const TextDocument::pos_t newEnd = (endLine + 1 < mgr.lineCount())
			? mgr.lineStartPos(endLine + 1)
			: mgr.lineEndPos(endLine);
		m_cursor.setPosition(newStart);
		m_cursor.setPosition(newEnd, TextCursor::KeepAnchor);
	}
	return changed;
}

void TextEdit::handleReturnKey()
{
	if( !m_cursor.hasSelection() ) {
		const ViewLineMgr &mgr = m_doc.lineMgr();
		const int ln = mgr.lineAtPos(m_cursor.position());
		const TextDocument::pos_t lineStart = mgr.lineStartPos(ln);
		const TextDocument::pos_t lineEnd = mgr.lineEndPos(ln);
		if( mgr.lineType(ln) == LineMgr::LT_LIST && m_cursor.position() == lineEnd ) {
			QString lineText = m_doc.findBlockByNumber(ln).text();
			if( lineText.endsWith(QChar('\n')) ) lineText.chop(1);
			if( lineText.endsWith(QChar('\r')) ) lineText.chop(1);
			qsizetype i = 0;
			while( i < lineText.size() && (lineText[i] == QChar(' ') || lineText[i] == QChar('\t')) )
				++i;
			if( i + 2 <= lineText.size() ) {
				if( i + 2 == lineText.size() ) {
					//	"- " だけの空リスト行で Enter → リストマーカーごと消してリスト終了
					m_doc.erase(lineStart, lineEnd);
					return;
				}
				const QString prefix = lineText.left(i + 2);	//	インデント + リストマーカー（"- " 等）
				m_cursor.insertText(QStringLiteral("\n") + prefix);
				return;
			}
		}
	}
	m_cursor.insertText(QStringLiteral("\n"));
}

void TextEdit::inputMethodEvent(QInputMethodEvent *event)
{
	bool textChanged = false;
	if( event->replacementLength() > 0 ) {
		TextDocument::pos_t pos = m_cursor.position() + event->replacementStart();
		m_cursor.setPosition(pos);
		m_cursor.setPosition(pos + event->replacementLength(), TextCursor::KeepAnchor);
		m_cursor.removeSelectedText();
		textChanged = true;
	}
	if( !event->commitString().isEmpty() ) {
		m_cursor.insertText(event->commitString());
		textChanged = true;
	}
	m_preedit = event->preeditString();
	m_preeditCursor = (int)m_preedit.size();
	for( const QInputMethodEvent::Attribute &attr : event->attributes() ) {
		if( attr.type == QInputMethodEvent::Cursor ) {
			m_preeditCursor = qBound(0, attr.start, (int)m_preedit.size());
			break;
		}
	}
	afterChange(textChanged);
	event->accept();
}

QVariant TextEdit::inputMethodQuery(Qt::InputMethodQuery query) const
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	switch( query ) {
	case Qt::ImEnabled:
		return true;
	case Qt::ImCursorRectangle: {
		const int lh = lineHeight();
		const int top = verticalScrollBar()->value();
		const int cv = mgr.viewLineAtPos(m_cursor.position());
		int x = xOfPos(m_cursor.position());
		if( !m_preedit.isEmpty() )
			x += fontMetrics().horizontalAdvance(m_preedit.left(m_preeditCursor));
		int y = (cv - top) * lh;
		QPoint pt = viewport()->mapTo(const_cast<TextEdit*>(this), QPoint(x, y));
		return QRect(pt.x(), pt.y(), 1, fontMetrics().height());
	}
	case Qt::ImFont:
		return font();
	case Qt::ImCursorPosition: {
		int ln = mgr.lineAtPos(m_cursor.position());
		return (int)(m_cursor.position() - mgr.lineStartPos(ln));
	}
	case Qt::ImSurroundingText: {
		int ln = mgr.lineAtPos(m_cursor.position());
		return m_doc.findBlockByNumber(ln).text();
	}
	case Qt::ImCurrentSelection:
		return m_cursor.selectedText();
	default:
		return QAbstractScrollArea::inputMethodQuery(query);
	}
}
