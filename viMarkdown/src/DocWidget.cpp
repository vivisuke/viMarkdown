#include <QTextBlock>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <algorithm>
#include "MarkdownEditor.h"
#include "DocWidget.h"
#include "MarkdownPreview.h"
#include "MainWindow.h"

uchar blockType(const QTextBlock &block);

static void updateMiniMapFromY(MiniMap *mm, qreal y) {
	if( mm->m_docWidget == nullptr || mm->m_docWidget->m_editor == nullptr ) return;
	int mx = mm->m_docWidget->m_editor->verticalScrollBar()->maximum();
	int h = mm->height();
	int targetLine = (mm->m_totalLines > h && h > 0)
		? (int)((qint64)y * mm->m_totalLines / h)
		: (int)y;
	mm->m_firstVisibleLine = qBound(0, targetLine - mm->m_visibleLines / 2, mx);
	mm->m_docWidget->syncEditorWithMinimap(mm->m_firstVisibleLine);
	mm->update();
}
void MiniMap::mousePressEvent(QMouseEvent *event) {
	if( event->button() != Qt::LeftButton ) return;
	m_mousePressed = true;
	updateMiniMapFromY(this, event->position().y());
}
void MiniMap::mouseReleaseEvent(QMouseEvent *event) {
	if( event->button() == Qt::LeftButton )
		m_mousePressed = false;
}
void MiniMap::mouseMoveEvent(QMouseEvent *event) {
	if( !m_mousePressed ) return;
	updateMiniMapFromY(this, event->position().y());
}
void MiniMap::resizeEvent(QResizeEvent *event) {
	QWidget::resizeEvent(event);
	if( m_mapDoc1 != nullptr )		//	diff 未実施のときは再描画しない
		updateMap(m_mapDoc1, m_mapEditor1, m_mapDoc2, m_mapEditor2);
}
//----------------------------------------------------------------------
BlockData* getBlockData(QTextBlock srcBlock, bool init /*, int length*/) {
	assert( srcBlock.isValid() );
	int bn = srcBlock.blockNumber();	//	for Debug
	assert( bn >= 0 );
	QString txt = srcBlock.text();	//	for Debug
	BlockData* data = dynamic_cast<BlockData*>(srcBlock.userData());
	//if( !length )
	int length = srcBlock.text().size();
	if (!data) {
	    data = new BlockData();
		srcBlock.setUserData(data);
		data->m_charFlags.resize(length);
		data->m_charFlags.fill(0);
	} else {
		auto len0 = data->m_charFlags.size();
		data->m_charFlags.resize(length);
		if( init ) {
			data->m_charFlags.fill(0);
		} else {
			if( len0 < length ) {
				for(int i = len0; i < length; ++i)
					data->m_charFlags[i] = 0;
			}
		}
	}
	return data;
}
const QByteArray& getCharFlags(QTextBlock block) {
	const BlockData* data = getBlockData(block);
	return data->m_charFlags;
}
void printCharFlags(QTextBlock block) {
#ifdef _DEBUG
	//qDebug() << "blockNumber = " << block.blockNumber();
	const BlockData* data = getBlockData(block);
	QString bn = QString::number((int)block.blockNumber()) + ": ";
	QString txt;
	for(int i = 0; i < data->m_charFlags.size(); ++i)
		txt += QString::number((int)data->m_charFlags[i]) + u' ';
	//##qDebug() << bn << block.text() << "\t" << txt;
#endif
}
bool parseCsvLine(QStringList &fields, QByteArray& ba, const QString &line, bool inQuotes, bool &inComment, bool &commented, BlockData* data) {
	//QStringList fields;
	commented = false;
	if( !inComment && line.startsWith("```csv", Qt::CaseInsensitive) )
		return inQuotes;
	if( data != nullptr ) {
		assert( line.size() == data->m_charFlags.size() );
		data->m_charFlags.fill(0);
		//##qDebug() << "data != nullptr";
	}
	int i = 0;
	if( inComment ) {
		int ix = line.indexOf("-->");
		if( ix < 0 ) {
			if( data != nullptr ) data->m_charFlags.fill(PCF_COMMENTED);
			return inQuotes;
		}
		if( data != nullptr ) {
			for( int k = 0; k < ix + 3; ++k )
				data->m_charFlags[k] = PCF_COMMENTED;
		}
		inComment = false;
		if( (i = ix + 3) >= line.length() ) {	//	"-->" までスキップ
			commented = fields.isEmpty();
			return inQuotes;
		}

	}
	QString currentField;
	bool quoted = inQuotes;
	if( !inQuotes ) {
		fields.clear();
		ba.clear();
	} else if( !fields.isEmpty() ) {
		currentField = fields.back() + "<br />";	//	for insertMarkdown()
		//currentField = fields.back() + u'\n';		//	for insertText()
		fields.pop_back();
		ba.chop(1);		//ba.pop_back();
	}
	//bool inQuotes = false;
	int ix0 = i;
	for (; i < line.length(); ++i) {
		if( !inQuotes && QStringView(line).mid(i).startsWith(u"<!--") ) {
			int ix = line.indexOf("-->", i + 4);
			if( ix > 0 ) {	//	コメント終了を発見
				if( data != nullptr ) {
					for( int k = i; k < ix + 3; ++k )
						data->m_charFlags[k] = PCF_COMMENTED;
				}
				if( ix + 3 >= line.length() ) {	//	"-->" までスキップ
					if( fields.isEmpty() && currentField.isEmpty() ) {
						commented = true;
						return inQuotes;
					}
					break;
				}
				i = ix + 2;	//	for ループの ++i で ix + 3 へ進める
				continue;
			} else {
				if( data != nullptr ) {
					for( int k = i; k < line.length(); ++k )
						data->m_charFlags[k] = PCF_COMMENTED;
				}
				inComment = true;
				if( fields.isEmpty() && currentField.isEmpty() ) {
					commented = true;
					return inQuotes;
				}
				break;
			}
		}
		QChar c = line.at(i);
		if (c == '"') {
			// ダブルクォートの中のダブルクォート（エスケープ）をチェック
			if (inQuotes && i + 1 < line.length() && line.at(i + 1) == '"') {
				if( data != nullptr )
					data->m_charFlags[i] = PCF_CSV;
				currentField += '"';
				i++; // 1文字飛ばす
			} else if( !inQuotes && currentField.isEmpty() ) {
				if( data != nullptr )
					data->m_charFlags[i] = PCF_CSV;
				inQuotes = true;
				quoted = true;
			} else if( inQuotes ) {
				int next = i + 1;
				while( next < line.length() && line.at(next) == u' ' ) ++next;
				if( next == line.length() || line.at(next) == ',' ) {
					if( data != nullptr ) {
						data->m_charFlags[i] = PCF_CSV;
						for( int k = i + 1; k < next; ++k )
							data->m_charFlags[k] = PCF_NOT_VISIBLE;
					}
					inQuotes = false;
					i = next - 1;	//	for ループの ++i で ',' または行末へ進める
				} else {
					currentField += '"';
				}
			} else {
				currentField += '"';
			}
		} else if (c == ',' && !inQuotes) {		// クォートの外にあるカンマはセパレータ
			if( data != nullptr && !quoted ) {
				for(int k = i; --k >= 0 && line[k] == u' ';) {
					data->m_charFlags[k] = PCF_NOT_VISIBLE;
				}
			}
			fields.append(quoted ? currentField : currentField.trimmed());
			ba.push_back(quoted);
			currentField.clear();
			quoted = false;
			if( data != nullptr ) {
				data->m_charFlags[i] = PCF_CELL_SEPARATOR;
				for(int k = i+1; k < line.size() && line[k] == u' '; ++k)
					data->m_charFlags[k] = PCF_NOT_VISIBLE;
				updateCharFlags(data, line, ix0, i, true);		//	true for エスケープ文字処理
			}
			ix0 = i + 1;
		} else { // 通常の文字
			if( c == u' ' && !inQuotes ) {
				if( currentField.isEmpty() ) {
					if( data != nullptr )
						data->m_charFlags[i] = PCF_NOT_VISIBLE;
				} else
					currentField += c;
			} else
				currentField += c;
		}
	}
	if( data != nullptr && !quoted ) {
		for(int k = i; --k >= 0 && line[k] == u' ';) {
			data->m_charFlags[k] = PCF_NOT_VISIBLE;
		}
	}
	// 最後のフィールドを追加
	fields.append(quoted ? currentField : currentField.trimmed());
	ba.push_back(quoted);
	if( data != nullptr ) {
		updateCharFlags(data, line, ix0, i, true);		//	true for エスケープ文字処理
	}
	return inQuotes;
}
static QRegularExpression link_re(R"((?<![!\\])\[([^\]]+)\]\(([^)]+)\))");
static QRegularExpression image_re(R"((?<!\\)!\[([^\]]+)\]\(([^)]+)\))");
bool isTableLine(const QString& lnStr0, const QString& lnStr, QStringList &tableTokens, BlockData* data) {
	assert( data == nullptr || data->m_charFlags.size() == lnStr0.size() );
	const int flagLen = (data != nullptr) ? qMin((int)data->m_charFlags.size(), (int)lnStr0.size()) : 0;
	QString sv(lnStr);
	tableTokens.clear();
	//if( data != nullptr ) data->m_charFlags.fill(PCF_VISIBLE);	← ここでクリアするとすでに設定されているコメントアウト情報が消えてしまう
	int ix = 0;
	if (ix < sv.size() && sv[ix] == u'|') {
		if( data != nullptr && ix < flagLen )
			data->m_charFlags[ix] = PCF_TABLE;
		++ix;			//	先頭 '|' スキップ
	}
	while( ix < sv.size() && sv[ix] == u' ' ) {
		if( data != nullptr && ix < flagLen )
			data->m_charFlags[ix] = PCF_NOT_VISIBLE;	//	| 直後空白は非表示
		++ix;		//	空白スキップ
	}
	while( ix < sv.size() ) {
		int ix0 = ix;
		while( ix < sv.size() && sv[ix] != u'|' ) {		//	'|' までループ
			if( ix+1 < sv.size() && sv[ix] == u'\\' ) ix += 2;
			else ++ix;
		}
		tableTokens.push_back(sv.mid(ix0, ix-ix0));
		if( data != nullptr && ix0 < flagLen ) {
			const int end0 = qMin(ix, flagLen);
			for(int i = end0; --i >= 0 && lnStr0[i] == u' '; )	//	| 直前空白は非表示
				data->m_charFlags[i] = PCF_NOT_VISIBLE;
			//	注：末尾 '|' は PtoE/EtoP カーソル同期（AutoTest）との整合のため PCF_CELL_SEPARATOR のまま維持
			if( ix < sv.size() && ix < flagLen )
				data->m_charFlags[ix] = PCF_CELL_SEPARATOR;
			for(int i = ix + 1; i < flagLen && lnStr0[i] == u' '; ++i)	//	| 直後空白も非表示
				data->m_charFlags[i] = PCF_NOT_VISIBLE;
			updateCharFlags(data, lnStr0, ix0, end0, true);		//	true for エスケープ文字処理
			auto imatch = image_re.match(lnStr0, ix0);	//	セル内画像にも PCF_IMAGE を設定
			while( imatch.hasMatch() && imatch.capturedStart() < end0 ) {
				int start = imatch.capturedStart();
				int mend = imatch.capturedEnd();
				if( mend <= end0 ) {
					data->m_charFlags[start] = PCF_IMAGE_BEGIN;	//	'!'
					for(int i = start + 1; i < mend; ++i)
						data->m_charFlags[i] = PCF_IMAGE;
				}
				imatch = image_re.match(lnStr0, qMin(mend, end0));
			}
			auto lmatch = link_re.match(lnStr0, ix0);	//	セル内リンクの記号にも PCF_LINK を設定
			while( lmatch.hasMatch() && lmatch.capturedStart() < end0 ) {
				int start = lmatch.capturedStart();
				int mend = lmatch.capturedEnd();
				int ixr = lnStr0.indexOf(u']', start);
				if( mend <= end0 && ixr >= 0 && ixr < mend ) {
					data->m_charFlags[start] = PCF_LINK;	//	'['
					for(int i = ixr; i < mend; ++i)
						data->m_charFlags[i] = PCF_LINK;	//	'](url)' 部分
				}
				lmatch = link_re.match(lnStr0, qMin(mend, end0));
			}
		}
		++ix;			//	'|' スキップ
	}
	return tableTokens.size() > 1;
}
bool isTableHyphenLine(const QString& lnStr, std::vector<char> &tableAlign, BlockData *data) {
	tableAlign.clear();
	QStringView sv(lnStr);
	int ix = 0;
	while( ix < sv.size() && sv[ix] == u' ' ) ++ix;		//	空白スキップ
	if (ix < sv.size() && sv[ix] == u'|') ++ix;			//	先頭 '|' スキップ
	while( ix < sv.size() ) {
		while( ix < sv.size() && sv[ix] == u' ' ) ++ix;		//	空白スキップ
		if( ix >= sv.size() ) break;						//	末尾 '|' 後の空白のみ
		char aln = 0;
		if( sv[ix] == u':' ) { aln = ALIGHN_LEFT; ++ix; }
		int hyphens = 0;
		while( ix < sv.size() && sv[ix] == u'-' ) { ++hyphens; ++ix; }
		if( hyphens == 0 ) return false;					//	ハイフン1個以上必須
		if( ix < sv.size() && sv[ix] == u':' ) { aln |= ALIGHN_RIGHT; ++ix; }
		while( ix < sv.size() && sv[ix] == u' ' ) ++ix;		//	末尾空白スキップ
		if( ix < sv.size() && sv[ix] != u'|' ) return false;
		tableAlign.push_back(aln);
		if( ix < sv.size() ) ++ix;							//	'|' スキップ
	}
	if( tableAlign.size() > 1 ) {
		if( data != nullptr ) {
			assert( data->m_charFlags.size() == lnStr.size() );
			data->m_charFlags.fill(PCF_TABLE);
		}
		return true;
	} else
		return false;
}
bool isUnderlineHeading(const QString &txt) {
	static QRegularExpression re(R"(^(=+|-+)$)");
	return re.match(txt).hasMatch();
}
bool isEscapedChar(QChar ch) {
	ushort u = ch.unicode();
	return (u >= u'!' && u <= u'/') ||		//	!"#$%&'()*+,-./（半角スペースは除外）
			(u >= u':' && u <= u'@') ||		//	:;<=>?@
			(u >= u'[' && u <= u'`') ||		//	[\]^_`
			(u >= u'{' && u <= u'~');		//	{|}~
}


