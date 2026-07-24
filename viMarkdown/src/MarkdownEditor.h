#pragma once
#include <QTextEdit>
#include <QScrollBar>
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
//#include <QTimer>
//#include "C:\Qt\6.10.0\msvc2022_64\include\QtWidgets\qplaintextedit.h"
#include "MainWindow.h"
#include "diff.h"

enum class Align { Left, Center, Right };

const int LN_WIDTH = 7;


class MainWindow;
class DocWidget;
//class QTextEdit;
//class SvgCompleter;

extern Global g;

class DiffHighlighter : public QSyntaxHighlighter {
public:
	DiffHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent) {}
	void updateInlineColors() {
		rehighlight(); // これを呼ぶことでドキュメント全体の highlightBlock が再実行される
	}
protected:
	void highlightBlock(const QString &text) override {
		if( m_highlightDiff ) {
			QTextBlock block = currentBlock();
			const auto *userData = dynamic_cast<const DiffBlockUserData*>(block.userData());
            if (userData) {
                QTextCharFormat format;
                format.setBackground(QColor("#ffc0c0")); 
                for (const auto &range : userData->ranges) {
                    setFormat(range.start, range.length, format);
                }
            }
		}
	}
public:
	bool	m_highlightDiff = false;
};
class MarkdownHighlighter : public QSyntaxHighlighter {
public:
	MarkdownHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent)
	{
		m_boldItalicFormat.setForeground(g.m_boldItalicColor);
		//m_boldFormat.setFontWeight(QFont::Bold);
		m_boldFormat.setForeground(g.m_boldColor);
		m_italicFormat.setForeground(g.m_italicColor);
		m_strikethroughFormat.setForeground(g.m_strikethroughColor);
		m_boldItalicRegex = QRegularExpression(R"(\*\*\*([^\*]+)\*\*\*)");
		m_boldRegex = QRegularExpression(R"((?<![\*\\])\*\*([^\\\*]+)\*\*)");
		m_italicRegex = QRegularExpression(R"((?<![\*\\])\*([^\\\*]+)\*)");
		m_strikethroughRegex = QRegularExpression(R"((?<![\*\\])(\~\~([^\\\*]+)\~\~))");
	}
	//void setBoldColor(const QColor &color) {
	//	  m_boldFormat.setForeground(color);
	//	  rehighlight(); // これを呼ぶことでドキュメント全体の highlightBlock が再実行される
	//}
	void updateInlineColors() {
		m_boldItalicFormat.setForeground(g.m_boldItalicColor);
		m_boldFormat.setForeground(g.m_boldColor);
		m_italicFormat.setForeground(g.m_italicColor);
		m_strikethroughFormat.setForeground(g.m_strikethroughColor);
		rehighlight(); // これを呼ぶことでドキュメント全体の highlightBlock が再実行される
	}
protected:
	void highlightBlock(const QString &text) override {
		if( m_highlightMarkdown ) {
			if (text.startsWith("#")) {
				QTextCharFormat fmt_darkred;
				fmt_darkred.setForeground(g.m_headingsColor);
				setFormat(0, text.length(), fmt_darkred);
			} else {
				// デフォルトの色（黒）
				//QPalette currentPalette = parentWidget->palette();
				//setFormat(0, text.length(), currentPalette.color(QPalette::Text));
				auto it = m_boldItalicRegex.globalMatch(text);
				while (it.hasNext()) {
					QRegularExpressionMatch match = it.next();
					setFormat(match.capturedStart(), match.capturedLength(), m_boldItalicFormat);
				}
				it = m_boldRegex.globalMatch(text);
				while (it.hasNext()) {
					QRegularExpressionMatch match = it.next();
					setFormat(match.capturedStart(), match.capturedLength(), m_boldFormat);
				}
				it = m_italicRegex.globalMatch(text);
				while (it.hasNext()) {
					QRegularExpressionMatch match = it.next();
					setFormat(match.capturedStart(), match.capturedLength(), m_italicFormat);
				}
				it = m_strikethroughRegex.globalMatch(text);
				while (it.hasNext()) {
					QRegularExpressionMatch match = it.next();
					setFormat(match.capturedStart(), match.capturedLength(), m_strikethroughFormat);
				}
			}
		}
#if 0
		else if( m_highlightDiff ) {
			QTextBlock block = currentBlock();
			const auto *userData = dynamic_cast<const DiffBlockUserData*>(block.userData());
            // 差分データが存在する場合のみ、インラインハイライトを適用
            if (userData) {
                QTextCharFormat format;
                // テキスト文字が見えづらくならない、淡いピンク（パステルレッド）の背景色
                format.setBackground(QColor("#ffc0c0")); 

                // 登録されている全ての部分範囲に対してフォーマットを適用
                for (const auto &range : userData->ranges) {
                    setFormat(range.start, range.length, format);
                }
            }
		}
#endif
	}
