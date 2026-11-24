#include "ViEngine.h"
#include "TextEdit.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QKeyEvent>
#include <QRegularExpression>
#include <algorithm>

namespace {
constexpr uchar kUndoFlagLinewise = UndoAction::FLAG_LINEWISE;

enum ViCharClass {
	VCC_NEWLINE = 0,
	VCC_SPACE,
	VCC_ALNUM,
	VCC_HIRAGANA,
	VCC_KATAKANA,
	VCC_KANJI,
	VCC_SYMBOL,
};

ViCharClass viCharClass(QChar ch)
{
	const ushort u = ch.unicode();
	if( u == '\n' || u == '\r' ) return VCC_NEWLINE;
	if( u == ' ' || u == '\t' || u == 0x3000 ) return VCC_SPACE;
	if( u < 0x80 ) {
		if( ch.isLetterOrNumber() || u == '_' ) return VCC_ALNUM;
		return VCC_SYMBOL;
	}
	if( (u >= 0x3040 && u <= 0x309F) || u == 0x30FC ) return VCC_HIRAGANA;
	if( (u >= 0x30A0 && u <= 0x30FF) || (u >= 0xFF65 && u <= 0xFF9F) ) return VCC_KATAKANA;
	if( (u >= 0x4E00 && u <= 0x9FFF) || (u >= 0x3400 && u <= 0x4DBF) ) return VCC_KANJI;
	if( ch.isLetterOrNumber() ) return VCC_ALNUM;
	return VCC_SYMBOL;
}

bool isSpaceOrNl(QChar ch)
{
	const ushort u = ch.unicode();
	return u == ' ' || u == '\t' || u == '\n' || u == '\r' || u == 0x3000;
}

TextDocument::pos_t firstNonBlankPos(const TextDocument &doc, int line)
{
	const ViewLineMgr &mgr = doc.lineMgr();
	TextDocument::pos_t pos = mgr.lineStartPos(line);
	const TextDocument::pos_t end = mgr.lineEndPos(line);
	while( pos < end && (doc.characterAt(pos) == QChar(' ') || doc.characterAt(pos) == QChar('\t')) )
		++pos;
	return pos;
}

TextDocument::pos_t nextWordStart(const TextDocument &doc, TextDocument::pos_t pos, bool bigWord)
{
	const TextDocument::pos_t total = doc.characterCount();
	if( pos >= total ) return total;
	if( bigWord ) {
		while( pos < total && !isSpaceOrNl(doc.characterAt(pos)) ) ++pos;
		while( pos < total && isSpaceOrNl(doc.characterAt(pos)) ) ++pos;
		return pos;
	}
	const ViCharClass cls = viCharClass(doc.characterAt(pos));
	if( cls != VCC_SPACE && cls != VCC_NEWLINE ) {
		while( pos < total && viCharClass(doc.characterAt(pos)) == cls ) ++pos;
	}
	while( pos < total ) {
		const ViCharClass c = viCharClass(doc.characterAt(pos));
		if( c != VCC_SPACE && c != VCC_NEWLINE ) break;
		++pos;
	}
	return pos;
}

TextDocument::pos_t prevWordStart(const TextDocument &doc, TextDocument::pos_t pos, bool bigWord)
{
	if( pos <= 0 ) return 0;
	--pos;
	if( bigWord ) {
		while( pos > 0 && isSpaceOrNl(doc.characterAt(pos)) ) --pos;
		while( pos > 0 && !isSpaceOrNl(doc.characterAt(pos - 1)) ) --pos;
		return pos;
	}
	while( pos > 0 ) {
		const ViCharClass c = viCharClass(doc.characterAt(pos));
		if( c != VCC_SPACE && c != VCC_NEWLINE ) break;
		--pos;
	}
	const ViCharClass cls = viCharClass(doc.characterAt(pos));
	while( pos > 0 && viCharClass(doc.characterAt(pos - 1)) == cls )
		--pos;
	return pos;
}

TextDocument::pos_t nextWordEnd(const TextDocument &doc, TextDocument::pos_t pos, bool bigWord)
{
	const TextDocument::pos_t total = doc.characterCount();
	if( total <= 0 ) return 0;
	const TextDocument::pos_t maxPos = total - 1;
	if( pos >= maxPos ) return maxPos;
	++pos;
	if( bigWord ) {
		while( pos < maxPos && isSpaceOrNl(doc.characterAt(pos)) ) ++pos;
		while( pos < maxPos && !isSpaceOrNl(doc.characterAt(pos + 1)) ) ++pos;
		return pos;
	}
	while( pos < maxPos ) {
		const ViCharClass c = viCharClass(doc.characterAt(pos));
		if( c != VCC_SPACE && c != VCC_NEWLINE ) break;
		++pos;
	}
	const ViCharClass cls = viCharClass(doc.characterAt(pos));
	while( pos < maxPos && viCharClass(doc.characterAt(pos + 1)) == cls )
		++pos;
	return pos;
}

TextDocument::pos_t findMatchingBracket(const TextDocument &doc, TextDocument::pos_t pos)
{
	const TextDocument::pos_t total = doc.characterCount();
	if( total <= 0 ) return pos;
	const ViewLineMgr &mgr = doc.lineMgr();
	const int ln = mgr.lineAtPos(pos);
	const TextDocument::pos_t lineEnd = mgr.lineEndPos(ln);

	auto matchOf = [](QChar c, QChar &other, int &dir) -> bool {
		switch( c.unicode() ) {
		case '(': other = ')'; dir = 1; return true;
		case ')': other = '('; dir = -1; return true;
		case '[': other = ']'; dir = 1; return true;
		case ']': other = '['; dir = -1; return true;
		case '{': other = '}'; dir = 1; return true;
		case '}': other = '{'; dir = -1; return true;
		case '<': other = '>'; dir = 1; return true;
		case '>': other = '<'; dir = -1; return true;
		default: return false;
		}
	};

	QChar openCh, closeCh;
	int dir = 0;
	TextDocument::pos_t p = pos;
	while( p < lineEnd ) {
		if( matchOf(doc.characterAt(p), closeCh, dir) ) {
			openCh = doc.characterAt(p);
			break;
		}
		++p;
	}
	if( dir == 0 ) return pos;

	int depth = 0;
	for( TextDocument::pos_t i = p; i >= 0 && i < total; i += dir ) {
		const QChar ch = doc.characterAt(i);
		if( ch == openCh ) ++depth;
		else if( ch == closeCh && --depth == 0 ) return i;
	}
	return pos;
}

QString lineTextWithoutNl(const TextDocument &doc, int ln)
{
	QString s = doc.findBlockByNumber(ln).text();
	if( s.endsWith(QChar('\n')) ) s.chop(1);
	if( s.endsWith(QChar('\r')) ) s.chop(1);
	return s;
}

//	ex 行指定（数字 / '.' / '$' / '/pat/' + [+-]N）を 1-based 行番号へパース
int parseExLineSpec(const QString &text, qsizetype &ix, int curLine, int totalLines, const TextDocument &doc)
{
	int base = -1;
	if( ix >= text.size() ) return -1;
	if( text[ix] == QChar('.') ) {
		base = curLine;
		++ix;
	} else if( text[ix] == QChar('+') || text[ix] == QChar('-') ) {
		base = curLine;	//	'.' 省略の相対指定（:+2 / :- / :1;+2）
	} else if( text[ix] == QChar('$') ) {
		base = totalLines;
		++ix;
	} else if( text[ix].isDigit() ) {
		qsizetype s = ix;
		while( ix < text.size() && text[ix].isDigit() ) ++ix;
		base = text.mid(s, ix - s).toInt();
	} else if( text[ix] == QChar('/') ) {
		++ix;
		qsizetype s = ix;
		while( ix < text.size() && text[ix] != QChar('/') ) {
			if( text[ix] == QChar('\\') && ix + 1 < text.size() ) ++ix;
			++ix;
		}
		const QString pat = text.mid(s, ix - s);
		if( ix < text.size() && text[ix] == QChar('/') ) ++ix;
		const QRegularExpression re(pat);
		if( !re.isValid() ) return -1;
		for( int step = 1; step <= totalLines; ++step ) {
			const int ln = ((curLine - 1 + step) % totalLines);
			if( re.match(lineTextWithoutNl(doc, ln)).hasMatch() ) {
				base = ln + 1;
				break;
			}
		}
		if( base < 0 ) return -1;
	} else {
		return -1;
	}

	while( ix < text.size() && (text[ix] == QChar('+') || text[ix] == QChar('-')) ) {
		const int sign = (text[ix] == QChar('+')) ? 1 : -1;
		++ix;
		int offset = 1;
		if( ix < text.size() && text[ix].isDigit() ) {
			qsizetype s = ix;
			while( ix < text.size() && text[ix].isDigit() ) ++ix;
			offset = text.mid(s, ix - s).toInt();
		}
		base += sign * offset;
	}
	return qBound(1, base, qMax(1, totalLines));
}

bool exCmdMatch(const QString &cmd, const char *shortName, const char *fullName)
{
	if( cmd.isEmpty() ) return false;
	const QString full = QString::fromLatin1(fullName);
	const QString shrt = QString::fromLatin1(shortName);
	return cmd.size() >= shrt.size() && full.startsWith(cmd);
}
}

