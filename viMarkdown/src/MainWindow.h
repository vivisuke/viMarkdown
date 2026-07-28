#pragma once

#include <QtWidgets/QMainWindow>
#include <QTextCursor>
#include <QSplitter>
#include <QTimer>
//#include "ui_MainWindow.h"
//#include "markdowntohtmlconvertor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindowClass; };
QT_END_NAMESPACE

#define	KEY_VI_KEYBINDINGS			u"viKeybindings"
#define	KEY_IGNORE_CASE				u"ignoreCase"
#define	KEY_REGEXP					u"regexp"
#define	KEY_CLEAR_OUTPUT			u"clearOutput"
#define	KEY_EDITOR_FONT_SIZE		u"editorFontSize"
#define	KEY_PREVIEW_FONT_SIZE		u"previewFontSize"
#define	KEY_ACTIVA_LINE_COLOR		u"activeLineColor"
#define	KEY_INACTIVA_LINE_COLOR		u"inactiveLineColor"
#define	KEY_HEADINGS_COLOR			u"headingsColor"
#define	KEY_BOLD_ITALIC_COLOR		u"boldItalicColor"
#define	KEY_BOLD_COLOR				u"boldColor"
#define	KEY_ITALIC_COLOR			u"italicColor"
#define	KEY_STRIKETHROUGH_COLOR		u"strikethroughColor"
#define	KEY_MATCH_COLOR				u"matchColor"
#define	KEY_TABLE_TEXT_COLOR		u"TableTextColor"
#define	KEY_CSV_HEADER_COLOR		u"CSVHeadingsColor"
#define	KEY_CSV_ZEBRA_COLOR1		u"CSVZebraColor1"
#define	KEY_CSV_ZEBRA_COLOR2		u"CSVZebraColor2"
#define	KEY_QUOTE_COLOR				u"quoteColor"
#define	KEY_CODE_BLOCK_COLOR		u"codeBlockColor"
#define	KEY_KEISEN_BLOCK_COLOR		u"keisenBlockColor"
#define	KEY_LANGUAGE				u"language"
#define	KEY_AUTO_SVG_CMPL			u"autoSvgCmpl"
#define	KEY_DEFAULT_DIR				u"defaultDir"

enum {
    SystemDefault = 0,  // Localeに従う（OSの設定に依存）
    English,            // 強制的に英語
    Japanese            // 強制的に日本語
};

enum CharType {
	Type_Other,
	Type_Space,
	Type_NewLine,
	Type_Kanji,
	Type_Hiragana,
	Type_Katakana,
	Type_HalfAlphaNum,
	Type_FullSymbol
};
//	ブロックフラグ
enum {
	BT_DEFAULT = 0xff,	//	本文など
	//BT_BODY = 0,		//	本文など
	BT_HEADING = 0,		//	タイトル・見出し
	BT_LIST,			//	リスト
	BT_CHECKBOX,		//	チェックボックス
	BT_NUMLIST,			//	連番
	BT_IN_COMMENT,		//	行頭がコメントブロック（<!-- -->）の中
	BT_CODE_BLOCK,		//	コードブロック内（最初の``` 行も含む）
	BT_CODE_BLOCK_END,	//	コードブロック最後の```
	BT_CSV_BLOCK,		//	CSVブロック内 ```CSV ～ ```
	BT_KEISEN_BEGIN,	//	罫線ブロック開始 ```keisen
	BT_KEISEN_BLOCK,	//	罫線ブロック内 ```keisenの次行 ～ ```
	BT_SVG_BEGIN,		//	SVGブロック開始 ```SVG
	BT_SVG_BLOCK,		//	SVGブロック内 ```SVGの次行 ～ ```
	BT_TABLE,			//	マークダウンテーブル前後ダミーブロック？
	BT_CELL,			//	マークダウンテーブル内セル
	BT_CELL_DQ,			//	"" で囲まれたテーブル内セル
};
using uchar = unsigned char;
const int BLOCK_FLAG_BITS = 0xff;		//	userState の下位8bitがブロックタイプ
const int BLOCK_FOLDED = 0x0100;		//	見出し行の子テキストが折り畳まれているか？
const int BLOCK_GFLAG = 0x1000;			//	:{range}g/pat/ フラグ

const int MINMAP_WIDTH = 40;

const QChar STX(0x0002);		//	行頭仮想文字
const QChar ETX(0x0003);		//	行末仮想文字
const QChar EOB(0x0017);		//	罫線ブロック末仮想文字

