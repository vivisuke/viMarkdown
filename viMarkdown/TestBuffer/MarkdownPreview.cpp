#include "MarkdownPreview.h"
#include "TextDocument.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHash>
#include <QImage>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTextList>
#include <QTextListFormat>
#include <QTextTable>
#include <QTextTableCell>
#include <QTextTableCellFormat>
#include <algorithm>
#include <QTextTableFormat>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>
#include <QXmlStreamReader>
#include <algorithm>
#include <memory>
#include "lunasvg.h"

extern ushort getKeisenConnectionBits(QChar ch);

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

bool isEscapablePunct(QChar ch)
{
	switch( ch.unicode() ) {
	case '\\': case '`': case '*': case '_': case '~':
	case '^': case '[': case ']': case '(': case ')':
	case '{': case '}': case '#': case '-': case '+': case '<': case '>':
		return true;
	default:
		return false;
	}
}

qsizetype findClosingDelim(const QString &text, qsizetype from, const QString &delim)
{
	const qsizetype dlen = delim.size();
	const qsizetype len = text.size();
	for( qsizetype pos = from; pos + dlen <= len; ++pos ) {
		if( text[pos] == QChar('\\') ) {
			++pos;
			continue;
		}
		if( QStringView(text).mid(pos, dlen) == delim ) {
			if( pos > from && text[pos - 1] != QChar(' ') )
				return pos;
		}
	}
	return -1;
}

//	インライン装飾（**太字** / *斜体* / ***太字斜体*** / ~~打消し~~ / `コード` / ![alt](src) / [text](url)）をパースして挿入
void insertInlineFormatted(
	QTextCursor &cursor,
	const QString &text,
	const QTextCharFormat &baseFmt,
	qreal basePt,
	qreal maxImgWidth,
	bool *outHasImages = nullptr,
	int srcBasePos = -1,
	QVector<MarkdownPreview::SyncSpan> *spans = nullptr)
{
	const qsizetype len = text.size();
	QString plainBuf;
	int plainSrcStart = -1;
	auto flushPlain = [&]() {
		if( !plainBuf.isEmpty() ) {
			const int prvPos = cursor.position();
			cursor.insertText(plainBuf, baseFmt);
			if( spans && srcBasePos >= 0 && plainSrcStart >= 0 )
				spans->push_back({plainSrcStart, prvPos, static_cast<int>(plainBuf.size())});
			plainBuf.clear();
			plainSrcStart = -1;
		}
	};

	qsizetype i = 0;
	while( i < len ) {
		const QChar ch = text[i];

		//	バックスラッシュエスケープ（2文字→1文字なので境界でフラッシュして1:1対応を維持）
		if( ch == QChar('\\') && i + 1 < len && isEscapablePunct(text[i + 1]) ) {
			flushPlain();
			const int prvPos = cursor.position();
			cursor.insertText(QString(text[i + 1]), baseFmt);
			if( spans && srcBasePos >= 0 )
				spans->push_back({srcBasePos + static_cast<int>(i + 1), prvPos, 1});
			i += 2;
			continue;
		}

		//	インラインコード（`...` / ``...``）
		if( ch == QChar('`') ) {
			qsizetype tickCount = 0;
			while( i + tickCount < len && text[i + tickCount] == QChar('`') )
				++tickCount;
			qsizetype closePos = -1;
			for( qsizetype pos = i + tickCount; pos < len; ) {
				if( text[pos] == QChar('`') ) {
					qsizetype run = 0;
					while( pos + run < len && text[pos + run] == QChar('`') )
						++run;
					if( run == tickCount ) {
						closePos = pos;
						break;
					}
					pos += run;
				} else {
					++pos;
				}
			}
			if( closePos >= 0 ) {
				flushPlain();
				qsizetype codeStart = i + tickCount;
				QString code = text.mid(codeStart, closePos - codeStart);
				if( code.size() >= 2 && code.front() == QChar(' ') && code.back() == QChar(' ') && !code.trimmed().isEmpty() ) {
					code = code.mid(1, code.size() - 2);
					++codeStart;
				}
				QTextCharFormat cf = baseFmt;
				const qreal pt = (baseFmt.fontPointSize() > 0.0) ? baseFmt.fontPointSize() : basePt;
				cf.setFont(codeBlockFont(pt));
				cf.setBackground(QColor(0xEA, 0xEC, 0xEF));
				const int prvPos = cursor.position();
				cursor.insertText(code, cf);
				if( spans && srcBasePos >= 0 )
					spans->push_back({srcBasePos + static_cast<int>(codeStart), prvPos, static_cast<int>(code.size())});
				i = closePos + tickCount;
				continue;
			}
			if( plainBuf.isEmpty() )
				plainSrcStart = (srcBasePos >= 0) ? (srcBasePos + static_cast<int>(i)) : -1;
			plainBuf.append(text.mid(i, tickCount));
			i += tickCount;
			continue;
		}

		//	画像（![alt](src)）
		if( ch == QChar('!') && i + 1 < len && text[i + 1] == QChar('[') ) {
			int depth = 1;
			qsizetype closeBracket = -1;
			for( qsizetype pos = i + 2; pos < len; ++pos ) {
				if( text[pos] == QChar('\\') ) { ++pos; continue; }
				if( text[pos] == QChar('[') ) ++depth;
				else if( text[pos] == QChar(']') ) {
					if( --depth == 0 ) { closeBracket = pos; break; }
				}
			}
			if( closeBracket >= 0 && closeBracket + 1 < len && text[closeBracket + 1] == QChar('(') ) {
				int pdepth = 1;
				qsizetype closeParen = -1;
				for( qsizetype pos = closeBracket + 2; pos < len; ++pos ) {
					if( text[pos] == QChar('\\') ) { ++pos; continue; }
					if( text[pos] == QChar('(') ) ++pdepth;
					else if( text[pos] == QChar(')') ) {
						if( --pdepth == 0 ) { closeParen = pos; break; }
					}
				}
				if( closeParen >= 0 ) {
					flushPlain();
					const QString alt = text.mid(i + 2, closeBracket - (i + 2));
					QString src = text.mid(closeBracket + 2, closeParen - (closeBracket + 2)).trimmed();
					if( src.startsWith(QChar('<')) && src.endsWith(QChar('>')) && src.size() >= 2 )
						src = src.mid(1, src.size() - 2);
					QTextDocument *doc = cursor.document();
					QUrl url(src);
					if( url.isRelative() && doc && doc->baseUrl().isValid() )
						url = doc->baseUrl().resolved(url);
					const QString localPath = url.isLocalFile() ? url.toLocalFile() : src;
					QImage img(localPath);
					const int prvPos = cursor.position();
					if( !img.isNull() && doc ) {
						const QUrl resUrl = QUrl::fromLocalFile(QFileInfo(localPath).absoluteFilePath());
						doc->addResource(QTextDocument::ImageResource, resUrl, img);
						QTextImageFormat imgFmt;
						imgFmt.setName(resUrl.toString());
						if( maxImgWidth > 0.0 && img.width() > maxImgWidth ) {
							imgFmt.setWidth(maxImgWidth);
							imgFmt.setHeight(img.height() * maxImgWidth / img.width());
						} else {
							imgFmt.setWidth(img.width());
							imgFmt.setHeight(img.height());
						}
						cursor.insertImage(imgFmt);
						if( outHasImages )
							*outHasImages = true;
						if( spans && srcBasePos >= 0 ) {
							spans->push_back({srcBasePos + static_cast<int>(i), prvPos, 0});
							spans->push_back({srcBasePos + static_cast<int>(closeParen + 1), cursor.position(), 0});
						}
					} else {
						QTextCharFormat cf = baseFmt;
						cf.setForeground(QColor(0x88, 0x88, 0x88));
						cursor.insertText(alt.isEmpty() ? src : QStringLiteral("[%1]").arg(alt), cf);
						if( spans && srcBasePos >= 0 ) {
							spans->push_back({srcBasePos + static_cast<int>(i), prvPos, 0});
							spans->push_back({srcBasePos + static_cast<int>(closeParen + 1), cursor.position(), 0});
						}
					}
					i = closeParen + 1;
					continue;
				}
			}
		}

		//	リンク（[label](url)）
		if( ch == QChar('[') ) {
			int depth = 1;
			qsizetype closeBracket = -1;
			for( qsizetype pos = i + 1; pos < len; ++pos ) {
				if( text[pos] == QChar('\\') ) { ++pos; continue; }
				if( text[pos] == QChar('[') ) ++depth;
				else if( text[pos] == QChar(']') ) {
					if( --depth == 0 ) { closeBracket = pos; break; }
				}
			}
			if( closeBracket >= 0 && closeBracket + 1 < len && text[closeBracket + 1] == QChar('(') ) {
				int pdepth = 1;
				qsizetype closeParen = -1;
				for( qsizetype pos = closeBracket + 2; pos < len; ++pos ) {
					if( text[pos] == QChar('\\') ) { ++pos; continue; }
					if( text[pos] == QChar('(') ) ++pdepth;
					else if( text[pos] == QChar(')') ) {
						if( --pdepth == 0 ) { closeParen = pos; break; }
					}
				}
				if( closeParen >= 0 ) {
					flushPlain();
					const QString label = text.mid(i + 1, closeBracket - (i + 1));
					const QString url = text.mid(closeBracket + 2, closeParen - (closeBracket + 2)).trimmed();
					QTextCharFormat cf = baseFmt;
					cf.setAnchor(true);
					cf.setAnchorHref(url);
					cf.setForeground(QColor(0x00, 0x55, 0xCC));
					cf.setFontUnderline(true);
					const int subSrcBase = (srcBasePos >= 0)
						? (srcBasePos + static_cast<int>(label.isEmpty() ? (closeBracket + 2) : (i + 1)))
						: -1;
					insertInlineFormatted(cursor, label.isEmpty() ? url : label, cf, basePt, maxImgWidth, outHasImages, subSrcBase, spans);
					i = closeParen + 1;
					continue;
				}
			}
		}

		//	太字＋斜体（***...***）
		if( ch == QChar('*') && i + 3 < len && text[i + 1] == QChar('*') && text[i + 2] == QChar('*') && text[i + 3] != QChar(' ') ) {
			const qsizetype closePos = findClosingDelim(text, i + 3, QStringLiteral("***"));
			if( closePos > i + 3 ) {
				flushPlain();
				QTextCharFormat cf = baseFmt;
				cf.setFontWeight(QFont::Bold);
				cf.setFontItalic(true);
				const int subSrcBase = (srcBasePos >= 0) ? (srcBasePos + static_cast<int>(i + 3)) : -1;
				insertInlineFormatted(cursor, text.mid(i + 3, closePos - (i + 3)), cf, basePt, maxImgWidth, outHasImages, subSrcBase, spans);
				i = closePos + 3;
				continue;
			}
		}

		//	太字（**...**）
		if( ch == QChar('*') && i + 2 < len && text[i + 1] == QChar('*') && text[i + 2] != QChar(' ') ) {
			const qsizetype closePos = findClosingDelim(text, i + 2, QStringLiteral("**"));
			if( closePos > i + 2 ) {
				flushPlain();
				QTextCharFormat cf = baseFmt;
				cf.setFontWeight(QFont::Bold);
				const int subSrcBase = (srcBasePos >= 0) ? (srcBasePos + static_cast<int>(i + 2)) : -1;
				insertInlineFormatted(cursor, text.mid(i + 2, closePos - (i + 2)), cf, basePt, maxImgWidth, outHasImages, subSrcBase, spans);
				i = closePos + 2;
				continue;
			}
		}

		//	打ち消し線（~~...~~）
		if( ch == QChar('~') && i + 2 < len && text[i + 1] == QChar('~') && text[i + 2] != QChar(' ') ) {
			const qsizetype closePos = findClosingDelim(text, i + 2, QStringLiteral("~~"));
			if( closePos > i + 2 ) {
				flushPlain();
				QTextCharFormat cf = baseFmt;
				cf.setFontStrikeOut(true);
				const int subSrcBase = (srcBasePos >= 0) ? (srcBasePos + static_cast<int>(i + 2)) : -1;
				insertInlineFormatted(cursor, text.mid(i + 2, closePos - (i + 2)), cf, basePt, maxImgWidth, outHasImages, subSrcBase, spans);
				i = closePos + 2;
				continue;
			}
		}

		//	斜体（*...*）
		if( ch == QChar('*') && i + 1 < len && text[i + 1] != QChar(' ') && text[i + 1] != QChar('*') ) {
			qsizetype closePos = -1;
			for( qsizetype pos = i + 1; pos < len; ++pos ) {
				if( text[pos] == QChar('\\') ) { ++pos; continue; }
				if( text[pos] == QChar('*') ) {
					if( pos + 1 < len && text[pos + 1] == QChar('*') && text[pos - 1] != QChar('*') ) {
						++pos;	//	内部の "**" をスキップ
						continue;
					}
					if( text[pos - 1] != QChar(' ') ) {
						closePos = pos;
						break;
					}
				}
			}
			if( closePos > i + 1 ) {
				flushPlain();
				QTextCharFormat cf = baseFmt;
				cf.setFontItalic(true);
				const int subSrcBase = (srcBasePos >= 0) ? (srcBasePos + static_cast<int>(i + 1)) : -1;
				insertInlineFormatted(cursor, text.mid(i + 1, closePos - (i + 1)), cf, basePt, maxImgWidth, outHasImages, subSrcBase, spans);
				i = closePos + 1;
				continue;
			}
		}

		//	上付き文字（^{...} / ^2 / ^-3 / ^...^）
		if( ch == QChar('^') && i + 1 < len ) {
			if( text[i + 1] == QChar('{') ) {
				int bdepth = 1;
				qsizetype closeBrace = -1;
				for( qsizetype pos = i + 2; pos < len; ++pos ) {
					if( text[pos] == QChar('\\') ) { ++pos; continue; }
					if( text[pos] == QChar('{') ) ++bdepth;
					else if( text[pos] == QChar('}') ) {
						if( --bdepth == 0 ) { closeBrace = pos; break; }
					}
				}
				if( closeBrace > i + 2 ) {
					flushPlain();
					QTextCharFormat cf = baseFmt;
					cf.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
					const int subSrcBase = (srcBasePos >= 0) ? (srcBasePos + static_cast<int>(i + 2)) : -1;
					insertInlineFormatted(cursor, text.mid(i + 2, closeBrace - (i + 2)), cf, basePt, maxImgWidth, outHasImages, subSrcBase, spans);
					i = closeBrace + 1;
					continue;
				}
			}
			qsizetype numStart = i + 1;
			if( (text[numStart] == QChar('-') || text[numStart] == QChar('+')) && numStart + 1 < len && text[numStart + 1].isDigit() )
				++numStart;
			if( numStart < len && text[numStart].isDigit() ) {
				qsizetype numEnd = numStart;
				while( numEnd < len && text[numEnd].isDigit() )
					++numEnd;
				flushPlain();
				QTextCharFormat cf = baseFmt;
				cf.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
				const int prvPos = cursor.position();
				const QString numStr = text.mid(i + 1, numEnd - (i + 1));
				cursor.insertText(numStr, cf);
				if( spans && srcBasePos >= 0 )
					spans->push_back({srcBasePos + static_cast<int>(i + 1), prvPos, static_cast<int>(numStr.size())});
				i = numEnd;
				continue;
			}
			if( !text[i + 1].isSpace() && text[i + 1] != QChar('^') ) {
				qsizetype closePos = -1;
				for( qsizetype pos = i + 1; pos < len; ++pos ) {
					if( text[pos].isSpace() ) break;
					if( text[pos] == QChar('\\') ) { ++pos; continue; }
					if( text[pos] == QChar('^') ) { closePos = pos; break; }
				}
				if( closePos > i + 1 ) {
					flushPlain();
					QTextCharFormat cf = baseFmt;
					cf.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
					const int subSrcBase = (srcBasePos >= 0) ? (srcBasePos + static_cast<int>(i + 1)) : -1;
					insertInlineFormatted(cursor, text.mid(i + 1, closePos - (i + 1)), cf, basePt, maxImgWidth, outHasImages, subSrcBase, spans);
					i = closePos + 1;
					continue;
				}
			}
		}

		//	下付き文字（_{...} / _2）
		if( ch == QChar('_') && i + 1 < len ) {
			if( text[i + 1] == QChar('{') ) {
				int bdepth = 1;
				qsizetype closeBrace = -1;
				for( qsizetype pos = i + 2; pos < len; ++pos ) {
					if( text[pos] == QChar('\\') ) { ++pos; continue; }
					if( text[pos] == QChar('{') ) ++bdepth;
					else if( text[pos] == QChar('}') ) {
						if( --bdepth == 0 ) { closeBrace = pos; break; }
					}
				}
				if( closeBrace > i + 2 ) {
					flushPlain();
					QTextCharFormat cf = baseFmt;
					cf.setVerticalAlignment(QTextCharFormat::AlignSubScript);
					const int subSrcBase = (srcBasePos >= 0) ? (srcBasePos + static_cast<int>(i + 2)) : -1;
					insertInlineFormatted(cursor, text.mid(i + 2, closeBrace - (i + 2)), cf, basePt, maxImgWidth, outHasImages, subSrcBase, spans);
					i = closeBrace + 1;
					continue;
				}
			}
			if( i > 0 && !text[i - 1].isSpace() && text[i + 1].isDigit() ) {
				qsizetype numEnd = i + 1;
				while( numEnd < len && text[numEnd].isDigit() )
					++numEnd;
				flushPlain();
				QTextCharFormat cf = baseFmt;
				cf.setVerticalAlignment(QTextCharFormat::AlignSubScript);
				const int prvPos = cursor.position();
				const QString numStr = text.mid(i + 1, numEnd - (i + 1));
				cursor.insertText(numStr, cf);
				if( spans && srcBasePos >= 0 )
					spans->push_back({srcBasePos + static_cast<int>(i + 1), prvPos, static_cast<int>(numStr.size())});
				i = numEnd;
				continue;
			}
		}

		if( plainBuf.isEmpty() )
			plainSrcStart = (srcBasePos >= 0) ? (srcBasePos + static_cast<int>(i)) : -1;
		plainBuf.append(ch);
		++i;
	}
	flushPlain();
}