void ViEngine::setEnabled(bool enabled)
{
	m_enabled = enabled;
	m_changeUndoOpen = false;
	resetPending();
	m_mode = enabled ? ViMode::Normal : ViMode::Insert;
}

void ViEngine::setMode(ViMode m, TextEdit *edit)
{
	if( m == ViMode::Normal && m_changeUndoOpen && edit ) {
		edit->document()->undoMgr().closeBlock();
		m_changeUndoOpen = false;
	}
	const ViMode prevMode = m_mode;
	m_mode = m;
	resetPending();
	if( edit ) {
		if( m_mode == ViMode::Normal ) {
			TextCursor c = edit->textCursor();
			if( prevMode == ViMode::Visual || prevMode == ViMode::VisualLine )
				c.setPosition(m_visualCursor);
			else
				c.clearSelection();
			edit->setTextCursor(c);
			clampCursorInNormal(edit);
		} else if( m_mode == ViMode::Visual || m_mode == ViMode::VisualLine ) {
			m_visualAnchor = edit->textCursor().position();
			m_visualCursor = m_visualAnchor;
			updateVisualSelection(edit);
		}
		notifyMode(edit);
	}
}

QString ViEngine::modeLabel() const
{
	if( !m_enabled ) return QString();
	switch( m_mode ) {
	case ViMode::Normal:		return QStringLiteral("-- NORMAL --");
	case ViMode::Insert:		return QStringLiteral("-- INSERT --");
	case ViMode::Visual:		return QStringLiteral("-- VISUAL --");
	case ViMode::VisualLine:	return QStringLiteral("-- VISUAL LINE --");
	case ViMode::Replace:		return QStringLiteral("-- REPLACE --");
	}
	return QString();
}

void ViEngine::resetPending()
{
	m_repeatCount = 0;
	m_operator = QChar();
	m_prefix = QChar();
}

int ViEngine::effectiveCount() const
{
	return m_repeatCount > 0 ? m_repeatCount : 1;
}

void ViEngine::notifyMode(TextEdit *edit)
{
	if( m_cb.showStatus )
		m_cb.showStatus(modeLabel());
	if( edit )
		edit->viewport()->update();
}

void ViEngine::clampCursorInNormal(TextEdit *edit)
{
	if( !edit || m_mode != ViMode::Normal )
		return;
	TextDocument *doc = edit->document();
	const ViewLineMgr &mgr = doc->lineMgr();
	TextCursor c = edit->textCursor();
	const int ln = mgr.lineAtPos(c.position());
	const TextDocument::pos_t lineStart = mgr.lineStartPos(ln);
	const TextDocument::pos_t lineEnd = mgr.lineEndPos(ln);
	if( c.position() > lineStart && c.position() >= lineEnd ) {
		c.setPosition(lineEnd - 1);
		edit->setTextCursor(c);
	}
}

void ViEngine::updateVisualSelection(TextEdit *edit)
{
	TextDocument *doc = edit->document();
	const ViewLineMgr &mgr = doc->lineMgr();
	TextCursor c = edit->textCursor();
	const TextDocument::pos_t cur = m_visualCursor;
	if( m_mode == ViMode::VisualLine ) {
		const int l1 = mgr.lineAtPos(qMin(m_visualAnchor, cur));
		const int l2 = mgr.lineAtPos(qMax(m_visualAnchor, cur));
		const TextDocument::pos_t s = mgr.lineStartPos(l1);
		const TextDocument::pos_t e = (l2 + 1 < mgr.lineCount()) ? mgr.lineStartPos(l2 + 1) : mgr.lineEndPos(l2);
		c.setPosition(s);
		c.setPosition(e, TextCursor::KeepAnchor);
		edit->setTextCursor(c);
	} else if( m_mode == ViMode::Visual ) {
		const TextDocument::pos_t total = doc->characterCount();
		if( cur >= m_visualAnchor ) {
			c.setPosition(m_visualAnchor);
			c.setPosition(qMin(cur + 1, total), TextCursor::KeepAnchor);
		} else {
			c.setPosition(qMin(m_visualAnchor + 1, total));
			c.setPosition(cur, TextCursor::KeepAnchor);
		}
		edit->setTextCursor(c);
	}
}