//	ボールド、イタリック、打ち消し線 部分に PCF_EMPHASIZED 設定
bool updateCharFlags(BlockData* data, const QString &buf, int ix, int ix9, bool esc) {
	bool modified = false;
	if( esc ) {
		int i = ix;
		while( (i = buf.indexOf(u'\\', i)) >= 0 && i < ix9 ) {
			if( i + 1 < ix9 && isEscapedChar(buf[i+1]) ) {
				modified = true;
				data->m_charFlags[i] = PCF_ESCAPE;
				i += 2;
			} else
				++i;
		}
	}
	while( ix < ix9 ) {
		if( ix + 1 < ix9 && buf[ix] == u'\\' && isEscapedChar(buf[ix+1]) )
			ix += 2;
		else if( buf[ix] == u'*' || buf[ix] == u'_' || buf[ix] == u'`' || (buf[ix] == u'~' && ix+1 < ix9 && buf[ix+1] == buf[ix]) ) {
			QString sym;
			if( buf[ix] == u'~' ) {
				sym = "~~";
			} else {
				if( ix+1 < ix9 && buf[ix+1] == buf[ix] ) {
					if(ix+2 < ix9 && buf[ix+2] == buf[ix] )
						sym = buf.mid(ix, 3);
					else
						sym = buf.mid(ix, 2);
				} else
					sym = buf[ix];
			}
			// 探索範囲 [ix + sym.size(), ix9) 内でエスケープされていない閉じマーカーを探す
			int ix2 = -1;
			int searchPos = ix + sym.size();
			while( searchPos + sym.size() <= ix9 ) {
				int found = buf.indexOf(sym, searchPos);
				if( found < 0 || found + sym.size() > ix9 ) break;
				if( sym[0] != u'`' ) {
					int bs = 0;
					for( int k = found - 1; k >= ix + sym.size() && buf[k] == u'\\'; --k ) ++bs;
					if( (bs & 1) != 0 ) {
						searchPos = found + 1;
						continue;
					}
				}
				ix2 = found;
				break;
			}
			if( ix2 >= 0 ) {	//	バランスしている場合
				for(int i = 0; i < sym.size(); ++i) {
					data->m_charFlags[ix+i] = PCF_EMPHASIZED;
					data->m_charFlags[ix2+i] = PCF_EMPHASIZED;
					modified = true;
				}
				if( sym[0] == '`' ) {	//	インラインコードの場合
					for(int i = ix + sym.size(); i < ix2; ++i)
						data->m_charFlags[i] = PCF_VISIBLE;
					modified = true;
				} else {
					if( updateCharFlags(data, buf, ix+sym.size(), ix2) )
						modified = true;
				}
				ix = ix2 + sym.size();
			} else
				++ix;
		} else
			++ix;
	}
	return modified;
}
void updateCharFlags(QTextBlock srcBlock) {
	QString buf = srcBlock.text();
	BlockData *data = getBlockData(srcBlock, false);
	bool modified = false;
	//	インラインコード区間（`...`）を先に収集し、コード内／跨ぎのリンク・画像誤判定を防ぐ
	QList<QPair<int, int>> codeSpans;
	for( int i = 0; i < buf.size(); ) {
		if( i + 1 < buf.size() && buf[i] == u'\\' && isEscapedChar(buf[i + 1]) ) {
			i += 2;
		} else if( buf[i] == u'`' ) {
			int n = 1;
			while( i + n < buf.size() && buf[i + n] == u'`' && n < 3 ) ++n;
			QString sym = buf.mid(i, n);
			int ix2 = buf.indexOf(sym, i + n);
			if( ix2 >= 0 ) {
				codeSpans.append({i, ix2 + n});
				i = ix2 + n;
			} else {
				i += n;
			}
		} else {
			++i;
		}
	}
	auto overlapsCodeSpan = [&](int s, int e) {
		for( const auto &sp : codeSpans ) {
			if( s < sp.second && e > sp.first ) return true;
		}
		return false;
	};
	auto match = image_re.match(buf);		//	![title](image.png) を含むか？
	while( match.hasMatch() ) {
		int start = match.capturedStart(); // マッチした最初の位置
	    int length = match.capturedLength(); // マッチした全体の長さ
		if( !overlapsCodeSpan(start, start + length) ) {
			modified = true;
			data->m_charFlags[start] = PCF_IMAGE_BEGIN;
			for(int i = start + 1; i < start + length; ++i)
				data->m_charFlags[i] = PCF_IMAGE;
		}
		match = image_re.match(buf, start + length);
	}
	match = link_re.match(buf);		//	[title](image.png) を含むか？
	while( match.hasMatch() ) {
		int start = match.capturedStart(); // マッチした最初の位置（'['）
	    int length = match.capturedLength(); // マッチした全体の長さ
		if( !overlapsCodeSpan(start, start + length) ) {
			modified = true;
			data->m_charFlags[start] = PCF_LINK;
			int ix = buf.indexOf(']', start);
			if( ix >= 0 ) {
				for(int i = ix; i < start + length && i < data->m_charFlags.size(); ++i)
					data->m_charFlags[i] = PCF_LINK;
			}
		}
		match = link_re.match(buf, start + length);
	}
	for(int i = 0; i < buf.size() && buf[i] == u' '; ++i)
		data->m_charFlags[i] = PCF_ESCAPE;
	for(int i = buf.size(); --i >= 0 && buf[i] == u' '; )
		data->m_charFlags[i] = PCF_ESCAPE;
	// undone: 前後の空白をスキップして処理した方がいいのでは？
#if 0
	int ix = 0;
	while( (ix = buf.indexOf(u'\\', ix)) >= 0 ) {
		if( ix + 1 < buf.size() && isEscapedChar(buf[ix+1]) ) {
			modified = true;
			data->m_charFlags[ix] = PCF_ESCAPE;
			ix += 2;
		} else
			++ix;
	}
#endif
	if( updateCharFlags(data, buf, 0, buf.size(), true) )
		modified = true;
	if( modified ) {
		srcBlock.setUserData(data);
		//##qDebug() << "updateCharFlags(srcBlock)";
		printCharFlags(srcBlock);
	}
}
//----------------------------------------------------------------------
DocWidget::DocWidget(MainWindow* mw, const QString& title, const QString& fullPath, QWidget *parent)
	: m_mainWindow(mw)
	, m_title(title)
	, m_fullPath(fullPath)
	, QWidget(parent)
{
	//connect(this, &DocWidget:onEditorContentsChange);
	m_srcHeadingBlocks.push_back(0);
	m_prvHeadingBlocks.push_back(0);
}
void DocWidget::updatePanes() {
	if( m_diffMode ) {
		m_headerWidget->show();
		m_minimap->show();
		m_preview->hide();
		m_diffview->show();
	} else {
		m_headerWidget->hide();
		m_minimap->hide();
		m_diffview->hide();
		if( m_docType == DocType::Markdown ) {
			m_preview->show();
		} else {
			m_preview->hide();
		}
	}
}
void DocWidget::setMiniMapCurHeight(int vl) {
	m_minimap->m_visibleLines = vl;
	m_minimap->update();
}
void DocWidget::setMiniMapCurPos(int fvl, int vl) {
	m_minimap->m_firstVisibleLine = fvl;
	m_minimap->m_visibleLines = vl;
	m_minimap->update();
}
void DocWidget::removeDummyBlocks() {
	if( !m_diffMode ) return;
	if( m_editor->dummyInserted() ) {
		m_editor->setProcessing(true);
		m_editor->removeAllDummyLines();
		m_editor->setProcessing(false);
	}
	if( m_diffview->dummyInserted() ) {
		m_diffview->setProcessing(true);
		m_diffview->removeAllDummyLines();
		m_diffview->setProcessing(false);
	}
}
bool DocWidget::isModified() const {
	return m_editor->document()->isModified();
}
void DocWidget::setModified(bool b) {
	m_editor->document()->setModified(b);
}
QString DocWidget::getTitle() const {
	QString title;
	QTextDocument *doc = m_editor->document();
	QTextBlock block = doc->begin();
	bool inComment = false;
	while( block.isValid() ) {
		QString txt = block.text().trimmed();
		if( inComment ) {
			int endIx = txt.indexOf("-->");
			if( endIx >= 0 ) {
				inComment = false;
				if( txt.mid(endIx + 3).trimmed().isEmpty() ) {
					block = block.next();
					continue;
				}
				break;
			}
			block = block.next();
			continue;
		}
		if( txt.isEmpty() ) {
			block = block.next();
			continue;
		}
		if( txt.startsWith("<!--") ) {
			int endIx = txt.indexOf("-->", 4);
			if( endIx < 0 ) {
				inComment = true;
				block = block.next();
				continue;
			}
			if( txt.mid(endIx + 3).trimmed().isEmpty() ) {
				block = block.next();
				continue;
			}
		}
		break;
	}
	if( block.isValid() && blockType(block) == BT_HEADING ) {		//	コメントを除いた最初の行が見出し行
		title = block.text();
		int i = 0;
		while( i < title.size() && title[i] == u'#' ) ++i;
		while( i < title.size() && title[i] == u' ' ) ++i;
		title = title.mid(i);
	}
	return title;
}
int DocWidget::previewPosToEditorPos(int pos) {
	return pos;
}
int DocWidget::prvToSrcHeading(int blockNum) {
	assert( m_prvHeadingBlocks.size() == m_srcHeadingBlocks.size() );
	if (m_prvHeadingBlocks.empty()) return 0;
	auto it = std::upper_bound(m_prvHeadingBlocks.begin(), m_prvHeadingBlocks.end(), blockNum);
	if (it == m_prvHeadingBlocks.begin()) return 0;
	size_t ix = std::distance(m_prvHeadingBlocks.begin(), std::prev(it));
	return m_srcHeadingBlocks[ix];
}
int DocWidget::srcToPrvHeading(int blockNum) {
	assert( m_prvHeadingBlocks.size() == m_srcHeadingBlocks.size() );
	if (m_srcHeadingBlocks.empty()) return 0;
	auto it = std::upper_bound(m_srcHeadingBlocks.begin(), m_srcHeadingBlocks.end(), blockNum);
	if (it == m_srcHeadingBlocks.begin()) return 0;
	size_t ix = std::distance(m_srcHeadingBlocks.begin(), std::prev(it));
	return m_prvHeadingBlocks[ix];
}
void DocWidget::setEditorCurPos(int pos) {
	QTextCursor cursor = m_editor->textCursor();
	cursor.setPosition(pos);
	m_editor->setTextCursor(cursor);
	m_editor->ensureCursorVisible();
}
#if 1
void DocWidget::syncScrollFromLeft(int value)
{
    if (m_isSyncingScroll) return;
    if (m_mainWindow->is_doing_diff()) return;
    m_isSyncingScroll = true;
    // 左が末尾までスクロールされたら、右も右自身の末尾に合わせる（最大値の差を吸収）
    QScrollBar *src = m_editor->verticalScrollBar();
    QScrollBar *dst = m_diffview->verticalScrollBar();
    dst->setValue(src->maximum() > 0 && value >= src->maximum() ? dst->maximum() : value);
    m_isSyncingScroll = false;
}