//	コードフェンス行（先頭 0〜3 個の空白 + "```"）を判定
bool isCodeFence(const QString &line)
{
	qsizetype i = 0;
	while( i < line.size() && i < 3 && line[i] == QChar(' ') )
		++i;
	return QStringView(line).mid(i).startsWith(u"```");
}

//	CSV フェンス開始行（先頭 0〜3 個の空白 + "```csv"、大文字小文字不問）を判定
bool isCsvFence(const QString &line)
{
	qsizetype i = 0;
	while( i < line.size() && i < 3 && line[i] == QChar(' ') )
		++i;
	const QStringView rest = QStringView(line).mid(i);
	if( !rest.startsWith(u"```") )
		return false;
	const QStringView lang = rest.mid(3).trimmed();
	return lang.compare(u"csv", Qt::CaseInsensitive) == 0;
}

//	SVG フェンス開始行（先頭 0〜3 個の空白 + "```svg"、大文字小文字不問）を判定
bool isSvgFence(const QString &line)
{
	qsizetype i = 0;
	while( i < line.size() && i < 3 && line[i] == QChar(' ') )
		++i;
	const QStringView rest = QStringView(line).mid(i);
	if( !rest.startsWith(u"```") )
		return false;
	const QStringView lang = rest.mid(3).trimmed();
	return lang.compare(u"svg", Qt::CaseInsensitive) == 0;
}

//	Keisen フェンス開始行（先頭 0〜3 個の空白 + "```keisen" + 任意オプション、大文字小文字不問）を判定
bool isKeisenFence(const QString &line)
{
	qsizetype i = 0;
	while( i < line.size() && i < 3 && line[i] == QChar(' ') )
		++i;
	const QStringView rest = QStringView(line).mid(i);
	if( !rest.startsWith(u"```") )
		return false;
	const QStringView after = rest.mid(3);
	if( !after.startsWith(u"keisen", Qt::CaseInsensitive) )
		return false;
	const qsizetype kLen = 6;	//	"keisen"
	return after.size() == kLen || after[kLen].isSpace();
}

//	GFM テーブルの1行をセル配列に分解（\| はエスケープされたリテラル '|' として扱う）
bool parseGfmTableRow(const QString &line, QStringList &outCells, QVector<int> *outCellCols = nullptr)
{
	outCells.clear();
	if( outCellCols ) outCellCols->clear();
	qsizetype leadWs = 0;
	while( leadWs < line.size() && line[leadWs].isSpace() )
		++leadWs;
	qsizetype end = line.size();
	while( end > leadWs && line[end - 1].isSpace() )
		--end;
	if( leadWs >= end )
		return false;

	bool hasUnescapedPipe = false;
	for( qsizetype i = leadWs; i < end; ++i ) {
		if( line[i] == QChar('\\') ) { ++i; continue; }
		if( line[i] == QChar('|') ) { hasUnescapedPipe = true; break; }
	}
	if( !hasUnescapedPipe )
		return false;

	qsizetype start = leadWs;
	if( start < end && line[start] == QChar('|') )
		++start;
	if( end > start && line[end - 1] == QChar('|') ) {
		qsizetype bs = 0;
		for( qsizetype k = end - 2; k >= start && line[k] == QChar('\\'); --k )
			++bs;
		if( (bs % 2) == 0 )
			--end;
	}

	QString cell;
	qsizetype cellRawStart = start;
	auto pushCell = [&](qsizetype rawEnd) {
		qsizetype trimmedStart = cellRawStart;
		while( trimmedStart < rawEnd && line[trimmedStart].isSpace() )
			++trimmedStart;
		outCells.append(cell.trimmed());
		if( outCellCols )
			outCellCols->append(static_cast<int>(trimmedStart));
		cell.clear();
	};

	for( qsizetype i = start; i < end; ++i ) {
		if( line[i] == QChar('\\') && i + 1 < end && line[i + 1] == QChar('|') ) {
			cell.append(QChar('|'));
			++i;
		} else if( line[i] == QChar('|') ) {
			pushCell(i);
			cellRawStart = i + 1;
		} else {
			cell.append(line[i]);
		}
	}
	pushCell(end);
	return !outCells.isEmpty();
}

//	GFM テーブルの区切り行（| --- | :---: | ---: |）を判定し、列ごとの配置を返す
bool parseGfmTableSeparator(const QString &line, QVector<Qt::Alignment> &outAligns)
{
	outAligns.clear();
	QStringList cells;
	if( !parseGfmTableRow(line, cells) )
		return false;
	for( const QString &c : cells ) {
		if( c.isEmpty() )
			return false;
		qsizetype s = 0;
		qsizetype e = c.size();
		const bool leftColon = (c[s] == QChar(':'));
		if( leftColon ) ++s;
		const bool rightColon = (e > s && c[e - 1] == QChar(':'));
		if( rightColon ) --e;
		if( e <= s )
			return false;
		for( qsizetype k = s; k < e; ++k ) {
			if( c[k] != QChar('-') )
				return false;
		}
		if( leftColon && rightColon )
			outAligns.append(Qt::AlignCenter);
		else if( rightColon )
			outAligns.append(Qt::AlignRight);
		else
			outAligns.append(Qt::AlignLeft);
	}
	return !outAligns.isEmpty();
}

//	RFC 4180 風 CSV 1行パース（ダブルクォート・"" エスケープ・複数行セル対応）。行が完了したら true
bool parseCsvLine(const QString &line, bool &inQuotes, QStringList &curFields, QString &curField, QVector<int> *curFieldCols = nullptr)
{
	if( !inQuotes ) {
		curFields.clear();
		curField.clear();
		if( curFieldCols ) curFieldCols->clear();
	}
	const qsizetype len = line.size();
	qsizetype fieldStartCol = 0;
	bool recordedCol = false;
	auto recordStart = [&](qsizetype col) {
		if( !recordedCol ) {
			fieldStartCol = col;
			recordedCol = true;
		}
	};
	for( qsizetype i = 0; i < len; ++i ) {
		const QChar ch = line[i];
		if( inQuotes ) {
			if( ch == QChar('"') ) {
				if( i + 1 < len && line[i + 1] == QChar('"') ) {
					recordStart(i);
					curField.append(QChar('"'));
					++i;
				} else {
					inQuotes = false;
				}
			} else {
				recordStart(i);
				curField.append(ch);
			}
		} else {
			if( ch == QChar('"') && curField.trimmed().isEmpty() ) {
				curField.clear();
				inQuotes = true;
				fieldStartCol = i + 1;
				recordedCol = true;
			} else if( ch == QChar(',') ) {
				curFields.append(curField.trimmed());
				if( curFieldCols )
					curFieldCols->append(static_cast<int>(recordedCol ? fieldStartCol : i));
				curField.clear();
				fieldStartCol = i + 1;
				recordedCol = false;
			} else {
				if( !ch.isSpace() )
					recordStart(i);
				curField.append(ch);
			}
		}
	}
	if( inQuotes ) {
		curField.append(QChar('\n'));
		return false;
	}
	curFields.append(curField.trimmed());
	if( curFieldCols )
		curFieldCols->append(static_cast<int>(recordedCol ? fieldStartCol : len));
	return true;
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

//	番号付きリスト行（[空白]* [0-9]+ ". " テキスト）を判定し、インデント階層・開始番号・本文を返す
bool parseNumList(const QString &line, int &outIndent, int &outNum, QString &outText)
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
	qsizetype d = i;
	while( d < line.size() && line[d] >= QChar('0') && line[d] <= QChar('9') )
		++d;
	if( d == i || d + 1 >= line.size() )
		return false;
	if( line[d] != QChar('.') )
		return false;
	if( line[d + 1] != QChar(' ') && line[d + 1] != QChar('\t') )
		return false;
	outIndent = spaces / 2;
	outNum = line.mid(i, d - i).toInt();
	outText = line.mid(d + 2);
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

//	行頭インデント幅を計測（半角スペース=1、タブ=2）
int measureIndent(const QString &line)
{
	int spaces = 0;
	for( qsizetype i = 0; i < line.size(); ++i ) {
		if( line[i] == QChar(' ') )
			++spaces;
		else if( line[i] == QChar('\t') )
			spaces += 2;
		else
			break;
	}
	return spaces;
}

//	行頭から最大 maxIndent 幅分の空白・タブを除去し、除去した文字数を返す
QString stripIndentWidth(const QString &line, int maxIndent, int *outStrippedChars = nullptr)
{
	int acc = 0;
	qsizetype i = 0;
	while( i < line.size() && acc < maxIndent ) {
		if( line[i] == QChar(' ') ) {
			++acc;
			++i;
		} else if( line[i] == QChar('\t') ) {
			acc += 2;
			++i;
		} else {
			break;
		}
	}
	if( outStrippedChars ) *outStrippedChars = static_cast<int>(i);
	return line.mid(i);
}

struct BoxOptions {
	bool horizontal = false;
	bool arrow = false;
	Qt::Alignment align = Qt::AlignLeft;
};

//	枠ブロック行（[空白]* "[オプション...] テキスト" または単独の "[オプション...]"）を判定
//	オプションは空白区切り: > (横並び), -> (矢印付き横並び), left, center, right
bool parseBoxLine(const QString &line, int &outIndent, BoxOptions &outOpt, QString &outText, int *outTextCol = nullptr)
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
	if( i >= line.size() || line[i] != QChar('[') )
		return false;
	const qsizetype closePos = line.indexOf(QChar(']'), i + 1);
	if( closePos < 0 )
		return false;
	if( closePos + 1 < line.size() && line[closePos + 1] != QChar(' ') && line[closePos + 1] != QChar('\t') )
		return false;

	BoxOptions opt;
	const QStringView inner = QStringView(line).mid(i + 1, closePos - (i + 1));
	qsizetype p = 0;
	while( p < inner.size() ) {
		while( p < inner.size() && (inner[p] == QChar(' ') || inner[p] == QChar('\t')) )
			++p;
		if( p >= inner.size() )
			break;
		const qsizetype tokStart = p;
		while( p < inner.size() && inner[p] != QChar(' ') && inner[p] != QChar('\t') )
			++p;
		const QStringView tok = inner.mid(tokStart, p - tokStart);
		if( tok == u">" ) {
			opt.horizontal = true;
		} else if( tok == u"->" ) {
			opt.horizontal = true;
			opt.arrow = true;
		} else if( tok.compare(u"left", Qt::CaseInsensitive) == 0 ) {
			opt.align = Qt::AlignLeft;
		} else if( tok.compare(u"center", Qt::CaseInsensitive) == 0 ) {
			opt.align = Qt::AlignCenter;
		} else if( tok.compare(u"right", Qt::CaseInsensitive) == 0 ) {
			opt.align = Qt::AlignRight;
		} else {
			return false;	//	未知の単語を含む [Note] 等は通常のテキストとして扱う
		}
	}

	outIndent = spaces;
	outOpt = opt;
	if( closePos + 1 == line.size() ) {
		outText.clear();
		if( outTextCol ) *outTextCol = static_cast<int>(closePos + 1);
		return true;
	}
	outText = line.mid(closePos + 2);
	if( outTextCol ) *outTextCol = static_cast<int>(closePos + 2);
	return true;
}

struct ParseState {
	int activeListIndent = 0;
	int listNum = 0;
	bool inQuote = false;
	bool inCodeBlock = false;
	bool codeHasLines = false;
	bool inHtmlComment = false;
	bool hasEmittedBlock = false;
	bool lastLineEmitted = false;
	bool hasImages = false;
	qreal basePt = 10.0;
	qreal maxImgWidth = 0.0;
	QColor tableHeaderColor = QColor(0xE6, 0xF0, 0xFF);
	QColor tableZebraColor1 = QColor(0xFF, 0xFF, 0xFF);
	QColor tableZebraColor2 = QColor(0xFF, 0xFF, 0xE0);
};

