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
#include <QTimer>
#include <QWheelEvent>

namespace {
const int LEFT_MARGIN = 2;

enum WordCharClass {
	WCC_NEWLINE = 0,
	WCC_SPACE,
	WCC_ALNUM,
	WCC_HIRAGANA,
	WCC_KATAKANA,
	WCC_KANJI,
	WCC_SYMBOL,
};

WordCharClass wordCharClass(QChar ch)
{
	const ushort u = ch.unicode();
	if( u == '\n' || u == '\r' ) return WCC_NEWLINE;
	if( u == ' ' || u == '\t' || u == 0x3000 ) return WCC_SPACE;
	if( u < 0x80 ) {
		if( ch.isLetterOrNumber() || u == '_' ) return WCC_ALNUM;
		return WCC_SYMBOL;
	}
	if( (u >= 0x3040 && u <= 0x309F) || u == 0x30FC ) return WCC_HIRAGANA;
	if( (u >= 0x30A0 && u <= 0x30FF) || (u >= 0xFF65 && u <= 0xFF9F) ) return WCC_KATAKANA;
	if( (u >= 0x4E00 && u <= 0x9FFF) || (u >= 0x3400 && u <= 0x4DBF) ) return WCC_KANJI;
	if( ch.isLetterOrNumber() ) return WCC_ALNUM;
	return WCC_SYMBOL;
}

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

	if( lineType == LineMgr::LT_CODE_BLOCK )
		return spans;	//	コードブロック内は Markdown 強調しない

	if( lineType == LineMgr::LT_CODE_FENCE ) {
		spans.push_back({0, lineText.size(), QColor(0x66, 0x66, 0x66)});
		return spans;
	}

	if( lineType == LineMgr::LT_HEADING ) {
		spans.push_back({0, lineText.size(), headingColor});
		return spans;
	}

	qsizetype inlineStart = 0;
	if( lineType == LineMgr::LT_CHECKBOX ) {
		while( inlineStart < lineText.size()
			&& (lineText[inlineStart] == QChar(' ') || lineText[inlineStart] == QChar('\t')) )
			++inlineStart;
		if( inlineStart + 6 <= lineText.size() )
			inlineStart += 6;	//	チェックボックスマーカー（"- [ ] " 等）をスキップ
	} else if( lineType == LineMgr::LT_LIST ) {
		while( inlineStart < lineText.size()
			&& (lineText[inlineStart] == QChar(' ') || lineText[inlineStart] == QChar('\t')) )
			++inlineStart;
		if( inlineStart + 2 <= lineText.size() )
			inlineStart += 2;	//	リストマーカー（"- " / "* " / "+ "）をスキップ
	} else if( lineType == LineMgr::LT_NUM_LIST ) {
		while( inlineStart < lineText.size()
			&& (lineText[inlineStart] == QChar(' ') || lineText[inlineStart] == QChar('\t')) )
			++inlineStart;
		while( inlineStart < lineText.size() && lineText[inlineStart] >= QChar('0') && lineText[inlineStart] <= QChar('9') )
			++inlineStart;
		if( inlineStart + 2 <= lineText.size() && lineText[inlineStart] == QChar('.') )
			inlineStart += 2;	//	番号付きリストマーカー（"1. " 等）をスキップ
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
	m_blinkTimer = new QTimer(this);
	m_blinkTimer->setInterval(500);
	connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
		m_caretVisible = !m_caretVisible;
		viewport()->update();
	});
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

void TextEdit::cut()
{
	if( !m_cursor.hasSelection() ) return;
	QApplication::clipboard()->setText(m_cursor.selectedText());
	m_cursor.removeSelectedText();
	afterChange(true);
}

void TextEdit::copy()
{
	if( !m_cursor.hasSelection() ) return;
	QApplication::clipboard()->setText(m_cursor.selectedText());
}

void TextEdit::paste()
{
	QString t = QApplication::clipboard()->text();
	if( t.isEmpty() ) return;
	m_cursor.insertText(t);
	afterChange(true);
}