public:
	bool	m_highlightMarkdown = true;
	//bool	m_highlightDiff = false;
private:
	QTextCharFormat m_boldItalicFormat;
	QTextCharFormat m_boldFormat;
	QTextCharFormat m_italicFormat;
	QTextCharFormat m_strikethroughFormat;
	QRegularExpression m_boldItalicRegex;
	QRegularExpression m_boldRegex;
	QRegularExpression m_italicRegex;
	QRegularExpression m_strikethroughRegex;
};

class SvgCompleter : public QTextEdit
{
	Q_OBJECT
public:
	SvgCompleter(QWidget* parent, bool svgtag);
	//SvgCompleter(QWidget* parent) : QTextEdit(parent) {}

	const QString&	completerText() const { return m_cmpl_lst[m_curix]; }
	void	highlight_cur_line();
signals:
    void	enter_pressed();
    void	tab_pressed();
    void	esc_pressed();
protected:
    void	keyPressEvent(QKeyEvent *e) override;
private:
    int		m_curix = 0;		//	現補完候補
    QStringList	m_cmpl_lst;		//	補完候補
};

#define		MarkdownBaseEdit	QPlainTextEdit
//#define		MarkdownBaseEdit	QTextEdit		//	QPlainTextEdit にしかないシグナルがあるため無理

class MarkdownEditor : public MarkdownBaseEdit
{
	Q_OBJECT
public:
	MarkdownEditor(const MainWindow* mainWindow, DocWidget*, QWidget *parent = nullptr, bool readOnly = false);
	~MarkdownEditor();
public:
	void	scrollToTop(int lineNum) {		//	lineNum: 0 org.
		verticalScrollBar()->setValue(lineNum);
	}
	void	lnAreaPaintEvent(QPaintEvent *event);
	void	lnAreaMousePressEvent(QMouseEvent *event);
	void	lnAreaMouseMoveEvent(QMouseEvent *event);
	void	lnAreaMouseReleaseEvent(QMouseEvent *event);
	int lnAreaWidth() const {
        return fontMetrics().horizontalAdvance('9') * LN_WIDTH;
    }
	void	updateViewportMargines();
	void	scrollToTop(const QTextCursor &cursor);
	//{
	//	this->scrollToTop(cursor.blockNumber());
	//}
	int getVisualLineNumber(const QTextCursor &cursor) const;
	void	onAlignLeft();
	void	onAlignCenter();
	void	onAlignRight();
	void	onKeisenMode(bool);
	void	openPrev();
	void	openNext();
	void	insertTodayString(const QString &fmt);
	void	convert_CSV_MarkdownTable();
	void	convert_MarkdownTable_CSV();
    void	setCursorAtNthPat(int srcHeadingBlockNum, QString pat, int nth, bool=false);
    void	setProcessing(bool b) { m_processing = b; }
    void	setIgnoreCC(bool b) { m_ignoreContentsChanged = b; }
    void	rehighlight();
    //void	setBoldColor(QColor);
    void	updateInlineColors();
    void	highlightVText(QTextCursor);		//	v V で選択されたテキストを強調表示
    void	highlightSearchText(const QString &searchText);
    void	jumpToHeading(const QString &name);		//	見出し（name）行にカーソル設定
	void	moveToNextWord(QTextCursor& cursor, bool select);
	void	moveToNextWordEnd(QTextCursor& cursor, bool select);
	void	moveToPrevWord(QTextCursor& cursor, bool select);
	void	moveToStartOfWord(QTextCursor& cursor, bool select);
	void	moveToEndOfWord(QTextCursor& cursor, bool select);
	void	setCursorByContext(const struct PosContext &context, const PosContext &acontext);
    int		countCharUntil(QTextBlock block, int pos, QChar ch) const;
    struct PosContext	contextAt(int pos);		//	pos 位置情報を構築
    //int		srcToPrvHeading(int blockNum);		//	エディタの見出し行番号（0 org.）をプレビューのそれに変換
    int		findPosition(const struct PosContext&);
    void	syncPreviewCursorFromEditor();
    void	syncDiffViewCursorFromEditor();
    void	tagJump();
	void	make_link();
	int		linkClickedPos() const { return m_linkClickedPos; }
	bool	isComposing() const { return m_isComposing; }
	QString	autoTextIndent(QTextBlock);		//	オートテキスト・インデント 文字列取得
	void	insertEnter();		//	カーソル位置に改行挿入（＋オートテキスト・インデント）
	void	deleteWord();
	void	backSpaceWord();
    void	onCursorPosChanged();
    void	check_svg_completer();
    void	setDiffMode(bool b) { setHighlightMarkdown(!(m_diffMode = b)); }
    void	expandAll();		//	すべて展開
    void	setHighlightMarkdown(bool b) { m_highlighter->m_highlightMarkdown = b; }
    void	setHighlightDiff(bool b) { m_diffHighlighter->m_highlightDiff = b; }
    bool	dummyInserted() const { return m_dummyInserted; }
    void	setDummyInserted(bool b) { m_dummyInserted = b; }
    void	applyStyle(const QFont &font);
	void	removeAllDummyLines();
    void	do_insertText(QTextCursor& cursor, const QString&);		//	Undo/Redo 対象の挿入関数
    void	do_deleteText(QTextCursor& cursor);		//	Undo/Redo 対象の削除関数、選択・非選択状態対応
    bool	canUndo() const;
    bool	canRedo() const;
    void	openUndoBlock();
    void	closeUndoBlock();
    void	do_undo();
    void	do_redo();
    int		getVisibleLineCount() const;

signals:
    void	tab_pressed();
    void	esc_pressed();
    //void	title_clicked(const QString title, const QStrin);
    void	link_clicked(const QString& title, const QString&, const QString&, bool readOnly = false);
    void	changeFontSize(int delta);
    void	posContextChanged(const PosContext &context, const PosContext &acontext);
    void	do_viCmd(QChar, QTextCursor&);
    void	canUndoRedoChanged();

protected:
    void	keyPressEvent(QKeyEvent *e) override;
    void	mouseReleaseEvent(QMouseEvent *event) override;
    void	mouseDoubleClickEvent(QMouseEvent *e) override;
    void	wheelEvent(QWheelEvent *event) override;
    void	dragEnterEvent(QDragEnterEvent *event) override;
    void	dropEvent(QDropEvent *event) override;
    void	paintEvent(QPaintEvent *e) override;
    void	resizeEvent(QResizeEvent *event) override;
    void	inputMethodEvent(QInputMethodEvent *event) override;
    void	insertFromMimeData(const QMimeData *source) override;
    QVariant	inputMethodQuery(Qt::InputMethodQuery query) const ;
    void	contextMenuEvent(QContextMenuEvent *event);
    void	reflectWordToPeer(const QTextBlock &block, int start, int length, const QString &wordText);
#if 0
    void	dragEnterEvent(QDragEnterEvent *e) override {
        e->acceptProposedAction();
    }
    void	dropEvent(QDropEvent *e) override {
    	e->ignore();
    }
#endif
    void	updateLnArea(const QRect &rect, int dy);
    void	onContentsChanged(int position, int charsRemoved, int charsAdded);
    int		nColumn(const QString&) const;		//	表示カラム数を計算
    void	setLineSpacing(int percentage);
    void	tagJump_sub(QTextCursor);
    //void	getWordStartEnd(QTextCursor, int& start, int&end);
    void	selectWordAt(QTextCursor&);