//	HTML コメント（<!-- ... -->、複数行またぎ対応）を除去する。インラインコード内の `<!--` は保持する。
QString stripHtmlComments(const QString &line, bool &inComment, bool &outCommentOnly, qsizetype *outLeadStrip = nullptr)
{
	outCommentOnly = false;
	if( outLeadStrip ) *outLeadStrip = 0;
	const qsizetype len = line.size();
	QString out;
	out.reserve(len);
	bool hadComment = false;
	qsizetype i = 0;

	while( i < len ) {
		if( inComment ) {
			hadComment = true;
			const qsizetype endPos = line.indexOf(QStringLiteral("-->"), i);
			if( endPos < 0 ) {
				outCommentOnly = out.trimmed().isEmpty();
				return out;
			}
			inComment = false;
			i = endPos + 3;
			if( out.isEmpty() && outLeadStrip )
				*outLeadStrip = i;
			continue;
		}
		if( line[i] == QChar('`') ) {
			qsizetype ticks = 0;
			while( i + ticks < len && line[i + ticks] == QChar('`') )
				++ticks;
			qsizetype closePos = -1;
			for( qsizetype pos = i + ticks; pos < len; ) {
				if( line[pos] == QChar('`') ) {
					qsizetype run = 0;
					while( pos + run < len && line[pos + run] == QChar('`') )
						++run;
					if( run == ticks ) {
						closePos = pos + ticks;
						break;
					}
					pos += run;
				} else {
					++pos;
				}
			}
			const qsizetype spanEnd = (closePos >= 0) ? closePos : (i + ticks);
			out.append(QStringView(line).mid(i, spanEnd - i));
			i = spanEnd;
			continue;
		}
		if( i + 4 <= len && QStringView(line).mid(i, 4) == u"<!--" ) {
			hadComment = true;
			const qsizetype endPos = line.indexOf(QStringLiteral("-->"), i + 4);
			if( endPos < 0 ) {
				inComment = true;
				break;
			}
			i = endPos + 3;
			if( out.isEmpty() && outLeadStrip )
				*outLeadStrip = i;
			continue;
		}
		out.append(line[i]);
		++i;
	}
	if( hadComment && out.trimmed().isEmpty() )
		outCommentOnly = true;
	return out;
}

//	行頭の <!-- align:left|center|right --> を判定して配置を返す（空白・大小文字許容）
bool parseAlignDirective(const QString &rawLine, Qt::Alignment &outAlign)
{
	qsizetype i = 0;
	while( i < rawLine.size() && (rawLine[i] == QChar(' ') || rawLine[i] == QChar('\t')) )
		++i;
	if( i + 4 > rawLine.size() || QStringView(rawLine).mid(i, 4) != u"<!--" )
		return false;
	const qsizetype endPos = rawLine.indexOf(QStringLiteral("-->"), i + 4);
	if( endPos < 0 )
		return false;
	const QStringView body = QStringView(rawLine).mid(i + 4, endPos - (i + 4)).trimmed();
	const qsizetype colon = body.indexOf(QChar(':'));
	if( colon < 0 )
		return false;
	if( body.left(colon).trimmed().compare(u"align", Qt::CaseInsensitive) != 0 )
		return false;
	const QStringView val = body.mid(colon + 1).trimmed();
	if( val.compare(u"left", Qt::CaseInsensitive) == 0 ) {
		outAlign = Qt::AlignLeft;
		return true;
	}
	if( val.compare(u"center", Qt::CaseInsensitive) == 0 ) {
		outAlign = Qt::AlignCenter;
		return true;
	}
	if( val.compare(u"right", Qt::CaseInsensitive) == 0 ) {
		outAlign = Qt::AlignRight;
		return true;
	}
	return false;
}

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

LineMgr::LineType parseAndAppendLine(
	QTextCursor &cursor,
	const QString &rawLine,
	ParseState &st,
	int srcLine = -1,
	int lineStartPos = -1,
	QVector<MarkdownPreview::SyncSpan> *spans = nullptr)
{
	static const qreal kHeadingScale[7] = { 1.0, 2.2, 1.8, 1.5, 1.3, 1.1, 1.0 };
	static const QTextListFormat::Style kListStyles[3] = {
		QTextListFormat::ListDisc,
		QTextListFormat::ListCircle,
		QTextListFormat::ListSquare,
	};
	static const QTextBlockFormat bodyBlockFmt;
	static const QTextCharFormat bodyCharFmt;

	st.lastLineEmitted = false;

	auto finishQuoteIfNeeded = [&]() {
		if( !st.inQuote ) return;
		st.inQuote = false;
		finishBlockMarginIfNeeded(cursor, kBlockQuote);
	};
	auto beginEmittedBlock = [&]() {
		if( st.hasEmittedBlock )
			cursor.insertBlock();
		st.hasEmittedBlock = true;
		st.lastLineEmitted = true;
	};
	auto emitInlineWithAnchor = [&](const QString &txt, const QTextCharFormat &fmt, int srcBase) {
		const qsizetype before = spans ? spans->size() : 0;
		const int prvStart = cursor.position();
		insertInlineFormatted(cursor, txt, fmt, st.basePt, st.maxImgWidth, &st.hasImages, srcBase, spans);
		if( spans && srcBase >= 0 && spans->size() == before )
			spans->push_back({srcBase, prvStart, 0});
	};

	if( !st.inHtmlComment && isCodeFence(rawLine) ) {
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
		const int prvPos = cursor.position();
		cursor.insertText(rawLine, cf);
		if( spans && lineStartPos >= 0 )
			spans->push_back({lineStartPos, prvPos, static_cast<int>(rawLine.size())});
		st.codeHasLines = true;
		return LineMgr::LT_CODE_BLOCK;
	}

	Qt::Alignment lineAlign = Qt::Alignment();
	const bool hasAlign = (!st.inHtmlComment && parseAlignDirective(rawLine, lineAlign));

	bool commentOnly = false;
	qsizetype leadStrip = 0;
	const QString line = stripHtmlComments(rawLine, st.inHtmlComment, commentOnly, &leadStrip);
	if( commentOnly )
		return LineMgr::LT_BODY;
	const int strippedLineStart = (lineStartPos >= 0) ? (lineStartPos + static_cast<int>(leadStrip)) : -1;

	auto applyAlignIfNeeded = [&]() {
		if( !hasAlign ) return;
		QTextBlockFormat bf = cursor.blockFormat();
		bf.setAlignment(lineAlign);
		cursor.setBlockFormat(bf);
	};

	QString parsedText;
	if( const int level = parseHeading(line, parsedText); level > 0 ) {
		finishQuoteIfNeeded();
		st.activeListIndent = 0;
		st.listNum = 0;
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
		applyAlignIfNeeded();
		cursor.block().setUserState(kBlockNormal);
		const int srcBase = (strippedLineStart >= 0) ? (strippedLineStart + static_cast<int>(line.size() - parsedText.size())) : -1;
		emitInlineWithAnchor(parsedText, cf, srcBase);
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
		applyAlignIfNeeded();
		cursor.block().setUserState(srcLine >= 0 ? (kBlockCheckboxBase + srcLine) : kBlockNormal);
		const int srcBase = (strippedLineStart >= 0) ? (strippedLineStart + static_cast<int>(line.size() - parsedText.size())) : -1;
		emitInlineWithAnchor(parsedText, bodyCharFmt, srcBase);
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
		applyAlignIfNeeded();
		cursor.block().setUserState(kBlockNormal);
		const int srcBase = (strippedLineStart >= 0) ? (strippedLineStart + static_cast<int>(line.size() - parsedText.size())) : -1;
		emitInlineWithAnchor(parsedText, bodyCharFmt, srcBase);
		return LineMgr::LT_LIST;
	}

	int listNum = 1;
	if( parseNumList(line, indent, listNum, parsedText) ) {
		finishQuoteIfNeeded();
		beginEmittedBlock();
		const int listIndent = indent + 1;
		st.activeListIndent = listIndent;
		if( listNum > 1 )
			st.listNum = listNum - 1;
		else if( listNum == 0 )
			st.listNum = 0;
		++st.listNum;
		cursor.setBlockCharFormat(bodyCharFmt);
		QTextList *curList = cursor.currentList();
		const bool needNewList = (listNum != 1)
			|| !curList
			|| curList->format().indent() != listIndent
			|| curList->format().style() != QTextListFormat::ListDecimal;
		if( needNewList ) {
			cursor.setBlockFormat(bodyBlockFmt);
			QTextListFormat lf;
			lf.setStyle(QTextListFormat::ListDecimal);
			lf.setIndent(listIndent);
			lf.setStart(st.listNum);
			cursor.createList(lf);
		} else if( cursor.blockFormat().marker() != QTextBlockFormat::MarkerType::NoMarker ) {
			QTextBlockFormat bf = cursor.blockFormat();
			bf.setMarker(QTextBlockFormat::MarkerType::NoMarker);
			cursor.setBlockFormat(bf);
		}
		applyAlignIfNeeded();
		cursor.block().setUserState(kBlockNormal);
		const int srcBase = (strippedLineStart >= 0) ? (strippedLineStart + static_cast<int>(line.size() - parsedText.size())) : -1;
		emitInlineWithAnchor(parsedText, bodyCharFmt, srcBase);
		return LineMgr::LT_NUM_LIST;
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
		applyAlignIfNeeded();
		cursor.setBlockCharFormat(bodyCharFmt);
		cursor.block().setUserState(kBlockQuote);
		const int srcBase = (strippedLineStart >= 0) ? (strippedLineStart + static_cast<int>(line.size() - parsedText.size())) : -1;
		emitInlineWithAnchor(parsedText, bodyCharFmt, srcBase);
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
		const int prvPos = cursor.position();
		cursor.insertText(line, bodyCharFmt);
		if( spans && strippedLineStart >= 0 )
			spans->push_back({strippedLineStart, prvPos, static_cast<int>(line.size())});
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
		applyAlignIfNeeded();
		const int srcBase = (strippedLineStart >= 0) ? (strippedLineStart + static_cast<int>(textStart)) : -1;
		emitInlineWithAnchor(line.mid(textStart), bodyCharFmt, srcBase);
		return LineMgr::LT_BODY;
	}

	cursor.setBlockFormat(bodyBlockFmt);
	applyAlignIfNeeded();
	emitInlineWithAnchor(line, bodyCharFmt, strippedLineStart);
	return LineMgr::LT_BODY;
}

//	```svg ... ``` フェンスブロックを LunaSVG でラスタライズして画像として挿入する
void insertSvgBlock(
	QTextCursor &cursor,
	const QString &svgSource,
	ParseState &st,
	int startLn,
	int endLn,
	QVector<int> &srcLineToBlock,
	QVector<int> &blockToSrcLine)
{
	if( st.inQuote ) {
		st.inQuote = false;
		finishBlockMarginIfNeeded(cursor, kBlockQuote);
	}
	st.activeListIndent = 0;
	st.listNum = 0;

	if( st.hasEmittedBlock )
		cursor.insertBlock();
	st.hasEmittedBlock = true;
	st.lastLineEmitted = true;
	if( QTextList *curList = cursor.currentList() )
		curList->remove(cursor.block());

	QTextBlockFormat bf;
	bf.setAlignment(Qt::AlignCenter);
	bf.setTopMargin(6.0);
	bf.setBottomMargin(6.0);
	cursor.setBlockFormat(bf);
	cursor.setBlockCharFormat(QTextCharFormat());
	cursor.block().setUserState(kBlockNormal);

	const QString trimmed = svgSource.trimmed();
	const QByteArray utf8 = trimmed.toUtf8();
	auto svgDoc = !utf8.isEmpty() ? lunasvg::Document::loadFromData(utf8.constData(), static_cast<size_t>(utf8.size())) : nullptr;
	QTextDocument *doc = cursor.document();
	if( svgDoc && doc ) {
		int width = 0;
		int height = 0;
		double wdScale = 1.0;
		double htScale = 1.0;
		bool hasWidth = false;
		bool hasHeight = false;
		QXmlStreamReader reader(trimmed);
		while( !reader.atEnd() ) {
			const QXmlStreamReader::TokenType token = reader.readNext();
			if( token == QXmlStreamReader::StartElement && reader.name().compare(u"svg", Qt::CaseInsensitive) == 0 ) {
				const auto attrs = reader.attributes();
				if( attrs.hasAttribute(u"width") ) {
					auto txt = attrs.value(u"width").trimmed();
					if( txt.endsWith(QChar('%')) ) {
						wdScale = txt.left(txt.size() - 1).toDouble() / 100.0;
					} else {
						if( txt.endsWith(u"px", Qt::CaseInsensitive) ) txt.chop(2);
						width = qRound(txt.toDouble());
						hasWidth = (width > 0);
					}
				}
				if( attrs.hasAttribute(u"height") ) {
					auto txt = attrs.value(u"height").trimmed();
					if( txt.endsWith(QChar('%')) ) {
						htScale = txt.left(txt.size() - 1).toDouble() / 100.0;
					} else {
						if( txt.endsWith(u"px", Qt::CaseInsensitive) ) txt.chop(2);
						height = qRound(txt.toDouble());
						hasHeight = (height > 0);
					}
				}
				if( attrs.hasAttribute(u"viewBox") ) {
					static const QRegularExpression reSep(QStringLiteral("[\\s,]+"));
					const auto list = attrs.value(u"viewBox").toString().split(reSep, Qt::SkipEmptyParts);
					if( list.size() >= 4 ) {
						const double vbW = list[2].toDouble();
						const double vbH = list[3].toDouble();
						if( !hasWidth && !hasHeight ) {
							width = qRound(vbW);
							height = qRound(vbH);
						} else if( !hasWidth && vbH > 0.0 ) {
							width = qRound(height * vbW / vbH);
						} else if( !hasHeight && vbW > 0.0 ) {
							height = qRound(width * vbH / vbW);
						}
					}
				}
				break;
			}
		}
		if( width <= 0 ) width = qRound(svgDoc->width());
		if( height <= 0 ) height = qRound(svgDoc->height());
		if( width <= 0 ) width = 320;
		if( height <= 0 ) height = 200;
		const qreal logW = static_cast<qreal>(qBound(1, qRound(width * wdScale), 4096));
		const qreal logH = static_cast<qreal>(qBound(1, qRound(height * htScale), 4096));

		const qreal dpr = (logW <= 2048.0 && logH <= 2048.0) ? 2.0 : 1.0;
		const int pxW = qMax(1, qRound(logW * dpr));
		const int pxH = qMax(1, qRound(logH * dpr));
		QImage img(pxW, pxH, QImage::Format_ARGB32_Premultiplied);
		if( !img.isNull() ) {
			img.fill(Qt::white);
			img.setDevicePixelRatio(dpr);
			lunasvg::Bitmap bitmap(img.bits(), img.width(), img.height(), img.bytesPerLine());
			svgDoc->render(bitmap);

			const QUrl resUrl(QStringLiteral("svg://block_%1").arg(startLn));
			doc->addResource(QTextDocument::ImageResource, resUrl, img);
			QTextImageFormat imgFmt;
			imgFmt.setName(resUrl.toString());
			if( st.maxImgWidth > 0.0 && logW > st.maxImgWidth ) {
				imgFmt.setWidth(st.maxImgWidth);
				imgFmt.setHeight(logH * st.maxImgWidth / logW);
			} else {
				imgFmt.setWidth(logW);
				imgFmt.setHeight(logH);
			}
			cursor.insertImage(imgFmt);
			st.hasImages = true;
		}
	} else {
		QTextCharFormat cf;
		cf.setFont(codeBlockFont(st.basePt));
		cf.setForeground(QColor(0x88, 0x88, 0x88));
		cursor.insertText(trimmed.isEmpty() ? QStringLiteral("[SVG]") : QStringLiteral("[SVG: invalid markup]"), cf);
	}

	const int bn = cursor.block().blockNumber();
	if( blockToSrcLine.size() <= bn )
		blockToSrcLine.resize(bn + 1, startLn);
	blockToSrcLine[bn] = startLn;
	for( int ln = startLn; ln <= endLn && ln < srcLineToBlock.size(); ++ln )
		srcLineToBlock[ln] = bn;
}

