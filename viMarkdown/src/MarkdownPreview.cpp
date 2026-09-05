#include <vector>
#include <QTextDocument>
#include <QTextBlock>
#include <QRegularExpression>
#include <QTextTable>
#include <QTextList>
#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QDir>
#include <QImage>
#include <QSvgRenderer>
#include <QXmlStreamReader>
#include <qpainter.h>
#include <QStatusBar>
#include <assert.h>
#include "MarkdownPreview.h"
#include "MainWindow.h"
#include "DocWidget.h"
#include "lunasvg.h"

#ifdef	Q_OS_WIN
#include <windows.h>
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#endif

using namespace std;

extern Global g;
extern ViStatus gvi;

uchar blockType(const QTextBlock &block);
void setBlockType(QTextBlock block, uchar type);

#if 0
class SvgImage : public QImage {
public:
	SvgImage() : QImage() {}
	SvgImage(int width, int height, QImage::Format format)
		: QImage(width, height, format)
	{}
};
#endif

#if 0
bool isTableLine(const QString& lnStr, QList<QStringView> &tableTokens);
bool isTableLine(const QString& lnStr, QStringList &tableTokens);
bool isTableHyphenLine(const QString& lnStr, std::vector<char> &tableAlign);
#endif


const QChar endOfCell(0xfdd0);		//	セル終端文字
const QChar ZWSP(0x200b);			//	ゼロ幅空白文字

static QRegularExpression re_tailspc(" +$");	//	行末半角空白列

extern CharType getCharType(QChar ch);
extern void drawTextCursor(QWidget *viewport, QPainter& p, QTextCursor cursor, QRect rect, QFontMetrics fontMetrics, bool hasFocus, bool);

MarkdownPreview::MarkdownPreview(const MainWindow *mainWindow, DocWidget *docWidget, QWidget* parent, bool readOnly)
	: m_mainWindow(mainWindow), m_docWidget(docWidget), QTextEdit(parent)
{
	setUndoRedoEnabled(false);		//	Qt > undo/redo ディセーブル
	setReadOnly(readOnly);
	setUndoRedoEnabled(false);
	qDebug() << "doc->isUndoRedoEnabled() = " << document()->isUndoRedoEnabled();
	setFrameStyle(QFrame::NoFrame);
	setCursorWidth(0);	//	標準テキストカーソル非表示
#if 0
	document()->setDefaultStyleSheet(
	    "blockquote {"
	    "    background-color: #f0f8ff;" /* 引用の背景色 */
	    "    padding: 10px;"            /* ★ここで内側パディングを指定！ */
	    "    margin: 10px 0;"           /* 外側の上下マージン */
	    "    border-left: 4px solid #ccc;" /* 左側の縦線（よくある引用の装飾） */
	    "}"
	);
#endif
	//QString css = "hr { height: 1px; border: none; background-color: #333; margin-top: 10px; margin-bottom: 10px; }";
	//document()->setDefaultStyleSheet(css);
	//setStyleSheet("QTextEdit { caret-color: red; }");
	connect(this, &MarkdownPreview::cursorPositionChanged, this, &MarkdownPreview::onCursorPosChanged);
	//connect(ui->action_Cut, &QAction::triggered, this, &MarkdownPreview::onAction_Cut);
	connect(document(), &QTextDocument::contentsChange, this, &MarkdownPreview::onContentsChanged);
	QFont font = this->font();
	font.setPointSize(12);
	setFont(font);
    //m_blinkTimer = new QTimer(this);	// カーソル点滅用タイマーの設定 (500ms)
    //connect(m_blinkTimer, &QTimer::timeout, this, &MarkdownPreview::toggleCursor);
    //m_blinkTimer->start(500);
}
void MarkdownPreview::inputMethodEvent(QInputMethodEvent *event) {
	m_isComposing = !event->preeditString().isEmpty();
	m_commitString = event->commitString();
	//qDebug() << "isComposing = " << m_isComposing << ", IME commitString = " << txt;
	//if( m_isComposing && !txt.isEmpty() )
	//	emit textInserted(txt);
	QTextEdit::inputMethodEvent(event);
}
void MarkdownPreview::onAction_Cut() {
}
void MarkdownPreview::onCursorPosChanged() {
	QTextCursor cursor = this->textCursor();
	m_lastCurBlockText = cursor.block().text();
	viewport()->update();	//	強制再描画
	if( m_procContentsChanged ) return;
	//if (!m_commitString.isEmpty()) return;
	//	カーソル同期処理
	if( m_processing || m_mainWindow->isCursorCyncing() ) return;		//	再入禁止
	m_processing = true;
	m_mainWindow->setCursorCyncing();
	//QTextCursor cursor = this->textCursor();
	auto context = contextAt(cursor.position());
	context.m_srcHBlockNum = m_docWidget->prvToSrcHeading(context.m_prvHBlockNum);
	PosContext acontext;
	if( cursor.hasSelection() ) {
		acontext = contextAt(cursor.anchor());
		acontext.m_srcHBlockNum = m_docWidget->prvToSrcHeading(acontext.m_prvHBlockNum);
	}
	emit posContextChanged(context, acontext);
	m_mainWindow->setCursorCyncing(false);
	m_processing = false;
}
void MarkdownPreview::onContentsChanged(int position, int charsRemoved, int charsAdded) {
	if( m_processing ) return;
	if (m_isComposing) {		//	IME変換中
		if( !m_commitString.isEmpty() )		//	確定文字列がある場合
			emit textInserted(m_commitString);
		return;
	}
	//##qDebug() << "*** MarkdownPreview::onContentsChanged(" << position << ", " << charsRemoved << ", " << charsAdded << ")";
	m_processing = true;
	m_procContentsChanged = true;
	QTextCursor cursor = this->textCursor();
	QTextBlock block = cursor.block();
	const QString &text = block.text();		//	編集後ブロックテキスト
	int cpos = cursor.position();
	int bpos = position - cursor.block().position();	//	ブロック先頭からの編集位置
	QString strAdded = text.mid(bpos, charsAdded);
	if( blockType(block) == BT_DEFAULT && charsAdded != 0 && strAdded.isEmpty() &&
		document()->characterAt(position) == QChar::ParagraphSeparator )
	{
		strAdded = "\n";
		//strAdded = "  \n";
	}
	QString strRemoved = m_lastCurBlockText.mid(bpos, charsRemoved);
	charsRemoved = qMin(charsRemoved, strRemoved.size());		//	行末を超えている場合対応
	charsAdded = qMin(charsAdded, strAdded.size());		//	行末を超えている場合対応
	int c = 0;
	while(charsAdded-c-1 >= 0 && charsRemoved-c-1 >= 0 &&
		strAdded[charsAdded-c-1] == strRemoved[charsRemoved-c-1] )
	{
		++c;	//	末尾共通部分
	}
	charsRemoved -= c;
	charsAdded -= c;
	//	先頭共通部分削除
	while( !strAdded.isEmpty() && !strRemoved.isEmpty() && strAdded[0] == strRemoved[0] ) {
		--charsRemoved;
		--charsAdded;
		++position;
		strAdded = strAdded.mid(1);
		strRemoved = strRemoved.mid(1);
	}
	if( charsAdded > 0 ) {
		QTextCursor cursor = this->textCursor();
		int pos0 = cursor.position();		//	文字挿入後プレビューカーソル位置
		cursor.setPosition(position);
		QTextBlock block = cursor.block();
		QString addedStr = block.text().mid(cursor.position() - block.position(), charsAdded);
		addedStr.replace("\\", "\\\\").replace("#", "\\#").replace("-", "\\-").replace("<", "\\<")
				.replace("`", "\\`").replace("[", "\\[").replace("*", "\\*").replace("_", "\\_").replace("~", "\\~");
		if( blockType(block) == BT_CELL_DQ )
			addedStr.replace("\"", "\"\"");
		if( addedStr.isEmpty() && charsRemoved == 0 && charsAdded == 1  )
		{		//	改行が入力された場合
			//if( blockType(block) == BT_DEFAULT )
			//	addedStr = "  \n";
			//else
				addedStr = "\n";
			++pos0;
		}
		//##qDebug() << "addedStr = " << addedStr << ", pos0 = " << pos0;
		emit textInserted(addedStr);
		cursor.setPosition(pos0);
		setTextCursor(cursor);
	}
	if( charsRemoved > 0 ) {
		emit textRemoved(charsRemoved);
	}
	m_procContentsChanged = false;
	m_processing = false;
	((MainWindow*)m_mainWindow)->syncEditorToPreviewCursor();	//	暫定的、undone: シグナルスロットに変更
}

//bool isUnderlineHeading(const QString& txt);
#if 1
#else
bool MarkdownPreview::isTableLine(const QString& lnStr) {
	QStringView sv(lnStr);
	m_tableTokens.clear();
	int ix = 0;
	while( ix < sv.size() && sv[ix] == u' ' ) ++ix;		//	空白スキップ
	if (ix < sv.size() && sv[ix] == u'|') ++ix;			//	先頭の '|' スキップ
	while( ix < sv.size() ) {
		int ix0 = ix;
		while( ix < sv.size() && sv[ix] != u'|' ) {		//	'|' までループ
			if( ix+1 < sv.size() && sv[ix] == u'\\' ) ix += 2;
			else ++ix;
		}
		m_tableTokens.push_back(sv.mid(ix0, ix-ix0));
		++ix;			//	'|' スキップ
	}
	return m_tableTokens.size() > 1;
}
bool MarkdownPreview::isTableHyphenLine(const QString& lnStr) {
	m_tableAlign.clear();
	QStringView sv(lnStr);
	int ix = 0;
	while( ix < sv.size() && sv[ix] == u' ' ) ++ix;		//	空白スキップ
	if (ix < sv.size() && sv[ix] == u'|') ++ix;			//	先頭の '|' スキップ
	while( ix < sv.size() ) {
		char aln = 0;
		while( ix < sv.size() && sv[ix] == u' ' ) ++ix;		//	空白スキップ
		if( ix < sv.size() && sv[ix] == u':' ) aln = ALIGHN_LEFT;
		while( ix < sv.size() && sv[ix] != u'|' ) {		//	次の'|' までループ
			if( ix+1 < sv.size() && sv[ix] == u'\\' ) ix += 2;
			else ++ix;
		}
		int i = ix - 1;
		while( i >= 0 && sv[i] == u' ' ) --i;		//	空白スキップ
		if( i >= 0 && sv[i] == u':' )
			aln |= ALIGHN_RIGHT;
		m_tableAlign.push_back(aln);
		++ix;
	}
	return m_tableAlign.size() > 1;
}
#endif

