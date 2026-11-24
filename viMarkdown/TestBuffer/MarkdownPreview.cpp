#include "MarkdownPreview.h"
#include "TextDocument.h"

#include <QAbstractTextDocumentLayout>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QImage>
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
#include <QTextTableFormat>
#include <QUrl>
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
void insertInlineFormatted(QTextCursor &cursor, const QString &text, const QTextCharFormat &baseFmt, qreal basePt, qreal maxImgWidth, bool *outHasImages = nullptr)
{
	const qsizetype len = text.size();
	QString plainBuf;
	auto flushPlain = [&]() {
		if( !plainBuf.isEmpty() ) {
			cursor.insertText(plainBuf, baseFmt);
			plainBuf.clear();
		}
	};

	qsizetype i = 0;
	while( i < len ) {
		const QChar ch = text[i];

		//	バックスラッシュエスケープ
		if( ch == QChar('\\') && i + 1 < len && isEscapablePunct(text[i + 1]) ) {
			plainBuf.append(text[i + 1]);
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
				QString code = text.mid(i + tickCount, closePos - (i + tickCount));
				if( code.size() >= 2 && code.front() == QChar(' ') && code.back() == QChar(' ') && !code.trimmed().isEmpty() )
					code = code.mid(1, code.size() - 2);
				QTextCharFormat cf = baseFmt;
				const qreal pt = (baseFmt.fontPointSize() > 0.0) ? baseFmt.fontPointSize() : basePt;
				cf.setFont(codeBlockFont(pt));
				cf.setBackground(QColor(0xEA, 0xEC, 0xEF));
				cursor.insertText(code, cf);
				i = closePos + tickCount;
				continue;
			}
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
					} else {
						QTextCharFormat cf = baseFmt;
						cf.setForeground(QColor(0x88, 0x88, 0x88));
						cursor.insertText(alt.isEmpty() ? src : QStringLiteral("[%1]").arg(alt), cf);
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
					insertInlineFormatted(cursor, label.isEmpty() ? url : label, cf, basePt, maxImgWidth, outHasImages);
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
				insertInlineFormatted(cursor, text.mid(i + 3, closePos - (i + 3)), cf, basePt, maxImgWidth, outHasImages);
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
				insertInlineFormatted(cursor, text.mid(i + 2, closePos - (i + 2)), cf, basePt, maxImgWidth, outHasImages);
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
				insertInlineFormatted(cursor, text.mid(i + 2, closePos - (i + 2)), cf, basePt, maxImgWidth, outHasImages);
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
				insertInlineFormatted(cursor, text.mid(i + 1, closePos - (i + 1)), cf, basePt, maxImgWidth, outHasImages);
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
					insertInlineFormatted(cursor, text.mid(i + 2, closeBrace - (i + 2)), cf, basePt, maxImgWidth, outHasImages);
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
				cursor.insertText(text.mid(i + 1, numEnd - (i + 1)), cf);
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
					insertInlineFormatted(cursor, text.mid(i + 1, closePos - (i + 1)), cf, basePt, maxImgWidth, outHasImages);
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
					insertInlineFormatted(cursor, text.mid(i + 2, closeBrace - (i + 2)), cf, basePt, maxImgWidth, outHasImages);
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
				cursor.insertText(text.mid(i + 1, numEnd - (i + 1)), cf);
				i = numEnd;
				continue;
			}
		}

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