    void	do_keisen_left(bool erase = false, bool thickKeisen = false);
    void	do_keisen_right(bool erase = false, bool thickKeisen = false);
    void	do_keisen_up(bool erase = false, bool thickKeisen = false);
    void	do_keisen_down(bool erase = false, bool thickKeisen = false);
	void	applyAlignment(Align align);
	bool	isInComment(int pos) const;
	bool	isInLinkURL(int pos, int& openIX, int& closeIX) const;
	void	svg_enter_pressed();
	void	svg_esc_pressed();
	void	applyDiffBlock(QTextBlock block);

private:
	bool	m_ignoreContentsChanged = false;
	bool	m_processing = false;			//	罫線保護処理中
	bool	m_isComposing = false;			// IME入力中フラグ
	bool	m_lnAreaPressed = false;
	bool	m_isCursorAboveAnchor = false;		//	アンカーから上（ドキュメント先頭方向）に向かって選択されている
	bool	m_diffMode = false;
	bool	m_dummyInserted = false;		//	ダミー行が挿入された
	int		m_anchorStartPosition = 0;
	int		m_anchorBlockNum = 0;
	int		m_curBlockNum = 0;
	int		m_selStart = 0;
	int		m_selEnd = 0;
	int		m_linkClickedPos = -1;			//	リンククリック位置
	int		m_charWidth = 0;
	//QTimer	*m_blinkTimer;
    //bool	m_cursorVisible = true;
	QString	m_lastCurBlockText;				//	事前のカーソルブロックテキスト
	//QString	m_completerText;
    class MarkdownHighlighter	*m_highlighter;
    class DiffHighlighter		*m_diffHighlighter;
	class LnAreaWidget	*m_lnAreaWidget = nullptr;
	class UndoMgr		*m_undoMgr = nullptr;
	SvgCompleter		*m_svgCompleter = nullptr;
    DocWidget			*m_docWidget;
    const MainWindow	*m_mainWindow = nullptr;
};