void DocWidget::syncScrollFromRight(int value)
{
    if (m_isSyncingScroll) return;
    if (m_mainWindow->is_doing_diff()) return;
    m_isSyncingScroll = true;
    // 右が末尾までスクロールされたら、左も左自身の末尾に合わせる（最大値の差を吸収）
    QScrollBar *src = m_diffview->verticalScrollBar();
    QScrollBar *dst = m_editor->verticalScrollBar();
    dst->setValue(src->maximum() > 0 && value >= src->maximum() ? dst->maximum() : value);
    m_isSyncingScroll = false;
}
#else
void DocWidget::syncScrollFromLeft(int value)
{
    if (m_isSyncingScroll) return;
    m_isSyncingScroll = true;
    // 左スクロールの値を右スクロールバーにそのまま適用
    m_diffview->verticalScrollBar()->setValue(value);
    m_isSyncingScroll = false;
}

void DocWidget::syncScrollFromRight(int value)
{
    if (m_isSyncingScroll) return;
    m_isSyncingScroll = true;
    // 右スクロールの値を左スクロールバーにそのまま適用
    m_editor->verticalScrollBar()->setValue(value);
    m_isSyncingScroll = false;
}
#endif
void DocWidget::syncMinimapWithEditor(int value) {
	m_minimap->m_firstVisibleLine = value;
	m_minimap->update();
}
void DocWidget::syncEditorWithMinimap(int value) {
	m_editor->verticalScrollBar()->setValue(value);
}
void DocWidget::onEditorContentsChange(int pos, int charsRemoved, int charsAdded) {
#if 0
	qDebug() << "DocWidget::onEditorContentsChange(" << pos << ", rmv: " << charsRemoved << ", add: " << charsAdded << ")";
	qDebug() << "=== DOC RAW TEXT ===";
    qDebug().noquote() << m_editor->document()->toPlainText();
    qDebug() << "====================";
	QTextBlock b = m_editor->document()->begin();
	while( b.isValid() ) {
		qDebug() << "'" << b.text() << "'";
		b = b.next();
	}
    qDebug() << "====================";
#endif
	QElapsedTimer timer;
    timer.start();
	if( m_editor == nullptr || m_preview == nullptr ) return;
	if( m_mainWindow->is_opening_file() ) return;
	if( m_incrementalUpdating || m_diffMode || m_docType != DocType::Markdown ) return;
	QTextCursor cur0 = m_editor->textCursor();
	//##QTimer::singleShot(0, this, [this, pos]() {
		m_incrementalUpdating = true;
		auto incGuard = qScopeGuard([this]() { m_incrementalUpdating = false; });
		QTextBlock block = m_editor->document()->findBlock(pos);
		//	undone: 最初からブロック行の場合＆非ブロック行になった場合対応
		while( block.isValid() && blockType(block) != BT_HEADING )
			block = block.previous();
		QTextBlock edBlock = block.isValid() ? block : m_editor->document()->begin();
		qDebug() << "edBlock.position() = " << edBlock.position();
		int i = 1;		//	ブロックインデックス
		QTextBlock pvBlock = m_preview->document()->begin();		//	edBlock に対応するプレビュー block
		if( edBlock.position() != 0 ) {		//	edBlock が文書先頭でない場合
			int bn = edBlock.blockNumber();
			for(; i < (int)m_srcHeadingBlocks.size(); ++i) {
				if( m_srcHeadingBlocks[i] >= bn ) break;
			}
			if( i < (int)m_prvHeadingBlocks.size() )
				pvBlock = m_preview->document()->findBlockByNumber(m_prvHeadingBlocks[i]);
			else
				pvBlock = m_preview->document()->end();
		}
		qDebug() << "pvBlock.position() = " << pvBlock.position();
		QTextBlock pvEndBlock = i+1 < (int)m_prvHeadingBlocks.size() ? m_preview->document()->findBlockByNumber(m_prvHeadingBlocks[i+1]) : m_preview->document()->end();
		qDebug() << "pvEndBlock.position() = " << pvEndBlock.position() << ", isValid(): " << pvEndBlock.isValid();
		QTextCursor cursor = pvBlock.isValid() ? QTextCursor(pvBlock) : QTextCursor(m_preview->document());
		if( !pvBlock.isValid() )
			cursor.movePosition(QTextCursor::End);
		else if( pvEndBlock.isValid() )
			cursor.setPosition(pvEndBlock.position(), QTextCursor::KeepAnchor);
		else
			cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		qDebug() << "hasSelection: " << cursor.hasSelection();
		cursor.beginEditBlock();
		if( cursor.hasSelection() ) {
			cursor.removeSelectedText();
			m_preview->setTextCursor(cursor);
		}
		//	見出しテーブルは src / prv を常にペアで削除してサイズ整合を保つ
		if( edBlock.position() != 0
		 && i < (int)m_srcHeadingBlocks.size()
		 && i < (int)m_prvHeadingBlocks.size()
		 && m_srcHeadingBlocks[i] == edBlock.blockNumber() ) {
			m_srcHeadingBlocks.erase(m_srcHeadingBlocks.begin() + i);
			m_prvHeadingBlocks.erase(m_prvHeadingBlocks.begin() + i);
		}
#if 1
		QStringList lst;
		block = edBlock;
		lst << block.text();
		while( (block = block.next()).isValid() ) {
			if( blockType(block) == BT_HEADING ) break;
			lst << block.text();
		}
		qDebug() << "lst: " << lst;
		m_preview->insertMarkdown(m_editor->document(), edBlock.blockNumber(), lst.size(), cursor);
#endif
		cursor.endEditBlock();
	//##});
	m_editor->setTextCursor(cur0);
	m_editor->syncPreviewCursorFromEditor();
    qint64 elapsedMs = timer.elapsed();
    qDebug() << "[Benchmark] onEditorContentsChange() > incremental update completed:" << elapsedMs << "ms (" 
             << timer.nsecsElapsed() / 1000.0 << "μs)";
}