//	```keisen ... ``` フェンスブロックを QImage に描画して画像として挿入する（v0.3 do_keisen_block 移植）
void insertKeisenBlock(
	QTextCursor &cursor,
	const QString &fenceLine,
	const QStringList &lines,
	ParseState &st,
	int startLn,
	int endLn,
	QVector<int> &srcLineToBlock,
	QVector<int> &blockToSrcLine)
{
	if( st.inQuote ) {
		st.inQuote = false;
		finishBlockMarginIfNeeded(cursor, kBlockQuote);
	}
	st.activeListIndent = 0;
	st.listNum = 0;

	const int nLines = static_cast<int>(lines.size());
	if( nLines <= 0 )
		return;

	QTextDocument *doc = cursor.document();
	if( !doc )
		return;

	//	```keisen 以降のオプション（background-color: ...; color: ...;）を解析
	qsizetype fencePos = 0;
	while( fencePos < fenceLine.size() && fencePos < 3 && fenceLine[fencePos] == QChar(' ') )
		++fencePos;
	QStringView buf = QStringView(fenceLine).mid(fencePos + QStringLiteral("```keisen").size());
	QColor bgcolor(0xF5, 0xF5, 0xF5);
	QColor color(Qt::black);
	for( ;; ) {
		while( !buf.isEmpty() && buf[0] == u' ' ) buf = buf.mid(1);
		if( buf.startsWith(u"background-color:", Qt::CaseInsensitive) ) {
			buf = buf.mid(QStringLiteral("background-color:").size());
			while( !buf.isEmpty() && buf[0] == u' ' ) buf = buf.mid(1);
			const qsizetype ix = buf.indexOf(u';');
			if( ix < 0 ) break;
			const QColor c(buf.left(ix).trimmed().toString());
			if( c.isValid() ) bgcolor = c;
			buf = buf.mid(ix + 1);
		} else if( buf.startsWith(u"color:", Qt::CaseInsensitive) ) {
			buf = buf.mid(QStringLiteral("color:").size());
			while( !buf.isEmpty() && buf[0] == u' ' ) buf = buf.mid(1);
			const qsizetype ix = buf.indexOf(u';');
			if( ix < 0 ) break;
			const QColor c(buf.left(ix).trimmed().toString());
			if( c.isValid() ) color = c;
			buf = buf.mid(ix + 1);
		} else {
			break;
		}
	}

	QFont font;
	font.setFamilies({QStringLiteral("MS Gothic"), QStringLiteral("MS UI Gothic"),
	                  QStringLiteral("Osaka-mono"), QStringLiteral("monospace")});
	font.setFixedPitch(true);
	font.setPointSizeF(st.basePt > 0.0 ? st.basePt * 1.2 : 12.0);
	QFontMetrics fm(font);

	int width = 100;
	for( const QString &lineText : lines )
		width = qMax(width, fm.horizontalAdvance(lineText) + 10);
	const int lineHeight = qMax(1, fm.lineSpacing());
	const int height = lineHeight * nLines;
	const int halfW = qMax(1, fm.horizontalAdvance(u'A'));

	//	各行の「表示列(vc) → 連結ビット」マップを構築（隣接セルとの太さ合わせ用）
	QVector<QHash<int, ushort>> rowBits(nLines);
	for( int i = 0; i < nLines; ++i ) {
		const QString &line = lines[i];
		for( int j = 0; j < line.size(); ++j ) {
			const ushort b = getKeisenConnectionBits(line[j]);
			if( b != 0 ) {
				const int vc = fm.horizontalAdvance(line.left(j)) / halfW;
				rowBits[i].insert(vc, b);
			}
		}
	}

	//	KeisenDir ビット定数（TextEdit.cpp の enum KeisenDir と同一）
	constexpr ushort K_UP = 0x01, K_DN = 0x02, K_LT = 0x04, K_RT = 0x08;
	constexpr int K_THICK = 4, K_DBL = 8;
	auto armStyle = [](ushort b, ushort dir) -> int {
		if( b & (dir << K_DBL) )   return 3;
		if( b & (dir << K_THICK) ) return 2;
		if( b & dir )              return 1;
		return 0;
	};

	QImage img(width, height, QImage::Format_RGB32);
	img.fill(bgcolor);
	QPainter painter(&img);
	painter.setFont(font);
	const QPen textPen(color);
	const QPen thinPen(color, 1, Qt::SolidLine, Qt::FlatCap);
	const QPen thickPen(color, 3, Qt::SolidLine, Qt::FlatCap);

	for( int i = 0; i < nLines; ++i ) {
		const QString &line = lines[i];
		const int yTop = lineHeight * i;
		const int yBot = yTop + lineHeight;
		const int cy = yTop + lineHeight / 2;
		const int yBaseline = yTop + (lineHeight + fm.ascent() - fm.descent()) / 2;

		for( int j = 0; j < line.size(); ++j ) {
			const QChar ch = line[j];
			if( ch == u' ' ) continue;
			const int adv0 = fm.horizontalAdvance(line.left(j));
			const int adv1 = fm.horizontalAdvance(line.left(j + 1));
			const int xL = 5 + adv0;
			const int xR = 5 + adv1;
			const int cx = (xL + xR) / 2;
			const ushort bits = getKeisenConnectionBits(ch);

			if( bits == 0 ) {
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(textPen);
				painter.drawText(xL, yBaseline, QString(ch));
				continue;
			}

			int u = armStyle(bits, K_UP);
			int d = armStyle(bits, K_DN);
			int l = armStyle(bits, K_LT);
			int r = armStyle(bits, K_RT);
			const int vc = adv0 / halfW;

			//	隣接セルが細線で接続している場合、太線側の接続半辺を細線に合わせて段差を解消
			if( u == 2 && i > 0 && armStyle(rowBits[i - 1].value(vc, 0), K_DN) == 1 ) u = 1;
			if( d == 2 && i + 1 < nLines && armStyle(rowBits[i + 1].value(vc, 0), K_UP) == 1 ) d = 1;
			if( l == 2 && armStyle(rowBits[i].value(vc - 2, 0), K_RT) == 1 ) l = 1;
			if( r == 2 && armStyle(rowBits[i].value(vc + 2, 0), K_LT) == 1 ) r = 1;

			painter.setRenderHint(QPainter::Antialiasing, false);
			auto drawArm = [&](int style, int x1, int y1, int x2, int y2, bool vert) {
				if( style == 0 ) return;
				if( style == 3 ) {
					painter.setPen(thinPen);
					if( vert ) {
						painter.drawLine(x1 - 1, y1, x2 - 1, y2);
						painter.drawLine(x1 + 1, y1, x2 + 1, y2);
					} else {
						painter.drawLine(x1, y1 - 1, x2, y2 - 1);
						painter.drawLine(x1, y1 + 1, x2, y2 + 1);
					}
					return;
				}
				painter.setPen(style == 2 ? thickPen : thinPen);
				painter.drawLine(x1, y1, x2, y2);
			};
			const int extU = (u == 2) ? 1 : 0;
			const int extD = (d == 2) ? 1 : 0;
			const int extL = (l == 2) ? 1 : 0;
			const int extR = (r == 2) ? 1 : 0;
			drawArm(u, cx, yTop, cx, cy + extU, true);
			drawArm(d, cx, cy - extD, cx, yBot, true);
			drawArm(l, xL, cy, cx + extL, cy, false);
			drawArm(r, cx - extR, cy, xR, cy, false);
		}
	}
	painter.end();

	if( st.hasEmittedBlock )
		cursor.insertBlock();
	st.hasEmittedBlock = true;
	st.lastLineEmitted = true;
	if( QTextList *curList = cursor.currentList() )
		curList->remove(cursor.block());

	QTextBlockFormat bf;
	bf.setTopMargin(lineHeight / 2.0);
	bf.setBottomMargin(6.0);
	cursor.setBlockFormat(bf);
	cursor.setBlockCharFormat(QTextCharFormat());
	cursor.block().setUserState(kBlockNormal);

	const QUrl resUrl(QStringLiteral("keisen://block_%1").arg(startLn));
	doc->addResource(QTextDocument::ImageResource, resUrl, img);
	QTextImageFormat imgFmt;
	imgFmt.setName(resUrl.toString());
	const qreal logW = static_cast<qreal>(width);
	const qreal logH = static_cast<qreal>(height);
	if( st.maxImgWidth > 0.0 && logW > st.maxImgWidth ) {
		imgFmt.setWidth(st.maxImgWidth);
		imgFmt.setHeight(logH * st.maxImgWidth / logW);
	} else {
		imgFmt.setWidth(logW);
		imgFmt.setHeight(logH);
	}
	cursor.insertImage(imgFmt);
	st.hasImages = true;

	const int bn = cursor.block().blockNumber();
	if( blockToSrcLine.size() <= bn )
		blockToSrcLine.resize(bn + 1, startLn);
	blockToSrcLine[bn] = startLn;
	for( int ln = startLn; ln <= endLn && ln < srcLineToBlock.size(); ++ln )
		srcLineToBlock[ln] = bn;
}
}

MarkdownPreview::MarkdownPreview(QWidget *parent)
	: QTextEdit(parent)
{
	setReadOnly(false);
	setCursorWidth(0);	//	自前の赤カーソル／黒 I ビームを描画する
	viewport()->setMouseTracking(true);
	m_blinkTimer = new QTimer(this);
	m_blinkTimer->setInterval(500);
	connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
		m_caretVisible = !m_caretVisible;
		viewport()->update();
	});
	connect(this, &QTextEdit::cursorPositionChanged, this, [this]() {
		m_caretVisible = true;
		if( hasFocus() && m_blinkTimer )
			m_blinkTimer->start();
		viewport()->update();
	});
}