void TextEdit::selectAll()
{
	m_cursor.setPosition(0);
	m_cursor.setPosition(m_doc.characterCount(), TextCursor::KeepAnchor);
	afterChange(false);
}

void TextEdit::toggleCheckboxAtLine(int line, bool checked)
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	if( line < 0 || line >= mgr.lineCount() || mgr.lineType(line) != LineMgr::LT_CHECKBOX )
		return;
	const TextDocument::pos_t lineStart = mgr.lineStartPos(line);
	const TextDocument::pos_t lineLen = mgr.lineLength(line);
	TextDocument::pos_t i = 0;
	while( i < lineLen && (m_doc.characterAt(lineStart + i) == QChar(' ') || m_doc.characterAt(lineStart + i) == QChar('\t')) )
		++i;
	if( i + 4 >= lineLen )
		return;
	const TextDocument::pos_t chkPos = lineStart + i + 3;	//	"- [ ]" の中括弧内の文字位置
	const QChar target = checked ? QChar('x') : QChar(' ');
	if( m_doc.characterAt(chkPos) == target )
		return;
	m_doc.undoMgr().openBlock();
	m_doc.erase(chkPos, chkPos + 1);
	m_doc.insert(chkPos, QString(target));
	m_doc.undoMgr().closeBlock();
	afterChange(true);
}

namespace {
bool isCsvFenceLine(const QString &line)
{
	qsizetype i = 0;
	while( i < line.size() && i < 3 && line[i] == QChar(' ') )
		++i;
	const QStringView rest = QStringView(line).mid(i);
	if( !rest.startsWith(u"```") )
		return false;
	return rest.mid(3).trimmed().compare(u"csv", Qt::CaseInsensitive) == 0;
}

bool isAnyFenceLine(const QString &line)
{
	qsizetype i = 0;
	while( i < line.size() && i < 3 && line[i] == QChar(' ') )
		++i;
	return QStringView(line).mid(i).startsWith(u"```");
}

bool parseCsvRow(const QString &line, bool &inQuotes, QStringList &curFields, QString &curField)
{
	if( !inQuotes ) {
		curFields.clear();
		curField.clear();
	}
	const qsizetype len = line.size();
	for( qsizetype i = 0; i < len; ++i ) {
		const QChar ch = line[i];
		if( inQuotes ) {
			if( ch == QChar('"') ) {
				if( i + 1 < len && line[i + 1] == QChar('"') ) {
					curField.append(QChar('"'));
					++i;
				} else {
					inQuotes = false;
				}
			} else {
				curField.append(ch);
			}
		} else {
			if( ch == QChar('"') && curField.trimmed().isEmpty() ) {
				curField.clear();
				inQuotes = true;
			} else if( ch == QChar(',') ) {
				curFields.append(curField.trimmed());
				curField.clear();
			} else {
				curField.append(ch);
			}
		}
	}
	if( inQuotes ) {
		curField.append(QStringLiteral("<br>"));
		return false;
	}
	curFields.append(curField.trimmed());
	return true;
}

bool splitGfmRow(const QString &line, QStringList &outCells)
{
	outCells.clear();
	const QString trimmed = line.trimmed();
	if( trimmed.isEmpty() )
		return false;
	bool hasPipe = false;
	for( qsizetype i = 0; i < trimmed.size(); ++i ) {
		if( trimmed[i] == QChar('\\') ) { ++i; continue; }
		if( trimmed[i] == QChar('|') ) { hasPipe = true; break; }
	}
	if( !hasPipe )
		return false;
	qsizetype start = 0;
	qsizetype end = trimmed.size();
	if( start < end && trimmed[start] == QChar('|') )
		++start;
	if( end > start && trimmed[end - 1] == QChar('|') ) {
		qsizetype bs = 0;
		for( qsizetype k = end - 2; k >= start && trimmed[k] == QChar('\\'); --k )
			++bs;
		if( (bs % 2) == 0 )
			--end;
	}
	QString cell;
	for( qsizetype i = start; i < end; ++i ) {
		if( trimmed[i] == QChar('\\') && i + 1 < end && trimmed[i + 1] == QChar('|') ) {
			cell.append(QChar('|'));
			++i;
		} else if( trimmed[i] == QChar('|') ) {
			outCells.append(cell.trimmed());
			cell.clear();
		} else {
			cell.append(trimmed[i]);
		}
	}
	outCells.append(cell.trimmed());
	return !outCells.isEmpty();
}

bool isGfmSepRow(const QString &line)
{
	QStringList cells;
	if( !splitGfmRow(line, cells) )
		return false;
	for( const QString &c : cells ) {
		if( c.isEmpty() ) return false;
		qsizetype s = 0, e = c.size();
		if( c[s] == QChar(':') ) ++s;
		if( e > s && c[e - 1] == QChar(':') ) --e;
		if( e <= s ) return false;
		for( qsizetype k = s; k < e; ++k ) {
			if( c[k] != QChar('-') ) return false;
		}
	}
	return true;
}

QString escapeCsvCell(const QString &cell)
{
	if( cell.contains(QChar(',')) || cell.contains(QChar('"')) || cell.contains(QChar('\n')) ) {
		QString escaped = cell;
		escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
		return QChar('"') + escaped + QChar('"');
	}
	return cell;
}

QString escapeGfmCell(QString cell)
{
	cell.replace(QStringLiteral("|"), QStringLiteral("\\|"));
	return cell;
}
}

