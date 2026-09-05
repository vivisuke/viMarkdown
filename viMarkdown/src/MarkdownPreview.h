#pragma once
#include <vector>
#include <QTextCursor>
#include <QDragEnterEvent>
#include <QScrollBar>
#include <QTextEdit>
//#include <QTimer>
//#include "C:\Qt\6.10.0\msvc2022_64\include\QtWidgets\qtextedit.h"

class MainWindow;
class DocWidget;

/*
	マークダウンプレビューを表示するためのクラス

	QTextBlock.userState : int型：行種別情報（BT_XXX）

*/

class MarkdownPreview : public QTextEdit
{
	Q_OBJECT 

public:
    MarkdownPreview(const MainWindow*, DocWidget*, QWidget* parent = nullptr, bool readOnly = false);

public:
    bool	isProcessing() const { return m_processing; }
    void	insertMarkdown(class QTextDocument*, QTextCursor&, const QStringList&);
    void	setMarkdown(class QTextDocument*);
    const QStringList&	getHeadings() const { return m_headingList; }
    //const std::vector<int>&	getSrcHeadingsBlocks() const { return m_srcHeadingBlocks; }
    //const std::vector<int>&	getPrvHeadingsBlocks() const { return m_prvHeadingBlocks; }		//	見出し行だけの行番号（0 org）リスト
    void	setCursorAt(int srcBlockNum, QString=QString(), int=0);
    void	setCursorAtNthPat(int srcBlockNum, QString pat, int nth, bool=false);
    void	ensureLineVisible(int srcBlockNum);
    void	scrollToBlock(int blockIndex);
    struct PosContext	contextAt(int pos);
    int		prvToSrcHeading(int blockNum);		//	プレビューの見出し行番号（0 org.）をエディタのそれに変換
	void	setCursorByContext(const struct PosContext &context, const PosContext &acontext);
    int		findPosition(const struct PosContext&);
	void	moveToNextWord(QTextCursor& cursor, bool select);
	void	moveToNextWordEnd(QTextCursor& cursor, bool select);
	void	moveToPrevWord(QTextCursor& cursor, bool select);
	void	moveToStartOfWord(QTextCursor& cursor, bool select);
	void	moveToEndOfWord(QTextCursor& cursor, bool select);
#if 0
	void	scrollToTop(int lineNum) {		//	lineNum: 0 org.
		verticalScrollBar()->setValue(lineNum);
	}
	void	scrollToTop(const QTextCursor &cursor);
	int getVisualLineNumber(const QTextCursor &cursor) const;
#endif

signals:
    // クリックされたブロック番号を通知するシグナル
    void	lineClicked(int blockNumber);
    void	checkboxLineClicked(/*int nth,*/ bool checked);
    void	anchorClicked(const QString &title, const QString &anchor, const QString name, bool readOnly = false);
    void	textInserted(QString);
    void	textRemoved(int);
    void	Enter_pressed();	//	or Return
    void	Tab_pressed();		//	Tab
    void	BS_pressed(bool);		//	BackSpace
    void	Del_pressed(bool);		//	Delete
    void	cut_triggered();
    void	undo_triggered();
    void	redo_triggered();
    void	posContextChanged(const PosContext &context, const PosContext &acontext);
    void	fontSizeChanged(int delta);
    void	do_output(const QString&);
    void	do_viCmd(QChar, QTextCursor&);

protected:
    void	focusInEvent(QFocusEvent *event) override;
    void	keyPressEvent(QKeyEvent *e) override;
    void	mousePressEvent(QMouseEvent *e) override;
    void	mouseMoveEvent(QMouseEvent *e) override;
    void	mouseReleaseEvent(QMouseEvent *e) override;    // マウスクリックイベントをオーバーライド
    void	mouseDoubleClickEvent(QMouseEvent *e) override;
    void	wheelEvent(QWheelEvent *event) override;
    void	paintEvent(QPaintEvent *e) override;
    void	dragEnterEvent(QDragEnterEvent *e) override {
    	e->ignore();
        //e->acceptProposedAction();
    }
    void	dropEvent(QDropEvent *e) override {
    	e->ignore();
    }
    void	inputMethodEvent(QInputMethodEvent *event) override;

    void	onAction_Cut();

    void	onCursorPosChanged();
    void	onContentsChanged(int position, int charsRemoved, int charsAdded);
    //bool	isTableLine(const QString&);
    //bool	isTableHyphenLine(const QString&);
    int		countCheckBox(QTextBlock block);
    //void	toggleCursor();

    void	do_body(QTextBlock srcBlock, QTextCursor&, bool = false);
    void	do_body_sub(QTextCursor&, const QString&);
    void	do_table(QTextBlock&, QTextCursor&);
    bool	do_underlineHeading(QTextCursor&, QString buf);
    void	do_heading(QTextBlock&, QTextCursor&, QString buf);
    void	do_heading_sub(QTextCursor&, QString buf, int h, int ln);
    void	do_list(QTextBlock srcBlock, QTextCursor&, QString buf);
    void	do_numlist(QTextBlock srcBlock, QTextCursor&, QString buf);
    void	do_quote(QTextBlock&, QTextCursor&, QString buf);
    void	do_code(QTextBlock, QTextCursor&);
    void	do_keisen_block(QTextBlock&, QTextCursor&);
    void	do_SVG(QTextBlock&, QTextCursor&);
    void	do_CSV(QTextBlock&, QTextCursor&);

private:
    int		m_ln;
    int		m_nSpaces;
    //int		m_nEmptyLines = 0;		//	本文最後の空行数
    bool	m_hasBody = false;		//	do_body() で空文以外を出力したか？
    bool	m_inComment = false;
    bool	m_processing = false;	//	再入防止用フラグ
    bool	m_procContentsChanged = false;	//	コンテンツ変更通知処理中
	bool	m_isComposing = false;			// IME入力中フラグ
	bool	m_isPrevLineEmpty = false;		//	本文最後が空行か？
	QString	m_commitString;
	QString	m_lastCurBlockText;				//	事前のカーソルブロックテキスト
    //QString	m_bodyText;
    int		m_bodyLineNum = -1;		//	m_bodyList に入っている最初の行のソース行番号
    QStringList	m_bodyList;
	QStringList	m_lst;
	QStringList	m_headingList;		//	見出しレベル（'1'～'9'）＋見出し文字列
	//std::vector<int>	m_srcHeadingBlocks;		//	各見出し行 ブロック番号（0 org.）in マークダウンソース
	//std::vector<int>	m_prvHeadingBlocks;		//	各見出し行 ブロック番号（0 org.）in マークダウンプレビューワ
	//QList<QStringView>	m_tableTokens;
	QStringList			m_tableTokens;
	std::vector<char>	m_tableAlign;		//	各カラムの水平方向アライメント
	//QTimer	*m_blinkTimer;
    //bool	m_cursorVisible = true;

	class DocWidget	*m_docWidget;
    const MainWindow *m_mainWindow = nullptr;
};