bool ViEngine::computeMotionTarget(
	TextEdit *edit, QChar key, bool ctrl, int count,
	bool forOperator, QChar opChar,
	TextDocument::pos_t &outPos, bool &outLinewise, bool &outInclusive)
{
	TextDocument *doc = edit->document();
	const ViewLineMgr &mgr = doc->lineMgr();
	const TextDocument::pos_t pos = (m_mode == ViMode::Visual || m_mode == ViMode::VisualLine)
		? m_visualCursor
		: edit->textCursor().position();
	const int curLine = mgr.lineAtPos(pos);
	const int nLines = mgr.lineCount();
	outPos = pos;
	outLinewise = false;
	outInclusive = false;

	if( ctrl ) {
		if( key == QChar('f') || key == QChar('F') || key == QChar('b') || key == QChar('B')
			|| key == QChar('d') || key == QChar('D') || key == QChar('u') || key == QChar('U') ) {
			const int page = qMax(1, edit->viewport()->height() / qMax(1, edit->fontMetrics().height() + edit->lineSpacing()));
			const int step = (key.toLower() == QChar('d') || key.toLower() == QChar('u')) ? qMax(1, page / 2) : page;
			const int dir = (key.toLower() == QChar('f') || key.toLower() == QChar('d')) ? 1 : -1;
			const int targetLn = qBound(0, curLine + dir * step * count, nLines - 1);
			outPos = firstNonBlankPos(*doc, targetLn);
			outLinewise = true;
			return true;
		}
		return false;
	}

	switch( key.unicode() ) {
	case 'h': {
		const TextDocument::pos_t lineStart = mgr.lineStartPos(curLine);
		outPos = qMax(lineStart, pos - count);
		return true;
	}
	case 'l': {
		const TextDocument::pos_t lineEnd = mgr.lineEndPos(curLine);
		const TextDocument::pos_t limit = forOperator ? lineEnd : qMax(mgr.lineStartPos(curLine), lineEnd - 1);
		outPos = qMin(limit, pos + count);
		return true;
	}
	case 'j': {
		const int col = static_cast<int>(pos - mgr.lineStartPos(curLine));
		const int targetLn = qMin(nLines - 1, curLine + count);
		const TextDocument::pos_t s = mgr.lineStartPos(targetLn);
		const TextDocument::pos_t e = mgr.lineEndPos(targetLn);
		outPos = qMin(s + col, qMax(s, e - 1));
		outLinewise = true;
		return true;
	}
	case 'k': {
		const int col = static_cast<int>(pos - mgr.lineStartPos(curLine));
		const int targetLn = qMax(0, curLine - count);
		const TextDocument::pos_t s = mgr.lineStartPos(targetLn);
		const TextDocument::pos_t e = mgr.lineEndPos(targetLn);
		outPos = qMin(s + col, qMax(s, e - 1));
		outLinewise = true;
		return true;
	}
	case '0':
		outPos = mgr.lineStartPos(curLine);
		return true;
	case '^':
		outPos = firstNonBlankPos(*doc, curLine);
		return true;
	case '$': {
		const int targetLn = qMin(nLines - 1, curLine + count - 1);
		const TextDocument::pos_t e = mgr.lineEndPos(targetLn);
		outPos = forOperator ? e : qMax(mgr.lineStartPos(targetLn), e - 1);
		outInclusive = !forOperator;
		return true;
	}
	case '+': {
		const int targetLn = qMin(nLines - 1, curLine + count);
		outPos = firstNonBlankPos(*doc, targetLn);
		outLinewise = true;
		return true;
	}
	case '-': {
		const int targetLn = qMax(0, curLine - count);
		outPos = firstNonBlankPos(*doc, targetLn);
		outLinewise = true;
		return true;
	}
	case 'w':
	case 'W': {
		const bool big = (key == QChar('W'));
		TextDocument::pos_t p = pos;
		if( forOperator && opChar == QChar('c') ) {
			//	cw / cW: 現在単語（または空白列）の末尾まで（後続空白を食べない）
			for( int i = 0; i < count; ++i ) {
				const TextDocument::pos_t total = doc->characterCount();
				if( p >= total ) break;
				if( big ) {
					const bool onSp = isSpaceOrNl(doc->characterAt(p));
					while( p + 1 < total && doc->characterAt(p + 1) != QChar('\n')
						&& isSpaceOrNl(doc->characterAt(p + 1)) == onSp )
						++p;
				} else {
					const ViCharClass cls = viCharClass(doc->characterAt(p));
					if( cls != VCC_NEWLINE ) {
						while( p + 1 < total && viCharClass(doc->characterAt(p + 1)) == cls )
							++p;
					}
				}
				if( i + 1 < count ) p = nextWordStart(*doc, p, big);
			}
			outPos = p;
			outInclusive = true;
			return true;
		}
		for( int i = 0; i < count; ++i )
			p = nextWordStart(*doc, p, big);
		outPos = p;
		return true;
	}
	case 'b':
	case 'B': {
		const bool big = (key == QChar('B'));
		TextDocument::pos_t p = pos;
		for( int i = 0; i < count; ++i )
			p = prevWordStart(*doc, p, big);
		outPos = p;
		return true;
	}
	case 'e':
	case 'E': {
		const bool big = (key == QChar('E'));
		TextDocument::pos_t p = pos;
		for( int i = 0; i < count; ++i )
			p = nextWordEnd(*doc, p, big);
		outPos = p;
		outInclusive = true;
		return true;
	}
	case 'G': {
		const int targetLn = (m_repeatCount > 0) ? qBound(0, m_repeatCount - 1, nLines - 1) : (nLines - 1);
		outPos = firstNonBlankPos(*doc, targetLn);
		outLinewise = true;
		return true;
	}
	case '%':
		outPos = findMatchingBracket(*doc, pos);
		outInclusive = true;
		return true;
	case ';':
	case ',': {
		if( m_lastFindCmd.isNull() || m_lastFindChar.isNull() )
			return false;
		QChar cmd = m_lastFindCmd;
		if( key == QChar(',') ) {
			if( cmd == QChar('f') ) cmd = QChar('F');
			else if( cmd == QChar('F') ) cmd = QChar('f');
			else if( cmd == QChar('t') ) cmd = QChar('T');
			else if( cmd == QChar('T') ) cmd = QChar('t');
		}
		const bool fwd = (cmd == QChar('f') || cmd == QChar('t'));
		const bool till = (cmd == QChar('t') || cmd == QChar('T'));
		const TextDocument::pos_t lineStart = mgr.lineStartPos(curLine);
		const TextDocument::pos_t lineEnd = mgr.lineEndPos(curLine);
		TextDocument::pos_t p = pos;
		int found = 0;
		for( TextDocument::pos_t i = pos + (fwd ? 1 : -1); i >= lineStart && i < lineEnd; i += (fwd ? 1 : -1) ) {
			if( doc->characterAt(i) == m_lastFindChar ) {
				p = i;
				if( ++found == count ) break;
			}
		}
		if( found < count ) return false;
		if( till ) p += (fwd ? -1 : 1);
		outPos = p;
		outInclusive = fwd;
		return true;
	}
	default:
		return false;
	}
}

bool ViEngine::applyOperatorRange(
	TextEdit *edit, QChar op, TextDocument::pos_t p1, TextDocument::pos_t p2,
	bool linewise, bool inclusive)
{
	TextDocument *doc = edit->document();
	const ViewLineMgr &mgr = doc->lineMgr();
	if( p1 > p2 ) std::swap(p1, p2);

	if( linewise ) {
		const int l1 = mgr.lineAtPos(p1);
		const int l2 = mgr.lineAtPos(p2);
		const TextDocument::pos_t start = mgr.lineStartPos(l1);
		const TextDocument::pos_t endWithNl = (l2 + 1 < mgr.lineCount()) ? mgr.lineStartPos(l2 + 1) : mgr.lineEndPos(l2);

		if( op == QChar('>') || op == QChar('<') ) {
			TextCursor c = edit->textCursor();
			c.setPosition(start);
			c.setPosition(endWithNl, TextCursor::KeepAnchor);
			edit->setTextCursor(c);
			if( op == QChar('>') ) edit->indent();
			else edit->unindent();
			c = edit->textCursor();
			c.clearSelection();
			c.setPosition(firstNonBlankPos(*doc, l1));
			edit->setTextCursor(c);
			return true;
		}

		QString yanked;
		for( TextDocument::pos_t i = start; i < endWithNl; ++i )
			yanked.append(doc->characterAt(i));
		if( !yanked.endsWith(QChar('\n')) )
			yanked.append(QChar('\n'));
		m_yankBuf = yanked;
		m_yankLinewise = true;
		QApplication::clipboard()->setText(m_yankBuf);

		if( op == QChar('y') ) {
			TextCursor c = edit->textCursor();
			c.clearSelection();
			c.setPosition(start);
			edit->setTextCursor(c);
			if( m_cb.showStatus )
				m_cb.showStatus(QStringLiteral("%1 line(s) yanked").arg(l2 - l1 + 1));
			return true;
		}
		if( op == QChar('c') ) {
			//	cc: 選択行の内容を消して空行にし Insert モードへ（1 Undo ブロックにまとめる）
			const TextDocument::pos_t endNoNl = mgr.lineEndPos(l2);
			doc->undoMgr().openBlock();
			m_changeUndoOpen = true;
			edit->applyPreviewEdit(static_cast<int>(start), static_cast<int>(endNoNl), QString());
			setMode(ViMode::Insert, edit);
			return true;
		}
		if( op == QChar('d') ) {
			TextDocument::pos_t delStart = start;
			TextDocument::pos_t delEnd = endWithNl;
			//	最終行を含む削除で直前に改行があればそれも消す
			if( l2 + 1 >= mgr.lineCount() && delStart > 0 && doc->characterAt(delStart - 1) == QChar('\n') )
				--delStart;
			edit->applyPreviewEdit(static_cast<int>(delStart), static_cast<int>(delEnd), QString());
			const int targetLn = qMin(l1, doc->lineMgr().lineCount() - 1);
			TextCursor c = edit->textCursor();
			c.setPosition(firstNonBlankPos(*doc, targetLn));
			edit->setTextCursor(c);
			clampCursorInNormal(edit);
			return true;
		}
		return false;
	}

	if( inclusive && p2 < doc->characterCount() && doc->characterAt(p2) != QChar('\n') )
		++p2;
	if( p2 <= p1 )
		return false;

	QString yanked;
	for( TextDocument::pos_t i = p1; i < p2; ++i )
		yanked.append(doc->characterAt(i));
	m_yankBuf = yanked;
	m_yankLinewise = false;
	QApplication::clipboard()->setText(m_yankBuf);

	if( op == QChar('y') ) {
		TextCursor c = edit->textCursor();
		c.clearSelection();
		c.setPosition(p1);
		edit->setTextCursor(c);
		return true;
	}
	if( op == QChar('d') ) {
		edit->applyPreviewEdit(static_cast<int>(p1), static_cast<int>(p2), QString());
		clampCursorInNormal(edit);
		return true;
	}
	if( op == QChar('c') ) {
		doc->undoMgr().openBlock();
		m_changeUndoOpen = true;
		edit->applyPreviewEdit(static_cast<int>(p1), static_cast<int>(p2), QString());
		setMode(ViMode::Insert, edit);
		return true;
	}
	return false;
}