bool TextEdit::convertCsvToGfmTable()
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	const int nLines = mgr.lineCount();
	auto lineStr = [&](int ln) {
		QString s = m_doc.findBlockByNumber(ln).text();
		if( s.endsWith(QChar('\n')) ) s.chop(1);
		if( s.endsWith(QChar('\r')) ) s.chop(1);
		return s;
	};

	int startLine = -1;
	int endLine = -1;
	int dataStart = -1;
	int dataEnd = -1;

	//	1) カーソル位置（または選択先頭）を含む ```csv ... ``` ブロックを探索
	const int curLine = mgr.lineAtPos(m_cursor.hasSelection() ? m_cursor.selectionStart() : m_cursor.position());
	int openFence = -1;
	for( int ln = curLine; ln >= 0; --ln ) {
		const QString s = lineStr(ln);
		if( isCsvFenceLine(s) ) {
			openFence = ln;
			break;
		}
		if( ln < curLine && isAnyFenceLine(s) )
			break;
	}
	if( openFence >= 0 ) {
		int closeFence = -1;
		for( int ln = openFence + 1; ln < nLines; ++ln ) {
			if( isAnyFenceLine(lineStr(ln)) ) {
				closeFence = ln;
				break;
			}
		}
		if( closeFence >= curLine ) {
			startLine = openFence;
			endLine = closeFence;
			dataStart = openFence + 1;
			dataEnd = closeFence - 1;
		}
	}

	//	2) フェンス外で選択範囲がある場合は選択行をそのまま CSV として変換
	if( startLine < 0 && m_cursor.hasSelection() ) {
		startLine = mgr.lineAtPos(m_cursor.selectionStart());
		TextDocument::pos_t selEnd = m_cursor.selectionEnd();
		endLine = mgr.lineAtPos(selEnd);
		if( endLine > startLine && selEnd == mgr.lineStartPos(endLine) )
			--endLine;
		dataStart = startLine;
		dataEnd = endLine;
	}

	if( startLine < 0 || dataStart > dataEnd )
		return false;

	QList<QStringList> rows;
	bool inQuotes = false;
	QStringList curFields;
	QString curField;
	for( int ln = dataStart; ln <= dataEnd; ++ln ) {
		const QString s = lineStr(ln);
		if( !inQuotes && s.trimmed().isEmpty() )
			continue;
		if( parseCsvRow(s, inQuotes, curFields, curField) )
			rows.append(curFields);
	}
	if( inQuotes && !curFields.isEmpty() ) {
		curFields.append(curField.trimmed());
		rows.append(curFields);
	}
	if( rows.isEmpty() )
		return false;

	int maxCols = 0;
	for( const QStringList &r : rows )
		maxCols = qMax(maxCols, static_cast<int>(r.size()));
	if( maxCols <= 0 )
		return false;

	static const QRegularExpression kNumberRe(QStringLiteral(R"(^[+-]?(\d+\.\d*|\d+|\.\d+)%?$)"));
	QStringList outLines;
	auto buildRow = [&](const QStringList &r) {
		QString line = QStringLiteral("|");
		for( int c = 0; c < maxCols; ++c ) {
			const QString cell = (c < r.size()) ? escapeGfmCell(r[c]) : QString();
			line += QStringLiteral(" ") + cell + QStringLiteral(" |");
		}
		return line;
	};
	outLines.append(buildRow(rows[0]));

	QString sepLine = QStringLiteral("|");
	for( int c = 0; c < maxCols; ++c ) {
		bool anyData = false;
		bool allNumeric = true;
		for( int r = 1; r < rows.size(); ++r ) {
			if( c < rows[r].size() && !rows[r][c].isEmpty() ) {
				anyData = true;
				if( !kNumberRe.match(rows[r][c]).hasMatch() ) {
					allNumeric = false;
					break;
				}
			}
		}
		sepLine += (anyData && allNumeric) ? QStringLiteral(" ---: |") : QStringLiteral(" --- |");
	}
	outLines.append(sepLine);

	for( int r = 1; r < rows.size(); ++r )
		outLines.append(buildRow(rows[r]));

	const TextDocument::pos_t repStart = mgr.lineStartPos(startLine);
	TextDocument::pos_t repEnd = mgr.lineStartPos(endLine) + mgr.lineLength(endLine);
	if( repEnd > repStart && m_doc.characterAt(repEnd - 1) == QChar('\n') )
		--repEnd;

	m_doc.undoMgr().openBlock();
	m_doc.erase(repStart, repEnd);
	m_doc.insert(repStart, outLines.join(QChar('\n')));
	m_doc.undoMgr().closeBlock();
	m_cursor.setPosition(repStart);
	afterChange(true);
	return true;
}