struct PosContext {
    QChar	m_anchorChar;			//	アンカー文字、行末の場合は QChar() かもしれない
    int		m_nth = 0;				//	直前見出しからの m_anchorChar 出現回数 (1 org. 自分自身も含むため)、0 for 無効
    int		m_offset = 0;			//	m_anchorChar からカーソル位置までの距離（0以上）
    int		m_srcHBlockNum = 0;		//	直前見出し行ブロック番号（0 org.）、見つからない場合は 0
    int		m_prvHBlockNum = 0;		//	直前見出し行ブロック番号（0 org.）、見つからない場合は 0
};
#define		U_KEISEN_BLOCK	0xe000		//	罫線ブロック文字 ユニコード（PAU)

class MarkdownEditor;
class MarkdownPreview;
class DocWidget;
class BlockData;
class QTreeWidgetItem;

#define		N_PATH_HIST		32

bool isCommentOuted(const BlockData* data);

//using QStringPair = std::pair<QString, QString>;
struct DocLocation {		//	Alt + 左右 ナビゲーション用
	QString	m_title;
	QString	m_fullPath;
	//int		m_cursorLine;		//	0 オリジンブロック番号
	int		m_position;		//	カーソル位置
public:
	DocLocation(const QString title = QString(), const QString fullPath = QString(), int pos = -1)
		: m_title(title), m_fullPath(fullPath), m_position(pos)
	{
	}
};