bool ViEngine::doPut(TextEdit *edit, bool before, int count)
{
	if( m_yankBuf.isEmpty() ) {
		const QString clip = QApplication::clipboard()->text();
		if( clip.isEmpty() ) return false;
		m_yankBuf = clip;
		m_yankLinewise = clip.endsWith(QChar('\n'));
	}
	TextDocument *doc = edit->document();
	const ViewLineMgr &mgr = doc->lineMgr();
	const TextDocument::pos_t pos = edit->textCursor().position();
	const int ln = mgr.lineAtPos(pos);

	QString rep;
	rep.reserve(m_yankBuf.size() * count);
	for( int i = 0; i < count; ++i )
		rep += m_yankBuf;

	if( m_yankLinewise ) {
		if( before ) {
			const TextDocument::pos_t insPos = mgr.lineStartPos(ln);
			edit->applyPreviewEdit(static_cast<int>(insPos), static_cast<int>(insPos), rep);
			doc->undoMgr().setFlag(kUndoFlagLinewise);
			TextCursor c = edit->textCursor();
			c.setPosition(firstNonBlankPos(*doc, ln));
			edit->setTextCursor(c);
		} else {
			const TextDocument::pos_t lineEnd = mgr.lineEndPos(ln);
			QString toIns = rep;
			if( toIns.endsWith(QChar('\n')) ) toIns.chop(1);
			toIns.prepend(QChar('\n'));
			edit->applyPreviewEdit(static_cast<int>(lineEnd), static_cast<int>(lineEnd), toIns);
			doc->undoMgr().setFlag(kUndoFlagLinewise);
			const int nextLn = qMin(ln + 1, doc->lineMgr().lineCount() - 1);
			TextCursor c = edit->textCursor();
			c.setPosition(firstNonBlankPos(*doc, nextLn));
			edit->setTextCursor(c);
		}
		return true;
	}

	const TextDocument::pos_t lineEnd = mgr.lineEndPos(ln);
	const TextDocument::pos_t insPos = before ? pos : qMin(pos + 1, lineEnd);
	edit->applyPreviewEdit(static_cast<int>(insPos), static_cast<int>(insPos), rep);
	if( !rep.isEmpty() ) {
		TextCursor c = edit->textCursor();
		c.setPosition(qMax(insPos, insPos + static_cast<TextDocument::pos_t>(rep.size()) - 1));
		edit->setTextCursor(c);
	}
	return true;
}

bool ViEngine::search(TextEdit *edit, const QString &pattern, bool forward, int count)
{
	if( !edit || pattern.isEmpty() )
		return false;
	m_lastSearchPat = pattern;
	m_lastSearchForward = forward;
	edit->setSearchHighlight(pattern);

	QRegularExpression re(pattern);
	if( !re.isValid() )
		re = QRegularExpression(QRegularExpression::escape(pattern));

	TextDocument *doc = edit->document();
	const QString fullText = doc->toPlainText();
	if( fullText.isEmpty() )
		return false;

	TextDocument::pos_t cur = edit->textCursor().position();
	bool wrapped = false;
	for( int k = 0; k < qMax(1, count); ++k ) {
		qsizetype foundPos = -1;
		if( forward ) {
			const qsizetype start = qMin(static_cast<qsizetype>(cur) + 1, fullText.size());
			QRegularExpressionMatch m = re.match(fullText, start);
			if( m.hasMatch() ) {
				foundPos = m.capturedStart();
			} else {
				m = re.match(fullText, 0);
				if( m.hasMatch() ) {
					foundPos = m.capturedStart();
					wrapped = true;
				}
			}
		} else {
			//	後方検索：cur より前の最後のマッチを探す
			auto it = re.globalMatch(fullText);
			qsizetype lastBefore = -1;
			qsizetype lastAny = -1;
			while( it.hasNext() ) {
				const qsizetype p = it.next().capturedStart();
				if( p < cur ) lastBefore = p;
				lastAny = p;
			}
			if( lastBefore >= 0 ) {
				foundPos = lastBefore;
			} else if( lastAny >= 0 ) {
				foundPos = lastAny;
				wrapped = true;
			}
		}
		if( foundPos < 0 ) {
			if( m_cb.showStatus )
				m_cb.showStatus(QStringLiteral("Pattern not found: %1").arg(pattern));
			return false;
		}
		cur = static_cast<TextDocument::pos_t>(foundPos);
	}

	if( m_mode == ViMode::Visual || m_mode == ViMode::VisualLine ) {
		m_visualCursor = cur;
		updateVisualSelection(edit);
	} else {
		TextCursor c = edit->textCursor();
		c.setPosition(cur);
		edit->setTextCursor(c);
	}
	if( m_cb.showStatus )
		m_cb.showStatus(wrapped ? QStringLiteral("Search wrapped: %1").arg(pattern) : QStringLiteral("/%1").arg(pattern));
	return true;
}

bool ViEngine::searchNext(TextEdit *edit, bool sameDir, int count)
{
	if( m_lastSearchPat.isEmpty() )
		return false;
	const bool origDir = m_lastSearchForward;
	const bool ok = search(edit, m_lastSearchPat, sameDir ? origDir : !origDir, count);
	m_lastSearchForward = origDir;
	return ok;
}