bool TextEdit::convertGfmTableToCsv()
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	const int nLines = mgr.lineCount();
	auto lineStr = [&](int ln) {
		QString s = m_doc.findBlockByNumber(ln).text();
		if( s.endsWith(QChar('\n')) ) s.chop(1);
		if( s.endsWith(QChar('\r')) ) s.chop(1);
		return s;
	};

	int startLine = -1;
	int endLine = -1;

	if( m_cursor.hasSelection() ) {
		startLine = mgr.lineAtPos(m_cursor.selectionStart());
		TextDocument::pos_t selEnd = m_cursor.selectionEnd();
		endLine = mgr.lineAtPos(selEnd);
		if( endLine > startLine && selEnd == mgr.lineStartPos(endLine) )
			--endLine;
	} else {
		const int curLine = mgr.lineAtPos(m_cursor.position());
		QStringList tmp;
		if( !splitGfmRow(lineStr(curLine), tmp) )
			return false;
		startLine = curLine;
		while( startLine > 0 && splitGfmRow(lineStr(startLine - 1), tmp) )
			--startLine;
		endLine = curLine;
		while( endLine + 1 < nLines && splitGfmRow(lineStr(endLine + 1), tmp) )
			++endLine;
	}

	if( startLine < 0 || endLine <= startLine )
		return false;
	if( !isGfmSepRow(lineStr(startLine + 1)) )
		return false;

	QStringList outLines;
	outLines.append(QStringLiteral("```csv"));
	for( int ln = startLine; ln <= endLine; ++ln ) {
		if( ln == startLine + 1 )
			continue;	//	GFM 区切り行をスキップ
		QStringList cells;
		if( !splitGfmRow(lineStr(ln), cells) )
			continue;
		QStringList csvCells;
		csvCells.reserve(cells.size());
		for( const QString &c : cells )
			csvCells.append(escapeCsvCell(c));
		outLines.append(csvCells.join(QChar(',')));
	}
	outLines.append(QStringLiteral("```"));

	const TextDocument::pos_t repStart = mgr.lineStartPos(startLine);
	TextDocument::pos_t repEnd = mgr.lineStartPos(endLine) + mgr.lineLength(endLine);
	if( repEnd > repStart && m_doc.characterAt(repEnd - 1) == QChar('\n') )
		--repEnd;

	m_doc.undoMgr().openBlock();
	m_doc.erase(repStart, repEnd);
	m_doc.insert(repStart, outLines.join(QChar('\n')));
	m_doc.undoMgr().closeBlock();
	m_cursor.setPosition(repStart);
	afterChange(true);
	return true;
}