struct Global {
	bool	m_cursorVisible = true;
	bool	m_ignoreCase;			//	検索時：大文字小文字同一視
	bool	m_regexp;				//	正規表現検索
	bool	m_clearOutput;			//	grep前に Output をクリア
	bool	m_japanese;
	bool	m_auto_svg_completer;	//	SVG補完ダイアログ自動表示
	bool	m_viKeybindings;		//	vi コマンドキー割り当て有効/無効
	//bool	m_editBlockOpen = false;
	bool	m_searchForward = true;		//	/ or ? 検索、true for /
	int		m_editorFontSize;
	int		m_previewFontSize;
	int		m_matchedPosition;		//	マッチ位置
    QString	m_defaultDir;			//	（load, save) デフォルトディレクトリ
    QString	m_lastSearchedPat;		//	最新検索文字列
    //QTextCursor	m_matchedCursor;	//	一致位置カーソル
	QColor	m_activeLnColor;		//	アクティブ時アンダーライン行カーソル色
	QColor	m_inactiveLnColor;		//	非アクティブ時アンダーライン行カーソル色
	QColor	m_headingsColor;
	QColor	m_boldItalicColor;
	QColor	m_boldColor;
	QColor	m_italicColor;
	QColor	m_strikethroughColor;
	QColor	m_matchColor;			//	検索マッチ背景色
	QColor	m_tableTextColor;		//	表テキスト色
	QColor	m_CSVHeaderColor;
	QColor	m_CSVZebraColor1;		//	本体奇数行
	QColor	m_CSVZebraColor2;		//	本体偶数行
	QColor	m_quoteColor;			//	引用ブロック背景色
	QColor	m_codeBlockColor;		//	コードブロック背景色
	QColor	m_keisenBlockColor;		//	罫線ブロック背景色
};
enum class ViMode {
    Normal = 0,
    Insert,
    Replace,
    CommandLine,		//	for :/?
};
struct ViStatus {
    ViMode	m_currentMode = ViMode::Insert;
    //bool	m_viCmdMode = false;		//	false for Insert mode
    //bool	m_cmdlineMode = false;		//	:/? 表示・編集用
    bool	m_isEditCommand = false;
    bool	m_linewiseMoved = false;	//	行単位移動した（jkG等）
    bool	m_linewiseYanked = false;	//	行単位にヤンクされた
    bool	m_redoing = false;			//	. コマンド処理中
    //bool	m_joinEditBlock = false;
    bool	m_recInsertedText = false;	//	挿入文字列保存
    bool	m_inGlobal = false;			//	:g 処理中
    QChar	m_operator = u' ';			//	{c|d|y|<|>}<move> の先頭部分
    QChar	m_prefix = u' ';			//	{z|g|r|]|[}<char> の先頭部分
    QChar	m_fFtT = ' ';				//	{fFtT}
    QChar	m_last_fFtT = ' ';			//	{fFtT}
    QChar	m_last_fFtT_char = ' ';		//	{fFtT}
    QChar	m_vMode = ' ';				//	v V モード
	//int		m_preferred_x = 0;			//	本来の x 座標
	int		m_vAnchor;					//	v コマンド用アンカー位置（position）
	int		m_vAnchorBlock;				//	v コマンド用アンカー位置（blocknumber）
    int		m_opCount = 1;				//	op 繰り返し回数 for c d y
    int		m_repeatCount = 0;			//	vi コマンド繰り返し回数
    int		m_insRepCount = 1;			//	挿入回数
    int		m_rangeStart = 1;
    int		m_nRange = 0;				//	指定された行番号数
    int		m_rangeEnd = 1;
    int		m_exhist_ix = -1;			//	現コマンド履歴インデックス
	QString	m_lastEditCommand;			//	最後に実行した vi 編集コマンド
	QString	m_pendingCommand;			//	入力中の vi コマンド
	QString	m_insertedText;				//	i 等で挿入された文字列
    QString	m_yankBuffer;
    QStringList	m_exhist;				//	ex コマンド履歴、先頭が最新コマンド
    MarkdownEditor	*m_editor = nullptr;		//	フォーカスを持っているマークダウンエディタ
    MarkdownPreview	*m_preview = nullptr;	//	フォーカスを持っているマークダウンプレビュー
    QWidget	*m_prevFocusWidget = nullptr;	//	:/? 押下時点でフォーカスを持っていた Widget
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    bool	isKeisenMode() const { return m_keisenMode; }
    bool	isThickKeisenMode() const { return m_thickKeisen; }
    //int		editorFontSize() const { return m_editorFontSize; }		//	暫定的
    //const QString	srcText() const { return m_srcText; }
    void	setCursorCyncing(bool b = true) const { m_isCursorSyncing = b; }
    bool	isCursorCyncing() const { return m_isCursorSyncing; }
    bool	isEdittingInPreview() const { return m_edittingInPreview; }
    bool	do_open(const QString& title, const QString& fullPath, const QString name = QString(), bool readOnly = false);
    void	do_open_pl(const QString fullPath, int ln);
    void	syncEditorToPreviewCursor();
    void	onChangeEditorFontSize(int);
    void	onChangePreviewFontSize(int);
    void	exitInsertMode(QTextCursor& cursor);
    void	do_diff();

protected:
    void	load_settings();
    void	save_settings();
    void	setup_statusBar();
    void	setup_connections();
    void	setup_tabMenu();
    void	setup_encodingCombo();
    void	restore_win();
    DocWidget	*newTabWidget(const QString& title, const QString& fullPath, bool readOnly = false);
    MarkdownEditor	*newEditor(DocWidget*, QSplitter*, bool);
    MarkdownPreview	*newPreview(DocWidget*, QSplitter*, bool);
    //void	updatePanes(DocWidget*);						//	スプリッター下の各ペインの表示・非表示設定
    void	onMDTextChanged();
    void	onDiffViewChanged();
    void	invertActionIcons(QMenuBar*);
    void	invertActionIcons(QMenu*);
    QSplitter	*getCurTabSplitter();
    DocWidget	*getCurDocWidget();
    //DocWidget	*findDocWidget(const QString& fullPath);
    int		tabIndexOf(const QString& title, const QString& fullPath);
    void	addTab(const QString& title, const QString fullPath = "", const QString txt = "",
    				bool=true, QStringConverter::Encoding encoding = QStringConverter::Utf8, bool readOnly = false);
    void	addTopItemToTreeWidget(const QString& title, const QString fullPath);
    void	updateHTMLModeCheck();
    void	updateThinThickCheck();
    void	updateSearchOptions();
    void	updatePreview();
    void	updateOutlineTree();
    void	insertInline(const QString&);
    QTreeWidgetItem* findTopLevelItemByFullPath(const QString& title, const QString fullPath);
    void	removeTopLevelItem(DocWidget*);
    void	do_load(const QString&);
    void	close_empty_doc();
    void	do_save(bool fDialog = false);
    int		treeItemToTabIndex(QTreeWidgetItem *current);
    void	addToRecentFiles(const QString& fullPath);
    void	insertSearchComboBox();
    void	do_find(bool backward=false);
    void	do_search(const QString txt, bool backward);
    void	do_replace_next(const QString, const QString);
    void	do_replace_all(const QString, const QString);
    void	do_undo_replaceDlg();
    void	do_redo_replaceDlg();
    void	zoomIn();
    void	zoomOut();
    void	onImeLocaleChanged();