bool ViEngine::handleKeyPress(TextEdit *edit, QKeyEvent *event)
{
	if( !m_enabled || !edit )
		return false;

	const int key = event->key();
	const bool ctrl = (event->modifiers() & Qt::ControlModifier) != 0;
	const bool alt = (event->modifiers() & Qt::AltModifier) != 0;
	const QString txt = event->text();
	const QChar ch = !txt.isEmpty() ? txt[0] : QChar();

	//	Insert / Replace モード
	if( m_mode == ViMode::Insert || m_mode == ViMode::Replace ) {
		if( key == Qt::Key_Escape || (ctrl && key == Qt::Key_BracketLeft) ) {
			const bool wasInsert = (m_mode == ViMode::Insert);
			TextCursor c = edit->textCursor();
			const int ln = edit->document()->lineMgr().lineAtPos(c.position());
			if( wasInsert && c.position() > edit->document()->lineMgr().lineStartPos(ln) )
				c.setPosition(c.position() - 1);
			edit->setTextCursor(c);
			setMode(ViMode::Normal, edit);
			return true;
		}
		if( m_mode == ViMode::Replace && !ctrl && !alt && !txt.isEmpty() && ch >= QChar(0x20) && ch != QChar(0x7f) ) {
			TextDocument *doc = edit->document();
			const TextDocument::pos_t pos = edit->textCursor().position();
			const int ln = doc->lineMgr().lineAtPos(pos);
			const TextDocument::pos_t lineEnd = doc->lineMgr().lineEndPos(ln);
			const int delEnd = (pos < lineEnd) ? static_cast<int>(pos + 1) : static_cast<int>(pos);
			edit->applyPreviewEdit(static_cast<int>(pos), delEnd, txt);
			return true;
		}
		return false;	//	Insert の通常入力は TextEdit へ任せる
	}

	//	Normal / Visual / VisualLine モード
	if( key == Qt::Key_Escape || (ctrl && key == Qt::Key_BracketLeft) ) {
		resetPending();
		if( m_mode != ViMode::Normal )
			setMode(ViMode::Normal, edit);
		else
			notifyMode(edit);
		return true;
	}

	//	単独修飾キーは無視
	if( key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Meta )
		return true;

	//	Ctrl+C / Ctrl+V / Ctrl+S / Ctrl+O / Ctrl+T / Ctrl+W / F8 等のメニューショートカットは邪魔しない
	if( ctrl && !alt ) {
		if( key == Qt::Key_R ) {
			resetPending();
			edit->redo();
			clampCursorInNormal(edit);
			return true;
		}
		if( key == Qt::Key_F || key == Qt::Key_B || key == Qt::Key_D || key == Qt::Key_U ) {
			TextDocument::pos_t target = 0;
			bool lw = false, inc = false;
			if( computeMotionTarget(edit, QChar(key), true, effectiveCount(), false, QChar(), target, lw, inc) ) {
				if( m_mode == ViMode::Visual || m_mode == ViMode::VisualLine ) {
					m_visualCursor = target;
					updateVisualSelection(edit);
				} else {
					TextCursor c = edit->textCursor();
					c.setPosition(target);
					edit->setTextCursor(c);
				}
				resetPending();
				return true;
			}
		}
		return false;
	}

	//	矢印キーを h/j/k/l にマップ
	QChar effCh = ch;
	if( key == Qt::Key_Left ) effCh = QChar('h');
	else if( key == Qt::Key_Right ) effCh = QChar('l');
	else if( key == Qt::Key_Up ) effCh = QChar('k');
	else if( key == Qt::Key_Down ) effCh = QChar('j');
	else if( key == Qt::Key_Home ) effCh = QChar('0');
	else if( key == Qt::Key_End ) effCh = QChar('$');
	else if( key == Qt::Key_Return || key == Qt::Key_Enter ) effCh = QChar('+');
	else if( key == Qt::Key_Backspace ) effCh = QChar('h');
	else if( key == Qt::Key_Delete ) effCh = QChar('x');

	//	プレフィックス待ち（r / f / F / t / T / g / Z）
	if( !m_prefix.isNull() ) {
		const QChar pref = m_prefix;
		const int cnt = effectiveCount();
		const QChar op = m_operator;
		resetPending();

		if( pref == QChar('r') ) {
			if( !ch.isNull() && ch >= QChar(0x20) && ch != QChar(0x7f) ) {
				TextDocument *doc = edit->document();
				const TextDocument::pos_t pos = edit->textCursor().position();
				const int ln = doc->lineMgr().lineAtPos(pos);
				const TextDocument::pos_t lineEnd = doc->lineMgr().lineEndPos(ln);
				if( pos + cnt <= lineEnd ) {
					edit->applyPreviewEdit(static_cast<int>(pos), static_cast<int>(pos + cnt), QString(cnt, ch));
					TextCursor c = edit->textCursor();
					c.setPosition(pos + cnt - 1);
					edit->setTextCursor(c);
				}
			}
			return true;
		}
		if( pref == QChar('f') || pref == QChar('F') || pref == QChar('t') || pref == QChar('T') ) {
			if( !ch.isNull() ) {
				m_lastFindCmd = pref;
				m_lastFindChar = ch;
				TextDocument::pos_t target = 0;
				bool lw = false, inc = false;
				if( computeMotionTarget(edit, QChar(';'), false, cnt, !op.isNull(), op, target, lw, inc) ) {
					if( !op.isNull() ) {
						applyOperatorRange(edit, op, edit->textCursor().position(), target, lw, inc);
					} else if( m_mode == ViMode::Visual || m_mode == ViMode::VisualLine ) {
						m_visualCursor = target;
						updateVisualSelection(edit);
					} else {
						TextCursor c = edit->textCursor();
						c.setPosition(target);
						edit->setTextCursor(c);
					}
				}
			}
			return true;
		}
		if( pref == QChar('g') ) {
			if( effCh == QChar('g') ) {
				TextDocument *doc = edit->document();
				const int targetLn = qBound(0, cnt - 1, doc->lineMgr().lineCount() - 1);
				const TextDocument::pos_t target = firstNonBlankPos(*doc, targetLn);
				if( !op.isNull() ) {
					applyOperatorRange(edit, op, edit->textCursor().position(), target, true, false);
				} else if( m_mode == ViMode::Visual || m_mode == ViMode::VisualLine ) {
					m_visualCursor = target;
					updateVisualSelection(edit);
				} else {
					TextCursor c = edit->textCursor();
					c.setPosition(target);
					edit->setTextCursor(c);
				}
			}
			return true;
		}
		if( pref == QChar('[') || pref == QChar(']') ) {
			if( effCh == pref ) {
				TextDocument *doc = edit->document();
				const ViewLineMgr &mgr = doc->lineMgr();
				const int totalLines = mgr.lineCount();
				const int curLn = mgr.lineAtPos(edit->textCursor().position());
				int targetLn = curLn;
				const int step = (pref == QChar(']')) ? 1 : -1;
				int found = 0;
				for( int ln = curLn + step; ln >= 0 && ln < totalLines; ln += step ) {
					if( mgr.lineType(ln) == LineMgr::LT_HEADING ) {
						targetLn = ln;
						if( ++found >= cnt ) break;
					}
				}
				if( targetLn != curLn ) {
					const TextDocument::pos_t target = mgr.lineStartPos(targetLn);
					if( !op.isNull() ) {
						applyOperatorRange(edit, op, edit->textCursor().position(), target, true, false);
					} else if( m_mode == ViMode::Visual || m_mode == ViMode::VisualLine ) {
						m_visualCursor = target;
						updateVisualSelection(edit);
					} else {
						TextCursor c = edit->textCursor();
						c.setPosition(target);
						edit->setTextCursor(c);
					}
				}
			}
			return true;
		}
		if( pref == QChar('Z') ) {
			if( effCh == QChar('Z') ) {
				if( m_cb.requestSave && m_cb.requestSave() && m_cb.requestQuit )
					m_cb.requestQuit(false);
			} else if( effCh == QChar('Q') ) {
				if( m_cb.requestQuit )
					m_cb.requestQuit(true);
			}
			return true;
		}
		return true;
	}

	//	数値繰り返しプレフィックス（'0' は既に入力中の場合のみ）
	if( effCh.isDigit() && (effCh != QChar('0') || m_repeatCount > 0) ) {
		m_repeatCount = qMin(100000, m_repeatCount * 10 + (effCh.unicode() - '0'));
		return true;
	}

	const int cnt = effectiveCount();
	TextDocument *doc = edit->document();
	const ViewLineMgr &mgr = doc->lineMgr();
	const TextDocument::pos_t pos = edit->textCursor().position();
	const int curLine = mgr.lineAtPos(pos);

	//	Visual / VisualLine モードでの操作
	if( m_mode == ViMode::Visual || m_mode == ViMode::VisualLine ) {
		if( effCh == QChar('v') && m_mode == ViMode::Visual ) { setMode(ViMode::Normal, edit); return true; }
		if( effCh == QChar('V') && m_mode == ViMode::VisualLine ) { setMode(ViMode::Normal, edit); return true; }
		if( effCh == QChar('d') || effCh == QChar('x') || effCh == QChar('c') || effCh == QChar('y')
			|| effCh == QChar('>') || effCh == QChar('<') ) {
			const QChar op = (effCh == QChar('x')) ? QChar('d') : effCh;
			const TextCursor c = edit->textCursor();
			const bool lw = (m_mode == ViMode::VisualLine) || op == QChar('>') || op == QChar('<');
			const TextDocument::pos_t s = c.selectionStart();
			const TextDocument::pos_t e = c.selectionEnd();
			m_mode = ViMode::Normal;
			resetPending();
			applyOperatorRange(edit, op, s, lw ? qMax(s, e - 1) : e, lw, false);
			if( m_mode == ViMode::Normal )
				notifyMode(edit);
			return true;
		}
		if( effCh == QChar('~') ) {
			const TextCursor c = edit->textCursor();
			const int s = static_cast<int>(c.selectionStart());
			const int e = static_cast<int>(c.selectionEnd());
			QString toggled;
			for( int i = s; i < e; ++i ) {
				const QChar ch0 = doc->characterAt(i);
				toggled.append(ch0.isUpper() ? ch0.toLower() : ch0.toUpper());
			}
			m_mode = ViMode::Normal;
			resetPending();
			edit->applyPreviewEdit(s, e, toggled);
			notifyMode(edit);
			return true;
		}
	}

	//	オペレータ待ち中
	if( !m_operator.isNull() ) {
		const QChar op = m_operator;
		if( effCh == op ) {
			//	dd / cc / yy / >> / <<
			const int targetLn = qMin(mgr.lineCount() - 1, curLine + cnt - 1);
			resetPending();
			applyOperatorRange(edit, op, pos, mgr.lineStartPos(targetLn), true, false);
			return true;
		}
		if( effCh == QChar('g') || effCh == QChar('f') || effCh == QChar('F') || effCh == QChar('t') || effCh == QChar('T')
			|| effCh == QChar('[') || effCh == QChar(']') ) {
			m_prefix = effCh;
			return true;
		}
		TextDocument::pos_t target = 0;
		bool lw = false, inc = false;
		if( computeMotionTarget(edit, effCh, false, cnt, true, op, target, lw, inc) ) {
			resetPending();
			applyOperatorRange(edit, op, pos, target, lw, inc);
			return true;
		}
		resetPending();
		return true;
	}

	//	モーション単体（Normal / Visual 共通）
	if( effCh == QChar('g') || effCh == QChar('f') || effCh == QChar('F') || effCh == QChar('t') || effCh == QChar('T')
		|| effCh == QChar('[') || effCh == QChar(']') ) {
		m_prefix = effCh;
		return true;
	}
	{
		TextDocument::pos_t target = 0;
		bool lw = false, inc = false;
		if( computeMotionTarget(edit, effCh, false, cnt, false, QChar(), target, lw, inc) ) {
			resetPending();
			if( m_mode == ViMode::Visual || m_mode == ViMode::VisualLine ) {
				m_visualCursor = target;
				updateVisualSelection(edit);
			} else {
				TextCursor c = edit->textCursor();
				c.setPosition(target);
				edit->setTextCursor(c);
			}
			return true;
		}
	}

	//	コマンドライン・検索起動
	if( effCh == QChar(':') || effCh == QChar('/') || effCh == QChar('?') ) {
		resetPending();
		if( m_cb.openCmdLine )
			m_cb.openCmdLine(effCh);
		return true;
	}
	if( effCh == QChar('n') || effCh == QChar('N') ) {
		resetPending();
		searchNext(edit, effCh == QChar('n'), cnt);
		return true;
	}
	if( effCh == QChar('*') || effCh == QChar('#') ) {
		resetPending();
		const TextDocument::pos_t total = doc->characterCount();
		if( pos < total ) {
			const ViCharClass cls = viCharClass(doc->characterAt(pos));
			if( cls != VCC_SPACE && cls != VCC_NEWLINE ) {
				TextDocument::pos_t s = pos, e = pos + 1;
				while( s > 0 && viCharClass(doc->characterAt(s - 1)) == cls ) --s;
				while( e < total && viCharClass(doc->characterAt(e)) == cls ) ++e;
				QString word;
				for( TextDocument::pos_t i = s; i < e; ++i ) word.append(doc->characterAt(i));
				search(edit, QRegularExpression::escape(word), effCh == QChar('*'), cnt);
			}
		}
		return true;
	}

	//	Normal モード専用コマンド
	if( m_mode != ViMode::Normal ) {
		resetPending();
		return true;
	}

	switch( effCh.unicode() ) {
	case 'i':
		setMode(ViMode::Insert, edit);
		return true;
	case 'a': {
		const TextDocument::pos_t lineEnd = mgr.lineEndPos(curLine);
		TextCursor c = edit->textCursor();
		if( pos < lineEnd ) c.setPosition(pos + 1);
		edit->setTextCursor(c);
		setMode(ViMode::Insert, edit);
		return true;
	}
	case 'I': {
		TextCursor c = edit->textCursor();
		c.setPosition(firstNonBlankPos(*doc, curLine));
		edit->setTextCursor(c);
		setMode(ViMode::Insert, edit);
		return true;
	}
	case 'A': {
		TextCursor c = edit->textCursor();
		c.setPosition(mgr.lineEndPos(curLine));
		edit->setTextCursor(c);
		setMode(ViMode::Insert, edit);
		return true;
	}
	case 'o': {
		const TextDocument::pos_t lineEnd = mgr.lineEndPos(curLine);
		edit->applyPreviewEdit(static_cast<int>(lineEnd), static_cast<int>(lineEnd), QStringLiteral("\n"));
		setMode(ViMode::Insert, edit);
		return true;
	}
	case 'O': {
		const TextDocument::pos_t lineStart = mgr.lineStartPos(curLine);
		edit->applyPreviewEdit(static_cast<int>(lineStart), static_cast<int>(lineStart), QStringLiteral("\n"));
		TextCursor c = edit->textCursor();
		c.setPosition(lineStart);
		edit->setTextCursor(c);
		setMode(ViMode::Insert, edit);
		return true;
	}
	case 'R':
		setMode(ViMode::Replace, edit);
		return true;
	case 'v':
		setMode(ViMode::Visual, edit);
		return true;
	case 'V':
		setMode(ViMode::VisualLine, edit);
		return true;
	case 'd':
	case 'c':
	case 'y':
	case '>':
	case '<':
		m_operator = effCh;
		return true;
	case 'r':
	case 'Z':
		m_prefix = effCh;
		return true;
	case 'x': {
		const TextDocument::pos_t lineEnd = mgr.lineEndPos(curLine);
		const TextDocument::pos_t delEnd = qMin(lineEnd, pos + cnt);
		resetPending();
		if( delEnd > pos )
			applyOperatorRange(edit, QChar('d'), pos, delEnd, false, false);
		return true;
	}
	case 'X': {
		const TextDocument::pos_t lineStart = mgr.lineStartPos(curLine);
		const TextDocument::pos_t delStart = qMax(lineStart, pos - cnt);
		resetPending();
		if( pos > delStart )
			applyOperatorRange(edit, QChar('d'), delStart, pos, false, false);
		return true;
	}
	case 'D': {
		const TextDocument::pos_t lineEnd = mgr.lineEndPos(curLine);
		resetPending();
		applyOperatorRange(edit, QChar('d'), pos, lineEnd, false, false);
		return true;
	}
	case 'C': {
		const TextDocument::pos_t lineEnd = mgr.lineEndPos(curLine);
		resetPending();
		if( lineEnd > pos )
			applyOperatorRange(edit, QChar('c'), pos, lineEnd, false, false);
		else
			setMode(ViMode::Insert, edit);
		return true;
	}
	case 'Y': {
		const int targetLn = qMin(mgr.lineCount() - 1, curLine + cnt - 1);
		resetPending();
		applyOperatorRange(edit, QChar('y'), pos, mgr.lineStartPos(targetLn), true, false);
		return true;
	}
	case 's': {
		const TextDocument::pos_t lineEnd = mgr.lineEndPos(curLine);
		const TextDocument::pos_t delEnd = qMin(lineEnd, pos + cnt);
		resetPending();
		if( delEnd > pos )
			applyOperatorRange(edit, QChar('c'), pos, delEnd, false, false);
		else
			setMode(ViMode::Insert, edit);
		return true;
	}
	case 'S': {
		const int targetLn = qMin(mgr.lineCount() - 1, curLine + cnt - 1);
		resetPending();
		applyOperatorRange(edit, QChar('c'), pos, mgr.lineStartPos(targetLn), true, false);
		return true;
	}
	case '~': {
		const TextDocument::pos_t lineEnd = mgr.lineEndPos(curLine);
		const TextDocument::pos_t end = qMin(lineEnd, pos + cnt);
		resetPending();
		if( end > pos ) {
			QString toggled;
			for( TextDocument::pos_t i = pos; i < end; ++i ) {
				const QChar ch0 = doc->characterAt(i);
				toggled.append(ch0.isUpper() ? ch0.toLower() : ch0.toUpper());
			}
			edit->applyPreviewEdit(static_cast<int>(pos), static_cast<int>(end), toggled);
			clampCursorInNormal(edit);
		}
		return true;
	}
	case 'J': {
		resetPending();
		const int joins = qMax(1, cnt - (cnt > 1 ? 1 : 0));
		for( int j = 0; j < joins; ++j ) {
			const int ln = doc->lineMgr().lineAtPos(edit->textCursor().position());
			if( ln + 1 >= doc->lineMgr().lineCount() ) break;
			const TextDocument::pos_t nlPos = doc->lineMgr().lineEndPos(ln);
			TextDocument::pos_t nextStart = doc->lineMgr().lineStartPos(ln + 1);
			const TextDocument::pos_t nextEnd = doc->lineMgr().lineEndPos(ln + 1);
			while( nextStart < nextEnd && (doc->characterAt(nextStart) == QChar(' ') || doc->characterAt(nextStart) == QChar('\t')) )
				++nextStart;
			const bool needSpace = (nlPos > doc->lineMgr().lineStartPos(ln) && doc->characterAt(nlPos - 1) != QChar(' ') && nextStart < nextEnd);
			edit->applyPreviewEdit(static_cast<int>(nlPos), static_cast<int>(nextStart), needSpace ? QStringLiteral(" ") : QString());
			TextCursor c = edit->textCursor();
			c.setPosition(nlPos);
			edit->setTextCursor(c);
		}
		return true;
	}
	case 'p':
	case 'P':
		resetPending();
		doPut(edit, effCh == QChar('P'), cnt);
		return true;
	case 'u':
		resetPending();
		for( int i = 0; i < cnt; ++i ) {
			const bool wasLinewise = (doc->undoMgr().undoActionFlags() & kUndoFlagLinewise) != 0;
			edit->undo();
			if( wasLinewise ) {
				const int ln = doc->lineMgr().lineAtPos(edit->textCursor().position());
				TextCursor c = edit->textCursor();
				c.setPosition(firstNonBlankPos(*doc, ln));
				edit->setTextCursor(c);
			}
		}
		clampCursorInNormal(edit);
		return true;
	default:
		resetPending();
		return true;
	}
}