void TextEdit::setLineAlignment(Qt::Alignment align)
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	int startLine = 0;
	int endLine = 0;
	if( m_cursor.hasSelection() ) {
		startLine = mgr.lineAtPos(m_cursor.selectionStart());
		const TextDocument::pos_t selEnd = m_cursor.selectionEnd();
		endLine = mgr.lineAtPos(selEnd);
		if( endLine > startLine && selEnd == mgr.lineStartPos(endLine) )
			--endLine;
	} else {
		startLine = endLine = mgr.lineAtPos(m_cursor.position());
	}

	auto findAlignCommentRange = [](const QString &line, qsizetype &outStart, qsizetype &outEnd) -> bool {
		qsizetype i = 0;
		while( i < line.size() && (line[i] == QChar(' ') || line[i] == QChar('\t')) )
			++i;
		if( i + 4 > line.size() || QStringView(line).mid(i, 4) != u"<!--" )
			return false;
		const qsizetype endPos = line.indexOf(QStringLiteral("-->"), i + 4);
		if( endPos < 0 )
			return false;
		const QStringView body = QStringView(line).mid(i + 4, endPos - (i + 4)).trimmed();
		const qsizetype colon = body.indexOf(QChar(':'));
		if( colon < 0 )
			return false;
		if( body.left(colon).trimmed().compare(u"align", Qt::CaseInsensitive) != 0 )
			return false;
		const QStringView val = body.mid(colon + 1).trimmed();
		if( val.compare(u"left", Qt::CaseInsensitive) != 0
			&& val.compare(u"center", Qt::CaseInsensitive) != 0
			&& val.compare(u"right", Qt::CaseInsensitive) != 0 )
			return false;
		outStart = i;
		outEnd = endPos + 3;
		return true;
	};

	auto isH1Text = [](const QString &s) -> bool {
		qsizetype i = 0;
		while( i < s.size() && (s[i] == QChar(' ') || s[i] == QChar('\t')) )
			++i;
		return i < s.size() && s[i] == QChar('#')
			&& (i + 1 == s.size() || s[i + 1] == QChar(' ') || s[i + 1] == QChar('\t'));
	};

	const bool multiLine = (endLine > startLine);
	bool changed = false;
	m_doc.undoMgr().openBlock();
	for( int ln = endLine; ln >= startLine; --ln ) {
		QString line = m_doc.findBlockByNumber(ln).text();
		if( line.endsWith(QChar('\n')) ) line.chop(1);
		if( line.endsWith(QChar('\r')) ) line.chop(1);
		if( multiLine && line.trimmed().isEmpty() )
			continue;

		const TextDocument::pos_t lineStart = mgr.lineStartPos(ln);
		qsizetype cStart = 0, cEnd = 0;
		const bool hadComment = findAlignCommentRange(line, cStart, cEnd);
		const QString stripped = hadComment ? (line.left(cStart) + line.mid(cEnd)) : line;

		QString tag;
		if( align == Qt::AlignCenter )
			tag = QStringLiteral("<!-- align:center -->");
		else if( align == Qt::AlignRight )
			tag = QStringLiteral("<!-- align:right -->");
		else if( align == Qt::AlignLeft && isH1Text(stripped) )
			tag = QStringLiteral("<!-- align:left -->");

		if( hadComment ) {
			if( line.mid(cStart, cEnd - cStart) == tag )
				continue;
			m_doc.erase(lineStart + cStart, lineStart + cEnd);
			if( !tag.isEmpty() )
				m_doc.insert(lineStart + cStart, tag);
			changed = true;
		} else if( !tag.isEmpty() ) {
			m_doc.insert(lineStart, tag);
			changed = true;
		}
	}
	m_doc.undoMgr().closeBlock();
	if( changed )
		afterChange(true);
}