//	GFM テーブルの1行をセル配列に分解（\| はエスケープされたリテラル '|' として扱う）
bool parseGfmTableRow(const QString &line, QStringList &outCells)
{
	outCells.clear();
	const QString trimmed = line.trimmed();
	if( trimmed.isEmpty() )
		return false;
	bool hasUnescapedPipe = false;
	for( qsizetype i = 0; i < trimmed.size(); ++i ) {
		if( trimmed[i] == QChar('\\') ) { ++i; continue; }
		if( trimmed[i] == QChar('|') ) { hasUnescapedPipe = true; break; }
	}
	if( !hasUnescapedPipe )
		return false;

	qsizetype start = 0;
	qsizetype end = trimmed.size();
	if( start < end && trimmed[start] == QChar('|') )
		++start;
	if( end > start && trimmed[end - 1] == QChar('|') ) {
		//	末尾の '|' がエスケープされていない場合のみ除去
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
bool parseCsvLine(const QString &line, bool &inQuotes, QStringList &curFields, QString &curField)
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
		curField.append(QChar('\n'));
		return false;
	}
	curFields.append(curField.trimmed());
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
QString stripHtmlComments(const QString &line, bool &inComment, bool &outCommentOnly)
{
	outCommentOnly = false;
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

LineMgr::LineType parseAndAppendLine(QTextCursor &cursor, const QString &rawLine, ParseState &st, int srcLine = -1)
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
		cursor.insertText(rawLine, cf);
		st.codeHasLines = true;
		return LineMgr::LT_CODE_BLOCK;
	}

	Qt::Alignment lineAlign = Qt::Alignment();
	const bool hasAlign = (!st.inHtmlComment && parseAlignDirective(rawLine, lineAlign));

	bool commentOnly = false;
	const QString line = stripHtmlComments(rawLine, st.inHtmlComment, commentOnly);
	if( commentOnly )
		return LineMgr::LT_BODY;

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
		insertInlineFormatted(cursor, parsedText, cf, st.basePt, st.maxImgWidth, &st.hasImages);
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
		insertInlineFormatted(cursor, parsedText, bodyCharFmt, st.basePt, st.maxImgWidth, &st.hasImages);
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
		insertInlineFormatted(cursor, parsedText, bodyCharFmt, st.basePt, st.maxImgWidth, &st.hasImages);
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
		insertInlineFormatted(cursor, parsedText, bodyCharFmt, st.basePt, st.maxImgWidth, &st.hasImages);
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
		insertInlineFormatted(cursor, parsedText, bodyCharFmt, st.basePt, st.maxImgWidth, &st.hasImages);
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
		applyAlignIfNeeded();
		insertInlineFormatted(cursor, line.mid(textStart), bodyCharFmt, st.basePt, st.maxImgWidth, &st.hasImages);
		return LineMgr::LT_BODY;
	}

	cursor.setBlockFormat(bodyBlockFmt);
	applyAlignIfNeeded();
	insertInlineFormatted(cursor, line, bodyCharFmt, st.basePt, st.maxImgWidth, &st.hasImages);
	return LineMgr::LT_BODY;
}
}

MarkdownPreview::MarkdownPreview(QWidget *parent)
	: QTextEdit(parent)
{
	setReadOnly(true);
	viewport()->setMouseTracking(true);
}