    void	onOutlineBarVisibilityChanged(bool visible);
    void	onOutputBarVisibilityChanged(bool visible);
    void	onTreeCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void	onTreeItemActivated(QTreeWidgetItem *current, int);
    void	onMdEditCurPosChanged();
    void	onMdEditTabPressed();
    void	onMdEditEscPressed();
    void	onFileChanged(const QString&);
    void	onModificationChanged(bool);
    //void	onSearchCBActivated();
    void	onMarkdownPreviewLineClicked(/*int,*/ bool);
    //void	onEditorCurPosChanged();
    void	onPreviewCurPosChanged();
    //void	onAnchorClicked(const QString&);
    void	onCurrentTabChanged(int);
    void	printDocLocHist() const;
    void	appendToDocLoc(const QString&, const QString&, int);
    void	removeHistoryEntries(const QString&, const QString&, int);
    void	onEncodingChanged(int);
    void	onTextInsertedAtPreview(QString);
    void	onTextRemovedAtPreview(int);
    void	onEnter_pressed();
    void	onTab_pressed();
    void	onBS_pressed(bool);
    void	onDel_pressed(bool);
    //void	syncPreviewCursorWithEditor();
    void	updateEditorFontSize(int);
    void	updatePreviewFontSize(int);
    void	onSettingsChanged();
    void	onCutTriggered();
    void	onUndoTriggered();
    void	onRedoTriggered();
    void	onPrvPosContextChanged(const struct PosContext&, const PosContext &acontext);
    void	onSrcPosContextChanged(const struct PosContext&, const PosContext &acontext);
    //void	onSrcCursorPosChanged();
    void	syncEditorPreviewScroll();
    void	test_charFlags(DocWidget*);
    void	test_contextAt(DocWidget*);
    void	do_test(int type);
    void	do_test(DocWidget*, int nth_path);
    void	run_previewTestScript(const QString&);
    void	do_output(const QString&);
    void	toggleCursor();
    void	do_settings(int page=0);
    void	do_close(bool forced = false);
    void	diffview_open();
    void	updateUndoRedoEnabled();

    void	onAction_New();
    void	onAction_NewTab();
    void	onAction_Open();
    void	onAction_Save();
    void	onAction_SaveAs();
    void	onAction_ExportAsPDF();
    void	onAction_Close();
    void	onAction_Print();
    void	onAction_Undo();
    void	onAction_Redo();
    void	onAction_Cut();
    void	onAction_SelectAll();
    void	onAction_Paste();
    void	onAction_Heading();
    void	onAction_Bold();
    void	onAction_Italic();
    void	onAction_Strikethrough();
    void	onAction_Link();
    void	onAction_List();
    void	onAction_NumList();
    void	onAction_Checkbox();
    void	onAction_Indent();
    void	onAction_UnIndent();
    void	onAction_AlignLeft();
    void	onAction_AlignCenter();
    void	onAction_AlignRight();
    void	onAction_TodayString_1();
    void	onAction_TodayString_2();
    void	onAction_TodayString_3();
    void	onAction_CSV_MarkdownTable();
    void	onAction_MarkdownTable_CSV();
    void	onAction_NaviForward();
    void	onAction_NaviBack();
    void	onAction_IgnoreCase(bool);
    void	onAction_RegExp(bool);
    void	onAction_Find();
    void	onAction_ForwardAgain();
    void	onAction_BackwardAgain();
    void	onAction_FindWord();
    void	onAction_ClearSearchHighlights();
    void	onAction_Replace();
    void	onAction_Grep();
    void	onAction_DiffMode(bool);
    void	onAction_DiffWithFile();
    void	onAction_KeisenMode(bool);
    void	onAction_ThinKeisen(bool);
    void	onAction_ThickKeisen(bool);
    void	onAction_OpenPrev();
    void	onAction_OpenNext();
    void	onAction_HTML(bool);
    void	onAction_Source(bool);
    void	onAction_OutlineBar(bool);
    void	onAction_FocusOutline();
    void	onAction_OutputBar(bool);
    void	onAction_ClearOutput();
    void	onAction_SlideShow();
    void	onAction_NextTab();
    void	onAction_PrevTab();
    void	onAction_ToggleFocus();
    void	onAction_SwitchToAltFile();
    void	onAction_TagJump();
    void	onAction_About();
    void	onAction_AddThisFavorite();
    void	onAction_Language();
    void	onAction_Settings();
    void	onAction_ColorSettings();
    void	onAction_ViKeybindings(bool);
    void	onAction_Help();
    void	onAction_MarkdownCheatSheet();
    void	onAction_ViCheatSheet();
    void	onAction_Test();
    void	onAction_TestCharFlags();
    void	onAction_TestContextAt();
    void	onAction_TestLineCrsp();
    void	onAction_TestEtoPCurSync();
    void	onAction_TestViCommands();
    void	onAction_TestAll();
    //void	onAction_DumpCharFlags();
    void	onAction_DumpBlockUserData();
    void	onAction_DumpBlockUserStates();
    void	onAction_RunPTS();
    void	onAction_Exit();