void MarkdownPreview::focusInEvent(QFocusEvent *e) {
	if( gvi.m_currentMode == ViMode::CommandLine )
		((MainWindow*)m_mainWindow)->close_cmdLine();
	QTextEdit::focusInEvent(e);
}
void MarkdownPreview::keyPressEvent(QKeyEvent *e) {
	QTextCursor cursor = textCursor();
	if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {		//	改行入力
#if 0
		if( gvi.m_currentMode == ViMode::Normal ) {
			emit do_viCmd(u'\n', cursor);
			setTextCursor(cursor);
		} else
#endif
		{
			emit Enter_pressed();
		}
		return;
	}
	if (e->key() == Qt::Key_Escape) {
#if 0
		if( g.m_viKeybindings ) {
			//if( g.m_editBlockOpen ) {
			//	cursor.endEditBlock();
			//	g.m_editBlockOpen = false;
			//}
			gvi.m_currentMode = ViMode::Normal;
			//gvi.m_viCmdMode = true;
		}
#endif
		//gvi.m_viCmdMode = true;
		//QTextCursor cursor = textCursor();
		if( cursor.hasSelection() ) {	//	選択状態ならば
			cursor.setPosition(cursor.position());		//	選択解除
			setTextCursor(cursor);
		}
#ifdef Q_OS_WIN
		HWND hwnd = (HWND)this->winId();
		HIMC himc = ImmGetContext(hwnd);
		if( himc ) {
			ImmSetOpenStatus(himc, FALSE);
			ImmReleaseContext(hwnd, himc);
		}
#endif
		return;
	}
	if (e->key() == Qt::Key_Tab) {
		emit Tab_pressed();
		return;
	}
	if (e->key() == Qt::Key_Backspace) {
		emit BS_pressed((e->modifiers() & Qt::ControlModifier) != 0);
		return;
	}
	if (e->key() == Qt::Key_Delete) {
		emit Del_pressed((e->modifiers() & Qt::ControlModifier) != 0);
		return;
	}
	if( (e->modifiers() & Qt::ControlModifier) != 0 ) {		//	Ctrl +
		bool shift = (e->modifiers() & Qt::ShiftModifier) != 0;
		if (e->key() == Qt::Key_Right ) {
			//QTextCursor cursor = textCursor();
			moveToNextWord(cursor, shift);
			setTextCursor(cursor);
			return;
		} else if (e->key() == Qt::Key_Left) {
			//QTextCursor cursor = textCursor();
			moveToPrevWord(cursor, shift);
			setTextCursor(cursor);
			return;
		}
		if (e->key() == Qt::Key_X ) {
			emit cut_triggered();
			return;
		}
		if (e->key() == Qt::Key_Z ) {
			emit undo_triggered();
			return;
		}
		if (e->key() == Qt::Key_Y ) {
			emit redo_triggered();
			return;
		}
	} else {	//	NOT Ctrl +
		if (e->key() == Qt::Key_Right || e->key() == Qt::Key_Down) {
			QTextEdit::keyPressEvent(e);
			QTextCursor cursor = textCursor();
			QTextBlock block = cursor.block();
			//auto text = block.text();
			if( blockType(block) == BT_TABLE /*&& !(block.previous().isValid() && block.previous().userState() == BT_CELL)*/ ) {
				//block = block.next();
				//auto text2 = block.text();
				//cursor.setPosition(block.position());
				cursor.movePosition(QTextCursor::Right);
				setTextCursor(cursor);
			}
			return;
		}
		if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Up) {
			QTextEdit::keyPressEvent(e);
			QTextCursor cursor = textCursor();
			QTextBlock block = cursor.block();
			if( blockType(block) == BT_TABLE /*&& block.previous().isValid() && block.previous().userState() == BT_CELL*/ ) {
				cursor.movePosition(QTextCursor::Left);
				setTextCursor(cursor);
			}
			return;
		}
	}
#if PREVIEW_VICMD
	if( gvi.m_currentMode == ViMode::Normal ) {
		QString txt = e->text();
		//QTextCursor cursor = textCursor();
		if( !txt.isEmpty() ) {
			emit do_viCmd(txt[0], cursor);
			setTextCursor(cursor);
			return;
		}
	}