void ViEngine::executeExCommand(TextEdit *edit, const QString &cmdLine, const ExCallbacks &cb)
{
	const QString text = cmdLine.trimmed();
	if( text.isEmpty() ) return;

	TextDocument *doc = edit ? edit->document() : nullptr;
	const int totalLines = doc ? doc->lineMgr().lineCount() : 1;
	const int curLine = (doc && edit) ? (doc->lineMgr().lineAtPos(edit->textCursor().position()) + 1) : 1;

	qsizetype ix = 0;
	int rangeStart = curLine;
	int rangeEnd = curLine;
	bool hasRange = false;

	if( text[ix] == QChar('%') ) {
		++ix;
		rangeStart = 1;
		rangeEnd = totalLines;
		hasRange = true;
	} else if( doc ) {
		qsizetype saved = ix;
		const int r1 = parseExLineSpec(text, ix, curLine, totalLines, *doc);
		if( r1 > 0 ) {
			hasRange = true;
			rangeStart = rangeEnd = r1;
			if( ix < text.size() && (text[ix] == QChar(',') || text[ix] == QChar(';')) ) {
				const int base2 = (text[ix] == QChar(';')) ? r1 : curLine;
				++ix;
				const int r2 = parseExLineSpec(text, ix, base2, totalLines, *doc);
				if( r2 < 0 ) {
					if( cb.showStatus ) cb.showStatus(QStringLiteral("Invalid range"));
					return;
				}
				rangeEnd = r2;
			}
		} else {
			ix = saved;
		}
	}

	while( ix < text.size() && text[ix].isSpace() ) ++ix;

	//	:{n} のみ → 指定行ジャンプ
	if( ix >= text.size() ) {
		if( hasRange && edit && doc ) {
			const int targetLn = qBound(0, rangeEnd - 1, totalLines - 1);
			TextCursor c = edit->textCursor();
			c.setPosition(firstNonBlankPos(*doc, targetLn));
			edit->setTextCursor(c);
		}
		return;
	}

	if( rangeStart > rangeEnd ) std::swap(rangeStart, rangeEnd);

	//	コマンド名抽出
	qsizetype cmdStart = ix;
	while( ix < text.size() && text[ix].isLetter() ) ++ix;
	const QString cmd = text.mid(cmdStart, ix - cmdStart);
	bool bang = false;
	if( ix < text.size() && text[ix] == QChar('!') ) {
		bang = true;
		++ix;
	}
	const QString arg = text.mid(ix).trimmed();

	if( exCmdMatch(cmd, "noh", "nohlsearch") ) {
		if( edit ) edit->clearSearchHighlight();
		return;
	}
	if( exCmdMatch(cmd, "q", "quit") ) {
		if( cb.closeTab ) cb.closeTab(bang);
		return;
	}
	if( exCmdMatch(cmd, "w", "write") ) {
		if( cb.saveFile ) cb.saveFile(arg);
		return;
	}
	if( cmd == QStringLiteral("wq") || exCmdMatch(cmd, "x", "xit") ) {
		if( cb.saveFile && cb.saveFile(arg) && cb.closeTab )
			cb.closeTab(false);
		return;
	}
	if( exCmdMatch(cmd, "e", "edit") ) {
		if( cb.openFile ) cb.openFile(arg);
		return;
	}
	if( cmd == QStringLiteral("pwd") ) {
		const QString cur = QDir::currentPath();
		if( cb.showOutput ) cb.showOutput(QStringLiteral("\ncurrent directory: %1\n").arg(cur));
		if( cb.showStatus ) cb.showStatus(cur);
		return;
	}
	if( cmd == QStringLiteral("cd") ) {
		QString targetDir = arg;
		if( targetDir == QStringLiteral("~") )
			targetDir = QDir::homePath();
		else if( targetDir.startsWith(QStringLiteral("~/")) || targetDir.startsWith(QStringLiteral("~\\")) )
			targetDir = QDir::homePath() + targetDir.mid(1);
		if( !targetDir.isEmpty() ) {
			if( QDir::setCurrent(targetDir) ) {
				const QString cur = QDir::currentPath();
				if( cb.showOutput ) cb.showOutput(QStringLiteral("\ncurrent directory: %1\n").arg(cur));
				if( cb.showStatus ) cb.showStatus(cur);
			} else if( cb.showStatus ) {
				cb.showStatus(QStringLiteral("Cannot change directory: %1").arg(targetDir));
			}
		}
		return;
	}

	if( !edit || !doc ) return;

	//	:{range}d / :{range}y
	if( exCmdMatch(cmd, "d", "delete") || exCmdMatch(cmd, "y", "yank") ) {
		const int l1 = qBound(0, rangeStart - 1, totalLines - 1);
		const int l2 = qBound(l1, rangeEnd - 1, totalLines - 1);
		applyOperatorRange(edit, cmd[0], doc->lineMgr().lineStartPos(l1), doc->lineMgr().lineStartPos(l2), true, false);
		return;
	}

	//	:{range}s/pat/rep/[g][i]
	if( exCmdMatch(cmd, "s", "substitute") ) {
		const QString rest = text.mid(ix);
		if( rest.isEmpty() ) return;
		const QChar delim = rest[0];
		auto splitNext = [&](qsizetype &p) -> QString {
			QString out;
			while( p < rest.size() && rest[p] != delim ) {
				if( rest[p] == QChar('\\') && p + 1 < rest.size() && rest[p + 1] == delim ) {
					out.append(delim);
					p += 2;
				} else {
					out.append(rest[p++]);
				}
			}
			if( p < rest.size() && rest[p] == delim ) ++p;
			return out;
		};
		qsizetype p = 1;
		const QString pat = splitNext(p);
		const QString rep = splitNext(p);
		const QString flags = rest.mid(p);
		const bool global = flags.contains(QChar('g'));
		const bool icase = flags.contains(QChar('i'));
		const QRegularExpression re(pat, icase ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption);
		if( !re.isValid() ) {
			if( cb.showStatus ) cb.showStatus(QStringLiteral("Invalid regex: %1").arg(pat));
			return;
		}
		const int l1 = qBound(0, rangeStart - 1, totalLines - 1);
		const int l2 = qBound(l1, rangeEnd - 1, totalLines - 1);
		int count = 0;
		doc->undoMgr().openBlock();
		for( int ln = l1; ln <= l2; ++ln ) {
			const QString orig = lineTextWithoutNl(*doc, ln);
			QString replaced;
			if( global ) {
				auto it = re.globalMatch(orig);
				qsizetype last = 0;
				while( it.hasNext() ) {
					const auto m = it.next();
					replaced += orig.mid(last, m.capturedStart() - last);
					replaced += rep;
					last = m.capturedEnd();
					++count;
				}
				if( last == 0 ) continue;
				replaced += orig.mid(last);
			} else {
				const auto m = re.match(orig);
				if( !m.hasMatch() ) continue;
				replaced = orig.left(m.capturedStart()) + rep + orig.mid(m.capturedEnd());
				++count;
			}
			const TextDocument::pos_t ls = doc->lineMgr().lineStartPos(ln);
			const TextDocument::pos_t le = doc->lineMgr().lineEndPos(ln);
			doc->erase(ls, le);
			doc->insert(ls, replaced);
		}
		doc->undoMgr().closeBlock();
		if( count > 0 ) {
			TextCursor c = edit->textCursor();
			c.setPosition(firstNonBlankPos(*doc, qMin(l1, doc->lineMgr().lineCount() - 1)));
			edit->setTextCursor(c);
			edit->notifyTextChanged();
		}
		if( cb.showStatus )
			cb.showStatus(QStringLiteral("%1 substitution(s)").arg(count));
		return;
	}

	//	:{range}g/pat/d または :v/pat/d
	if( cmd == QStringLiteral("g") || cmd == QStringLiteral("global") || cmd == QStringLiteral("v") ) {
		const bool invert = (cmd == QStringLiteral("v")) ^ bang;
		const QString rest = text.mid(ix);
		if( rest.isEmpty() ) return;
		const QChar delim = rest[0];
		qsizetype p = 1;
		QString pat;
		while( p < rest.size() && rest[p] != delim ) {
			if( rest[p] == QChar('\\') && p + 1 < rest.size() && rest[p + 1] == delim ) {
				pat.append(delim);
				p += 2;
			} else {
				pat.append(rest[p++]);
			}
		}
		if( p < rest.size() && rest[p] == delim ) ++p;
		const QString subCmd = rest.mid(p).trimmed();
		const QRegularExpression re(pat);
		if( !re.isValid() ) {
			if( cb.showStatus ) cb.showStatus(QStringLiteral("Invalid regex: %1").arg(pat));
			return;
		}
		const int l1 = hasRange ? qBound(0, rangeStart - 1, totalLines - 1) : 0;
		const int l2 = hasRange ? qBound(l1, rangeEnd - 1, totalLines - 1) : (totalLines - 1);
		if( subCmd == QStringLiteral("d") || subCmd == QStringLiteral("delete") ) {
			int delCount = 0;
			doc->undoMgr().openBlock();
			for( int ln = l2; ln >= l1; --ln ) {
				const bool matched = re.match(lineTextWithoutNl(*doc, ln)).hasMatch();
				if( matched != invert ) {
					TextDocument::pos_t ds = doc->lineMgr().lineStartPos(ln);
					TextDocument::pos_t de = (ln + 1 < doc->lineMgr().lineCount())
						? doc->lineMgr().lineStartPos(ln + 1)
						: doc->lineMgr().lineEndPos(ln);
					if( ln + 1 >= doc->lineMgr().lineCount() && ds > 0 && doc->characterAt(ds - 1) == QChar('\n') )
						--ds;
					doc->erase(ds, de);
					++delCount;
				}
			}
			doc->undoMgr().closeBlock();
			if( delCount > 0 ) {
				TextCursor c = edit->textCursor();
				c.setPosition(firstNonBlankPos(*doc, qMin(l1, doc->lineMgr().lineCount() - 1)));
				edit->setTextCursor(c);
				edit->notifyTextChanged();
			}
			if( cb.showStatus )
				cb.showStatus(QStringLiteral("%1 line(s) deleted").arg(delCount));
			return;
		}
	}

	if( cb.showStatus )
		cb.showStatus(QStringLiteral("Unknown ex command: :%1").arg(text));
}