void MarkdownPreview::mouseMoveEvent(QMouseEvent *event)
{
	const bool ctrl = (event->modifiers() & Qt::ControlModifier) != 0;
	const QString href = ctrl ? anchorAt(event->pos()) : QString();
	viewport()->setCursor(href.isEmpty() ? Qt::IBeamCursor : Qt::PointingHandCursor);
	QTextEdit::mouseMoveEvent(event);
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
		if( (event->modifiers() & Qt::ControlModifier) != 0 ) {
			const QString href = anchorAt(event->pos());
			if( !href.isEmpty() ) {
				QUrl url(href);
				if( url.isRelative() && document() && document()->baseUrl().isValid() )
					url = document()->baseUrl().resolved(url);
				QDesktopServices::openUrl(url);
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

void MarkdownPreview::resizeEvent(QResizeEvent *event)
{
	QTextEdit::resizeEvent(event);
	updateImageSizes();
}

void MarkdownPreview::focusInEvent(QFocusEvent *event)
{
	QTextEdit::focusInEvent(event);
	m_caretVisible = true;
	if( m_blinkTimer )
		m_blinkTimer->start();
	viewport()->update();
}

void MarkdownPreview::focusOutEvent(QFocusEvent *event)
{
	QTextEdit::focusOutEvent(event);
	if( m_blinkTimer )
		m_blinkTimer->stop();
	m_caretVisible = true;
	viewport()->update();
}

void MarkdownPreview::cutToSource()
{
	if( !m_onEdit ) {
		QTextEdit::cut();
		return;
	}
	const QTextCursor pc = textCursor();
	if( !pc.hasSelection() )
		return;
	QString sel = pc.selectedText();
	sel.replace(QChar::ParagraphSeparator, QChar('\n'));
	QApplication::clipboard()->setText(sel);
	const int srcStart = prvToSrcPos(pc.selectionStart());
	const int srcEnd = prvToSrcPos(pc.selectionEnd());
	m_onEdit(srcStart, srcEnd, QString());
}

void MarkdownPreview::pasteToSource()
{
	if( !m_onEdit ) {
		QTextEdit::paste();
		return;
	}
	QString text = QApplication::clipboard()->text();
	if( text.isEmpty() )
		return;
	text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
	text.replace(QChar('\r'), QChar('\n'));
	const QTextCursor pc = textCursor();
	const int srcStart = pc.hasSelection() ? prvToSrcPos(pc.selectionStart()) : prvToSrcPos(pc.position());
	const int srcEnd = pc.hasSelection() ? prvToSrcPos(pc.selectionEnd()) : srcStart;
	m_onEdit(srcStart, srcEnd, text);
}

void MarkdownPreview::keyPressEvent(QKeyEvent *event)
{
	if( !m_onEdit ) {
		QTextEdit::keyPressEvent(event);
		return;
	}

	const bool ctrl = (event->modifiers() & Qt::ControlModifier) != 0;
	const bool alt = (event->modifiers() & Qt::AltModifier) != 0;
	const bool shift = (event->modifiers() & Qt::ShiftModifier) != 0;

	if( ctrl && !alt ) {
		if( event->key() == Qt::Key_Z ) {
			if( m_onUndoRedo ) m_onUndoRedo(shift);
			event->accept();
			return;
		}
		if( event->key() == Qt::Key_Y ) {
			if( m_onUndoRedo ) m_onUndoRedo(true);
			event->accept();
			return;
		}
		if( event->key() == Qt::Key_X ) {
			cutToSource();
			event->accept();
			return;
		}
		if( event->key() == Qt::Key_V ) {
			pasteToSource();
			event->accept();
			return;
		}
		//	Ctrl+C / Ctrl+A / Ctrl+矢印 などは QTextEdit に任せる
		QTextEdit::keyPressEvent(event);
		return;
	}

	const QTextCursor pc = textCursor();
	if( event->key() == Qt::Key_Backspace ) {
		if( pc.hasSelection() ) {
			m_onEdit(prvToSrcPos(pc.selectionStart()), prvToSrcPos(pc.selectionEnd()), QString());
		} else if( pc.position() > 0 ) {
			const int srcEnd = prvToSrcPos(pc.position());
			int srcStart = prvToSrcPos(pc.position() - 1);
			if( srcStart >= srcEnd && srcEnd > 0 )
				srcStart = srcEnd - 1;
			if( srcStart < srcEnd )
				m_onEdit(srcStart, srcEnd, QString());
		}
		event->accept();
		return;
	}
	if( event->key() == Qt::Key_Delete ) {
		if( pc.hasSelection() ) {
			m_onEdit(prvToSrcPos(pc.selectionStart()), prvToSrcPos(pc.selectionEnd()), QString());
		} else if( document() && pc.position() < document()->characterCount() - 1 ) {
			const int srcStart = prvToSrcPos(pc.position());
			int srcEnd = prvToSrcPos(pc.position() + 1);
			if( srcEnd <= srcStart )
				srcEnd = srcStart + 1;
			m_onEdit(srcStart, srcEnd, QString());
		}
		event->accept();
		return;
	}
	if( event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ) {
		const int srcStart = pc.hasSelection() ? prvToSrcPos(pc.selectionStart()) : prvToSrcPos(pc.position());
		const int srcEnd = pc.hasSelection() ? prvToSrcPos(pc.selectionEnd()) : srcStart;
		m_onEdit(srcStart, srcEnd, QStringLiteral("\n"));
		event->accept();
		return;
	}

	const QString txt = event->text();
	if( !ctrl && !alt && !txt.isEmpty() && txt[0] >= QChar(0x20) && txt[0] != QChar(0x7f) ) {
		const int srcStart = pc.hasSelection() ? prvToSrcPos(pc.selectionStart()) : prvToSrcPos(pc.position());
		const int srcEnd = pc.hasSelection() ? prvToSrcPos(pc.selectionEnd()) : srcStart;
		m_onEdit(srcStart, srcEnd, txt);
		event->accept();
		return;
	}

	QTextEdit::keyPressEvent(event);
}

void MarkdownPreview::inputMethodEvent(QInputMethodEvent *event)
{
	if( !m_onEdit ) {
		QTextEdit::inputMethodEvent(event);
		return;
	}
	const QString commit = event->commitString();
	const QString preedit = event->preeditString();
	if( !commit.isEmpty() ) {
		const QTextCursor pc = textCursor();
		const int srcStart = pc.hasSelection() ? prvToSrcPos(pc.selectionStart()) : prvToSrcPos(pc.position());
		const int srcEnd = pc.hasSelection() ? prvToSrcPos(pc.selectionEnd()) : srcStart;
		m_onEdit(srcStart, srcEnd, commit);
		if( !preedit.isEmpty() ) {
			QInputMethodEvent preEvent(preedit, event->attributes());
			QTextEdit::inputMethodEvent(&preEvent);
		}
		event->accept();
		return;
	}
	QTextEdit::inputMethodEvent(event);
}

void MarkdownPreview::updateImageSizes()
{
	if( !m_hasImages )
		return;
	QTextDocument *doc = document();
	if( !doc )
		return;
	const int maxWInt = qMax(0, viewport()->width() - 24);
	if( maxWInt <= 0 || maxWInt == m_lastImageMaxWidth )
		return;
	m_lastImageMaxWidth = maxWInt;
	const qreal maxW = static_cast<qreal>(maxWInt);

	const QSignalBlocker blocker(verticalScrollBar());
	QTextCursor cur(doc);
	bool began = false;
	for( QTextBlock b = doc->begin(); b.isValid(); b = b.next() ) {
		for( QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it ) {
			const QTextFragment frag = it.fragment();
			if( !frag.isValid() || !frag.charFormat().isImageFormat() )
				continue;
			QTextImageFormat imgFmt = frag.charFormat().toImageFormat();
			const QImage img = doc->resource(QTextDocument::ImageResource, QUrl(imgFmt.name())).value<QImage>();
			const QSizeF dipSize = img.deviceIndependentSize();
			if( img.isNull() || dipSize.width() <= 0.0 )
				continue;
			const qreal targetW = (dipSize.width() > maxW) ? maxW : dipSize.width();
			const qreal targetH = dipSize.height() * targetW / dipSize.width();
			if( qAbs(imgFmt.width() - targetW) > 0.5 || qAbs(imgFmt.height() - targetH) > 0.5 ) {
				if( !began ) {
					cur.beginEditBlock();
					began = true;
				}
				imgFmt.setWidth(targetW);
				imgFmt.setHeight(targetH);
				cur.setPosition(frag.position());
				cur.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
				cur.setCharFormat(imgFmt);
			}
		}
	}
	if( began ) {
		cur.endEditBlock();
		doc->setModified(false);
	}
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
	//	カーソル（フォーカス時は赤 2px 点滅、非フォーカス時は黒 2px 幅 I ビーム）
	if( !textCursor().hasSelection() ) {
		const QRect r = cursorRect();
		const int h = qMax(4, r.height());
		const QRect caretBounds(r.left() - 2, r.top(), 6, h);
		if( r.height() > 0 && viewport()->rect().intersects(caretBounds) ) {
			QPainter p(viewport());
			const int x = r.left();
			const int y0 = r.top();
			if( hasFocus() ) {
				if( m_caretVisible )
					p.fillRect(x, y0, 2, h, Qt::red);
			} else {
				p.fillRect(x, y0, 2, h, Qt::black);
				p.fillRect(x - 2, y0, 6, 2, Qt::black);
				p.fillRect(x - 2, y0 + h - 2, 6, 2, Qt::black);
			}
		}
	}
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

int MarkdownPreview::srcToPrvPos(int srcPos) const
{
	if( m_syncSpans.isEmpty() )
		return 0;
	auto it = std::upper_bound(m_syncSpans.begin(), m_syncSpans.end(), srcPos,
		[](int pos, const SyncSpan &s) { return pos < s.srcPos; });
	if( it == m_syncSpans.begin() )
		return m_syncSpans.front().prvPos;
	const SyncSpan &prev = *(it - 1);
	if( srcPos <= prev.srcPos + prev.len )
		return prev.prvPos + (srcPos - prev.srcPos);
	if( it != m_syncSpans.end() )
		return it->prvPos;
	return prev.prvPos + prev.len;
}

int MarkdownPreview::prvToSrcPos(int prvPos) const
{
	if( m_syncSpans.isEmpty() )
		return 0;
	auto it = std::upper_bound(m_syncSpans.begin(), m_syncSpans.end(), prvPos,
		[](int pos, const SyncSpan &s) { return pos < s.prvPos; });
	if( it == m_syncSpans.begin() )
		return m_syncSpans.front().srcPos;
	const SyncSpan &prev = *(it - 1);
	if( prvPos <= prev.prvPos + prev.len )
		return prev.srcPos + (prvPos - prev.prvPos);
	if( it != m_syncSpans.end() )
		return it->srcPos;
	return prev.srcPos + prev.len;
}

void MarkdownPreview::setCursorFromSrcPos(int srcPos, int srcAnchor)
{
	QTextDocument *doc = document();
	if( !doc || m_syncSpans.isEmpty() )
		return;
	const int maxPos = qMax(0, doc->characterCount() - 1);
	const int prvPos = qBound(0, srcToPrvPos(srcPos), maxPos);
	const QSignalBlocker blocker(this);
	QTextCursor cur = textCursor();
	if( srcAnchor >= 0 && srcAnchor != srcPos ) {
		const int prvAnchor = qBound(0, srcToPrvPos(srcAnchor), maxPos);
		cur.setPosition(prvAnchor);
		cur.setPosition(prvPos, QTextCursor::KeepAnchor);
	} else {
		cur.setPosition(prvPos);
	}
	setTextCursor(cur);
	viewport()->update();
}

void insertStyledTable(
	QTextCursor &cursor,
	const QList<QStringList> &rows,
	const QVector<int> &rowSrcLines,
	const QVector<int> &rowStartPos,
	const QList<QVector<int>> &rowCellCols,
	const QVector<Qt::Alignment> &aligns,
	ParseState &st,
	QVector<int> &srcLineToBlock,
	QVector<int> &blockToSrcLine,
	QVector<MarkdownPreview::SyncSpan> *spans)
{
	if( rows.isEmpty() )
		return;
	int maxCols = 0;
	for( const QStringList &r : rows )
		maxCols = qMax(maxCols, static_cast<int>(r.size()));
	if( maxCols <= 0 )
		return;

	if( st.inQuote ) {
		st.inQuote = false;
		finishBlockMarginIfNeeded(cursor, kBlockQuote);
	}
	st.activeListIndent = 0;
	if( QTextList *curList = cursor.currentList() ) {
		if( st.hasEmittedBlock )
			cursor.insertBlock();
		curList->remove(cursor.block());
	}

	QTextTableFormat tableFmt;
	tableFmt.setBorder(1.0);
	tableFmt.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
	tableFmt.setBorderBrush(QColor(0xB8, 0xC4, 0xD0));
	tableFmt.setBorderCollapse(true);
	tableFmt.setCellPadding(5.0);
	tableFmt.setCellSpacing(0.0);
	tableFmt.setTopMargin(6.0);
	tableFmt.setBottomMargin(6.0);

	QTextTable *table = cursor.insertTable(static_cast<int>(rows.size()), maxCols, tableFmt);
	if( !table )
		return;

	static const QRegularExpression kNumberRe(QStringLiteral(R"(^[+-]?(\d+\.\d*|\d+|\.\d+)%?$)"));
	const QColor &headerBg = st.tableHeaderColor;
	const QColor &zebraBg1 = st.tableZebraColor1;
	const QColor &zebraBg2 = st.tableZebraColor2;

	const int nRows = static_cast<int>(rows.size());
	auto cellTextAt = [&](int r, int c) -> QString {
		return (r >= 0 && r < nRows && c >= 0 && c < rows[r].size()) ? rows[r][c] : QString();
	};

	QVector<QVector<bool>> consumed(nRows, QVector<bool>(maxCols, false));
	for( int r = 0; r < nRows; ++r ) {
		const int srcLine = (r < rowSrcLines.size()) ? rowSrcLines[r] : -1;
		for( int c = 0; c < maxCols; ++c ) {
			if( consumed[r][c] )
				continue;

			//	右方向の "<" セルで横結合スパンを算出
			int colSpan = 1;
			while( c + colSpan < maxCols
				&& !consumed[r][c + colSpan]
				&& cellTextAt(r, c + colSpan) == QStringLiteral("<") ) {
				++colSpan;
			}

			//	下方向の "^" セル（2D 結合時は右隣に "<" または "^"）で縦結合スパンを算出
			int rowSpan = 1;
			while( r + rowSpan < nRows ) {
				const int r2 = r + rowSpan;
				if( consumed[r2][c] || cellTextAt(r2, c) != QStringLiteral("^") )
					break;
				bool rowOk = true;
				for( int k = 1; k < colSpan; ++k ) {
					const QString t = cellTextAt(r2, c + k);
					if( consumed[r2][c + k] || (t != QStringLiteral("^") && t != QStringLiteral("<")) ) {
						rowOk = false;
						break;
					}
				}
				if( !rowOk )
					break;
				++rowSpan;
			}

			for( int dr = 0; dr < rowSpan; ++dr ) {
				for( int dc = 0; dc < colSpan; ++dc )
					consumed[r + dr][c + dc] = true;
			}
			if( rowSpan > 1 || colSpan > 1 )
				table->mergeCells(r, c, rowSpan, colSpan);

			QTextTableCell cell = table->cellAt(r, c);
			if( !cell.isValid() )
				continue;

			QTextTableCellFormat cellFmt = cell.format().toTableCellFormat();
			cellFmt.setBackground(r == 0 ? headerBg : ((r % 2) != 0 ? zebraBg1 : zebraBg2));
			if( rowSpan > 1 )
				cellFmt.setVerticalAlignment(QTextCharFormat::AlignMiddle);
			cell.setFormat(cellFmt);

			const QString cellText = cellTextAt(r, c);
			QTextCursor cellCur = cell.firstCursorPosition();
			QTextBlockFormat bf;
			if( r == 0 || colSpan > 1 ) {
				bf.setAlignment(Qt::AlignCenter);
			} else if( !aligns.isEmpty() ) {
				bf.setAlignment(c < aligns.size() ? aligns[c] : Qt::AlignLeft);
			} else if( kNumberRe.match(cellText).hasMatch() ) {
				bf.setAlignment(Qt::AlignRight);
			} else {
				bf.setAlignment(Qt::AlignLeft);
			}
			cellCur.setBlockFormat(bf);

			QTextCharFormat cf;
			if( r == 0 )
				cf.setFontWeight(QFont::Bold);
			cellCur.setCharFormat(cf);
			int cellSrcBase = -1;
			if( r < rowStartPos.size() && rowStartPos[r] >= 0
				&& r < rowCellCols.size() && c < rowCellCols[r].size() ) {
				cellSrcBase = rowStartPos[r] + rowCellCols[r][c];
			}
			const qsizetype spansBefore = spans ? spans->size() : 0;
			const int cellPrvStart = cellCur.position();
			insertInlineFormatted(cellCur, cellText, cf, st.basePt, st.maxImgWidth, &st.hasImages, cellSrcBase, spans);
			if( spans && cellSrcBase >= 0 && spans->size() == spansBefore )
				spans->push_back({cellSrcBase, cellPrvStart, 0});

			const int bn = cell.firstCursorPosition().block().blockNumber();
			if( srcLine >= 0 ) {
				if( srcLine < srcLineToBlock.size() && srcLineToBlock[srcLine] < 0 )
					srcLineToBlock[srcLine] = bn;
				if( blockToSrcLine.size() <= bn )
					blockToSrcLine.resize(bn + 1, srcLine);
				blockToSrcLine[bn] = srcLine;
			}
		}
	}

	//	テーブル直後の空ブロックへカーソルを移動（次のブロックで余分な空行が出ないよう hasEmittedBlock をリセット）
	cursor.setPosition(table->lastPosition());
	cursor.movePosition(QTextCursor::NextCharacter);
	cursor.setBlockFormat(QTextBlockFormat());
	cursor.setBlockCharFormat(QTextCharFormat());
	st.hasEmittedBlock = false;
}

struct BoxNode;
struct BoxChild {
	bool isBox = false;
	std::shared_ptr<BoxNode> box;
	QString rawLine;
	int srcLine = -1;
	int lineStartPos = -1;
};

struct BoxNode {
	int indent = 0;
	BoxOptions opt;
	QString title;
	int srcLine = -1;
	int srcBasePos = -1;
	int contentBaseIndent = -1;
	QVector<BoxChild> children;
};

//	枠ブロックノードを再帰的に QTextTable として描画する
void renderBoxNode(
	QTextCursor &cursor,
	const BoxNode &node,
	ParseState &st,
	QVector<int> &srcLineToBlock,
	QVector<int> &blockToSrcLine,
	QVector<MarkdownPreview::SyncSpan> *spans,
	int depth = 0)
{
	if( st.inQuote ) {
		st.inQuote = false;
		finishBlockMarginIfNeeded(cursor, kBlockQuote);
	}
	st.activeListIndent = 0;
	st.listNum = 0;
	if( QTextList *curList = cursor.currentList() ) {
		if( st.hasEmittedBlock )
			cursor.insertBlock();
		curList->remove(cursor.block());
	}

	QTextTableFormat tableFmt;
	tableFmt.setBorder(1.5);
	tableFmt.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
	tableFmt.setBorderBrush(QColor(0x66, 0x77, 0x88));
	tableFmt.setBorderCollapse(true);
	tableFmt.setCellPadding(8.0);
	tableFmt.setCellSpacing(0.0);
	tableFmt.setTopMargin(depth == 0 ? 6.0 : 4.0);
	tableFmt.setBottomMargin(depth == 0 ? 6.0 : 4.0);
	tableFmt.setWidth(QTextLength(QTextLength::PercentageLength, 100));

	QTextTable *table = cursor.insertTable(1, 1, tableFmt);
	if( !table )
		return;

	QTextTableCell cell = table->cellAt(0, 0);
	if( cell.isValid() ) {
		QTextCursor cellCur = cell.firstCursorPosition();
		ParseState cellSt = st;
		cellSt.activeListIndent = 0;
		cellSt.listNum = 0;
		cellSt.inQuote = false;
		cellSt.inCodeBlock = false;
		cellSt.codeHasLines = false;
		cellSt.inHtmlComment = false;
		cellSt.hasEmittedBlock = false;
		cellSt.lastLineEmitted = false;
		cellSt.hasImages = false;

		if( !node.title.isEmpty() || node.children.isEmpty() ) {
			QTextBlockFormat titleBf;
			titleBf.setAlignment(node.opt.align);
			cellCur.setBlockFormat(titleBf);
			QTextCharFormat cf;
			cellCur.setCharFormat(cf);
			const qsizetype spansBefore = spans ? spans->size() : 0;
			const int cellPrvStart = cellCur.position();
			insertInlineFormatted(cellCur, node.title, cf, cellSt.basePt, cellSt.maxImgWidth, &cellSt.hasImages, node.srcBasePos, spans);
			if( spans && node.srcBasePos >= 0 && spans->size() == spansBefore )
				spans->push_back({node.srcBasePos, cellPrvStart, 0});
			cellSt.hasEmittedBlock = true;

			const int bn = cell.firstCursorPosition().block().blockNumber();
			if( node.srcLine >= 0 ) {
				if( node.srcLine < srcLineToBlock.size() )
					srcLineToBlock[node.srcLine] = bn;
				if( blockToSrcLine.size() <= bn )
					blockToSrcLine.resize(bn + 1, node.srcLine);
				blockToSrcLine[bn] = node.srcLine;
			}
		} else if( node.srcLine >= 0 ) {
			const int bn = cell.firstCursorPosition().block().blockNumber();
			if( node.srcLine < srcLineToBlock.size() )
				srcLineToBlock[node.srcLine] = bn;
			if( blockToSrcLine.size() <= bn )
				blockToSrcLine.resize(bn + 1, node.srcLine);
			blockToSrcLine[bn] = node.srcLine;
		}

		if( node.opt.horizontal && !node.children.isEmpty() ) {
			//	[>] / [->] 横並び：子ボックスおよび連続テキスト群を1行N列のレイアウトテーブルに均等配置
			struct HorizCol {
				bool isBox = false;
				std::shared_ptr<BoxNode> box;
				QVector<BoxChild> lines;
			};
			QVector<HorizCol> cols;
			for( const BoxChild &child : node.children ) {
				if( child.isBox && child.box ) {
					HorizCol hc;
					hc.isBox = true;
					hc.box = child.box;
					cols.append(hc);
				} else if( child.rawLine.trimmed().isEmpty() ) {
					continue;
				} else {
					if( cols.isEmpty() || cols.back().isBox ) {
						HorizCol hc;
						hc.isBox = false;
						cols.append(hc);
					}
					cols.back().lines.append(child);
				}
			}
			if( !cols.isEmpty() ) {
				const int colCount = static_cast<int>(cols.size());
				const bool withArrows = node.opt.arrow && colCount >= 2;
				const int tableCols = withArrows ? (colCount * 2 - 1) : colCount;
				QTextTableFormat rowFmt;
				rowFmt.setBorder(0.0);
				rowFmt.setCellPadding(0.0);
				rowFmt.setCellSpacing(withArrows ? 2.0 : 6.0);
				rowFmt.setTopMargin(node.title.isEmpty() ? 0.0 : 6.0);
				rowFmt.setBottomMargin(0.0);
				rowFmt.setWidth(QTextLength(QTextLength::PercentageLength, 100));
				QVector<QTextLength> colWidths;
				colWidths.reserve(tableCols);
				if( withArrows ) {
					const int nArrows = colCount - 1;
					const qreal arrowPct = qMin(5.0, 20.0 / nArrows);
					const qreal itemPct = (100.0 - arrowPct * nArrows) / colCount;
					for( int tc = 0; tc < tableCols; ++tc ) {
						const qreal pct = ((tc % 2) == 1) ? arrowPct : itemPct;
						colWidths.append(QTextLength(QTextLength::PercentageLength, pct));
					}
				} else {
					colWidths.fill(QTextLength(QTextLength::PercentageLength, 100.0 / colCount), colCount);
				}
				rowFmt.setColumnWidthConstraints(colWidths);

				QTextTable *rowTable = cellCur.insertTable(1, tableCols, rowFmt);
				if( rowTable ) {
					for( int c = 0; c < colCount; ++c ) {
						const int tc = withArrows ? (c * 2) : c;
						QTextTableCell colCell = rowTable->cellAt(0, tc);
						if( colCell.isValid() ) {
							QTextTableCellFormat cfmt = colCell.format().toTableCellFormat();
							cfmt.setVerticalAlignment(withArrows ? QTextCharFormat::AlignMiddle : QTextCharFormat::AlignTop);
							colCell.setFormat(cfmt);

							QTextCursor colCur = colCell.firstCursorPosition();
							ParseState colSt = cellSt;
							colSt.activeListIndent = 0;
							colSt.listNum = 0;
							colSt.inQuote = false;
							colSt.inCodeBlock = false;
							colSt.codeHasLines = false;
							colSt.inHtmlComment = false;
							colSt.hasEmittedBlock = false;
							colSt.lastLineEmitted = false;
							colSt.hasImages = false;
							if( cols[c].isBox && cols[c].box ) {
								renderBoxNode(colCur, *cols[c].box, colSt, srcLineToBlock, blockToSrcLine, spans, depth + 1);
							} else {
								const int stripBase = (node.contentBaseIndent >= 0) ? node.contentBaseIndent : (node.indent + 2);
								for( const BoxChild &lineChild : cols[c].lines ) {
									int strippedChars = 0;
									const QString innerLine = stripIndentWidth(lineChild.rawLine, stripBase, &strippedChars);
									const int linePos = (lineChild.lineStartPos >= 0) ? (lineChild.lineStartPos + strippedChars) : -1;
									parseAndAppendLine(colCur, innerLine, colSt, lineChild.srcLine, linePos, spans);
									if( colSt.lastLineEmitted && lineChild.srcLine >= 0 ) {
										const int bn = colCur.block().blockNumber();
										if( lineChild.srcLine < srcLineToBlock.size() )
											srcLineToBlock[lineChild.srcLine] = bn;
										if( blockToSrcLine.size() <= bn )
											blockToSrcLine.resize(bn + 1, lineChild.srcLine);
										blockToSrcLine[bn] = lineChild.srcLine;
									}
								}
							}
							if( colSt.inQuote )
								finishBlockMarginIfNeeded(colCur, kBlockQuote);
							if( colSt.inCodeBlock && colSt.codeHasLines )
								finishBlockMarginIfNeeded(colCur, kBlockCode);
							if( colSt.hasImages )
								cellSt.hasImages = true;
						}
						if( withArrows && c + 1 < colCount ) {
							QTextTableCell arrowCell = rowTable->cellAt(0, tc + 1);
							if( arrowCell.isValid() ) {
								QTextTableCellFormat afmt = arrowCell.format().toTableCellFormat();
								afmt.setVerticalAlignment(QTextCharFormat::AlignMiddle);
								arrowCell.setFormat(afmt);
								QTextCursor arrowCur = arrowCell.firstCursorPosition();
								QTextBlockFormat abf;
								abf.setAlignment(Qt::AlignCenter);
								arrowCur.setBlockFormat(abf);
								QTextCharFormat acf;
								acf.setFontWeight(QFont::Bold);
								acf.setFontPointSize(cellSt.basePt * 1.2);
								acf.setForeground(QColor(0x55, 0x66, 0x77));
								arrowCur.insertText(QStringLiteral("\u2192"), acf);
							}
						}
					}
					cellCur.setPosition(rowTable->lastPosition());
					cellCur.movePosition(QTextCursor::NextCharacter);
					cellSt.hasEmittedBlock = false;
				}
			}
		} else {
			for( const BoxChild &child : node.children ) {
				if( child.isBox && child.box ) {
					renderBoxNode(cellCur, *child.box, cellSt, srcLineToBlock, blockToSrcLine, spans, depth + 1);
				} else {
					const int stripBase = (node.contentBaseIndent >= 0) ? node.contentBaseIndent : (node.indent + 2);
					int strippedChars = 0;
					const QString innerLine = child.rawLine.trimmed().isEmpty()
						? QString()
						: stripIndentWidth(child.rawLine, stripBase, &strippedChars);
					const int linePos = (child.lineStartPos >= 0) ? (child.lineStartPos + strippedChars) : -1;
					parseAndAppendLine(cellCur, innerLine, cellSt, child.srcLine, linePos, spans);
					if( cellSt.lastLineEmitted ) {
						if( node.opt.align != Qt::AlignLeft && cellCur.blockFormat().alignment() == Qt::Alignment() ) {
							QTextBlockFormat bf = cellCur.blockFormat();
							bf.setAlignment(node.opt.align);
							cellCur.setBlockFormat(bf);
						}
						if( child.srcLine >= 0 ) {
							const int bn = cellCur.block().blockNumber();
							if( child.srcLine < srcLineToBlock.size() )
								srcLineToBlock[child.srcLine] = bn;
							if( blockToSrcLine.size() <= bn )
								blockToSrcLine.resize(bn + 1, child.srcLine);
							blockToSrcLine[bn] = child.srcLine;
						}
					}
				}
			}
		}

		if( cellSt.inQuote )
			finishBlockMarginIfNeeded(cellCur, kBlockQuote);
		if( cellSt.inCodeBlock && cellSt.codeHasLines )
			finishBlockMarginIfNeeded(cellCur, kBlockCode);
		if( cellSt.hasImages )
			st.hasImages = true;
	}

	cursor.setPosition(table->lastPosition());
	cursor.movePosition(QTextCursor::NextCharacter);
	cursor.setBlockFormat(QTextBlockFormat());
	cursor.setBlockCharFormat(QTextCharFormat());
	st.hasEmittedBlock = false;
}

//	行 ln から始まる枠ブロック（インデントされた子アイテム・入れ子枠を含む）を消費して描画する
int parseAndInsertBoxBlock(
	QTextCursor &cursor,
	const QStringList &lines,
	const QVector<int> &lineStarts,
	int startLn,
	ParseState &st,
	QVector<int> &srcLineToBlock,
	QVector<int> &blockToSrcLine,
	QVector<MarkdownPreview::SyncSpan> *spans)
{
	const int nLines = static_cast<int>(lines.size());
	int rootIndent = 0;
	BoxOptions rootOpt;
	QString rootTitle;
	int rootCol = 0;
	if( !parseBoxLine(lines[startLn], rootIndent, rootOpt, rootTitle, &rootCol) )
		return startLn;

	auto root = std::make_shared<BoxNode>();
	root->indent = rootIndent;
	root->opt = rootOpt;
	root->title = rootTitle;
	root->srcLine = startLn;
	root->srcBasePos = lineStarts[startLn] + rootCol;

	QVector<BoxNode *> stack;
	stack.append(root.get());

	int lastConsumedLn = startLn;
	int ln = startLn + 1;
	bool afterBlank = false;
	while( ln < nLines ) {
		if( lines[ln].trimmed().isEmpty() ) {
			//	空行の先を覗いて、親枠より深くインデントされた行が続く場合のみ枠内空行として取り込む
			int probe = ln + 1;
			while( probe < nLines && lines[probe].trimmed().isEmpty() )
				++probe;
			if( probe >= nLines || measureIndent(lines[probe]) <= rootIndent )
				break;
			for( int k = ln; k < probe; ++k ) {
				BoxChild c;
				c.isBox = false;
				c.rawLine.clear();
				c.srcLine = k;
				c.lineStartPos = lineStarts[k];
				stack.back()->children.append(c);
				lastConsumedLn = k;
			}
			ln = probe;
			afterBlank = true;
			continue;
		}

		const int ind = measureIndent(lines[ln]);
		int childIndent = 0;
		BoxOptions childOpt;
		QString childTitle;
		int childCol = 0;
		const bool isBox = parseBoxLine(lines[ln], childIndent, childOpt, childTitle, &childCol);

		if( isBox ) {
			if( ind <= rootIndent )
				break;
			while( stack.size() > 1 && ind <= stack.back()->indent )
				stack.pop_back();
			auto childBox = std::make_shared<BoxNode>();
			childBox->indent = childIndent;
			childBox->opt = childOpt;
			childBox->title = childTitle;
			childBox->srcLine = ln;
			childBox->srcBasePos = lineStarts[ln] + childCol;
			BoxChild c;
			c.isBox = true;
			c.box = childBox;
			stack.back()->children.append(c);
			stack.append(childBox.get());
		} else {
			if( afterBlank ) {
				if( ind <= rootIndent )
					break;
				while( stack.size() > 1 && ind <= stack.back()->indent )
					stack.pop_back();
			}
			//	空行なしの非ボックス行は、インデントに関わらず直前ボックス（stack.back()）の枠内改行として取り込む
			if( stack.back()->contentBaseIndent < 0 && ind > stack.back()->indent )
				stack.back()->contentBaseIndent = ind;
			BoxChild c;
			c.isBox = false;
			c.rawLine = lines[ln];
			c.srcLine = ln;
			c.lineStartPos = lineStarts[ln];
			stack.back()->children.append(c);
		}
		afterBlank = false;
		lastConsumedLn = ln;
		++ln;
	}

	renderBoxNode(cursor, *root, st, srcLineToBlock, blockToSrcLine, spans, 0);
	return lastConsumedLn;
}

void MarkdownPreview::setTableColors(const QColor &header, const QColor &zebra1, const QColor &zebra2)
{
	if( header.isValid() ) m_tableHeaderColor = header;
	if( zebra1.isValid() ) m_tableZebraColor1 = zebra1;
	if( zebra2.isValid() ) m_tableZebraColor2 = zebra2;
}

void MarkdownPreview::setMarkdown(const QString &markdown)
{
	const QSignalBlocker blocker(verticalScrollBar());
	const QSignalBlocker selfBlocker(this);
	QTextDocument *doc = document();
	doc->clear();

	QTextCursor cursor(doc);
	cursor.beginEditBlock();

	ParseState st;
	st.basePt = doc->defaultFont().pointSizeF() > 0.0 ? doc->defaultFont().pointSizeF() : 10.0;
	const int maxWInt = qMax(0, viewport()->width() - 24);
	st.maxImgWidth = static_cast<qreal>(maxWInt);
	st.tableHeaderColor = m_tableHeaderColor;
	st.tableZebraColor1 = m_tableZebraColor1;
	st.tableZebraColor2 = m_tableZebraColor2;
	const QStringList rawLines = markdown.split(QChar('\n'));
	const int nLines = static_cast<int>(rawLines.size());
	QStringList lines;
	QVector<int> lineStarts;
	lines.reserve(nLines);
	lineStarts.reserve(nLines);
	int accPos = 0;
	for( int i = 0; i < nLines; ++i ) {
		lineStarts.append(accPos);
		accPos += static_cast<int>(rawLines[i].size()) + 1;
		QString s = rawLines[i];
		if( s.endsWith(QChar('\r')) ) s.chop(1);
		lines.append(s);
	}
	m_srcLineToBlock.fill(-1, nLines);
	m_blockToSrcLine.clear();
	m_syncSpans.clear();
	m_syncSpans.reserve(nLines * 2);

	for( int ln = 0; ln < nLines; ++ln ) {
		const QString &line = lines[ln];
		if( !st.inCodeBlock && !st.inHtmlComment && isCsvFence(line) ) {
			QList<QStringList> rows;
			QVector<int> rowSrcLines;
			QVector<int> rowStartPos;
			QList<QVector<int>> rowCellCols;
			bool inQuotes = false;
			bool csvInComment = false;
			QStringList curFields;
			QVector<int> curCols;
			QString curField;
			int rowStartLine = ln + 1;
			while( ++ln < nLines && !isCodeFence(lines[ln]) ) {
				bool commentOnly = false;
				const QString cleaned = inQuotes ? lines[ln] : stripHtmlComments(lines[ln], csvInComment, commentOnly);
				if( !inQuotes && (commentOnly || cleaned.trimmed().isEmpty()) ) {
					rowStartLine = ln + 1;
					continue;
				}
				if( !inQuotes )
					rowStartLine = ln;
				if( parseCsvLine(cleaned, inQuotes, curFields, curField, &curCols) ) {
					rows.append(curFields);
					rowSrcLines.append(rowStartLine);
					rowStartPos.append(lineStarts[rowStartLine]);
					rowCellCols.append(curCols);
				}
			}
			if( inQuotes && !curFields.isEmpty() ) {
				curFields.append(curField.trimmed());
				rows.append(curFields);
				rowSrcLines.append(rowStartLine);
				rowStartPos.append(lineStarts[rowStartLine]);
				rowCellCols.append(curCols);
			}
			insertStyledTable(cursor, rows, rowSrcLines, rowStartPos, rowCellCols, {}, st, m_srcLineToBlock, m_blockToSrcLine, &m_syncSpans);
			continue;
		}
		if( !st.inCodeBlock && !st.inHtmlComment && isSvgFence(line) ) {
			const int startLn = ln;
			QStringList svgLines;
			while( ++ln < nLines && !isCodeFence(lines[ln]) )
				svgLines.append(lines[ln]);
			const int endLn = qMin(ln, nLines - 1);
			insertSvgBlock(cursor, svgLines.join(QChar('\n')), st, startLn, endLn, m_srcLineToBlock, m_blockToSrcLine);
			continue;
		}
		if( !st.inCodeBlock && !st.inHtmlComment && isKeisenFence(line) ) {
			const int startLn = ln;
			const QString fenceLine = line;
			QStringList keisenLines;
			while( ++ln < nLines && !isCodeFence(lines[ln]) )
				keisenLines.append(lines[ln]);
			const int endLn = qMin(ln, nLines - 1);
			insertKeisenBlock(cursor, fenceLine, keisenLines, st, startLn, endLn, m_srcLineToBlock, m_blockToSrcLine);
			continue;
		}
		if( !st.inCodeBlock && !st.inHtmlComment ) {
			int boxIndent = 0;
			BoxOptions boxOpt;
			QString boxText;
			if( parseBoxLine(line, boxIndent, boxOpt, boxText) ) {
				ln = parseAndInsertBoxBlock(cursor, lines, lineStarts, ln, st, m_srcLineToBlock, m_blockToSrcLine, &m_syncSpans);
				continue;
			}
		}
		if( !st.inCodeBlock && !st.inHtmlComment && ln + 1 < nLines ) {
			QStringList headerCells;
			QVector<int> headerCols;
			QVector<Qt::Alignment> aligns;
			if( parseGfmTableRow(line, headerCells, &headerCols) && parseGfmTableSeparator(lines[ln + 1], aligns) ) {
				QList<QStringList> rows;
				QVector<int> rowSrcLines;
				QVector<int> rowStartPos;
				QList<QVector<int>> rowCellCols;
				rows.append(headerCells);
				rowSrcLines.append(ln);
				rowStartPos.append(lineStarts[ln]);
				rowCellCols.append(headerCols);
				ln += 1;	//	区切り行を消費
				bool tblComment = false;
				while( ln + 1 < nLines ) {
					bool commentOnly = false;
					const QString cleaned = stripHtmlComments(lines[ln + 1], tblComment, commentOnly);
					if( commentOnly ) {
						++ln;
						continue;
					}
					QStringList rowCells;
					QVector<int> rowCols;
					if( !parseGfmTableRow(cleaned, rowCells, &rowCols) )
						break;
					++ln;
					rows.append(rowCells);
					rowSrcLines.append(ln);
					rowStartPos.append(lineStarts[ln]);
					rowCellCols.append(rowCols);
				}
				insertStyledTable(cursor, rows, rowSrcLines, rowStartPos, rowCellCols, aligns, st, m_srcLineToBlock, m_blockToSrcLine, &m_syncSpans);
				continue;
			}
		}

		parseAndAppendLine(cursor, line, st, ln, lineStarts[ln], &m_syncSpans);
		if( st.lastLineEmitted ) {
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
	m_hasImages = st.hasImages;
	m_lastImageMaxWidth = maxWInt;
	doc->setModified(false);
	moveCursor(QTextCursor::Start);
}

void MarkdownPreview::setMarkdown(TextDocument *srcDoc)
{
	if( !srcDoc ) {
		m_srcLineToBlock.clear();
		m_blockToSrcLine.clear();
		m_syncSpans.clear();
		m_hasImages = false;
		m_lastImageMaxWidth = -1;
		clear();
		return;
	}

	const QSignalBlocker blocker(verticalScrollBar());
	const QSignalBlocker selfBlocker(this);
	const int prevScroll = verticalScrollBar() ? verticalScrollBar()->value() : 0;

	QTextDocument *doc = document();
	doc->clear();

	QTextCursor cursor(doc);
	cursor.beginEditBlock();

	ParseState st;
	st.basePt = doc->defaultFont().pointSizeF() > 0.0 ? doc->defaultFont().pointSizeF() : 10.0;
	const int maxWInt = qMax(0, viewport()->width() - 24);
	st.maxImgWidth = static_cast<qreal>(maxWInt);
	st.tableHeaderColor = m_tableHeaderColor;
	st.tableZebraColor1 = m_tableZebraColor1;
	st.tableZebraColor2 = m_tableZebraColor2;
	ViewLineMgr &mgr = srcDoc->lineMgr();
	const int nLines = mgr.lineCount();
	QStringList lines;
	QVector<int> lineStarts;
	lines.reserve(nLines);
	lineStarts.reserve(nLines);
	for( int ln = 0; ln < nLines; ++ln ) {
		lineStarts.append(static_cast<int>(mgr.lineStartPos(ln)));
		QString line = srcDoc->findBlockByNumber(ln).text();
		if( line.endsWith(QChar('\n')) ) line.chop(1);
		if( line.endsWith(QChar('\r')) ) line.chop(1);
		lines.append(line);
	}
	m_srcLineToBlock.fill(-1, nLines);
	m_blockToSrcLine.clear();
	m_syncSpans.clear();
	m_syncSpans.reserve(nLines * 2);

	for( int ln = 0; ln < nLines; ++ln ) {
		const QString &line = lines[ln];
		if( !st.inCodeBlock && !st.inHtmlComment && isCsvFence(line) ) {
			mgr.setLineType(ln, LineMgr::LT_CODE_FENCE);
			QList<QStringList> rows;
			QVector<int> rowSrcLines;
			QVector<int> rowStartPos;
			QList<QVector<int>> rowCellCols;
			bool inQuotes = false;
			bool csvInComment = false;
			QStringList curFields;
			QVector<int> curCols;
			QString curField;
			int rowStartLine = ln + 1;
			while( ++ln < nLines && !isCodeFence(lines[ln]) ) {
				mgr.setLineType(ln, LineMgr::LT_TABLE);
				bool commentOnly = false;
				const QString cleaned = inQuotes ? lines[ln] : stripHtmlComments(lines[ln], csvInComment, commentOnly);
				if( !inQuotes && (commentOnly || cleaned.trimmed().isEmpty()) ) {
					rowStartLine = ln + 1;
					continue;
				}
				if( !inQuotes )
					rowStartLine = ln;
				if( parseCsvLine(cleaned, inQuotes, curFields, curField, &curCols) ) {
					rows.append(curFields);
					rowSrcLines.append(rowStartLine);
					rowStartPos.append(lineStarts[rowStartLine]);
					rowCellCols.append(curCols);
				}
			}
			if( ln < nLines && isCodeFence(lines[ln]) )
				mgr.setLineType(ln, LineMgr::LT_CODE_FENCE);
			if( inQuotes && !curFields.isEmpty() ) {
				curFields.append(curField.trimmed());
				rows.append(curFields);
				rowSrcLines.append(rowStartLine);
				rowStartPos.append(lineStarts[rowStartLine]);
				rowCellCols.append(curCols);
			}
			insertStyledTable(cursor, rows, rowSrcLines, rowStartPos, rowCellCols, {}, st, m_srcLineToBlock, m_blockToSrcLine, &m_syncSpans);
			continue;
		}
		if( !st.inCodeBlock && !st.inHtmlComment && isSvgFence(line) ) {
			const int startLn = ln;
			mgr.setLineType(ln, LineMgr::LT_CODE_FENCE);
			QStringList svgLines;
			while( ++ln < nLines && !isCodeFence(lines[ln]) ) {
				mgr.setLineType(ln, LineMgr::LT_CODE_BLOCK);
				svgLines.append(lines[ln]);
			}
			if( ln < nLines && isCodeFence(lines[ln]) )
				mgr.setLineType(ln, LineMgr::LT_CODE_FENCE);
			const int endLn = qMin(ln, nLines - 1);
			insertSvgBlock(cursor, svgLines.join(QChar('\n')), st, startLn, endLn, m_srcLineToBlock, m_blockToSrcLine);
			continue;
		}
		if( !st.inCodeBlock && !st.inHtmlComment && isKeisenFence(line) ) {
			const int startLn = ln;
			const QString fenceLine = line;
			mgr.setLineType(ln, LineMgr::LT_CODE_FENCE);
			QStringList keisenLines;
			while( ++ln < nLines && !isCodeFence(lines[ln]) ) {
				mgr.setLineType(ln, LineMgr::LT_CODE_BLOCK);
				keisenLines.append(lines[ln]);
			}
			if( ln < nLines && isCodeFence(lines[ln]) )
				mgr.setLineType(ln, LineMgr::LT_CODE_FENCE);
			const int endLn = qMin(ln, nLines - 1);
			insertKeisenBlock(cursor, fenceLine, keisenLines, st, startLn, endLn, m_srcLineToBlock, m_blockToSrcLine);
			continue;
		}
		if( !st.inCodeBlock && !st.inHtmlComment ) {
			int boxIndent = 0;
			BoxOptions boxOpt;
			QString boxText;
			if( parseBoxLine(line, boxIndent, boxOpt, boxText) ) {
				const int endLn = parseAndInsertBoxBlock(cursor, lines, lineStarts, ln, st, m_srcLineToBlock, m_blockToSrcLine, &m_syncSpans);
				for( int k = ln; k <= endLn; ++k )
					mgr.setLineType(k, LineMgr::LT_TABLE);
				ln = endLn;
				continue;
			}
		}
		if( !st.inCodeBlock && !st.inHtmlComment && ln + 1 < nLines ) {
			QStringList headerCells;
			QVector<int> headerCols;
			QVector<Qt::Alignment> aligns;
			if( parseGfmTableRow(line, headerCells, &headerCols) && parseGfmTableSeparator(lines[ln + 1], aligns) ) {
				QList<QStringList> rows;
				QVector<int> rowSrcLines;
				QVector<int> rowStartPos;
				QList<QVector<int>> rowCellCols;
				mgr.setLineType(ln, LineMgr::LT_TABLE);
				mgr.setLineType(ln + 1, LineMgr::LT_TABLE);
				rows.append(headerCells);
				rowSrcLines.append(ln);
				rowStartPos.append(lineStarts[ln]);
				rowCellCols.append(headerCols);
				ln += 1;	//	区切り行を消費
				bool tblComment = false;
				while( ln + 1 < nLines ) {
					bool commentOnly = false;
					const QString cleaned = stripHtmlComments(lines[ln + 1], tblComment, commentOnly);
					if( commentOnly ) {
						++ln;
						mgr.setLineType(ln, LineMgr::LT_TABLE);
						continue;
					}
					QStringList rowCells;
					QVector<int> rowCols;
					if( !parseGfmTableRow(cleaned, rowCells, &rowCols) )
						break;
					++ln;
					mgr.setLineType(ln, LineMgr::LT_TABLE);
					rows.append(rowCells);
					rowSrcLines.append(ln);
					rowStartPos.append(lineStarts[ln]);
					rowCellCols.append(rowCols);
				}
				insertStyledTable(cursor, rows, rowSrcLines, rowStartPos, rowCellCols, aligns, st, m_srcLineToBlock, m_blockToSrcLine, &m_syncSpans);
				continue;
			}
		}

		const LineMgr::LineType type = parseAndAppendLine(cursor, line, st, ln, lineStarts[ln], &m_syncSpans);
		mgr.setLineType(ln, type);
		if( st.lastLineEmitted ) {
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
	m_prevSrcLineCount = nLines;
	m_prevSrcCharCount = static_cast<int>(srcDoc->characterCount());
	m_prevHasMultiLineState = st.inCodeBlock || st.inHtmlComment || st.inQuote;
	m_hasImages = st.hasImages;
	m_lastImageMaxWidth = maxWInt;
	doc->setModified(false);
	if( QScrollBar *sb = verticalScrollBar() )
		sb->setValue(qMin(prevScroll, sb->maximum()));
}

static bool isSimpleLineType(LineMgr::LineType t)
{
	return t == LineMgr::LT_BODY || t == LineMgr::LT_HEADING;
}

static bool isSimpleSingleLine(const QString &line)
{
	if( line.contains(QStringLiteral("<!--")) || line.contains(QStringLiteral("-->")) || line.contains(QChar('|')) )
		return false;
	qsizetype i = 0;
	while( i < line.size() && (line[i] == QChar(' ') || line[i] == QChar('\t')) )
		++i;
	const QStringView rest = QStringView(line).mid(i);
	if( rest.isEmpty() )
		return true;
	const QChar c0 = rest[0];
	if( c0 == QChar('`') && rest.startsWith(u"```") )
		return false;
	if( c0 == QChar('>') )
		return false;
	if( c0 == QChar('[') ) {
		int dummyIndent = 0;
		BoxOptions dummyOpt;
		QString dummyText;
		if( parseBoxLine(line, dummyIndent, dummyOpt, dummyText) )
			return false;
	}
	if( (c0 == QChar('-') || c0 == QChar('*') || c0 == QChar('+'))
		&& (rest.size() >= 2 && (rest[1] == QChar(' ') || rest[1] == QChar('\t'))) )
		return false;
	if( (c0 == QChar('-') || c0 == QChar('*') || c0 == QChar('_') || c0 == QChar('=')) && rest.size() >= 3 ) {
		bool allSame = true;
		for( qsizetype k = 0; k < rest.size(); ++k ) {
			if( rest[k] != c0 && rest[k] != QChar(' ') && rest[k] != QChar('\t') ) {
				allSame = false;
				break;
			}
		}
		if( allSame )
			return false;
	}
	if( c0 >= QChar('0') && c0 <= QChar('9') ) {
		qsizetype d = 0;
		while( d < rest.size() && rest[d] >= QChar('0') && rest[d] <= QChar('9') )
			++d;
		if( d < rest.size() && (rest[d] == QChar('.') || rest[d] == QChar(')'))
			&& d + 1 < rest.size() && (rest[d + 1] == QChar(' ') || rest[d + 1] == QChar('\t')) )
			return false;
	}
	return true;
}

void MarkdownPreview::updateMarkdown(TextDocument *srcDoc, int hintSrcLine, bool *outHeadingsChanged)
{
	if( outHeadingsChanged )
		*outHeadingsChanged = true;
	if( !srcDoc ) {
		clear();
		m_srcLineToBlock.clear();
		m_blockToSrcLine.clear();
		m_syncSpans.clear();
		m_prevSrcLineCount = 0;
		m_prevSrcCharCount = 0;
		m_prevHasMultiLineState = false;
		return;
	}

	QTextDocument *doc = document();
	ViewLineMgr &mgr = srcDoc->lineMgr();
	const int nLines = mgr.lineCount();
	const int newTotalSrcChars = static_cast<int>(srcDoc->characterCount());

	//	Fast Path A: 単一行インプレース編集（本文・見出しの1文字入力/削除等）
	if( !m_prevHasMultiLineState
		&& nLines == m_prevSrcLineCount
		&& hintSrcLine >= 0 && hintSrcLine < nLines
		&& m_srcLineToBlock.size() == nLines
		&& isSimpleLineType(mgr.lineType(hintSrcLine))
		&& (hintSrcLine == 0 || isSimpleLineType(mgr.lineType(hintSrcLine - 1))) )
	{
		const int bn = m_srcLineToBlock[hintSrcLine];
		QTextBlock block = (bn >= 0 && bn < doc->blockCount()) ? doc->findBlockByNumber(bn) : QTextBlock();
		if( block.isValid() ) {
			QTextCursor checkCur(block);
			if( !checkCur.currentTable() && !checkCur.currentList() ) {
				QString line = srcDoc->findBlockByNumber(hintSrcLine).text();
				if( line.endsWith(QChar('\n')) ) line.chop(1);
				if( line.endsWith(QChar('\r')) ) line.chop(1);
				if( isSimpleSingleLine(line) ) {
					const QSignalBlocker blocker(verticalScrollBar());
					const QSignalBlocker selfBlocker(this);
					const int prevScroll = verticalScrollBar() ? verticalScrollBar()->value() : 0;

					const LineMgr::LineType oldType = mgr.lineType(hintSrcLine);
					const int oldBlockPos = block.position();
					const int oldBlockTextLen = qMax(0, block.length() - 1);

					QTextCursor cursor(doc);
					cursor.beginEditBlock();
					cursor.setPosition(oldBlockPos);
					cursor.setPosition(oldBlockPos + oldBlockTextLen, QTextCursor::KeepAnchor);
					cursor.removeSelectedText();

					ParseState st;
					st.basePt = doc->defaultFont().pointSizeF() > 0.0 ? doc->defaultFont().pointSizeF() : 10.0;
					const int maxWInt = qMax(0, viewport()->width() - 24);
					st.maxImgWidth = static_cast<qreal>(maxWInt);
					st.tableHeaderColor = m_tableHeaderColor;
					st.tableZebraColor1 = m_tableZebraColor1;
					st.tableZebraColor2 = m_tableZebraColor2;
					st.hasEmittedBlock = false;

					QVector<SyncSpan> newSpans;
					const int lineStartPos = static_cast<int>(mgr.lineStartPos(hintSrcLine));
					const LineMgr::LineType newType = parseAndAppendLine(cursor, line, st, hintSrcLine, lineStartPos, &newSpans);
					cursor.endEditBlock();

					if( st.lastLineEmitted ) {
						const int newBlockTextLen = qMax(0, cursor.block().length() - 1);
						const int deltaPrv = newBlockTextLen - oldBlockTextLen;
						const int deltaSrc = newTotalSrcChars - m_prevSrcCharCount;

						auto itFirst = std::lower_bound(
							m_syncSpans.begin(), m_syncSpans.end(), oldBlockPos,
							[](const SyncSpan &s, int p) { return s.prvPos < p; });
						const int oldBlockEnd = oldBlockPos + oldBlockTextLen;
						auto itEnd = itFirst;
						while( itEnd != m_syncSpans.end() && itEnd->prvPos <= oldBlockEnd )
							++itEnd;

						const qsizetype firstIdx = itFirst - m_syncSpans.begin();
						const qsizetype oldCnt = itEnd - itFirst;
						const qsizetype newCnt = newSpans.size();
						if( oldCnt == newCnt ) {
							for( qsizetype k = 0; k < newCnt; ++k )
								m_syncSpans[firstIdx + k] = newSpans[k];
						} else {
							m_syncSpans.erase(itFirst, itEnd);
							for( qsizetype k = 0; k < newCnt; ++k )
								m_syncSpans.insert(firstIdx + k, newSpans[k]);
						}
						if( deltaSrc != 0 || deltaPrv != 0 ) {
							for( qsizetype k = firstIdx + newCnt; k < m_syncSpans.size(); ++k ) {
								m_syncSpans[k].srcPos += deltaSrc;
								m_syncSpans[k].prvPos += deltaPrv;
							}
						}

						mgr.setLineType(hintSrcLine, newType);
						m_prevSrcCharCount = newTotalSrcChars;
						if( st.hasImages ) m_hasImages = true;
						doc->setModified(false);
						if( QScrollBar *sb = verticalScrollBar() )
							sb->setValue(qMin(prevScroll, sb->maximum()));
						if( outHeadingsChanged )
							*outHeadingsChanged = (oldType == LineMgr::LT_HEADING || newType == LineMgr::LT_HEADING);
						return;
					}
				}
			}
		}
	}

	//	Fast Path B: 任意位置（先頭・途中・末尾）での改行による1行→2行分割
	if( !m_prevHasMultiLineState
		&& nLines == m_prevSrcLineCount + 1
		&& m_prevSrcLineCount > 0
		&& hintSrcLine >= 1 && hintSrcLine < nLines
		&& m_srcLineToBlock.size() == m_prevSrcLineCount )
	{
		const int ln0 = hintSrcLine - 1;
		const int ln1 = hintSrcLine;
		const LineMgr::LineType oldType0 = mgr.lineType(ln0);
		if( isSimpleLineType(oldType0)
			&& (ln0 == 0 || isSimpleLineType(mgr.lineType(ln0 - 1)))
			&& (ln1 + 1 >= nLines || isSimpleLineType(mgr.lineType(ln1 + 1))) )
		{
			const int bn = m_srcLineToBlock[ln0];
			QTextBlock block = (bn >= 0 && bn < doc->blockCount()) ? doc->findBlockByNumber(bn) : QTextBlock();
			if( block.isValid() ) {
				QTextCursor checkCur(block);
				if( !checkCur.currentTable() && !checkCur.currentList() ) {
					QString line0 = srcDoc->findBlockByNumber(ln0).text();
					if( line0.endsWith(QChar('\n')) ) line0.chop(1);
					if( line0.endsWith(QChar('\r')) ) line0.chop(1);
					QString line1 = srcDoc->findBlockByNumber(ln1).text();
					if( line1.endsWith(QChar('\n')) ) line1.chop(1);
					if( line1.endsWith(QChar('\r')) ) line1.chop(1);
					if( isSimpleSingleLine(line0) && isSimpleSingleLine(line1) ) {
						const QSignalBlocker blocker(verticalScrollBar());
						const QSignalBlocker selfBlocker(this);
						const int prevScroll = verticalScrollBar() ? verticalScrollBar()->value() : 0;

						const int oldBlockPos = block.position();
						const int oldBlockTextLen = qMax(0, block.length() - 1);

						QTextCursor cursor(doc);
						cursor.beginEditBlock();
						cursor.setPosition(oldBlockPos);
						cursor.setPosition(oldBlockPos + oldBlockTextLen, QTextCursor::KeepAnchor);
						cursor.removeSelectedText();

						ParseState st;
						st.basePt = doc->defaultFont().pointSizeF() > 0.0 ? doc->defaultFont().pointSizeF() : 10.0;
						const int maxWInt = qMax(0, viewport()->width() - 24);
						st.maxImgWidth = static_cast<qreal>(maxWInt);
						st.tableHeaderColor = m_tableHeaderColor;
						st.tableZebraColor1 = m_tableZebraColor1;
						st.tableZebraColor2 = m_tableZebraColor2;
						st.hasEmittedBlock = false;

						QVector<SyncSpan> newSpans;
						const LineMgr::LineType newType0 = parseAndAppendLine(
							cursor, line0, st, ln0, static_cast<int>(mgr.lineStartPos(ln0)), &newSpans);
						const bool emitted0 = st.lastLineEmitted;
						const int bn0 = cursor.block().blockNumber();
						const int blk0Len = cursor.block().length();

						const LineMgr::LineType newType1 = parseAndAppendLine(
							cursor, line1, st, ln1, static_cast<int>(mgr.lineStartPos(ln1)), &newSpans);
						const bool emitted1 = st.lastLineEmitted;
						const int bn1 = cursor.block().blockNumber();
						const int blk1Len = cursor.block().length();
						cursor.endEditBlock();

						if( emitted0 && emitted1 && bn0 == bn && bn1 == bn + 1 ) {
							const int deltaPrv = (blk0Len + blk1Len - 1) - oldBlockTextLen;
							const int deltaSrc = newTotalSrcChars - m_prevSrcCharCount;

							auto itFirst = std::lower_bound(
								m_syncSpans.begin(), m_syncSpans.end(), oldBlockPos,
								[](const SyncSpan &s, int p) { return s.prvPos < p; });
							const int oldBlockEnd = oldBlockPos + oldBlockTextLen;
							auto itEnd = itFirst;
							while( itEnd != m_syncSpans.end() && itEnd->prvPos <= oldBlockEnd )
								++itEnd;

							const qsizetype firstIdx = itFirst - m_syncSpans.begin();
							const qsizetype oldCnt = itEnd - itFirst;
							const qsizetype newCnt = newSpans.size();
							if( oldCnt == newCnt ) {
								for( qsizetype k = 0; k < newCnt; ++k )
									m_syncSpans[firstIdx + k] = newSpans[k];
							} else {
								m_syncSpans.erase(itFirst, itEnd);
								for( qsizetype k = 0; k < newCnt; ++k )
									m_syncSpans.insert(firstIdx + k, newSpans[k]);
							}
							if( deltaSrc != 0 || deltaPrv != 0 ) {
								for( qsizetype k = firstIdx + newCnt; k < m_syncSpans.size(); ++k ) {
									m_syncSpans[k].srcPos += deltaSrc;
									m_syncSpans[k].prvPos += deltaPrv;
								}
							}

							m_srcLineToBlock.insert(ln1, bn1);
							for( int k = ln1 + 1; k < m_srcLineToBlock.size(); ++k ) {
								if( m_srcLineToBlock[k] >= 0 )
									++m_srcLineToBlock[k];
							}
							if( bn1 <= m_blockToSrcLine.size() )
								m_blockToSrcLine.insert(bn1, ln1);
							else {
								m_blockToSrcLine.resize(bn1 + 1, ln1);
								m_blockToSrcLine[bn1] = ln1;
							}
							for( int k = bn1 + 1; k < m_blockToSrcLine.size(); ++k )
								++m_blockToSrcLine[k];

							mgr.setLineType(ln0, newType0);
							mgr.setLineType(ln1, newType1);
							m_prevSrcLineCount = nLines;
							m_prevSrcCharCount = newTotalSrcChars;
							if( st.hasImages ) m_hasImages = true;
							doc->setModified(false);
							if( QScrollBar *sb = verticalScrollBar() )
								sb->setValue(qMin(prevScroll, sb->maximum()));
							if( outHeadingsChanged )
								*outHeadingsChanged = (oldType0 == LineMgr::LT_HEADING || newType0 == LineMgr::LT_HEADING || newType1 == LineMgr::LT_HEADING);
							return;
						}
					}
				}
			}
		}
	}

	//	Fast Path C: 改行削除・1行削除による2行→1行統合（Backspace / Delete / dd 等）
	if( !m_prevHasMultiLineState
		&& nLines == m_prevSrcLineCount - 1
		&& nLines > 0
		&& hintSrcLine >= 0 && hintSrcLine < nLines
		&& m_srcLineToBlock.size() == m_prevSrcLineCount )
	{
		const int ln0 = hintSrcLine;
		const LineMgr::LineType oldType0 = mgr.lineType(ln0);
		if( isSimpleLineType(oldType0)
			&& (ln0 == 0 || isSimpleLineType(mgr.lineType(ln0 - 1)))
			&& (ln0 + 1 >= nLines || isSimpleLineType(mgr.lineType(ln0 + 1))) )
		{
			const int bn0 = m_srcLineToBlock[ln0];
			const int bn1 = m_srcLineToBlock[ln0 + 1];
			if( bn0 >= 0 && bn1 == bn0 + 1 && bn1 < doc->blockCount() ) {
				QTextBlock block0 = doc->findBlockByNumber(bn0);
				QTextBlock block1 = doc->findBlockByNumber(bn1);
				if( block0.isValid() && block1.isValid() ) {
					QTextCursor c0(block0), c1(block1);
					if( !c0.currentTable() && !c0.currentList() && !c1.currentTable() && !c1.currentList() ) {
						QString line0 = srcDoc->findBlockByNumber(ln0).text();
						if( line0.endsWith(QChar('\n')) ) line0.chop(1);
						if( line0.endsWith(QChar('\r')) ) line0.chop(1);
						if( isSimpleSingleLine(line0) ) {
							const QSignalBlocker blocker(verticalScrollBar());
							const QSignalBlocker selfBlocker(this);
							const int prevScroll = verticalScrollBar() ? verticalScrollBar()->value() : 0;

							const int oldStart = block0.position();
							const int blk0Len = block0.length();
							const int blk1Len = block1.length();
							const int oldEnd = block1.position() + qMax(0, blk1Len - 1);

							QTextCursor cursor(doc);
							cursor.beginEditBlock();
							cursor.setPosition(oldStart);
							cursor.setPosition(oldEnd, QTextCursor::KeepAnchor);
							cursor.removeSelectedText();

							ParseState st;
							st.basePt = doc->defaultFont().pointSizeF() > 0.0 ? doc->defaultFont().pointSizeF() : 10.0;
							const int maxWInt = qMax(0, viewport()->width() - 24);
							st.maxImgWidth = static_cast<qreal>(maxWInt);
							st.tableHeaderColor = m_tableHeaderColor;
							st.tableZebraColor1 = m_tableZebraColor1;
							st.tableZebraColor2 = m_tableZebraColor2;
							st.hasEmittedBlock = false;

							QVector<SyncSpan> newSpans;
							const LineMgr::LineType newType = parseAndAppendLine(
								cursor, line0, st, ln0, static_cast<int>(mgr.lineStartPos(ln0)), &newSpans);
							const int newBlkLen = cursor.block().length();
							cursor.endEditBlock();

							if( st.lastLineEmitted && cursor.block().blockNumber() == bn0 ) {
								const int deltaPrv = newBlkLen - (blk0Len + blk1Len);
								const int deltaSrc = newTotalSrcChars - m_prevSrcCharCount;

								auto itFirst = std::lower_bound(
									m_syncSpans.begin(), m_syncSpans.end(), oldStart,
									[](const SyncSpan &s, int p) { return s.prvPos < p; });
								auto itEnd = itFirst;
								while( itEnd != m_syncSpans.end() && itEnd->prvPos <= oldEnd )
									++itEnd;

								const qsizetype firstIdx = itFirst - m_syncSpans.begin();
								const qsizetype oldCnt = itEnd - itFirst;
								const qsizetype newCnt = newSpans.size();
								if( oldCnt == newCnt ) {
									for( qsizetype k = 0; k < newCnt; ++k )
										m_syncSpans[firstIdx + k] = newSpans[k];
								} else {
									m_syncSpans.erase(itFirst, itEnd);
									for( qsizetype k = 0; k < newCnt; ++k )
										m_syncSpans.insert(firstIdx + k, newSpans[k]);
								}
								if( deltaSrc != 0 || deltaPrv != 0 ) {
									for( qsizetype k = firstIdx + newCnt; k < m_syncSpans.size(); ++k ) {
										m_syncSpans[k].srcPos += deltaSrc;
										m_syncSpans[k].prvPos += deltaPrv;
									}
								}

								m_srcLineToBlock.removeAt(ln0 + 1);
								for( int k = ln0 + 1; k < m_srcLineToBlock.size(); ++k ) {
									if( m_srcLineToBlock[k] >= 0 )
										--m_srcLineToBlock[k];
								}
								if( bn1 < m_blockToSrcLine.size() )
									m_blockToSrcLine.removeAt(bn1);
								for( int k = bn1; k < m_blockToSrcLine.size(); ++k )
									--m_blockToSrcLine[k];

								mgr.setLineType(ln0, newType);
								m_prevSrcLineCount = nLines;
								m_prevSrcCharCount = newTotalSrcChars;
								if( st.hasImages ) m_hasImages = true;
								doc->setModified(false);
								if( QScrollBar *sb = verticalScrollBar() )
									sb->setValue(qMin(prevScroll, sb->maximum()));
								if( outHeadingsChanged )
									*outHeadingsChanged = true;
								return;
							}
						}
					}
				}
			}
		}
	}

	//	フォールバック: 複数行要素・複数行削除・構造変化時は全文再構築
	setMarkdown(srcDoc);
}
