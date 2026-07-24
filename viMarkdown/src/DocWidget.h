#pragma once
#include <QStringConverter>
#include <QFrame>
#include <QWidget>
#include <QPlainTextEdit>
#include <QPainter>

//#include "C:\Qt\6.10.0\msvc2022_64\include\QtWidgets\qwidget.h"
//#include "markdowntohtmlconvertor.h"

#define		CODE_IMAGE		0xfffc		//	プレビュー：画像アイコン
#define		LINE_SEPARATOR	0x2028		//	リスト文字列内改行コード

#define		ADDED_LINE		1
#define		CHANGED_LINE	2

//const int MINMAP_WIDTH = 40;

//class QPlainTextEdit;
class MarkdownEditor;
class QTextEdit;
class MarkdownPreview;

enum {
    PCF_VISIBLE = 0,	// プレビューに表示される
    PCF_IMAGE_BEGIN,	//	最初の '!' 位置
    PCF_NOT_VISIBLE,	//	以下以外で非表示
    PCF_COMMENTED,		//	コメントアウトされた文字
    PCF_ESCAPE,			//	エスケープ文字
    PCF_HEADING,		//	タイトル・見出し行
    PCF_LIST_MARK,		// "- " などリストマーカー
    PCF_NUM_LIST,		//	"1. " 連番
    PCF_QUOTE,
    PCF_LINK,
    PCF_IMAGE,
    PCF_CODE,			// ```
    PCF_CSV,
    PCF_TABLE,			//	マークダウン表要素
    //PCF_CELL,			//	表セルブロック
    PCF_CELL_SEPARATOR,	//	表区切り文字（, |）、ただし行頭・行末の | は区切り文字と見なさない
    PCF_KEISEN,
    PCF_SVG,
    PCF_EMPHASIZED,		//	ボールド、イタリック等
};
class BlockData : public QTextBlockUserData
{
public:
    QByteArray m_charFlags;   // 1 char = 1 byte
};

BlockData* getBlockData(QTextBlock srcBlock, bool init = false /*, int length = 0*/);
const QByteArray& getCharFlags(QTextBlock srcBlock);
void printCharFlags(QTextBlock block);
void printCharFlags(const BlockData*);

bool parseCsvLine(QStringList &fields, QByteArray&, const QString &line, bool inQuotes, bool &inComment, bool &commented, BlockData* = nullptr);
enum {
	ALIGHN_LEFT = 1, ALIGHN_RIGHT = 2, ALIGHN_CENTER = ALIGHN_LEFT| ALIGHN_RIGHT,
};
//bool isTableLine(const QString& lnStr0, const QString& lnStr, QList<QStringView> &tableTokens, BlockData* = nullptr);
bool isTableLine(const QString& lnStr0, const QString& lnStr, QStringList &tableTokens, BlockData* = nullptr);
bool isTableHyphenLine(const QString& lnStr, std::vector<char> &tableAlign, BlockData* = nullptr);
bool isUnderlineHeading(const QString& txt);
bool updateCharFlags(BlockData* data, const QString &buf, int ix, int ix9, bool esc = false);
void updateCharFlags(QTextBlock);

enum class DocType {
    Markdown,
    Plain,
    //Diff
};

//using DiffView = QPlainTextEdit;
using DiffView = MarkdownEditor;
//using MiniMap = QWidget;
class MiniMap : public QWidget {
	//##Q_OBJECT
public:
	explicit MiniMap(QWidget* parent = nullptr) : QWidget(parent)
	{
		resize(MINMAP_WIDTH, 100);
	}
	void resize(int width, int height) {
		m_mapPixmap = QPixmap(width, height);
		m_mapPixmap.fill(Qt::green);
	}
	void updateMap(QTextDocument* doc1, QTextDocument* doc2); 
protected:
    void paintEvent(QPaintEvent* event) override {
    	if (m_mapPixmap.isNull()) return;
    	QPainter p(this);
		p.drawPixmap(0, 0, m_mapPixmap);
		int y = m_firstVisibleLine;
		int w = width() - 1;
		int h = m_visibleLines;
		p.setPen(QPen(Qt::blue, 1, Qt::SolidLine));
        p.drawRect(0, y, w, h);
    }
public:
	int		m_firstVisibleLine = 0;			//	0 オリジン
	int		m_visibleLines = 10;			//	ビュー表示行数
	QPixmap	m_mapPixmap;
};

class DocWidget : public QWidget
{
public:
	DocWidget(const QString& title, const QString& fullPath, QWidget* parent = nullptr);

	bool	isModified() const;
	void	setModified(bool);
	QString	getTitle() const;
	int		previewPosToEditorPos(int pos);		//	プレビュー position をエディタの対応 position に変換
    const std::vector<int>&	getSrcHeadingsBlocks() const { return m_srcHeadingBlocks; }
    const std::vector<int>&	getPrvHeadingsBlocks() const { return m_prvHeadingBlocks; }		//	見出し行だけの行番号（0 org）リスト
    int		prvToSrcHeading(int blockNum);		//	プレビューの見出し行番号（0 org.）をエディタのそれに変換
    int		srcToPrvHeading(int blockNum);		//	エディタの見出し行番号（0 org.）をプレビューのそれに変換
    void	setEditorCurPos(int pos);
    void	updatePanes();						//	スプリッター下の各ペインの表示・非表示設定
    void	removeDummyBlocks();
    void	setMiniMapCurHeight(int vl);
    void	setMiniMapCurPos(int fvl, int vl);

    void	syncScrollFromLeft(int value);
    void	syncScrollFromRight(int value);
    void	syncMinimapWithEditor(int value);

public:
	DocType		m_docType = DocType::Markdown;
	//bool	m_modified = false;		//	編集＆未保存状態
	bool	m_diffMode = false;		//	diff 表示中か？
	bool	m_saving = false;
	bool	m_hasSaved = false;		//	保存直後
	bool	m_withBOM = true;		//	BOM付き
	bool	m_readOnly = false;
	bool	m_isSyncingScroll = false;		// 再入防止フラグ
	QStringConverter::Encoding m_encoding = QStringConverter::Utf8;
	QString	m_title;				//	タブタイトル
	QString	m_fullPath;
	//QPixmap				*m_mmPixmap = nullptr;	//	ミニマップ用画像
	QFrame				*m_headerWidget = nullptr;
	MarkdownEditor		*m_editor = nullptr;	//	マークダウンエディタへのポインタ
	MarkdownPreview		*m_preview = nullptr;	//	マークダウンプレビューワへのポインタ
	MiniMap				*m_minimap = nullptr;
	DiffView			*m_diffview = nullptr;	//	比較相手ビュー（QPlainTextEdit 派生クラス）
	std::vector<int>	m_srcHeadingBlocks;		//	各見出し行 ブロック番号（0 org.）in マークダウンソース
	std::vector<int>	m_prvHeadingBlocks;		//	各見出し行 ブロック番号（0 org.）in マークダウンプレビューワ
    //MarkdownToHtmlConvertor	m_htmlComvertor;
};

