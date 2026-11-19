#include "TextEdit.h"

#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>

namespace {
const int LEFT_MARGIN = 2;
}

TextEdit::TextEdit(QWidget *parent)
	: QAbstractScrollArea(parent), m_cursor(&m_doc)
{
	setFocusPolicy(Qt::StrongFocus);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	viewport()->setCursor(Qt::IBeamCursor);
	connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
			[this](int) { viewport()->update(); });
}

void TextEdit::setPlainText(const QString &text)
{
	m_doc.setPlainText(text);
	m_cursor.setPosition(0);
	verticalScrollBar()->setValue(0);
	updateScrollBars();
	viewport()->update();
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
}

void TextEdit::undo()
{
	if( !m_doc.canUndo() ) return;
	int pos = m_doc.undo();
	m_cursor.setPosition(pos);
	afterChange();
}

void TextEdit::redo()
{
	if( !m_doc.canRedo() ) return;
	int pos = m_doc.redo();
	m_cursor.setPosition(pos);
	afterChange();
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

int TextEdit::xOfPos(TextDocument::pos_t pos) const
{
	const VLineMgr &mgr = m_doc.lineMgr();
	int vline = mgr.vlineAtPos(pos);
	TextDocument::pos_t s = mgr.vlineStartPos(vline);
	const QFontMetrics fm = fontMetrics();
	int x = LEFT_MARGIN;
	for( TextDocument::pos_t i = s; i < pos; ++i )
		x += fm.horizontalAdvance(m_doc.characterAt(i));
	return x;
}

TextDocument::pos_t TextEdit::posFromPoint(const QPoint &pt) const
{
	const VLineMgr &mgr = m_doc.lineMgr();
	if( mgr.vlineCount() == 0 ) return 0;
	int vline = verticalScrollBar()->value() + pt.y() / lineHeight();
	if( vline < 0 ) vline = 0;
	if( vline >= mgr.vlineCount() ) vline = mgr.vlineCount() - 1;
	TextDocument::pos_t s = mgr.vlineStartPos(vline);
	TextDocument::pos_t len = mgr.vlineLength(vline);
	const QFontMetrics fm = fontMetrics();
	int x = LEFT_MARGIN;
	for( TextDocument::pos_t i = 0; i < len; ++i ) {
		int w = fm.horizontalAdvance(m_doc.characterAt(s + i));
		if( pt.x() < x + w / 2 ) return s + i;
		x += w;
	}
	return s + len;
}

void TextEdit::updateScrollBars()
{
	int maxTop = m_doc.lineMgr().vlineCount() - pageLines();
	if( maxTop < 0 ) maxTop = 0;
	QScrollBar *bar = verticalScrollBar();
	bar->setRange(0, maxTop);
	bar->setPageStep(pageLines());
	bar->setSingleStep(1);
}

void TextEdit::updateWrapWidth()
{
	int unit = fontMetrics().horizontalAdvance(QChar(' '));
	if( unit <= 0 ) unit = 1;
	int units = viewport()->width() / unit;
	if( units < 1 ) units = 1;
	if( m_doc.lineMgr().lineWidth() != units ) {
		m_doc.lineMgr().setLineWidth(units);
		updateScrollBars();
	}
}

void TextEdit::ensureCursorVisible()
{
	const VLineMgr &mgr = m_doc.lineMgr();
	int cv = mgr.vlineAtPos(m_cursor.position());
	QScrollBar *bar = verticalScrollBar();
	int top = bar->value();
	if( cv < top )
		bar->setValue(cv);
	else if( cv >= top + pageLines() )
		bar->setValue(cv - pageLines() + 1);
}

void TextEdit::afterChange()
{
	updateScrollBars();
	ensureCursorVisible();
	viewport()->update();
}

void TextEdit::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter painter(viewport());
	const QFontMetrics fm = fontMetrics();
	const int lh = lineHeight();
	const int top = verticalScrollBar()->value();
	const VLineMgr &mgr = m_doc.lineMgr();
	const int rows = viewport()->height() / lh + 1;
	const TextDocument::pos_t selS = m_cursor.selectionStart();
	const TextDocument::pos_t selE = m_cursor.selectionEnd();
	const bool hasSel = m_cursor.hasSelection();

	painter.fillRect(viewport()->rect(), palette().base());
	for( int i = 0; i < rows; ++i ) {
		int vline = top + i;
		if( vline >= mgr.vlineCount() ) break;
		VLineBlock vb = m_doc.findVLineByNumber(vline);
		TextDocument::pos_t linePos = vb.position();
		const int y = i * lh;
		if( hasSel && linePos + vb.length() > selS && linePos < selE ) {
			TextDocument::pos_t s = selS > linePos ? selS : linePos;
			TextDocument::pos_t e = selE < linePos + vb.length() ? selE : linePos + vb.length();
			int xs = xOfPos(s);
			int xe = xOfPos(e);
			painter.fillRect(xs, y, xe - xs, lh, palette().highlight());
		}
		QString text = vb.text();
		if( text.endsWith(QChar('\n')) ) text.chop(1);
		painter.setPen(palette().color(QPalette::Text));
		painter.drawText(LEFT_MARGIN, y + fm.ascent(), text);
	}
	//	カーソル（フォーカス時のみ）
	if( hasFocus() && !hasSel ) {
		int cv = mgr.vlineAtPos(m_cursor.position());
		if( cv >= top && cv < top + rows ) {
			int x = xOfPos(m_cursor.position());
			painter.fillRect(x, (cv - top) * lh, 1, lh, palette().color(QPalette::Text));
		}
	}
}