void MarkdownPreview::mouseMoveEvent(QMouseEvent *event)
{
	const QString href = anchorAt(event->pos());
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
			if( img.isNull() || img.width() <= 0 )
				continue;
			const qreal targetW = (img.width() > maxW) ? maxW : static_cast<qreal>(img.width());
			const qreal targetH = img.height() * targetW / img.width();
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

void insertStyledTable(
	QTextCursor &cursor,
	const QList<QStringList> &rows,
	const QVector<int> &rowSrcLines,
	const QVector<Qt::Alignment> &aligns,
	ParseState &st,
	QVector<int> &srcLineToBlock,
	QVector<int> &blockToSrcLine)
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
			insertInlineFormatted(cellCur, cellText, cf, st.basePt, st.maxImgWidth, &st.hasImages);

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

void MarkdownPreview::setTableColors(const QColor &header, const QColor &zebra1, const QColor &zebra2)
{
	if( header.isValid() ) m_tableHeaderColor = header;
	if( zebra1.isValid() ) m_tableZebraColor1 = zebra1;
	if( zebra2.isValid() ) m_tableZebraColor2 = zebra2;
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
	const int maxWInt = qMax(0, viewport()->width() - 24);
	st.maxImgWidth = static_cast<qreal>(maxWInt);
	st.tableHeaderColor = m_tableHeaderColor;
	st.tableZebraColor1 = m_tableZebraColor1;
	st.tableZebraColor2 = m_tableZebraColor2;
	const QStringList rawLines = markdown.split(QChar('\n'));
	const int nLines = static_cast<int>(rawLines.size());
	QStringList lines;
	lines.reserve(nLines);
	for( int i = 0; i < nLines; ++i ) {
		QString s = rawLines[i];
		if( s.endsWith(QChar('\r')) ) s.chop(1);
		lines.append(s);
	}
	m_srcLineToBlock.fill(-1, nLines);
	m_blockToSrcLine.clear();

	for( int ln = 0; ln < nLines; ++ln ) {
		const QString &line = lines[ln];
		if( !st.inCodeBlock && !st.inHtmlComment && isCsvFence(line) ) {
			QList<QStringList> rows;
			QVector<int> rowSrcLines;
			bool inQuotes = false;
			bool csvInComment = false;
			QStringList curFields;
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
				if( parseCsvLine(cleaned, inQuotes, curFields, curField) ) {
					rows.append(curFields);
					rowSrcLines.append(rowStartLine);
				}
			}
			if( inQuotes && !curFields.isEmpty() ) {
				curFields.append(curField.trimmed());
				rows.append(curFields);
				rowSrcLines.append(rowStartLine);
			}
			insertStyledTable(cursor, rows, rowSrcLines, {}, st, m_srcLineToBlock, m_blockToSrcLine);
			continue;
		}
		if( !st.inCodeBlock && !st.inHtmlComment && ln + 1 < nLines ) {
			QStringList headerCells;
			QVector<Qt::Alignment> aligns;
			if( parseGfmTableRow(line, headerCells) && parseGfmTableSeparator(lines[ln + 1], aligns) ) {
				QList<QStringList> rows;
				QVector<int> rowSrcLines;
				rows.append(headerCells);
				rowSrcLines.append(ln);
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
					if( !parseGfmTableRow(cleaned, rowCells) )
						break;
					++ln;
					rows.append(rowCells);
					rowSrcLines.append(ln);
				}
				insertStyledTable(cursor, rows, rowSrcLines, aligns, st, m_srcLineToBlock, m_blockToSrcLine);
				continue;
			}
		}

		parseAndAppendLine(cursor, line, st, ln);
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
		m_hasImages = false;
		m_lastImageMaxWidth = -1;
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
	const int maxWInt = qMax(0, viewport()->width() - 24);
	st.maxImgWidth = static_cast<qreal>(maxWInt);
	st.tableHeaderColor = m_tableHeaderColor;
	st.tableZebraColor1 = m_tableZebraColor1;
	st.tableZebraColor2 = m_tableZebraColor2;
	ViewLineMgr &mgr = srcDoc->lineMgr();
	const int nLines = mgr.lineCount();
	QStringList lines;
	lines.reserve(nLines);
	for( int ln = 0; ln < nLines; ++ln ) {
		QString line = srcDoc->findBlockByNumber(ln).text();
		if( line.endsWith(QChar('\n')) ) line.chop(1);
		if( line.endsWith(QChar('\r')) ) line.chop(1);
		lines.append(line);
	}
	m_srcLineToBlock.fill(-1, nLines);
	m_blockToSrcLine.clear();

	for( int ln = 0; ln < nLines; ++ln ) {
		const QString &line = lines[ln];
		if( !st.inCodeBlock && !st.inHtmlComment && isCsvFence(line) ) {
			mgr.setLineType(ln, LineMgr::LT_CODE_FENCE);
			QList<QStringList> rows;
			QVector<int> rowSrcLines;
			bool inQuotes = false;
			bool csvInComment = false;
			QStringList curFields;
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
				if( parseCsvLine(cleaned, inQuotes, curFields, curField) ) {
					rows.append(curFields);
					rowSrcLines.append(rowStartLine);
				}
			}
			if( ln < nLines && isCodeFence(lines[ln]) )
				mgr.setLineType(ln, LineMgr::LT_CODE_FENCE);
			if( inQuotes && !curFields.isEmpty() ) {
				curFields.append(curField.trimmed());
				rows.append(curFields);
				rowSrcLines.append(rowStartLine);
			}
			insertStyledTable(cursor, rows, rowSrcLines, {}, st, m_srcLineToBlock, m_blockToSrcLine);
			continue;
		}
		if( !st.inCodeBlock && !st.inHtmlComment && ln + 1 < nLines ) {
			QStringList headerCells;
			QVector<Qt::Alignment> aligns;
			if( parseGfmTableRow(line, headerCells) && parseGfmTableSeparator(lines[ln + 1], aligns) ) {
				QList<QStringList> rows;
				QVector<int> rowSrcLines;
				mgr.setLineType(ln, LineMgr::LT_TABLE);
				mgr.setLineType(ln + 1, LineMgr::LT_TABLE);
				rows.append(headerCells);
				rowSrcLines.append(ln);
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
					if( !parseGfmTableRow(cleaned, rowCells) )
						break;
					++ln;
					mgr.setLineType(ln, LineMgr::LT_TABLE);
					rows.append(rowCells);
					rowSrcLines.append(ln);
				}
				insertStyledTable(cursor, rows, rowSrcLines, aligns, st, m_srcLineToBlock, m_blockToSrcLine);
				continue;
			}
		}

		const LineMgr::LineType type = parseAndAppendLine(cursor, line, st, ln);
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
	m_hasImages = st.hasImages;
	m_lastImageMaxWidth = maxWInt;
	doc->setModified(false);
	if( QScrollBar *sb = verticalScrollBar() )
		sb->setValue(qMin(prevScroll, sb->maximum()));
}