void TextEdit::indent()
{
	if( indentSelection(false) )
		afterChange(true);
}

void TextEdit::unindent()
{
	if( indentSelection(true) )
		afterChange(true);
}

void TextEdit::setCursorPosition(int pos, int anchor)
{
	const int maxPos = m_doc.characterCount();
	pos = qBound(0, pos, maxPos);
	if( anchor >= 0 && anchor != pos ) {
		anchor = qBound(0, anchor, maxPos);
		m_cursor.setPosition(anchor);
		m_cursor.setPosition(pos, TextCursor::KeepAnchor);
	} else {
		m_cursor.setPosition(pos);
	}
	viewport()->update();
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
	if( hasFocus() && m_blinkTimer ) {
		m_caretVisible = true;
		m_blinkTimer->start();
	}
	updateScrollBars();
	ensureCursorVisible();
	viewport()->update();
	m_lineNumberArea->update();
	QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);
	if( m_onCursorChanged )
		m_onCursorChanged();
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

int TextEdit::logicalLineFromY(int y) const
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	if( mgr.viewLineCount() == 0 ) return 0;
	const int lh = lineHeight();
	int viewLine = verticalScrollBar()->value() + (y >= 0 ? y / lh : (y - lh + 1) / lh);
	viewLine = qBound(0, viewLine, mgr.viewLineCount() - 1);
	return mgr.viewLineToLine(viewLine);
}

void TextEdit::selectLogicalLineRange(int anchorLine, int curLine)
{
	const ViewLineMgr &mgr = m_doc.lineMgr();
	if( mgr.lineCount() == 0 ) return;
	const int lo = qBound(0, qMin(anchorLine, curLine), mgr.lineCount() - 1);
	const int hi = qBound(0, qMax(anchorLine, curLine), mgr.lineCount() - 1);
	const TextDocument::pos_t startPos = mgr.lineStartPos(lo);
	const TextDocument::pos_t endPos = (hi + 1 < mgr.lineCount()) ? mgr.lineStartPos(hi + 1) : mgr.lineEndPos(hi);
	if( curLine < anchorLine ) {
		m_cursor.setPosition(endPos);
		m_cursor.setPosition(startPos, TextCursor::KeepAnchor);
	} else {
		m_cursor.setPosition(startPos);
		m_cursor.setPosition(endPos, TextCursor::KeepAnchor);
	}
}

void TextEdit::lineNumberAreaMousePressEvent(QMouseEvent *event)
{
	if( event->button() == Qt::LeftButton ) {
		setFocus();
		const int ln = logicalLineFromY(event->pos().y());
		if( (event->modifiers() & Qt::ShiftModifier) != 0 && m_lineAnchor >= 0 ) {
			selectLogicalLineRange(m_lineAnchor, ln);
		} else {
			m_lineAnchor = ln;
			selectLogicalLineRange(ln, ln);
		}
		m_lineDragging = true;
		viewport()->update();
	}
}