void TextEdit::keyPressEvent(QKeyEvent *event)
{
	const bool shift = (event->modifiers() & Qt::ShiftModifier) != 0;
	const bool ctrl = (event->modifiers() & Qt::ControlModifier) != 0;
	const TextCursor::MoveMode mode = shift ? TextCursor::KeepAnchor : TextCursor::MoveAnchor;
	bool handled = true;
	switch( event->key() ) {
	case Qt::Key_Left:	m_cursor.movePosition(TextCursor::Left, mode); break;
	case Qt::Key_Right:	m_cursor.movePosition(TextCursor::Right, mode); break;
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
		const VLineMgr &mgr = m_doc.lineMgr();
		int cv = mgr.vlineAtPos(m_cursor.position());
		int target = cv + (event->key() == Qt::Key_PageDown ? pageLines() : -pageLines());
		if( target < 0 ) target = 0;
		if( target >= mgr.vlineCount() ) target = mgr.vlineCount() - 1;
		//	表示カラムを保って目標表示行へ
		TextDocument::pos_t s = mgr.vlineStartPos(cv);
		int col = 0;
		for( TextDocument::pos_t i = s; i < m_cursor.position(); ++i )
			col += VLineMgr::charWidth(m_doc.characterAt(i));
		TextDocument::pos_t ts = mgr.vlineStartPos(target);
		TextDocument::pos_t tlen = mgr.vlineLength(target);
		int acc = 0;
		TextDocument::pos_t ti = 0;
		while( ti < tlen && acc < col ) {
			acc += VLineMgr::charWidth(m_doc.characterAt(ts + ti));
			++ti;
		}
		m_cursor.setPosition(ts + ti, mode);
		break;
	}
	case Qt::Key_Backspace:	m_cursor.deletePreviousChar(); break;
	case Qt::Key_Delete:	m_cursor.deleteChar(); break;
	case Qt::Key_Return:
	case Qt::Key_Enter:	m_cursor.insertText(QStringLiteral("\n")); break;
	case Qt::Key_Tab:	m_cursor.insertText(QStringLiteral("\t")); break;
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
		} else handled = false;
		break;
	case Qt::Key_V:
		if( ctrl ) {
			QString t = QApplication::clipboard()->text();
			if( !t.isEmpty() ) m_cursor.insertText(t);
		} else handled = false;
		break;
	default:
		if( !ctrl && !event->text().isEmpty() )
			m_cursor.insertText(event->text());
		else
			handled = false;
		break;
	}
	if( handled )
		afterChange();
	else
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

void TextEdit::resizeEvent(QResizeEvent *event)
{
	QAbstractScrollArea::resizeEvent(event);
	updateWrapWidth();
	updateScrollBars();
	viewport()->update();
}