    void	onAboutToShow_RecentFiles();
    void	onAboutToShow_FavoriteFiles();
    void	onAboutToShow_Insert();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result);

    bool ASSERT(bool actual, int ln);
    bool ASSERT_EQ(bool expected, bool actual, int ln);
    bool ASSERT_EQ(int expected, int actual, int ln);
    bool ASSERT_EQ(int expected, int actual, int ln, QChar ch, int ix, int type);
    bool ASSERT_EQ(const QChar expected, const QChar actual, int ln);
    bool ASSERT_EQ(const QString &expected, const QString &actual, int ln);

    void	on_cmdLine_enter();
    void	on_cmdLine_escape();
    void	on_cmdLine_up();		//	↑
    void	on_cmdLine_down();		//	↓
    void	close_cmdLine();
    //void	do_fold(QTextBlock block, QTextDocument*);
    //void	do_unfold(QTextBlock block, QTextDocument*);
    void	do_viCmd(QChar cmd, QTextCursor&);
    void	do_exCmd(const QString&, QTextCursor&);
    void	do_exCmd(const QString&, int, QTextCursor&, QTextDocument*, DocWidget*);
    void	do_vi_insert(QChar cmd, QTextCursor&, int rcnt);		//	iIaAoOsS
    void	do_vi_delete(QChar cmd, QTextCursor&, int rcnt);		//	xXD
    void	do_prefix_cmd(QChar cmd, QTextCursor&, int rcnt, DocWidget*);		//	{g z [ ] r}<cmd>
    bool	do_vi_operator(QChar cmd, QTextCursor&, int rcnt, DocWidget*);		//	{c|d|y|<|>}<move>
    void	do_vi_motion(QChar cmd, QTextCursor&, int rcnt, DocWidget*);		//	hjklwW...
    void	do_vi_search(const QString&, QTextCursor&, int rcnt, DocWidget*);
    void	do_subst(const QString &, int, QTextDocument*);
    void	do_global(const QString &, int, QTextCursor&, QTextDocument*, DocWidget*);
    bool	do_cdy(QChar cmd, QTextCursor& cursor);
    void	do_cdy_moved(QTextCursor& cursor);
    void	save_preffered_x();

private:
    int		m_processing = 0;
    //bool	m_ignoreContentsChanged = false;
    mutable bool	m_isCursorSyncing = false;		//	カーソル同期処理中
    bool	m_opening_file = false;
    bool	m_ignore_changed = false;
    bool	m_htmlMode = true;
    bool	m_keisenMode = false;
    bool	m_thickKeisen = false;		//	Thin or Thick
    bool	m_edittingInPreview = false;		//	プレビューでの編集処理中
    int		m_tab_number = 0;
    int		m_QA_tab_number = 0;
    //int		m_editorFontSize = 12;		//	暫定的
    //QString	m_plainText;
    //QString	m_htmlText;
    //MarkdownToHtmlConvertor	m_htmlComvertor;
    QString		m_curFullPath;			//	現文書フルパス
    QString		m_curTitle;				//	現文書タイトル
    QString		m_altFullPath;			//	直前文書フルパス
    QString		m_altTitle;				//	直前文書タイトル
    //QString		m_srcText;				//	検索文字列
    QStringList	m_searchHist;			//	検索履歴、先頭が最新検索文字列
    QStringList	m_grepDirHist;			//	grep対象ディレクトリ履歴、先頭が最新
    QStringList	m_replaceHist;			//	置換履歴、先頭が最新置換文字列
    class QLineEdit		*m_cmdLine;		// ステータスバー左端のコマンドライン
    class QLabel		*m_lcLabel;		//	行カラムラベル
    class QLabel		*m_encLabel;	//	文字エンコーディングラベル
    class QComboBox				*m_encodingCombo = nullptr;
    class QComboBox				*m_searchCB = nullptr;
    class QFileSystemWatcher	*m_watcher;
    //QList<QStringPair>			m_pathTitleList;
	QLabel	*m_diffviewLabel = nullptr;
	QTimer	*m_blinkTimer;
    //bool	m_cursorVisible = true;
    int							m_docLocIX = -1;	//	m_docLocHist の現文書インデックス
    QList<DocLocation>			m_docLocHist;		//	文書履歴、新しい文書情報を末尾に置く（push_back）

    Ui::MainWindowClass *ui;
};