void TextEdit::lineNumberAreaMouseMoveEvent(QMouseEvent *event)
{
	if( !m_lineDragging || m_lineAnchor < 0 ) return;
	int y = event->pos().y();
	const int vh = viewport()->height();
	QScrollBar *vbar = verticalScrollBar();
	if( y < 0 ) {
		vbar->setValue(vbar->value() + qMin(-1, y / lineHeight()));
		y = 0;
	} else if( y > vh ) {
		vbar->setValue(vbar->value() + qMax(1, (y - vh) / lineHeight()));
		y = vh;
	}
	const int ln = logicalLineFromY(y);
	selectLogicalLineRange(m_lineAnchor, ln);
	ensureCursorVisible();
	viewport()->update();
	m_lineNumberArea->update();
}

void TextEdit::lineNumberAreaMouseReleaseEvent(QMouseEvent *event)
{
	if( event->button() == Qt::LeftButton )
		m_lineDragging = false;
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
	//	カーソル（フォーカス時は赤 2px 縦棒、非フォーカス時は黒 2px 幅 I ビーム）
	if( !hasSel ) {
		int cv = mgr.viewLineAtPos(m_cursor.position());
		if( cv >= top && cv < top + rows ) {
			int x = xOfPos(m_cursor.position());
			if( !m_preedit.isEmpty() )
				x += fm.horizontalAdvance(m_preedit.left(m_preeditCursor));
			const int y0 = (cv - top) * lh;
			const int caretH = fm.ascent() + 2;
			if( hasFocus() ) {
				if( m_caretVisible )
					painter.fillRect(x, y0, 2, caretH, Qt::red);
			} else {
				painter.fillRect(x, y0, 2, caretH, Qt::black);
				painter.fillRect(x - 2, y0, 6, 2, Qt::black);
				painter.fillRect(x - 2, y0 + caretH - 2, 6, 2, Qt::black);
			}
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

void TextEdit::wordBoundsAt(TextDocument::pos_t pos, TextDocument::pos_t &outStart, TextDocument::pos_t &outEnd) const
{
	const TextDocument::pos_t total = m_doc.characterCount();
	if( total == 0 ) {
		outStart = outEnd = 0;
		return;
	}
	if( pos < 0 ) pos = 0;
	if( pos >= total ) pos = total - 1;
	if( m_doc.characterAt(pos) == QChar('\n') ) {
		if( pos > 0 && m_doc.characterAt(pos - 1) != QChar('\n') ) {
			--pos;
		} else {
			outStart = outEnd = pos;
			return;
		}
	}
	const WordCharClass cls = wordCharClass(m_doc.characterAt(pos));
	TextDocument::pos_t s = pos;
	while( s > 0 && wordCharClass(m_doc.characterAt(s - 1)) == cls )
		--s;
	TextDocument::pos_t e = pos + 1;
	while( e < total && wordCharClass(m_doc.characterAt(e)) == cls )
		++e;
	outStart = s;
	outEnd = e;
}

void TextEdit::mousePressEvent(QMouseEvent *event)
{
	if( event->button() == Qt::LeftButton ) {
		m_dragging = true;
		m_wordDragging = false;
		bool keep = (event->modifiers() & Qt::ShiftModifier) != 0;
		m_cursor.setPosition(posFromPoint(event->pos()),
							 keep ? TextCursor::KeepAnchor : TextCursor::MoveAnchor);
		if( m_blinkTimer ) {
			m_caretVisible = true;
			m_blinkTimer->start();
		}
		viewport()->update();
		if( m_onCursorChanged )
			m_onCursorChanged();
	} else {
		QAbstractScrollArea::mousePressEvent(event);
	}
}

void TextEdit::mouseDoubleClickEvent(QMouseEvent *event)
{
	if( event->button() == Qt::LeftButton ) {
		wordBoundsAt(posFromPoint(event->pos()), m_wordAnchorStart, m_wordAnchorEnd);
		m_cursor.setPosition(m_wordAnchorStart);
		m_cursor.setPosition(m_wordAnchorEnd, TextCursor::KeepAnchor);
		m_wordDragging = true;
		m_dragging = false;
		viewport()->update();
		if( m_onCursorChanged )
			m_onCursorChanged();
	} else {
		QAbstractScrollArea::mouseDoubleClickEvent(event);
	}
}

void TextEdit::mouseMoveEvent(QMouseEvent *event)
{
	if( m_wordDragging ) {
		TextDocument::pos_t s = 0, e = 0;
		wordBoundsAt(posFromPoint(event->pos()), s, e);
		if( s < m_wordAnchorStart ) {
			m_cursor.setPosition(m_wordAnchorEnd);
			m_cursor.setPosition(s, TextCursor::KeepAnchor);
		} else {
			m_cursor.setPosition(m_wordAnchorStart);
			m_cursor.setPosition(e, TextCursor::KeepAnchor);
		}
		ensureCursorVisible();
		viewport()->update();
		if( m_onCursorChanged )
			m_onCursorChanged();
	} else if( m_dragging ) {
		m_cursor.setPosition(posFromPoint(event->pos()), TextCursor::KeepAnchor);
		ensureCursorVisible();
		viewport()->update();
		if( m_onCursorChanged )
			m_onCursorChanged();
	} else {
		QAbstractScrollArea::mouseMoveEvent(event);
	}
}

void TextEdit::mouseReleaseEvent(QMouseEvent *event)
{
	if( event->button() == Qt::LeftButton ) {
		m_dragging = false;
		m_wordDragging = false;
	}
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
	m_caretVisible = true;
	if( m_blinkTimer )
		m_blinkTimer->start();
	viewport()->update();
	QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);
}

void TextEdit::focusOutEvent(QFocusEvent *event)
{
	QAbstractScrollArea::focusOutEvent(event);
	if( m_blinkTimer )
		m_blinkTimer->stop();
	m_caretVisible = true;
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
		const LineMgr::LineType lt = mgr.lineType(ln);
		if( (lt == LineMgr::LT_LIST || lt == LineMgr::LT_CHECKBOX || lt == LineMgr::LT_NUM_LIST)
			&& m_cursor.position() == lineEnd ) {
			QString lineText = m_doc.findBlockByNumber(ln).text();
			if( lineText.endsWith(QChar('\n')) ) lineText.chop(1);
			if( lineText.endsWith(QChar('\r')) ) lineText.chop(1);
			qsizetype i = 0;
			while( i < lineText.size() && (lineText[i] == QChar(' ') || lineText[i] == QChar('\t')) )
				++i;
			if( lt == LineMgr::LT_NUM_LIST ) {
				qsizetype d = i;
				while( d < lineText.size() && lineText[d] >= QChar('0') && lineText[d] <= QChar('9') )
					++d;
				if( d > i && d + 2 <= lineText.size() && lineText[d] == QChar('.')
					&& (lineText[d + 1] == QChar(' ') || lineText[d + 1] == QChar('\t')) ) {
					if( d + 2 == lineText.size() ) {
						//	マーカーだけの空行で Enter → リスト終了
						m_doc.erase(lineStart, lineEnd);
						return;
					}
					const QString prefix = lineText.left(i) + QStringLiteral("1. ");
					m_cursor.insertText(QStringLiteral("\n") + prefix);
					return;
				}
			} else {
				const qsizetype markerLen = (lt == LineMgr::LT_CHECKBOX) ? 6 : 2;
				if( i + markerLen <= lineText.size() ) {
					if( i + markerLen == lineText.size() ) {
						//	マーカーだけの空行で Enter → リスト終了
						m_doc.erase(lineStart, lineEnd);
						return;
					}
					const QString prefix = (lt == LineMgr::LT_CHECKBOX)
						? (lineText.left(i + 2) + QStringLiteral("[ ] "))
						: lineText.left(i + 2);
					m_cursor.insertText(QStringLiteral("\n") + prefix);
					return;
				}
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