#endif
	QTextEdit::keyPressEvent(e);	// 通常キーは通常通りの処理
}
void MarkdownPreview::moveToNextWord(QTextCursor& cursor, bool shift) {
	int pos = cursor.position();
	QTextDocument *doc = document();
	if (pos >= doc->characterCount() - 1) return;
	CharType startType = getCharType(doc->characterAt(pos));
	// 同じ種別の間は進む
	while (pos < doc->characterCount() - 1 && getCharType(doc->characterAt(pos)) == startType) {
		pos++;
	}
	if( startType != Type_Space ) {
		while (pos < doc->characterCount() - 1 && getCharType(doc->characterAt(pos)) == Type_Space) {
			pos++;
		}
	}
	cursor.setPosition(pos, shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
}
void MarkdownPreview::moveToNextWordEnd(QTextCursor& cursor, bool shift) {
	int pos = cursor.position();
	QTextDocument *doc = document();
	const int maxPos = doc->characterCount() - 1;
	if (pos >= maxPos) return;
	pos++;
	while (pos < maxPos && getCharType(doc->characterAt(pos)) == Type_Space) {
		pos++;
	}
	if (pos >= maxPos) {
		cursor.setPosition(maxPos, shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
		return;
	}
	CharType currentType = getCharType(doc->characterAt(pos));
	while (pos < maxPos && getCharType(doc->characterAt(pos)) == currentType) {
		pos++;
	}
	if (pos > 0) --pos;
	cursor.setPosition(pos, shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
}
void MarkdownPreview::moveToPrevWord(QTextCursor& cursor, bool shift) {
	int pos = cursor.position();
	QTextDocument *doc = document();
	if (pos <= 0) return;
	CharType startType = getCharType(doc->characterAt(pos - 1));
	while (pos > 0 && getCharType(doc->characterAt(pos - 1)) == startType) {
		pos--;
	}
	if( startType == Type_Space ) {
		CharType startType = getCharType(doc->characterAt(pos - 1));
		while (pos > 0 && getCharType(doc->characterAt(pos - 1)) == startType) {
			pos--;
		}
	}
	cursor.setPosition(pos, shift ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
}
QString splitName(QString& anchor) {
	QString name;
	int ix = anchor.indexOf(u'#');
	if( ix >= 0 ) {
		name = anchor.mid(ix+1);
		anchor = anchor.left(ix);
	}
	return name;
}
QString anchorToFullPath(const QString &anchor) {
	if( anchor.isEmpty() )		//	文書内ジャンプ
		return anchor;
	QString fullPath = anchor;
	if( fullPath.startsWith("http://") || fullPath.startsWith("https://") )
		return fullPath;
	if( fullPath.startsWith("file://") ) {
		fullPath = fullPath.mid(strlen("file://"));
		static QRegularExpression re("^/[A-Za-z]:");
		if( fullPath.indexOf(re) == 0 )
			fullPath = fullPath.mid(strlen("/"));
	}
	if( !anchor.endsWith(".md", Qt::CaseInsensitive) && !QFile::exists(anchor) )
		fullPath += ".md";
	fullPath = QDir::cleanPath(QDir::current().absoluteFilePath(fullPath));
	return fullPath;
}
void MarkdownPreview::mousePressEvent(QMouseEvent *me) {
	m_processing = true;
	QTextEdit::mousePressEvent(me);
	//onCursorPosChanged();
}
void MarkdownPreview::mouseMoveEvent(QMouseEvent *me) {
	QString anchor = anchorAt(me->pos());
	QString name = splitName(anchor);
	//qDebug() << "anchor = " << anchor << ", name = " << name;
	if (!anchor.isEmpty() || !name.isEmpty() ) {	// リンクの上なら指差し
		viewport()->setCursor(Qt::PointingHandCursor);
		QString fullPath = anchorToFullPath(anchor);
		m_mainWindow->statusBar()->showMessage(fullPath);
	} else {	// それ以外なら通常（I型または矢印）
		viewport()->setCursor(Qt::IBeamCursor);
	}
	QTextEdit::mouseMoveEvent(me);
}
#if 0
int MarkdownPreview::countCheckBox(QTextBlock block) {
	int count = 1;
	while( (block = block.previous()).isValid() ) {
		auto t = blockType(block);
		if( t == BT_HEADING ) break;
		if( t == BT_CSV_BLOCK ) ++count;
	}
	return count;
}
#endif
void MarkdownPreview::mouseReleaseEvent(QMouseEvent *me)
{
	if (me->button() == Qt::LeftButton) {
		QString anchor = anchorAt(me->pos());
		QString name = splitName(anchor);
		QTextCursor cursor = cursorForPosition(me->pos());
		QTextBlock block = cursor.block();
		if( blockType(block) == BT_TABLE ) {	//	QTextTable 前後ダミー
			if( block.previous().isValid() && blockType(block.previous()) == BT_CELL ) {
				cursor.movePosition(QTextCursor::Left);
				setTextCursor(cursor);
				block = block.previous();
			} else {
				cursor.movePosition(QTextCursor::Right);
				setTextCursor(cursor);
				block = block.next();
			}
		}
		if (!anchor.isEmpty() || !name.isEmpty() ) {	// リンク上
			//qDebug() << "anchor = " << anchor;
			QString fullPath = anchor;
			if(!anchor.isEmpty() ) {
				fullPath = anchorToFullPath(anchor);
			}
			emit anchorClicked("", fullPath, name);
			QTextEdit::mouseReleaseEvent(me);
			m_processing = false;
			return;
		} else {
			if (blockType(block) == BT_CHECKBOX) {
				QTextEdit::mouseReleaseEvent(me);
				m_processing = false;
				onCursorPosChanged();
				QTextBlockFormat fmt = block.blockFormat();
				bool checked = fmt.marker() == QTextBlockFormat::MarkerType::Checked;
				//##qDebug() << "checked = " << checked;
				//emit lineClicked(block.blockNumber());
				//int nth = countCheckBox(block);
				emit checkboxLineClicked(/*nth,*/ checked);
				return;
			}
#if 0
			QTextCharFormat format = cursor.charFormat();
			if( format.isAnchor() ) {
				qDebug() << "checkbox is clicked";
			}
			emit lineClicked(cursor.block().userState());
#endif
		}
	}
	QTextEdit::mouseReleaseEvent(me);
	m_processing = false;
	onCursorPosChanged();
}
void MarkdownPreview::mouseDoubleClickEvent(QMouseEvent *e) {
	QTextCursor cursor = cursorForPosition(e->pos());
	int pos = cursor.position();
	QTextDocument *doc = document();
	CharType type = getCharType(doc->characterAt(pos));
	// 前方（左）への探索
	int start = pos;
	while (start > 0 && getCharType(doc->characterAt(start - 1)) == type) {
		start--;
	}
	// 後方（右）への探索
	int end = pos;
	while (end < doc->characterCount() - 1 && getCharType(doc->characterAt(end)) == type) {
		end++;
	}
	// 選択範囲を設定
	cursor.setPosition(start);
	cursor.setPosition(end, QTextCursor::KeepAnchor);
	setTextCursor(cursor);
}
#if 1
void MarkdownPreview::wheelEvent(QWheelEvent *event) {
	//##qDebug() << "MarkdownPreview::wheelEvent()";
	//##qDebug() << "e->angleDelta() = " << event->angleDelta();
	if ((event->modifiers() & Qt::ControlModifier) != 0)  {
		emit fontSizeChanged(event->angleDelta().y());
		//if (event->angleDelta().y() > 0)
        //    zoomIn();
        //else
        //    zoomOut();
		event->accept();
	} else
		QTextEdit::wheelEvent(event);
}
#endif
//void MarkdownPreview::toggleCursor() {
//	m_cursorVisible = !m_cursorVisible;
//    this->viewport()->update(); 
//}
void MarkdownPreview::paintEvent(QPaintEvent *e) {
	QTextEdit::paintEvent(e); // 先にテキストを普通に描画
#if 1
	QPainter p(viewport());
	if( !isReadOnly() ) {
		QRect rect = cursorRect();
		QTextCursor cursor = textCursor();
		QTextCharFormat fmt = cursor.charFormat();
		QFont font = fmt.font();
		QFontMetrics fm(font);
		drawTextCursor(viewport(), p, textCursor(), rect, fm, hasFocus(), false);		//	カーソル描画
#if 0
		if( gvi.m_viCmdMode ) {
			auto ht = rect.height();
			rect.setY(rect.y() + ht/2);
			rect.setHeight(ht - ht/2);
			QTextCursor cursor = textCursor();
			QChar ch = document()->characterAt(cursor.position());
			if (ch.isNull() || ch < ' ') ch = 'W'; 
			rect.setWidth(fontMetrics().horizontalAdvance(ch));
		} else
			rect.setWidth(3);
		auto col = hasFocus() ? g.m_activeLnColor : g.m_inactiveLnColor;
		if( !hasFocus() || m_cursorVisible ) {
			p.setPen(col);
			p.setBrush(col);
			p.drawRect(rect);
		}
		QPen pen(col, 1); // 色、太さ1px
		if( !hasFocus() ) pen.setStyle(Qt::DashLine);	//	破線
		p.setPen(pen);
		int y = rect.bottom();
		int left = 0;	//-lnAreaWidth();
		int right = viewport()->width();
		p.drawLine(left, y, right, y);
#endif
	}
#endif
}

void MarkdownPreview::do_body(QTextBlock srcBlock, QTextCursor& cursor, bool last) {
	m_isPrevLineEmpty = false;
	if( m_bodyList.isEmpty() ) return;
	//##qDebug() << "srcBlock.blockNumber() = " << srcBlock.blockNumber();
#if 0	//	コモンマークダウン方式
	static QRegularExpression re("(?<!!)\\[([^\\]]+)\\]\\(([^)]+)\\)");		//	[タイトル](パス#見出し)
	QString buf;
	for(auto txt: m_bodyList) {
		txt.replace(re, "<a href=\"\\2\">\\1</a>");
		buf += txt + "\n";
	}
	if( !buf.isEmpty() ) {
		//QTextBlock block = cursor.block();
		QTextBlockFormat blockFormat;
		blockFormat.setAlignment(Qt::AlignJustify); // 左右両端揃え
		cursor.mergeBlockFormat(blockFormat);
		cursor.insertMarkdown(buf);
	}
#else	//	GFM 方式
	QTextBlockFormat blockFormat;
	blockFormat.setAlignment(Qt::AlignJustify); // 左右両端揃え
	bool blankLine = false;
	//QTextBlock b = block0;
	for(auto txt: m_bodyList) {
		if( txt.isEmpty() ) {
			if( blankLine )
				continue;		//	連続する空行は無視
			blankLine = true;
		} else
			blankLine = false;
		txt.replace(re_tailspc, "&nbsp;");
		cursor.mergeBlockFormat(blockFormat);
		cursor.insertMarkdown(txt);
		cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
	}
#endif
	//cursor.setBlockFormat(QTextBlockFormat());				//	フォーマットリセット
	//cursor.setCharFormat(QTextCharFormat());
	m_isPrevLineEmpty = m_bodyList.back().isEmpty();	//	最後が空行か？
	m_bodyList.clear();
}

static QRegularExpression re_list(R"(^ *[-\*+] )");
static QRegularExpression re_block(R"(^ *[#`\d])");
static QRegularExpression re_numlist(R"(^(\s*)(\d+)([.)]) )");

int indexOfComment(QStringView buf, int start) {
	for(int i = start; i < buf.size(); ++i) {
		if( buf[i] == u'\\' && i+1 < buf.size() )	//	エスケープされた文字をスキップ
			++i;
		else if( buf.mid(i).startsWith(u"<!--") )
			return i;
	}
	return -1;
}
//void updateCharFlags(QTextBlock srcBlock);
void MarkdownPreview::setMarkdown(QTextDocument *doc) {		//	doc: markdown ソースドキュメント
	qDebug() << "MarkdownPreview::setMarkdown(): cursor.position = " << textCursor().position();
	m_headingList.clear();
	m_docWidget->m_srcHeadingBlocks.clear();
	m_docWidget->m_prvHeadingBlocks.clear();
	m_docWidget->m_srcHeadingBlocks.push_back(0);
	m_docWidget->m_prvHeadingBlocks.push_back(0);
	QString mdtext = doc->toPlainText();
	QList<QStringView> tableTokens;
	vector<char> tableAlign;

	m_processing = true;
	this->clear();
	QTextFrame *root = this->document()->rootFrame();
	QTextFrameFormat rformat = root->frameFormat();
	rformat.setTopMargin(9);
	rformat.setBottomMargin(9);
	rformat.setLeftMargin(9);
	rformat.setRightMargin(9);
	root->setFrameFormat(rformat);
	m_bodyList.clear();
	QTextCursor cursor(this->document());
	cursor.beginEditBlock();
	cursor.movePosition(QTextCursor::Start);
	m_lst = mdtext.split(u'\n');
	insertMarkdown(doc, cursor, m_lst);
	m_processing = false;
}
void MarkdownPreview::insertMarkdown(QTextDocument *doc, QTextCursor& cursor, const QStringList& lst) {
	//m_nEmptyLines = 0;
	m_inComment = false;
	QTextBlock srcBlock0;
	for(m_ln = 0; m_ln < lst.size(); ++m_ln) {
		bool bComment = false;		//	コメントがあった
		//QString buf = lst[m_ln];
		QTextBlock srcBlock = doc->findBlockByNumber(m_ln);
		if( !srcBlock.isVisible() ) continue;
		if (!srcBlock.isValid()) break;
		QString buf = srcBlock.text();
		BlockData *data = getBlockData(srcBlock, true);			//	true for 初期化
		const QString buf0 = buf;
		if( !m_inComment ) {
			setBlockType(srcBlock, BT_DEFAULT);
			//BlockData *data = getBlockData(srcBlock);
			int start = 0, ix;
			QString buf2;
			bool modified = false;
			while( (ix = indexOfComment(buf, start)) >= 0 ) {	//	"<!--" 有り
				bComment = true;
				modified = true;
				int ix2 = buf.indexOf("-->", ix + 4);
				if( ix2 < 0 ) {		//	"-->" 無し
					for(int i = ix; i < data->m_charFlags.size(); ++i)
						data->m_charFlags[i] = PCF_COMMENTED;
					buf2 += buf.mid(start, ix-start);
					m_inComment = true;
					break;
				} else {			//	"-->" 有り
					buf2 += buf.mid(start, ix-start) + buf.mid(ix2+3);
					for(int i = ix; i < ix2+3; ++i)
						data->m_charFlags[i] = PCF_COMMENTED;
					start = ix2+3;
				}
			}
			if( modified ) buf = buf2;
			srcBlock.setUserData(data);
			if( bComment && buf.isEmpty() ) continue;
		} else {
			setBlockType(srcBlock, BT_IN_COMMENT);
			//BlockData *data = getBlockData(srcBlock);
			int ix = buf.indexOf("-->");
			if( ix < 0 ) {
				data->m_charFlags.fill(PCF_COMMENTED);
				srcBlock.setUserData(data);
				continue;
			}
			for(int i = 0; i < ix + 3; ++i)
				data->m_charFlags[i] = PCF_COMMENTED;
			srcBlock.setUserData(data);
			m_inComment = false;
			buf = buf.mid(ix+3);
			if( buf.isEmpty() ) continue;
		}
		m_nSpaces = 0;
		while( m_nSpaces < buf.size() && buf[m_nSpaces] == u' ' ) ++m_nSpaces;
		if( m_nSpaces > 0 ) buf = buf.mid(m_nSpaces);
		//BlockData *data = getBlockData(srcBlock, /*init=*/true);	//	初期化
		//BlockData *data = getBlockData(srcBlock);
		BlockData *data2 = nullptr;
		if( m_ln + 1 < lst.size() && srcBlock.next().isValid())
			data2 = getBlockData(srcBlock.next());
		if( buf.startsWith('#') ) {
			do_body(srcBlock0, cursor);
			do_heading(srcBlock, cursor, buf);
		} else if( re_list.match(buf).hasMatch() ) {
			do_body(srcBlock0, cursor);
			do_list(srcBlock, cursor, buf);		//	"- " or "- [ ] "
		} else if( re_numlist.match(buf).hasMatch() ) {
			do_body(srcBlock0, cursor);
			do_numlist(srcBlock, cursor, buf);
		} else if( buf.startsWith("> ") ) {
			do_body(srcBlock0, cursor);
			do_quote(srcBlock, cursor, buf);
		} else if( buf.startsWith("```CSV", Qt::CaseInsensitive) ) {
			do_body(srcBlock0, cursor);
			do_CSV(srcBlock, cursor);
		} else if( buf.startsWith("```keisen", Qt::CaseInsensitive) ) {
			do_body(srcBlock0, cursor);
			do_keisen_block(srcBlock, cursor);
		} else if( buf.startsWith("```SVG", Qt::CaseInsensitive) ) {
			do_body(srcBlock0, cursor);
			do_SVG(srcBlock, cursor);
		} else if( buf.startsWith("```") ) {
			do_body(srcBlock0, cursor);
			do_code(srcBlock, cursor);
		} else if( isTableLine(buf0, buf, m_tableTokens /*, data*/) && m_ln + 1 < lst.size() && isTableHyphenLine(lst[m_ln+1], m_tableAlign, data2) ) {
			do_body(srcBlock0, cursor);
			do_table(srcBlock, cursor);
		} else {
			//printCharFlags(srcBlock);
			if( isUnderlineHeading(buf) && do_underlineHeading(cursor, buf) )
				continue;		//	アンダーライン見出しだった場合
			if( m_bodyList.isEmpty() ) {
				m_bodyLineNum = m_ln;
				srcBlock0 = srcBlock;
			}
			//printCharFlags(srcBlock);
			updateCharFlags(srcBlock);
			//printCharFlags(srcBlock);
			m_bodyList += buf;
		}
	}
	QTextBlock srcBlock = doc->findBlockByNumber(m_bodyLineNum);
	do_body(srcBlock, cursor, true);
	cursor.endEditBlock();
	//##qDebug() << "MarkdownPreview::setMarkdown(): cursor.position = " << textCursor().position();
}
void insertTable(QTextCursor& cursor, const QList<QStringList> &ll, const QList<QByteArray> &lba,
					int max_clmn, vector<char> *tableAlign = nullptr)
{
	static QRegularExpression numbers_re("^[+-]?(\\d+\\.\\d*|\\d+|\\.\\d+)%?$");
	QTextTable *table = cursor.insertTable(ll.size(), max_clmn);
	for(int row = 0; row < ll.size(); ++row) {
		for(int col = 0; col < ll[row].size(); ++col) {
			QTextTableCell cell = table->cellAt(row, col);
			if (cell.isValid()) {
				for (QTextFrame::iterator it = cell.begin(); !(it.atEnd()); ++it) {
					QTextBlock block = it.currentBlock();
					if( block.isValid() ) {
						if( !lba.isEmpty() && lba[row][col] )
							block.setUserState(BT_CELL_DQ);
						else
							block.setUserState(BT_CELL);
					}
				}
				QTextCursor cellCursor = cell.firstCursorPosition();
				QTextCharFormat charFormat;
				charFormat.setForeground(g.m_CSVTextColor);
				QTextBlockFormat blockFormat;
				if (row == 0) {
					QTextTableCellFormat cellFormat;
					cellFormat.setBackground(g.m_CSVHeaderColor);
					cell.setFormat(cellFormat);
					//charFormat.setForeground(Qt::red);
					charFormat.setFontWeight(QFont::Bold);
					blockFormat.setAlignment(Qt::AlignCenter); // ヘッダは中央
				} else {
					QTextTableCellFormat cellFormat;
					cellFormat.setBackground((row % 2) != 0 ? g.m_CSVZebraColor1 : g.m_CSVZebraColor2);
					cell.setFormat(cellFormat);
					//charFormat.setForeground(Qt::red);
					charFormat.setFontWeight(QFont::Normal);
					if( tableAlign != nullptr) {
						if( col < tableAlign->size() && ((*tableAlign)[col] & ALIGHN_RIGHT) != 0 )
							blockFormat.setAlignment(Qt::AlignRight);  // 右揃え
						else
							blockFormat.setAlignment(Qt::AlignLeft);   // その他は左
					} else if (numbers_re.match(ll[row][col]).hasMatch()) {
						blockFormat.setAlignment(Qt::AlignRight);  // 数値は右
					} else {
						blockFormat.setAlignment(Qt::AlignLeft);   // その他は左
					}
				}
				cellCursor.setCharFormat(charFormat);
				cellCursor.setBlockFormat(blockFormat);
				//cellCursor.insertText(ll[row][col]);
				cellCursor.insertMarkdown(ll[row][col]);

				cellCursor.setPosition(cell.firstPosition());
				cellCursor.setPosition(cell.lastPosition(), QTextCursor::KeepAnchor);
				cellCursor.mergeCharFormat(charFormat); // setCharFormat ではなく mergeCharFormat で太字などを維持しつつ色だけ適用
			}
		}
	}
	QTextCursor cur = cursor;
	cur.setPosition(table->firstPosition() - 1);
	setBlockType(cur.block(), BT_TABLE);
	cursor.setPosition(table->lastPosition());
	cursor.movePosition(QTextCursor::NextCharacter);
	cursor.insertBlock();
	cur = cursor;
	cur.setPosition(table->lastPosition() + 1);		//	テーブルの外に出るには +1
	setBlockType(cur.block(), BT_TABLE);
}
void MarkdownPreview::do_table(QTextBlock& srcBlock, QTextCursor& cursor) {
#if 0
	if( m_isPrevLineEmpty ) {
		cursor.insertBlock();
		cursor.insertText("\n");
		m_isPrevLineEmpty = false;
	}
#endif
#if 1		//	insertTable() 使用版
	QList<QStringList> ll;
	ll.push_back(m_tableTokens);
	assert(srcBlock.blockNumber() == m_ln);
	QString buf0 = srcBlock.text();
	BlockData *data0 = getBlockData(srcBlock, true);
	isTableLine(buf0, buf0, m_tableTokens, data0);
	m_ln += 2;
	setBlockType(srcBlock, BT_TABLE);
	srcBlock = srcBlock.next();
	setBlockType(srcBlock, BT_TABLE);
	srcBlock = srcBlock.next();
	//assert( srcBlock.blockNumber() == m_ln );  block が invalid の場合には失敗する
	//BlockData *data = nullptr;		//getBlockData(srcBlock);
	//if( srcBlock.isValid() )
	//	data = getBlockData(srcBlock, true);
	int max_clmn = m_tableTokens.size();
	bool inComment = false;
	while( m_ln < m_lst.size() ) {
		assert(srcBlock.blockNumber() == m_ln);
		assert( srcBlock.isValid() );
		BlockData *data = getBlockData(srcBlock, true);
		if( inComment ) {
			if( m_lst[m_ln++].indexOf("-->") >= 0 )		//	とりあえず --> 以降は無視
				inComment = false;
			srcBlock = srcBlock.next();
			continue;
		}
		if( m_lst[m_ln].startsWith("<!--") ) {		//	とりあえず行頭の <!-- のみ認識
			inComment = true;
			continue;
		}
		//data = getBlockData(srcBlock);
		auto t1 = srcBlock.text();
		auto t2 = m_lst[m_ln];
		assert( srcBlock.text() == m_lst[m_ln] );
		if( !isTableLine(m_lst[m_ln], m_lst[m_ln], m_tableTokens, data) ) break;
		ll.push_back(m_tableTokens);
		max_clmn = qMax(max_clmn, m_tableTokens.size());
		setBlockType(srcBlock, BT_TABLE);
		srcBlock.setUserData(data);
		++m_ln;
		srcBlock = srcBlock.next();
		if( srcBlock.isValid() )
			data = getBlockData(srcBlock, true);
	}
	cursor.beginEditBlock();
	QList<QByteArray> lba;
	insertTable(cursor, ll, lba, max_clmn, &m_tableAlign);
	//QTextTable *table = cursor.insertTable(ll.size(), max_clmn);
	//cursor.setPosition(table->lastPosition());
	//cursor.movePosition(QTextCursor::NextCharacter);
	//cursor.insertBlock();
	cursor.endEditBlock();
#else		//	insertMarkdown() 使用版
	QString buf = m_lst[m_ln] + "\n" + m_lst[m_ln+1] /*+ "\n"*/;
	setBlockType(srcBlock, BT_TABLE);
	srcBlock = srcBlock.next();
	setBlockType(srcBlock, BT_TABLE);
	srcBlock = srcBlock.next();
	m_ln += 2;
	BlockData *data = nullptr;		//getBlockData(srcBlock);
	bool inComment = false;
	while( m_ln < m_lst.size() /*&& (m_lst[m_ln].isEmpty() || isTableLine(m_lst[m_ln], m_tableTokens, data))*/ ) {
		//if( m_lst[m_ln].isEmpty() ) {		//	空行は無視
		//	++m_ln;
		//	continue;
		//}
		if( inComment ) {
			if( m_lst[m_ln++].indexOf("-->") >= 0 )		//	とりあえず --> 以降は無視
				inComment = false;
			srcBlock = srcBlock.next();
			continue;
		}
		if( m_lst[m_ln].startsWith("<!--") ) {		//	とりあえず行頭の <!-- のみ認識
			inComment = true;
			continue;
		}
		data = getBlockData(srcBlock);
		if( !isTableLine(m_lst[m_ln], m_lst[m_ln], m_tableTokens, data) ) break;
		buf += "\n" + m_lst[m_ln++];
		setBlockType(srcBlock, BT_TABLE);
		srcBlock = srcBlock.next();
		if( srcBlock.isValid() )
			data = getBlockData(srcBlock);
	}
	cursor.insertMarkdown(buf);
	cursor.movePosition(QTextCursor::Left);
	QTextTable *table = cursor.currentTable();
	if (table) {
		QColor headerBgColor(230, 240, 255); // #E6F0FF くらいの淡い青
		// 1行目（ヘッダ行）のすべての列をループ
		for (int col = 0; col < table->columns(); ++col) {
			QTextTableCell cell = table->cellAt(0, col);
			// --- 背景色の設定 ---
			QTextCharFormat cellFormat = cell.format();
			cellFormat.setBackground(headerBgColor);
			cell.setFormat(cellFormat);
			// --- センタリングの設定  ---
			QTextCursor cellCursor = cell.firstCursorPosition();
			QTextBlockFormat blockFormat = cellCursor.blockFormat();
			blockFormat.setAlignment(Qt::AlignCenter);
			cellCursor.setBlockFormat(blockFormat);
		}
		QColor cellBgColor(255, 255, 224);
		for (int row = 2; row < table->rows(); row+=2) {
			for (int col = 0; col < table->columns(); ++col) {
				QTextTableCell cell = table->cellAt(row, col);
				QTextCharFormat cellFormat = cell.format();
				cellFormat.setBackground(cellBgColor);
				cell.setFormat(cellFormat);
			}
		}
	}
	cursor.movePosition(QTextCursor::Right);
	cursor.insertBlock();
#endif
	--m_ln;
}
bool MarkdownPreview::do_underlineHeading(QTextCursor& cursor, QString buf) {
	if( m_bodyList.isEmpty() || m_bodyList.back() == "" ) return false;
	int h = 3;
	if( buf[0] == u'=' ) {
		if( cursor.blockNumber() == 0 )
			h = 1;
		else
			h = 2;
	}
	buf = m_bodyList.back();
	m_bodyList.pop_back();
	do_heading_sub(cursor, buf, h, m_ln-1);
	return true;
}
int h_font_size[] = {12, 26*4, 22*4, 18, 16, 14, 12};		//	body, h1, h2, h3 ... h6

void MarkdownPreview::do_heading(QTextBlock& srcBlock, QTextCursor& cursor, QString buf) {
	int i = 1;
	while( i < buf.size() && buf[i] == '#' ) ++i;
	int h = qMin(6, i);		//	[1, 6]
	while( i < buf.size() && buf[i] == u' ' ) ++i;
	BlockData *data = getBlockData(srcBlock);
	for(int k = 0; k < i; ++k)
		data->m_charFlags[k] = PCF_HEADING;
	srcBlock.setUserData(data);
	setBlockType(srcBlock, BT_HEADING);
	do_heading_sub(cursor, buf.mid(i), h, m_ln);
}
void MarkdownPreview::do_heading_sub(QTextCursor& cursor, QString buf, int h, int ln) {
	if( !cursor.atBlockStart() )
		cursor.insertBlock();			//	新規ブロック
#if 0
	if( m_isPrevLineEmpty ) {
		//cursor.insertBlock();
		cursor.insertText("\n");
		m_isPrevLineEmpty = false;
	}
#endif
	setBlockType(cursor.block(), BT_HEADING);
#if 1
	cursor.insertMarkdown(QString(h, u'#') + u' ' + buf /*+ "\n"*/);		//	改行を付加すると、２行になってしまう
	//cursor.insertMarkdown(QString(h, u'#') + u' ' + buf + "\n");		//	改行を付加し、２行に
	QTextBlock block = cursor.block();
	//QTextBlock headBlock = block.previous();
	//assert( headBlock.isValid() );
	QTextBlockFormat format = block.blockFormat();    // 現在のフォーマットをコピー
	if( h == 1 ) {		//	H1 の場合はセンタリング（viMarkdown 独自？仕様）
		format.setAlignment(Qt::AlignCenter);
	} else {
		format.setTopMargin(12);	 // 上側の余白（ピクセル）
		format.setBottomMargin(12); // 下側の余白（ピクセル）
	}
	//QTextCursor cur(block); // 編集用カーソルを作成
	cursor.setBlockFormat(format); // フォーマットを上書き適用
	//cursor.insertText("\n");
	//cursor.setBlockFormat(QTextBlockFormat());		//	フォーマット初期化
	cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());		//	新規ブロック
#else
	cursor.insertMarkdown(QString(h, u'#') + u' ' + buf /*+ "\n"*/);		//	改行を付加すると、２行になってしまう
	QTextBlockFormat blockFormat;
	if( h == 1 ) {		//	H1 の場合はセンタリング（viMarkdown 独自？仕様）
		blockFormat.setAlignment(Qt::AlignCenter);
	} else {
		blockFormat.setTopMargin(12);	 // 上側の余白（ピクセル）
		blockFormat.setBottomMargin(12); // 下側の余白（ピクセル）
	}
	cursor.mergeBlockFormat(blockFormat);
	cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());		//	新規ブロック
#endif
	m_docWidget->m_prvHeadingBlocks.push_back(cursor.blockNumber()-1);	//	プレビュー 見出し行 行番号
	QString text = block.text();
	m_headingList.push_back(QChar(u'0'+h) + text.remove("^ +"));
	m_docWidget->m_srcHeadingBlocks.push_back(ln);
	//m_nEmptyLines = 0;
}
void MarkdownPreview::do_CSV(QTextBlock& srcBlock, QTextCursor& cursor) {		//	cursor: プレビューカーソル
	setBlockType(srcBlock, BT_CSV_BLOCK);
	BlockData *data = getBlockData(srcBlock);
	data->m_charFlags.fill(PCF_CSV);
	srcBlock.setUserData(data);
	QList<QStringList> ll;
	QList<QByteArray> lba;
	int max_clmn = 0;
	bool inQuotes = false;
	bool inComment = false;
	bool commented = false;		//	行単位でコメントアウトされた
	QStringList fields;
	QByteArray ba;
	while( ++m_ln < m_lst.size() && !m_lst[m_ln].startsWith("```") ) {
		srcBlock = srcBlock.next();
		setBlockType(srcBlock, BT_CSV_BLOCK);
		data = getBlockData(srcBlock);
		assert(srcBlock.text() == m_lst[m_ln]);
		assert(srcBlock.text().size() == data->m_charFlags.size());
		inQuotes = parseCsvLine(fields, ba, m_lst[m_ln], inQuotes, inComment, commented, data);
		if( !inQuotes && !inComment && !commented ) {
			max_clmn = qMax(max_clmn, fields.size());
			ll.push_back(fields);
			lba.push_back(ba);
			srcBlock.setUserData(data);
		}
	}
	if( (srcBlock = srcBlock.next()).isValid() ) {
		setBlockType(srcBlock, BT_CSV_BLOCK);
		BlockData *data = getBlockData(srcBlock);
		data->m_charFlags.fill(PCF_CSV);
		srcBlock.setUserData(data);
	}
	if( ll.isEmpty() ) return;
	cursor.beginEditBlock();
#if 0
	if( m_isPrevLineEmpty ) {
		cursor.insertBlock();
		cursor.insertText("\n");
		m_isPrevLineEmpty = false;
	}
#endif
	insertTable(cursor, ll, lba, max_clmn);
#if 0
	QTextTable *table = cursor.insertTable(ll.size(), max_clmn);
	for(int row = 0; row < ll.size(); ++row) {
		for(int col = 0; col < ll[row].size(); ++col) {
			QTextTableCell cell = table->cellAt(row, col);
			if (cell.isValid()) {
				QTextCursor cellCursor = cell.firstCursorPosition();
				QTextCharFormat charFormat;
				QTextBlockFormat blockFormat;
				if (row == 0) {
					QTextTableCellFormat cellFormat;
					cellFormat.setBackground(g.m_CSVHeaderColor);
					cell.setFormat(cellFormat);
					charFormat.setFontWeight(QFont::Bold);
					blockFormat.setAlignment(Qt::AlignCenter); // ヘッダは中央
				} else {
					QTextTableCellFormat cellFormat;
					cellFormat.setBackground((row % 2) != 0 ? g.m_CSVZebraColor1 : g.m_CSVZebraColor2);
					cell.setFormat(cellFormat);
					charFormat.setFontWeight(QFont::Normal);
					if (re.match(ll[row][col]).hasMatch()) {
						blockFormat.setAlignment(Qt::AlignRight);  // 数値は右
					} else {
						blockFormat.setAlignment(Qt::AlignLeft);   // その他は左
					}
				}
				cellCursor.setCharFormat(charFormat);
				cellCursor.setBlockFormat(blockFormat);
				//cellCursor.insertText(ll[row][col]);
				cellCursor.insertMarkdown(ll[row][col]);
			}
		}
	}
#endif
	cursor.endEditBlock();
	//++m_ln;
}
void MarkdownPreview::do_keisen_block(QTextBlock& srcBlock, QTextCursor& cursor) {
	setBlockType(srcBlock, BT_KEISEN_BEGIN);
	BlockData *data = getBlockData(srcBlock);
	data->m_charFlags.fill(PCF_KEISEN);
	srcBlock.setUserData(data);
	QStringView buf = m_lst[m_ln].mid(QString("```keisen").size());
	QColor bgcolor = g.m_keisenBlockColor;
	QColor color("black");
	for(;;) {
		while( !buf.isEmpty() && buf[0] == u' ' ) buf = buf.mid(1);		//	skip u' ';
		if( buf.startsWith(u"background-color:", Qt::CaseInsensitive) ) {
			buf = buf.mid(QString(u"background-color:").size());
			while( !buf.isEmpty() && buf[0] == u' ' ) buf = buf.mid(1);		//	skip u' ';
			int ix = buf.indexOf(u';');
			if( ix < 0 ) break;
			bgcolor = QColor(buf.left(ix));
			buf = buf.mid(ix+1);
		} else if( buf.startsWith(u"color:", Qt::CaseInsensitive) ) {
			buf = buf.mid(QString(u"color:").size());
			while( !buf.isEmpty() && buf[0] == u' ' ) buf = buf.mid(1);		//	skip u' ';
			int ix = buf.indexOf(u';');
			if( ix < 0 ) break;
			color = QColor(buf.left(ix));
			buf = buf.mid(ix+1);
		} else
			break;
	}
	QFont font;
	font.setFamilies({"MS Gothic", "MS UI Gothic", "Osaka-mono", "monospace"});
	font.setFixedPitch(true);
	font.setPointSizeF(12);
	QFontMetrics fm(font);
	int width = 100;
	int m_ln0 = m_ln + 1;
	while( ++m_ln < m_lst.size() && !m_lst[m_ln].startsWith("```") ) {
		srcBlock = srcBlock.next();
		setBlockType(srcBlock, BT_KEISEN_BLOCK);
		width = qMax(width, fm.horizontalAdvance(m_lst[m_ln])+10);
	}
	srcBlock = srcBlock.next();
	if( srcBlock.isValid() ) {
		setBlockType(srcBlock, BT_KEISEN_BLOCK);
		data = getBlockData(srcBlock);
		data->m_charFlags.fill(PCF_KEISEN);
		srcBlock.setUserData(data);
	}
	int lineHeight = fm.lineSpacing(); // 行の間隔（高さ＋行間）
	
	int height = lineHeight * (m_ln - m_ln0);
	QImage img(width, height, QImage::Format_RGB32);
	img.fill(bgcolor);
	QPainter painter(&img);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setFont(font);
	painter.setPen(color);
	for(int i = 0; i < m_ln - m_ln0; ++i)
		painter.drawText(5, lineHeight*i+fm.height(), m_lst[m_ln0+i]);
	painter.end();
	//
	//cursor.insertBlock();
	setBlockType(cursor.block(), BT_KEISEN_BLOCK);
	QTextBlockFormat blockFormat;
	blockFormat.setTopMargin(lineHeight/2);
	cursor.mergeBlockFormat(blockFormat);
	cursor.insertImage(img);
	cursor.insertBlock();
	//cursor.setBlockFormat(QTextBlockFormat());		//	トップマージンリセット
}
void MarkdownPreview::do_SVG(QTextBlock& srcBlock, QTextCursor& cursor) {
	setBlockType(srcBlock, BT_SVG_BEGIN);
	BlockData *data = getBlockData(srcBlock);
	data->m_charFlags.fill(PCF_SVG);
	srcBlock.setUserData(data);
	QString buf;
	int width = 320;
	int height = 200;
	bool inSvgTag = false;
	QString svg;
	while( ++m_ln < m_lst.size() && !m_lst[m_ln].startsWith("```") ) {
		srcBlock = srcBlock.next();
		setBlockType(srcBlock, BT_SVG_BLOCK);
		QString t = m_lst[m_ln].trimmed();
#if 0
		if( t.startsWith("<svg", Qt::CaseInsensitive) ) {
			QXmlStreamReader reader(t);
			while (!reader.atEnd()) {
				QXmlStreamReader::TokenType token = reader.readNext();
				if (token == QXmlStreamReader::StartElement && reader.name() == "svg") {
					if (reader.attributes().hasAttribute("width"))
						width = reader.attributes().value("width").toInt();
					if (reader.attributes().hasAttribute("height"))
						height = reader.attributes().value("height").toInt();
					break;
				}
			}
		}
#endif
		buf += t + "\n";
	}
	double wd = 1.0, ht = 1.0;
	QXmlStreamReader reader(buf);
	while (!reader.atEnd()) {
		QXmlStreamReader::TokenType token = reader.readNext();
		if (token == QXmlStreamReader::StartElement && reader.name() == "svg") {
			if (reader.attributes().hasAttribute("width")) {
				auto txt = reader.attributes().value("width");
				if( !txt.isEmpty() && txt[txt.size() - 1] == u'%' )
					wd = txt.left(txt.size() - 1).toInt() / 100.0;
				else
					width = txt.toInt();
			}
			if (reader.attributes().hasAttribute("height")) {
				//height = reader.attributes().value("height").toInt();
				auto txt = reader.attributes().value("height");
				if( !txt.isEmpty() && txt[txt.size() - 1] == u'%' )
					ht = txt.left(txt.size() - 1).toInt() / 100.0;
				else
					height = txt.toInt();
			}
			if (reader.attributes().hasAttribute("viewBox")) {
				auto viewBox = reader.attributes().value("viewBox");
				auto list = viewBox.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
				width = list.value(2).toDouble();
				height = list.value(3).toDouble();
			}
			width *= wd;
			height *= ht;
			break;
		}
	}
	srcBlock = srcBlock.next();
	if( srcBlock.isValid() ) {
		setBlockType(srcBlock, BT_SVG_BLOCK);
		data = getBlockData(srcBlock);
		data->m_charFlags.fill(PCF_SVG);
		srcBlock.setUserData(data);
	}
	QImage image(width, height, QImage::Format_ARGB32);
	image.fill(Qt::white);
    auto document = lunasvg::Document::loadFromData(buf.toUtf8());
    if (!document) {
        // ロード失敗（SVGの構文エラーなど）
        //##qDebug() << "load failed.";
        return;
    }
    lunasvg::Bitmap bitmap(
        image.bits(),
        image.width(),
        image.height(),
        image.bytesPerLine()
    );
    document->render(bitmap);
#if 1
    QTextBlockFormat imgBlockFormat = cursor.blockFormat();
    imgBlockFormat.setAlignment(Qt::AlignCenter);
    cursor.setBlockFormat(imgBlockFormat);
    setBlockType(cursor.block(), BT_SVG_BLOCK);
    cursor.insertImage(image);
    cursor.insertBlock();
    QTextBlockFormat nextBlockFormat = cursor.blockFormat();
    nextBlockFormat.setAlignment(Qt::AlignLeft); // または元の配置にリセット
    cursor.setBlockFormat(nextBlockFormat);
#else
	setBlockType(cursor.block(), BT_SVG_BLOCK);
	cursor.insertImage(image);
	cursor.insertBlock();
#endif
}
void MarkdownPreview::do_code(QTextBlock srcBlock, QTextCursor& cursor) {
#if 0
	if( m_isPrevLineEmpty ) {
		cursor.insertBlock();
		//cursor.insertText("\n");
		m_isPrevLineEmpty = false;
	}
#endif
	setBlockType(srcBlock, BT_CODE_BLOCK);
	BlockData *data = getBlockData(srcBlock);
	data->m_charFlags.fill(PCF_CODE);
	srcBlock.setUserData(data);
	QStringList lst;
	QString buf;
	while( ++m_ln < m_lst.size() && !m_lst[m_ln].startsWith("```") ) {
		srcBlock = srcBlock.next();
		setBlockType(srcBlock, BT_CODE_BLOCK);
		if( !buf.isEmpty() ) buf += "\n";
		buf += m_lst[m_ln];
		lst += m_lst[m_ln];
	}
	if( (srcBlock = srcBlock.next()).isValid() ) {
		setBlockType(srcBlock, BT_CODE_BLOCK_END);
		BlockData *data = getBlockData(srcBlock);
		data->m_charFlags.fill(PCF_CODE);
		srcBlock.setUserData(data);
	}
	QTextCharFormat codeCharFormat;
#if 1	//	insertFrame() を使用しない版
	QTextBlockFormat blockFmt;
	blockFmt.setBackground(g.m_codeBlockColor);
	blockFmt.setLeftMargin(10);
	QTextCharFormat charFmt = codeCharFormat;
	cursor.setBlockFormat(blockFmt);
	for (auto txt : lst) {
	    cursor.insertText(txt, charFmt);
	    cursor.insertBlock(blockFmt);
	}
#else
#if 0
	codeCharFormat.setFontFamilies({"MS Gothic", "MS UI Gothic", "Osaka-mono", "monospace"});
	codeCharFormat.setFontFixedPitch(true); // 等幅フォントであることを明示
	codeCharFormat.setFontPointSize(10);	// 必要に応じてサイズを調整
#else
	QFont font;
	// 2. フォントの基本属性を設定
	font.setFamilies({"MS Gothic", "MS UI Gothic", "Osaka-mono", "monospace"});
	font.setFixedPitch(true);
	font.setPointSizeF(12); // 精密な計算のために setPointSize より setPointSizeF がおすすめ
	// 3. レタースペーシング（文字間隔）を設定
	// 100% よりわずかに小さくすることで、PDF出力時の隙間を埋めます
	font.setLetterSpacing(QFont::PercentageSpacing, 99.5);
	// 4. 設定済みの QFont を QTextCharFormat に適用
	codeCharFormat.setFont(font);
#endif
	// フレームの書式を設定
	QTextFrameFormat frameFormat;
	frameFormat.setBackground(g.m_codeBlockColor);
	frameFormat.setMargin(0);					  // 外側の余白
	frameFormat.setPadding(10);					  // 内側の余白（文字と端の隙間）
	frameFormat.setBorder(0);					  // 枠線はなし
	// 2. フレームを挿入（この中にカーソルが入る）
	cursor.insertFrame(frameFormat);
	QTextBlock before = cursor.block().previous();
	if (before.text().isEmpty()) {
	    QTextCursor c(before);
	    //c.deleteChar(); // or removeSelectedText		//→　直前空行は消えなかった
	    //c.select(QTextCursor::BlockUnderCursor);
	    //c.removeSelectedText();
	    c.movePosition(QTextCursor::EndOfBlock);
	    c.deleteChar();  // ブロック削除ではなく結合
	}

	// 3. フレームの中で Markdown を挿入
	cursor.insertText(buf, codeCharFormat);
	cursor.movePosition(QTextCursor::End);
#endif
	this->setTextCursor(cursor);
	//cursor.insertBlock();
	//QTextBlockFormat blockFormat;
	cursor.setBlockFormat(QTextBlockFormat());
	//--m_ln;
	//m_nEmptyLines = 0;
}
void MarkdownPreview::do_quote(QTextBlock &srcBlock, QTextCursor& cursor, QString buf) {
#if 0
	if( m_isPrevLineEmpty ) {
		cursor.insertBlock();
		//cursor.insertText("\n");
		m_isPrevLineEmpty = false;
	}
#endif
	BlockData* data = getBlockData(srcBlock);
	data->m_charFlags[0] = data->m_charFlags[1] = PCF_QUOTE;	//	"> " 固定
	srcBlock.setUserData(data);
	//QString buf0 = buf + "\n";
	buf = buf.mid(2);
	while( ++m_ln < m_lst.size() ) {
		if( !m_lst[m_ln].startsWith("> ") ) break;
		buf += "\n" + m_lst[m_ln].mid(2);
		//buf0 += m_lst[m_ln] + "\n";
		srcBlock = srcBlock.next();
		BlockData* data = getBlockData(srcBlock);
		data->m_charFlags[0] = data->m_charFlags[1] = PCF_QUOTE;	//	"> " 固定
		srcBlock.setUserData(data);
	}
#if 0	//	insertMarkdown 版 → なんか全然だめ
	cursor.insertMarkdown(buf0);
#elif 0	//	insertHtml 版
	buf += "\n";
	//QString html = QString("<blockquote><div>%1</div></blockquote>").arg(buf.replace("\n", "<br>"));
	//QString html = buf + "\n";
	//htmlBuf.replace("\n", "<br>");
	QString html = QString("<blockquote style='background-color: #f0f8ff; padding: 10px;'>%1</blockquote>").arg(buf.replace("\n", "<br>"));
	cursor.insertHtml(html);
#elif 1	//	QTextBlockFormat 使用版 → 問題はあるが一番マシか
	QTextBlockFormat blockFormat;
	blockFormat.setBackground(g.m_quoteColor); // 背景色
	// 左側にボーダー（引用の縦線）を引きたい場合は、QTextCharFormat やペイントイベントで工夫しますが、
	// 背景色とパディング（マージン）だけなら blockFormat で十分です。
	blockFormat.setLeftMargin(20);  // インデント（字下げ）
	blockFormat.setTopMargin(0);    // 上の余白を消す！
	blockFormat.setBottomMargin(0); // 下の余白を消す！
	// 行間（LineHeight）を少し広げることで、上下の「詰まり感」を軽減
	//blockFormat.setLineHeight(120, QTextBlockFormat::ProportionalHeight);
	cursor.beginEditBlock();
	// テキストを挿入（この時点で自動的にブロックが作られる）
	cursor.insertText(buf+"\n");
	// 挿入したテキストの範囲を選択
	cursor.setPosition(cursor.position() - buf.length(), QTextCursor::KeepAnchor);
	// ブロックフォーマット（背景色とマージン）を一括適用
	cursor.setBlockFormat(blockFormat);
	cursor.clearSelection();
	// 元のフォーマットに戻して次の行へ
	cursor.movePosition(QTextCursor::End);
	cursor.setBlockFormat(QTextBlockFormat()); // 次の行は通常のフォーマットに戻す
	cursor.endEditBlock();
#else	//	QTextFrameFormat 使用版 → 余分な空行が生成されてしまう
	//cursor.insertMarkdown(buf);
	// 1. フレームの書式を設定
	QTextFrameFormat frameFormat;
	frameFormat.setBackground(g.m_quoteColor);
	frameFormat.setMargin(0);					  // 外側の余白
	frameFormat.setPadding(10);					  // 内側の余白（文字と端の隙間）
	frameFormat.setBorder(0);					  // 枠線はなし
	// 2. フレームを挿入（この中にカーソルが入る）
	cursor.insertFrame(frameFormat);
	// 3. フレームの中で Markdown を挿入
	cursor.insertText(buf);
	cursor.movePosition(QTextCursor::End);
	this->setTextCursor(cursor);
	//cursor.insertBlock();
	//QTextBlockFormat blockFormat;
	cursor.setBlockFormat(QTextBlockFormat());
#endif
	--m_ln;
	//m_nEmptyLines = 0;
}
void MarkdownPreview::do_numlist(QTextBlock srcBlock, QTextCursor& cursor, QString buf) {
#if 1	//	insertMarkdown() を使用せず QTextListFormat を適用
	cursor.beginEditBlock();
	QTextListFormat listFormat;
	listFormat.setStyle(QTextListFormat::ListDecimal); // 1. 2. 3. の連番リスト
	auto *list = cursor.createList(listFormat);
	auto match = re_numlist.match(m_lst[m_ln]);
	while( match.hasMatch() ) {
		updateCharFlags(srcBlock);
		setBlockType(srcBlock, BT_NUMLIST);
		BlockData* data = getBlockData(srcBlock);
		for(int i = 0; i < match.capturedLength(); ++i)
			data->m_charFlags[i] = PCF_NUM_LIST;
		srcBlock.setUserData(data);
		//QTextBlock b = list->item(list->count() - 1);
		//cursor.insertText(m_lst[m_ln].remove(re_numlist) /*+ "\n"*/);
		setBlockType(cursor.block(), BT_NUMLIST);
		auto text = m_lst[m_ln];
		//##text.replace(re_tailspc, ZWSP);
		if( text.size() > 3 && text.back() == u' ' ) {	//	"1. " 部分は無視
			text.back() = ZWSP;
		}
		cursor.insertMarkdown(text + "\n");
		if( ++m_ln >= m_lst.size() ) break;
		srcBlock = srcBlock.next();
		match = re_numlist.match(m_lst[m_ln]);
		if( !match.hasMatch() ) break;
		cursor.insertBlock();
	}
#if 0
	QTextBlock lastBlock = list->item(list->count() - 1);
	QTextCursor delCursor(lastBlock);
	delCursor.select(QTextCursor::BlockUnderCursor);
    delCursor.removeSelectedText();
#endif
	cursor.endEditBlock();
#else
	bool init = true;
	auto match = re_numlist.match(m_lst[m_ln]);
	while( match.hasMatch() ) {
		BlockData* data = getBlockData(srcBlock);
		for(int i = 0; i < match.capturedLength(); ++i)
			data->m_charFlags[i] = PCF_NUM_LIST;
		srcBlock.setUserData(data);
		//if( !init )
		//	buf += u'\n' + m_lst[m_ln];
		if( ++m_ln >= m_lst.size() ) break;
		srcBlock = srcBlock.next();
		init = false;
		match = re_numlist.match(m_lst[m_ln]);
	}
	cursor.insertMarkdown(buf);
#endif
	cursor.insertBlock(QTextBlockFormat());
	//cursor.insertBlock();
	//QTextBlockFormat blockFormat;
	//cursor.setBlockFormat(blockFormat);
	--m_ln;
	//m_nEmptyLines = 0;
}
void MarkdownPreview::do_list(QTextBlock srcBlock, QTextCursor& cursor, QString buf) {
	//if( m_nEmptyLines >= 1 )
	//	cursor.insertBlock();			//	新規ブロック
#if 0
	if( m_isPrevLineEmpty ) {
		cursor.insertBlock();
		cursor.insertText("\n");
		m_isPrevLineEmpty = false;
	}
#endif
	if( m_nSpaces > 0 )
		buf = QString(m_nSpaces, QChar(u' ')) + buf;
	//buf.replace(re_tailspc, "&nbsp;");
	static QRegularExpression re_checkbox(R"(^( *)- \[[ xX]\] )");
	int pos = cursor.position();
	int n_item = 1;
	int ln = m_ln;
	auto match = re_checkbox.match(buf);
	bool is_checkbox = match.hasMatch();		//	チェックボックス（"- [ ] "）の場合
	if( is_checkbox ) {
		for(;;) {
			if( match.capturedLength() == srcBlock.text().size() )
				buf += ZWSP;	//	ゼロ幅空白文字
			updateCharFlags(srcBlock);
			BlockData* data = getBlockData(srcBlock);
			for(int i = 0; i < match.capturedLength(); ++i)
				data->m_charFlags[i] = PCF_LIST_MARK;
			srcBlock.setUserData(data);
			setBlockType(srcBlock, BT_CHECKBOX);
			if( ++m_ln >= m_lst.size() ) break;
			match = re_checkbox.match(m_lst[m_ln]);
			if( !match.hasMatch() ) break;
			srcBlock = srcBlock.next();
			//data = getBlockData(srcBlock);
			buf += u'\n' + m_lst[m_ln];
			//buf.replace(re_tailspc, "&nbsp;");
			++n_item;
		}
#if 0
		while( ++m_ln < m_lst.size() ) {
			if( !re_checkbox.match(m_lst[m_ln]).hasMatch() ) break;
			buf += u'\n' + m_lst[m_ln];
			++n_item;
		}
#endif
	} else {		//	リストの場合
		//static QRegularExpression re(R"(^( *)- )");
		auto mch = re_list.match(srcBlock.text());
		if( mch.capturedLength() == srcBlock.text().size() )
			buf += ZWSP;	//	ゼロ幅空白文字
		else
			buf.replace(re_tailspc, "&nbsp;");
		BlockData* data = getBlockData(srcBlock);
		for(int i = 0; i < mch.capturedLength(); ++i)
			data->m_charFlags[i] = PCF_LIST_MARK;
		srcBlock.setUserData(data);
		updateCharFlags(srcBlock);
		setBlockType(srcBlock, BT_LIST);
		//printCharFlags(srcBlock);
		bool isPrevlist = true;
		//bool spc2Prev = false;
		while( ++m_ln < m_lst.size() ) {
			srcBlock = srcBlock.next();
			QString text = m_lst[m_ln];
			if( text.isEmpty() ) break;		//	空行だった場合
			auto mch = re_list.match(text);
			if( mch.hasMatch() ) {	//	リスト行
				++n_item;
				buf += u'\n' + text;
				if( mch.capturedLength() == text.size() )
					buf += ZWSP;
				else
					buf.replace(re_tailspc, "&nbsp;");
				isPrevlist = true;
				//int length = mch.capturedLength();
				BlockData* data = getBlockData(srcBlock);
				for(int i = 0; i < mch.capturedLength(); ++i)
					data->m_charFlags[i] = PCF_LIST_MARK;
				srcBlock.setUserData(data);
				updateCharFlags(srcBlock);
				setBlockType(srcBlock, BT_LIST);
				//printCharFlags(srcBlock);
			} else {	//	非リスト行の場合
				if( re_block.match(text).hasMatch() )	//	ブロック行の場合
					break;
#if 1
				//static QRegularExpression re_headsp(R"(^ +)");
				//buf += "<br />" + text.remove(re_headsp);
				bool tailsp = text.endsWith(" ");
				buf += "<br />" + text.trimmed();
				if( tailsp ) buf += "&nbsp;";
				//buf += u"<br />\n" + text /*+ "<br />\n"*/;
				//if( isPrevlist )
				//	buf += u"<br/>" + text;
				//else
				//	buf += u"\n" + text;
#else
				bool spc2 = text.endsWith("  ");
				text = text.trimmed();
				if( isPrevlist || spc2Prev )
					buf += u"<br/>" + text;
				else
					buf += u"\n" + text;
#endif
				isPrevlist = false;
				//spc2Prev = spc2;
			}
		}
	}
	int startPos = cursor.position();
	//setBlockType(cursor.block(), is_checkbox ? BT_CHECKBOX : BT_LIST);
	buf.replace(re_tailspc, "&nbsp;");
	cursor.insertMarkdown(buf);
	QTextBlock firstBlock = document()->findBlock(startPos);
	if (firstBlock.isValid() && firstBlock.text().isEmpty()) {
		// ブロックが空なら削除する（バックスペース的な処理）
		QTextCursor helper(firstBlock);
		helper.deleteChar(); 
	}
	//if( is_checkbox ) {
		QTextBlock srcBlock2 = document()->findBlock(pos);
		for(int i = 0; i < n_item; ++i) {
			//setBlockType(srcBlock, ln++);
			srcBlock2.setUserState(is_checkbox ? BT_CHECKBOX : BT_LIST);
			srcBlock2 = srcBlock2.next();
		}
	//}
	cursor.insertBlock();
	QTextBlockFormat blockFormat;
	cursor.setBlockFormat(blockFormat);
	--m_ln;
	//m_nEmptyLines = 0;
}
void MarkdownPreview::setCursorAt(int srcBlockNum, QString srcText, int ix) {		//	ix: ブロック内カーソル位置
	int i = 0;
	while( i+1 < m_docWidget->m_srcHeadingBlocks.size() && m_docWidget->m_srcHeadingBlocks[i+1] <= srcBlockNum ) ++i;
	if( i >= m_docWidget->m_prvHeadingBlocks.size() ) return;
	QTextBlock block = document()->findBlockByNumber(m_docWidget->m_prvHeadingBlocks[i]);
	QTextCursor cursor = textCursor();
	cursor.setPosition(block.position());
	if( !srcText.isEmpty() ) {
		QString t3 = srcText.mid(ix, 3);
		auto cur = document()->find(t3, cursor);
		if( !cur.isNull()) {
			//cur.clearSelection();
			cur.setPosition(cur.selectionStart(), QTextCursor::MoveAnchor);
			cursor = cur;
		}
	}
	setTextCursor(cursor);
}
void MarkdownPreview::setCursorAtNthPat(int srcBlockNum, QString pat, int nth, bool tail) {		//	nth: 何番目か（>0）
	//##qDebug() << QString("MarkdownPreview::setCursorAtNthPat(%1, '%2', %3,").arg(srcBlockNum).arg(pat).arg(nth) << tail << ")";
	int i = 0;
	while( i+1 < m_docWidget->m_srcHeadingBlocks.size() && m_docWidget->m_srcHeadingBlocks[i+1] <= srcBlockNum ) ++i;
	if( i >= m_docWidget->m_prvHeadingBlocks.size() ) return;
	QTextBlock block = document()->findBlockByNumber(m_docWidget->m_prvHeadingBlocks[i]);
	QTextCursor cursor = textCursor();
	if( !tail ) {
		cursor.setPosition(block.position());
		for(int n = 0; n < nth; ++n) {
			cursor = document()->find(pat, cursor);
			if( cursor.isNull()) return;
		}
		cursor.setPosition(cursor.selectionStart(), QTextCursor::MoveAnchor);
	} else {
		for(;;) {
			if( block.text().endsWith(pat) ) {
				if( --nth == 0 )
					break;
			}
			block = block.next();
			if( !block.isValid() ) {	
				block = document()->lastBlock();
				break;
			}
		}
		cursor.setPosition(block.position());
		cursor.movePosition(QTextCursor::EndOfBlock);
	}
	setTextCursor(cursor);
}
void MarkdownPreview::ensureLineVisible(int srcBlockNum) {
	//qDebug() << "srcBlockNum = " << srcBlockNum;
	int i = 0;
	while( i+1 < m_docWidget->m_srcHeadingBlocks.size() && m_docWidget->m_srcHeadingBlocks[i+1] <= srcBlockNum ) ++i;
	if( i < m_docWidget->m_prvHeadingBlocks.size() )
		scrollToBlock(m_docWidget->m_prvHeadingBlocks[i]);
}
void MarkdownPreview::scrollToBlock(int blockIndex) {
	QTextBlock block = document()->findBlockByNumber(blockIndex);
	if (!block.isValid()) return;
#if 0
	QTextCursor cursor = textCursor();
	cursor.setPosition(block.position());
	setTextCursor(cursor);
	ensureCursorVisible();
#else
	int y = (int)document()->documentLayout()->blockBoundingRect(block).top();
	verticalScrollBar()->setValue(y);
#endif
}
#if 0
void MarkdownPreview::scrollToTop(const QTextCursor &cursor) {
	int visualLineNum = getVisualLineNumber(cursor);
	this->scrollToTop(visualLineNum);
}
int MarkdownPreview::getVisualLineNumber(const QTextCursor &cursor) const {
	//int visualLineNum = 0;
	QTextBlock targetBlock = cursor.block();
	return targetBlock.blockNumber();
}
#endif
#if 0
int MarkdownPreview::prvToSrcHeading(int blockNum) {
	assert( m_docWidget->m_prvHeadingBlocks.size() == m_docWidget->m_srcHeadingBlocks.size() );
#if 1
	auto it = std::lower_bound(m_docWidget->m_prvHeadingBlocks.begin(), m_docWidget->m_prvHeadingBlocks.end(), blockNum);
	if (it != m_docWidget->m_prvHeadingBlocks.end()) {
		size_t ix = std::distance(m_docWidget->m_prvHeadingBlocks.begin(), it);
		return m_docWidget->m_srcHeadingBlocks[ix];
	} else
		return 0;
#else
	int ix = 0;
	while( ix+1 < m_docWidget->m_prvHeadingBlocks.size() && m_docWidget->m_prvHeadingBlocks[ix] < blockNum ) ++ix;
	if( ix < m_docWidget->m_srcHeadingBlocks.size() )
		return m_docWidget->m_srcHeadingBlocks[ix];
	else
		return 0;
#endif
}
#endif
#if 0
bool isLastBlockInRow(QTextBlock block) {
	QTextCursor cursor(block);
	QTextTable *table = cursor.currentTable();
	if( table == nullptr ) return false;
	int col = table->cellAt(cursor).column();
	return col >= table->columns();
}
#endif
int MarkdownPreview::findPosition(const PosContext &context) {
	//##qDebug() << "MarkdownPreview::findPosition():";
	//##qDebug() << ".ancharChar = " << context.m_anchorChar << ", nth = " << context.m_nth << ", offset = " << context.m_offset;
	QTextBlock block = document()->findBlockByNumber(context.m_prvHBlockNum);
	const QChar ch = context.m_anchorChar;
	int nth = context.m_nth;
	int ix = 0, ix2;
	while( block.isValid() ) {
		if( blockType(block) == BT_TABLE		//	CSV, GFM表 のダミーブロックの場合
			/*|| blockType(block) == BT_KEISEN_BLOCK*/ )	//	罫線ブロック（```行）の場合
		{
			block = block.next();
			continue;
		}
		QString buf = block.text();
		if( ch == STX ) {		//	行頭の場合
			ix = 0;
			QTextCursor cursor(block);
			QTextTable *table = cursor.currentTable();
			//	~~暫定的：直前の空ブロックですでにデクリメントされているので、最初の行では nth をデクリメントしない~~
			if( table == nullptr || table->cellAt(cursor).column() == 0 /*&& table->cellAt(cursor).row() != 0*/ ) {
				if( --nth == 0 ) break;
			}
			if( blockType(block) == BT_LIST ) {	//	リスト行の場合
				while( (ix2 = buf.indexOf(QChar(LINE_SEPARATOR), ix)) >= 0 ) {
					ix = ix2 + 1;
					if( --nth == 0 ) break;
				}
				if( nth == 0 ) break;
			}
			block = block.next();
			//ix = 0;
		} else if( ch == ETX ) {		//	行末の場合
			//	undone: 表内の場合は、行最後のセルでのみカウント
			if( blockType(block) == BT_LIST ) {	//	リスト行の場合
				ix = 0;
				while( (ix2 = buf.indexOf(QChar(LINE_SEPARATOR), ix)) >= 0 ) {
					ix = ix2;
					if( --nth == 0 ) break;
					++ix;
				}
				if( nth == 0 ) break;
			}
			ix = buf.size();
			QTextCursor cursor(block);
			QTextTable *table = cursor.currentTable();
			if( table == nullptr ) {
				if( --nth == 0 ) break;
			} else {
				if( table->cellAt(cursor).column() >= table->columns() - 1)
					if( --nth == 0 ) break;
			}
			block = block.next();
			ix = 0;
		} else {		//	非行末の場合
			ix = buf.indexOf(ch, ix);
			if( ix >= 0 ) {
				if( --nth == 0 ) break;
				++ix;
			} else {
				block = block.next();
				ix = 0;
			}
		}
	}
	if( block.isValid() ) {
		return block.position() + ix + context.m_offset;
	} else
		return -1;
}
void MarkdownPreview::setCursorByContext(const PosContext &context, const PosContext &acontext) {
	if( m_processing ) return;		//	再入禁止
	//##qDebug() << "MarkdownPreview::setCursorByContext(context)";
	//##qDebug() << "context.ancharChar = " << context.m_anchorChar << ", nth = " << context.m_nth << ", offset = " << context.m_offset <<
					//##", srcHBNum = " << context.m_srcHBlockNum << ", prvHBNum = " << context.m_prvHBlockNum;
	if( acontext.m_nth != 0 ) {
		//##qDebug() << "acontext.ancharChar = " << acontext.m_anchorChar << ", nth = " << acontext.m_nth << ", offset = " << acontext.m_offset <<
					//##", srcHBNum = " << acontext.m_srcHBlockNum << ", prvHBNum = " << acontext.m_prvHBlockNum;
	}
	m_processing = true;
	QTextCursor cursor = textCursor();
	if( acontext.m_nth == 0 ) {		//	非選択状態
		int pos = findPosition(context);
		if( pos >= 0 ) {
			cursor.setPosition(pos);
			setTextCursor(cursor);
		}
	} else {
		int pos = findPosition(acontext);
		if( pos >= 0 ) {
			cursor.setPosition(pos);
			pos = findPosition(context);
			if (pos >= 0) {
				cursor.setPosition(pos, QTextCursor::KeepAnchor);
				setTextCursor(cursor);
			}
		}
	}
	m_processing = false;
}
PosContext MarkdownPreview::contextAt(int pos) {	//	pos 位置から PosContext を構築
	PosContext pc;
	QTextCursor cursor = this->textCursor();
	cursor.setPosition(pos);
	auto *doc = document();
	QTextBlock block = doc->findBlock(pos);
	QTextBlock block0 = block;
	QTextTable *table = cursor.currentTable();
	if( blockType(block) == BT_LIST ) {
		//auto ch = doc->characterAt(pos-1);
		pc.m_anchorChar = ListBullet;
		pc.m_offset = pos - block.position();
		if( pc.m_offset == 1 && doc->characterAt(pos-1) == ZWSP )
			pc.m_offset = 0;
	} else if( blockType(block) == BT_KEISEN_BLOCK ) {
		//pc.m_anchorChar = QChar(U_KEISEN_BLOCK);
		pc.m_anchorChar = pos == block.position() ? STX : EOB;
	} else if( pos == block.position() && (table == nullptr || table->cellAt(cursor).column() == 0) ) {		//	行頭にいる場合
		pc.m_anchorChar = STX;
	} else if( blockType(block) != BT_LIST &&
				pos == block.position() + block.text().size() &&		//	行末にいる場合
				(table == nullptr || table->cellAt(cursor).column() >= table->columns() - 1) )
	{
		pc.m_anchorChar = ETX;
	} else {
		//pc.m_chPrev = pos != block.position() ? doc->characterAt(pos-1) : ETX;
		auto ch = doc->characterAt(pos);
		// undone: 行頭に来たらそこでストップ
		int ix = pos - block.position();
		if( table != nullptr ) {	//	テーブル内の場合
			int row = table->cellAt(cursor).row();
			ix = pos - table->cellAt(row, 0).firstPosition();
		}
		while( ix > 0 && (ch == endOfCell || ch == u' ' || ch == QChar(LINE_SEPARATOR) || ch == QChar::ParagraphSeparator) ) {
			pc.m_offset += 1;
			--pos;
			if( --ix <= 0 ) {
				ch = STX;
				break;
			}
			ch = doc->characterAt(pos);
		}
#if 0
		if( pos > 0 && (chat == endOfCell || chat == u' ') ) {
			//while( pos > 0 && (chat == endOfCell || chat == u' ') ) {
				pc.m_offset += 1;
				chat = doc->characterAt(--pos);
			//}
		} else {
			while( pos > 0 && chat == QChar::ParagraphSeparator ) {
				pc.m_offset += 1;
				chat = doc->characterAt(--pos);
			}
		}
#endif
		pc.m_anchorChar = ch != QChar::ParagraphSeparator ? ch : ETX;
	}
	//pc.m_anchorChar = ch;
	while( blockType(block) != BT_HEADING ) {		//	直前の見出し行を探す
		if( !block.previous().isValid() )
			break;
		block = block.previous();
	}
	pc.m_prvHBlockNum = block.blockNumber();
	//	pos の {chPrev, chNext} が見出し行先頭から何番目かを計算
	int count = pc.m_anchorChar == EOB ? 0 : 1;
	bool found = false;
	if( pc.m_anchorChar == STX ) {			//	行頭の場合
		while( block.isValid() ) {
			if( block.position() >= pos )
				break;
			if( blockType(block) != BT_TABLE ) {	//	テーブル前後のダミーブロックではない
				QTextCursor cursor(block);
				QTextTable *table = cursor.currentTable();
				if( table == nullptr || table->cellAt(cursor).column() == 0 )
					++count;
			}
			if( blockType(block) == BT_LIST ) {	//	リスト行の場合
				const QString buf = block.text();
				for(int i = 0; i < buf.size(); ++i) {
					if( block.position() + i >= pos ) {
						found = true;
						break;
					}
					if( buf[i] == QChar(LINE_SEPARATOR ) )
						++count;
				}
			}
			if( found ) break;
			block = block.next();
		}
	} else if( pc.m_anchorChar == ETX ) {	//	行末の場合
		while( block.isValid() ) {
			if( block.position() + block.text().length() >= pos )
				break;
			if( blockType(block) == BT_LIST ) {	//	リスト行の場合
				++count;
				for(QChar c: block.text()) {
					if( c == QChar(LINE_SEPARATOR) ) ++count;
				}
			} else if( blockType(block) != BT_TABLE ) {	//	テーブル前後のダミーブロックではない
				QTextCursor cursor(block);
				QTextTable *table = cursor.currentTable();
				if( table == nullptr || table->cellAt(cursor).column() >= table->columns() - 1 )
					++count;
			}
			block = block.next();
		}
	} else if( pc.m_anchorChar == EOB ) {	//	罫線ブロック末の場合
		while( block.isValid() ) {
			if( blockType(block) == BT_KEISEN_BLOCK )
				++count;
			if( block.position() + block.text().size() >= pos ) break;
			block = block.next();

		}
	} else if( pc.m_anchorChar == QChar(U_KEISEN_BLOCK) ) {		//	罫線ブロックの場合
		while( block.isValid() ) {
			if( blockType(block) == U_KEISEN_BLOCK ) {
				while( blockType(block) == U_KEISEN_BLOCK ) {
					block = block.next();
					if( !block.isValid() ) break;
				}
				if( block.position() > pos )
					break;
				++count;
			} else
				block = block.next();
		}
	} else if( pc.m_anchorChar == ListBullet ) {		//	リスト行の場合
		while( block.isValid() ) {
			if( blockType(block) == BT_LIST ) {
				if( block == block0 )
					break;
				++count;
			}
			block = block.next();
		}
	} else {
		for (int i = block.position(); i < pos; ++i) {
			QChar chAt = doc->characterAt(i);
			if (chAt == QChar::ParagraphSeparator)		//	改行記号の場合
				chAt = ETX;
			if( chAt == pc.m_anchorChar )
				++count;
		}
	}
	pc.m_nth = count;
	return pc;
}
