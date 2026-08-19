#include <vector>
#include <QSplitter>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QComboBox>
#include <QClipboard>
#include <QLabel>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileSystemWatcher>
#include <QPrinter>
#include <QPrintDialog>
#include <QDesktopServices>
#include <QLineEdit>
#include <QToolButton>
#include <QTextTable>
#include <QRegularExpression>
#include <QStatusBar>
#include "ver.h"
#include "MainWindow.h"
#include "DocWidget.h"
#include "MarkdownEditor.h"
#include "MarkdownPreview.h"
#include "SettingsDialog.h"
#include "ReplaceDialog.h"
#include "diff.h"

using namespace std;

extern Global g;
extern ViStatus gvi;

uchar blockType(const QTextBlock &block);
void setBlockType(QTextBlock block, uchar type);

QChar g_flag_char[] = {
	u'v',	//	PCF_VISIBLE = 0,	// プレビューに表示される
	u'-',	//	PCF_COMMENTED,		//	コメントアウトされた文字
	u'E',	//	PCF_ESCAPE,			//	エスケープ文字
	u'H',	//	PCF_HEADING,		//	タイトル・見出し行
	u'L',	//	PCF_LIST_MARK,		// "- " などリストマーカー
	u'N',	//	PCF_NUM_LIST,		//	"1. " 連番
	u'Q',	//	PCF_QUOTE,
	u'K',	//	PCF_LINK,
	u'I',	//	PCF_IMAGE,
	u'C',	//	PCF_CODE,			// ```
	u'S',	//	PCF_CSV,
	u'T',	//	PCF_TABLE,			//	マークダウン表要素
	u'K',	//	PCF_KEISEN,
	u'=',	//	PCF_EMPHASIZED,		//	ボールド、イタリック等
};
enum {
	TEST_CHAR_FLAGS = 1,
	TEST_CONTEXT_AT = 2,
	TEST_LINE_CRSP = 4,			//	対応行テキストチェック
	TEST_EtoP_CUR_SYNC = 8,		//	対応行カーソル位置同期チェック
	TEST_PtoE_CUR_SYNC = 16,	//	対応行カーソル位置同期チェック
	TEST_ALL = TEST_CHAR_FLAGS|TEST_LINE_CRSP /*|TEST_EtoP_CUR_SYNC*/ |TEST_PtoE_CUR_SYNC,
};

const QString QA_MD_TEXT_2 =
#if 0
	"```CSV\n"	//	CSVブロック開始
	"abc, xy, z123\n"
	"id, , h3\n"
	"id, h*h*h2, **h3**\n"
	"69, ""hasshi"", h\\*o\\*ge\n"
	",,\n"
	"\"\",\"\",\n"	//	途中のセルが ""
	",,\"\"\n"	//	最後のセルが ""
	"```\n"		//	CSVブロック終了
	"text\n"
	//"```CSV\n"
	//",,,\n"
	//"```\n"
	//"1. item1\n"
	//"1. *italic*\n"
	//"\n"
	//"![v](url)\n"	//	画像
	//"\n"
#endif
#if 0
	"<!-- comment -->\n"
	"# title\n"
	"hoge<!-- -->\n"
	"<!-- -->fuga\n"
	"text\n"
#endif
	//"\\> quote-5\n"
	//"item2**bold**\n"
	//"- item2**bold**\n"
	//"fuga\\hoge\n"
#if 0
	"- item1\n"
	"  hoge fuga\n"
	"  abc xyzzzzy\n"
	"\n"
	//"\\- item3\n"
#endif
#if 0
	"```CSV\n"	//	CSVブロック開始
	"id, hhh2, h3\n"
	"69, ""hasshi"", hoge\n"
	"```\n"		//	CSVブロック終了
	"\n"
	"text\n"
#endif
#if 0
	"|he**ad**er|h|\n"
	"|-|-|\n"
	"|3|1415|\n"
	"|f\\*o\\*o|bar|\n"
	"\n"
	"text\n"
	"\n"
#endif
#if 1
	"TEST\n"
	"<!-- comment -->\n"
	"# title\n"
	"hoge<!-- -->\n"
	"<!-- -->fuga\n"
	"text\n"
	" text\n"
	"text \n"
	"abc \\xyzzz\n"		//	x はエスケープされず \x と表示される
	"hoge*fuga*foo\n"
	"hoge\\*fuga\\*foo\n"
	"hoge**fuga**foo\n"
	"hoge**fuga** **fuga**\n"
	"hoge***fuga***foo\n"
	"hoge~~fuga~~foo\n"
	"h'a\\*bc*xyz*abc'x\n"
	"[v](url)\n"	//	リンク
	"x[v](url)y\n"	//	リンク
	"![v](url)\n"	//	画像
	"x![v](url)y\n"	//	画像
	"x![v](url)y![v](url)z\n"	//	画像が複数
	"x![v](url)![v](url)z\n"	//	画像が連続
#if 1
	"## list\n"
	"text\n"
	"- item1\n"
	"  - item1.2\n"
	"- item2**bold**\n"
	"- item3\n"
	"hoge fuga\n"
	"xyz\n"
	"\n"
	"\\- item3\n"
	"\n"
	"1. item1\n"
	"1. *italic*\n"
	"1. item3\n"
	"1. **bold** *italic* ~~strike~~ \n"
	"1. [link](url) \n"
	"\n"
	"\\1. item\n"
	"\n"
	"- [ ] checkbox\n"
	"- [x] checkbox\n"
	"- [ ] checkbox\n"
	"\n"
	"\\- [ ] checkbox\n"
	"\n"
	"text\n"
	"> quote-1\n"
	"> q*uot*e-2\n"
	"> quote-3\n"
	"> quote-4\n"
	"\n"
	"\\> quote-5\n"
	"## block\n"
	"text\n"
	"```\n"		//	コードブロック開始
	"int main() { return 0; }\n"
	"```\n"		//	コードブロック終了
	"text\n"
	"```keisen\n"	//	罫線ブロック開始
	"┌───┐\n"
	"│      │\n"
	"└───┘\n"
	"```\n"		//	罫線ブロック終了
	"text\n"
	"```SVG\n"	//	SVGブロック開始
	"<svg width='200' height='100'>\n"
	"  <rect x='10' y='10' width='180' height='80' rx='5' fill='green' />\n"
	"  <circle cx='100' cy='50' r='35' fill='yellow' />\n"
	"</svg>\n"
	"```\n"		//	SVGブロック終了
	"text\n"
#if 1
	"## table\n"
	"```CSV\n"	//	CSVブロック開始
	"abc, xy, z123\n"
	"id, , h3\n"
	"id, h*h*h2, **h3**\n"
	"69, ""hasshi"", h\\*o\\*ge\n"
	",,\n"
	"\"\",\"\",\n"	//	途中のセルが ""
	",,\"\"\n"	//	最後のセルが ""
	"```\n"		//	CSVブロック終了
	"text\n"
#endif
#if 1
	"|he**ad**er|h|\n"
	"|-|-|\n"
	"|3|1415|\n"
	"| a |xyz|\n"
	"|a|\\[xyz\\]|\n"
	"|f\\*o\\*o|bar|\n"
#endif
	"text\n"
#endif
#endif
	"\n"
	"";
//const short CODE_IMAGE = 0xfffc;		//	プレビュー：画像アイコン
int g_tested_count = 0;
int g_failed_count = 0;
QString g_result;
bool MainWindow::ASSERT(bool actual, int ln) {
	++g_tested_count;
	if( actual ) return true;
	++g_failed_count;
	do_output(QString("%1: true expected. but false\n").arg(ln+1));
	return false;
}
//	undone: テンプレート関数化
bool MainWindow::ASSERT_EQ(bool expected, bool actual, int ln) {
	++g_tested_count;
	if( actual == expected ) return true;
	++g_failed_count;
	return false;
}
bool MainWindow::ASSERT_EQ(int expected, int actual, int ln) {
	++g_tested_count;
	if( actual == expected ) return true;
	++g_failed_count;
	do_output(QString("%1: %2 expected. but %3\n").arg(ln+1).arg(expected).arg(actual));
	return false;
}
bool MainWindow::ASSERT_EQ(int expected, int actual, int ln, QChar ch, int ix, int type) {
	++g_tested_count;
	if( actual == expected ) return true;
	++g_failed_count;
	if( type == TEST_EtoP_CUR_SYNC )
		do_output(QString("%1: E [%2] '%3' -> P ix: %4 expected. but %5\n").arg(ln+1).arg(ix).arg(ch).arg(expected).arg(actual));
	else
		do_output(QString("%1: P [%2] '%3' -> E ix: %4 expected. but %5\n").arg(ln+1).arg(ix).arg(ch).arg(expected).arg(actual));
	return false;
}
bool MainWindow::ASSERT_EQ(const QChar expected, const QChar actual, int ln) {
	++g_tested_count;
	if( actual == expected ) return true;
	++g_failed_count;
	do_output(QString("%1: '%2' expected. but '%3'\n").arg(ln+1).arg(expected).arg(actual));
	return false;
}
bool MainWindow::ASSERT_EQ(const QString &expected, const QString &actual, int ln) {
	++g_tested_count;
	if( actual == expected ) return true;
	++g_failed_count;
	do_output(QString("%1: '%2' expected. but '%3'\n").arg(ln+1).arg(expected).arg(actual));
	return false;
}
bool isCommentOuted(const BlockData* data) {
	if( data->m_charFlags.isEmpty() ) return false;			//	空行
	for(int i = 0; i < data->m_charFlags.size(); ++i) {
		if( data->m_charFlags[i] != PCF_COMMENTED ) return false;		//	表示文字があった
	}
	return true;
}
//enum { PATH_1 = 1, PATH_2, PATH_3, };
void MainWindow::onAction_Test() {
}
void MainWindow::onAction_TestCharFlags() {
	do_test(TEST_CHAR_FLAGS);
}
void MainWindow::onAction_TestContextAt() {
	do_test(TEST_CONTEXT_AT);
}
void MainWindow::onAction_TestLineCrsp() {
	do_test(TEST_LINE_CRSP);
}
void MainWindow::onAction_TestEtoPCurSync() {
	do_test(TEST_EtoP_CUR_SYNC);
}
void MainWindow::onAction_TestAll() {
	do_test(TEST_ALL);
}

void MainWindow::do_test(int type) {
	addTab(QString("QA-%1").arg(++m_QA_tab_number));
	DocWidget *docWidget = getCurDocWidget();;
	if( docWidget == nullptr ) return;
	int total_tested = 0;
	int total_failed = 0;
	do_output("\n# Test Result:\n\n");
	if( (type & TEST_CHAR_FLAGS) != 0 ) {
		g_tested_count = 0;
		g_failed_count = 0;
		test_charFlags(docWidget);			//	m_charFlags[] テスト
		do_output(QString("\nTest char flags: %1 failed / %2 tested.\n\n").arg(g_failed_count).arg(g_tested_count));
		total_tested += g_tested_count;
		total_failed += g_failed_count;
	}
	if( (type & TEST_CONTEXT_AT) != 0 ) {
		g_tested_count = 0;
		g_failed_count = 0;
		test_contextAt(docWidget);			//	MarkdownEditor::contextAt テスト
		do_output(QString("\nTest MarkdownEditor::contextAt: %1 failed / %2 tested.\n\n").arg(g_failed_count).arg(g_tested_count));
		total_tested += g_tested_count;
		total_failed += g_failed_count;
	}
	if( (type & TEST_LINE_CRSP) != 0 ) {
		g_tested_count = 0;
		g_failed_count = 0;
		docWidget->m_editor->setPlainText(QA_MD_TEXT_2);
		do_test(docWidget, TEST_LINE_CRSP);
		do_output(QString("\nTest Line Corresponding: %1 failed / %2 tested.\n\n").arg(g_failed_count).arg(g_tested_count));
		total_tested += g_tested_count;
		total_failed += g_failed_count;
	}
	if( (type & TEST_EtoP_CUR_SYNC) != 0 ) {
		g_tested_count = 0;
		g_failed_count = 0;
		do_test(docWidget, TEST_EtoP_CUR_SYNC);		//	EtoP 行内表示文字一致テスト
		//g_tested_count -= n_testted;	//	重複数分
		//g_failed_count -= n_failed;
		do_output(QString("\nTest EtoP CurSync: %1 failed / %2 tested.\n\n").arg(g_failed_count).arg(g_tested_count));
		total_tested += g_tested_count;
		total_failed += g_failed_count;
	}
	if( (type & TEST_PtoE_CUR_SYNC) != 0 ) {
		docWidget->m_preview->setFocus();
		g_tested_count = 0;
		g_failed_count = 0;
		do_test(docWidget, TEST_PtoE_CUR_SYNC);		//	PtoE 行内表示文字一致テスト
		//g_tested_count -= n_testted;	//	重複数分
		//g_failed_count -= n_failed;
		do_output(QString("\nTest PtoE CurSync: %1 failed / %2 tested.\n\n").arg(g_failed_count).arg(g_tested_count));
		total_tested += g_tested_count;
		total_failed += g_failed_count;
	}
	//do_test(docWidget, PATH_3);		//	PtoE 行内表示文字一致テスト
	//QString mess = QString("Total: %1 failed / %2 tested. (Fail:%3%)")
	//				.arg(total_failed).arg(total_tested).arg(total_failed*100.0/total_tested, 0, 'f', 1);
	QString mess = QString("\nTotal: %1 failed / %2 tested. (Success:%3%)")
					.arg(total_failed).arg(total_tested).arg(100.0 - total_failed*100.0/total_tested, 0, 'f', 1);
	statusBar()->showMessage(mess);
	//g_result += mess;
	do_output(mess);
#if 0
	qDebug() << "test result:\n" << g_result;
	QTextCursor cursor = docWidget->m_editor->textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.insertText(g_result);
#endif
}
void MainWindow::do_test(DocWidget *docWidget, int type) {
	QTextBlock block1 = docWidget->m_editor->document()->firstBlock();
	QTextBlock block2 = docWidget->m_preview->document()->firstBlock();
	//bool inTable = false;
	QStringList listStrings;
	int prevLen = 0;
	while( block1.isValid() && block2.isValid() ) {
		QCoreApplication::processEvents();		//	溜まっているイベント処理
#if 0
		while( blockType(block2) == BT_TABLE ) {		//	テーブル前後ダミーブロックはスキップ
			block2 = block2.next();
		}
		if (!block2.isValid()) break;
#endif
		int n1 = block1.blockNumber();
		int n2 = block2.blockNumber();
		QString t1 = block1.text();
		QString t2 = block2.text();
		assert( block2.isValid() );
		if( isCommentOuted(getBlockData(block1)) ) {		//	（非空行で、）１行全てコメントアウトされている場合はスキップ
			block1 = block1.next();
			continue;
		}
		std::vector<char> tableAlign;
		if( isTableHyphenLine(block1.text(), tableAlign) ) {		//	GFM表のハイフン行はスキップ
			block1 = block1.next();
			continue;
		}
		//if( blockType(block1) == BT_TABLE && blockType(block2) != BT_TABLE )		//	block2 には何故かダミー？がある
		//	block2 = block2.next();
		ASSERT(block2.isValid(), block1.blockNumber());
		while( (blockType(block1) == BT_CODE_BLOCK || blockType(block1) == BT_CODE_BLOCK_END) &&
			block1.text().startsWith("```") )
		{
			block1 = block1.next();
		}
		if( blockType(block1) == BT_KEISEN_BEGIN || blockType(block1) == BT_KEISEN_BLOCK ) {
			while( blockType(block1) == BT_KEISEN_BEGIN || blockType(block1) == BT_KEISEN_BLOCK )
				block1 = block1.next();
			block2 = block2.next();
			continue;
		}
		if( blockType(block1) == BT_SVG_BEGIN || blockType(block1) == BT_SVG_BLOCK ) {
			while( blockType(block1) == BT_SVG_BEGIN || blockType(block1) == BT_SVG_BLOCK )
				block1 = block1.next();
			block2 = block2.next();
			continue;
		}
		while( /*blockType(block1) == BT_KEISEN_BLOCK &&*/ block1.text().startsWith("```") ) {
			block1 = block1.next();
		}
#if 0
		if( blockType(block1) == BT_TABLE) {
			if( !inTable ) {
				block2 = block2.next();		//	GFMテーブル最初はテーブル全体のためのブロックなのでスキップ
			}
			inTable = true;
		} else {
			if( inTable ) {
				block2 = block2.next();		//	GFMテーブル最後もスキップ
			}
			inTable = false;
		}
#endif
		while( blockType(block2) == BT_TABLE ) {		//	テーブル前後ダミーブロックはスキップ
			block2 = block2.next();
		}
		qDebug() << "block1: " << block1.blockNumber() << ", block2: " << block2.blockNumber();
		qDebug() << "charAt(block2) = " << docWidget->m_preview->document()->characterAt(block2.position());
		QString buf1 = block1.text();
		QString buf2 = block2.text();
		int offset = 0;
		if( listStrings.isEmpty() && blockType(block2) == BT_LIST ) {
			listStrings = buf2.split(QChar(LINE_SEPARATOR));
			//if( listStrings.size() > 1 )	//	継続行がある場合
			//	offset = strlen("- ");
			prevLen = 0;
		}
		if( !listStrings.isEmpty() )
			buf2 = listStrings.front();
		int lineStartPos = block2.position();		//	行先頭位置
		QTextTable *table = QTextCursor(block2).currentTable();
		if( table != nullptr ) {	//	CSV・GFM テーブル中の場合
			for(int i = 0; i < table->columns() - 1; ++i) {
				block2 = block2.next();
				assert( block2.isValid() );
				buf2 += " " + block2.text().trimmed();		//	セル文字列は半角空白j区切り
			}
			//block2 = block2.next();
			assert( block2.isValid() );
			//inTable = true;
		} else {
			//inTable = false;
		}
		BlockData *data = getBlockData(block1);
		int k = 0;
		for(int i = 0; i < data->m_charFlags.size(); ++i) {		//	buf1: 非表示部分を削除
			if( data->m_charFlags[i] == PCF_VISIBLE ) {
				buf1[k++] = buf1[i];
			} else if(data->m_charFlags[i] == PCF_CELL_SEPARATOR) {
				buf1[k++] = u' ';
			}
		}
		buf1.resize(k);
		buf2.remove(QChar(CODE_IMAGE));
		static QRegularExpression allspc("^ +$");
		if( !allspc.match(buf1).hasMatch() )	//	空白以外を含んでいる
			buf1 = buf1.trimmed();
		ASSERT_EQ( buf1, buf2, block1.blockNumber());
		//if( ASSERT_EQ( buf1.trimmed(), buf2, block1.blockNumber()) )	//	表示テキストが一致した場合
		{
			if( type == TEST_EtoP_CUR_SYNC ) {
				//	エディタ → プレビュー カーソル同期テスト
				//		![v](url) の場合、m_charFlags[] = {I, I, I, ... I}
				//		column: 0 -> 0, 1 ～ 9 -> 1 （画像があるため１ずれる）
				QTextDocument *document = docWidget->m_editor->document();
				QTextCursor cursor(block1);
				const BlockData *data = getBlockData(block1);
				int nvcnt = 0;	//	非表示文字数
				for(int i = 0; i < block1.text().size(); ++i) {
					int k1 = i - nvcnt;			//	k1: 期待されるカーソル位置
					if( i != 0 && data->m_charFlags[i-1] == PCF_IMAGE && data->m_charFlags[i] == PCF_IMAGE )
						++k1;
					if( data->m_charFlags[i] != PCF_VISIBLE ) ++nvcnt;
					docWidget->m_editor->setTextCursor(cursor);
					QTextCursor cur2 = docWidget->m_preview->textCursor();		//	プレビューカーソル
					int k2 = cur2.position() - cur2.block().position();			//	k2: プレビューカーソルカラム
					QChar ch = document->characterAt(cursor.position());
					if( !ASSERT_EQ( k2, k1, block1.blockNumber(), ch, i, TEST_EtoP_CUR_SYNC ) ) {
						qDebug() << "ch = " << ch;
					}
					cursor.movePosition(QTextCursor::Right);
				}
			}
			if( type == TEST_PtoE_CUR_SYNC ) {
				//	プレビュー → エディタ カーソル同期テスト
				QTextDocument *document = docWidget->m_preview->document();
				//QTextCursor cur1(block1);		//	エディタカーソル
				QTextCursor cur2(block2);		//	プレビューカーソル
				cur2.setPosition(lineStartPos);
				//QTextCursor cur2 = docWidget->m_preview->textCursor();		//	プレビューカーソル
				const BlockData *data = getBlockData(block1);
				//int nvcnt = 0;	//	非表示文字数
				int k = 0;		//	エディタカーソルインデックス
				for(int i = 0; i <= buf2.size(); ++i) {		//	１行分のテキストについてチェック
					docWidget->m_preview->setTextCursor(cur2);
					QCoreApplication::processEvents();		//	溜まっているイベント処理
					int k0 = k;
					while( k < data->m_charFlags.size() && data->m_charFlags[k] >= PCF_NOT_VISIBLE &&
						data->m_charFlags[k] != PCF_CELL_SEPARATOR )	//	次の表示文字を探す
					{
						++k;
					}
					QChar ch1 = k < block1.text().size() ? block1.text()[k] : u'\n';
					QTextCursor cur1 = docWidget->m_editor->textCursor();
					int k1 = cur1.position() - cur1.block().position();		//	実際のエディタカーソルインデックス
					QChar ch = document->characterAt(cur2.position());
					//	"** " の様な場合は、カーソルは "** " 先頭位置（k0）を期待
					if( !ASSERT_EQ( (ch1 == u' ' || ch1 == u'\n' ? k0 : k)+offset-prevLen, k1, block1.blockNumber() , ch, i, TEST_PtoE_CUR_SYNC) ) {
						qDebug() << "ch1 = " << ch1;
					}
					cur2.movePosition(QTextCursor::Right);		//	カーソル右移動
					++k;
				}
			}
		}
		block1 = block1.next();
		if( !listStrings.isEmpty() ) {
			prevLen += listStrings.front().size() + 1;
			listStrings.pop_front();
		}
		if( listStrings.isEmpty() ) {
			prevLen = 0;
			block2 = block2.next();
		}
	}
	ASSERT( !block2.isValid(), block1.blockNumber());	//	同行数のはず
}
const QStringList QA_TEXT_FLAGS = {
	"text",
	"vvvv",
	" text",
	"-vvvv",
	"text ",
	"vvvv-",
	"<!--text-->",
	"-----------",
	"xyz<!--text-->",
	"vvv-----------",
	"<!--text-->xyz",
	"-----------vvv",
	"![i](png)",
	"IIIIIIIII",
	"x![i](png)x",
	"vIIIIIIIIIv",
	"[i](url)",
	"KvKKKKKK",
	"x[i](url)x",
	"vKvKKKKKKv",
	"text",
	"vvvv",
	"*i*",
	"=v=",
	"\\*i\\*",
	"=vv=v",
	"**b**",
	"==v==",
	"*i* **b** ***bi*** ~~s~~",
	"=v= ==v== ===vv=== ==v==",
	"h`a\\*bc*xyz*abc`x",
	"v=vvvvvvvvvvvvv=v",
	"",
	"",
	"1. hoge",
	"NNNvvvv",
	"1. h*og*e",
	"NNNv=vv=v",
	"1. h**og**e",
	"NNNv==vv==v",
	"",
	"",
	"- text",
	"LLvvvv",
	"- t*ex*t",
	"LLv=vv=v",
	"- t**ex**t",
	"LLv==vv==v",
	"",
	"",
	"- [ ] text",
	"LLLLLLvvvv",
	"- [x] t*ex*t",
	"LLLLLLv=vv=v",
	"- [X] t**ex**t",
	"LLLLLLv==vv==v",
	"",
	"",
	"```",
	"CCC",
	"abcdef",
	"vvvvvv",
	"a*bcde*f",
	"vvvvvvvv",
	"```",
	"CCC",
	"",
	"",
	"```keisen",
	"KKKKKKKKK",
	"───",
	"vvv",
	"a*bcde*f",
	"vvvvvvvv",
	"───",
	"vvv",
	"```",
	"KKK",
	"",
	"",
#if 1
	"```CSV",
	"SSSSSS",
	"h1,  h22,  h3*33",
	"vvSSSvvvSSSvvvvv",
	"ID,  9,  h*o*ge",
	"vvSSSvSSSv=v=vv",
	"ID,  9,  h\\*o\\*ge",
	"vvSSSvSSSv-vv-vvv",
	",,",
	"SS",
	"```",
	"SSS",
	"",
	"",
#endif
	"hdr|hdr2",
	"vvvTvvvv",
	"---|---:",
	"TTTTTTTT",
	"hac*k|69",
	"vvvvvTvv",
	"h*a*ck|69",
	"v=v=vvTvv",
	"69|h*a*ck",
	"vvTv=v=vv",
	"69|h\\*a\\*ck",
	"vvTv-vv-vvv",
	" a |xyz",
	"-v--vvv",
	"",
	"",
};
#if 0
const QString QA_MD_TEXT_1 =
	"text\n"
	"*i*\n"
	"**b**\n"
	"*i* **b** ***bi*** ~~s~~\n"
	"\n"
	"1. hoge\n"
	"1. h*og*e\n"
	"\n";
const QStringList QA_MD_FLAGS = {
	"vvvv",
	"=v=",
	"==v==",
	"=v= ==v== ===vv=== ==v==",
	"",
	"LLLvvvv",
	"LLLv=vv=v",
	"",
};
#endif
void MainWindow::test_charFlags(DocWidget *docWidget) {
	QString buf;
	for(int i = 0; i < QA_TEXT_FLAGS.size(); i += 2 )
		buf += QA_TEXT_FLAGS[i] + "\n";
	docWidget->m_editor->setPlainText(buf);
	QTextBlock block1 = docWidget->m_editor->document()->firstBlock();
	QTextBlock block2 = docWidget->m_preview->document()->firstBlock();
	//for(auto flags: QA_MD_FLAGS) {
	for(int i = 1; i < QA_TEXT_FLAGS.size(); i += 2 ) {
		QString buf1 = block1.text();
		QString flags = QA_TEXT_FLAGS[i];
		BlockData *data = getBlockData(block1);
		ASSERT_EQ( (int)flags.size(), (int)data->m_charFlags.size(), block1.blockNumber());
		for(int i = 0; i < flags.size(); ++i) {
			++g_tested_count;
			if( (flags[i] == u'v' || flags[i] == u' ') && data->m_charFlags[i] != PCF_VISIBLE ||
				!(flags[i] == u'v' || flags[i] == u' ') && data->m_charFlags[i] == PCF_VISIBLE )
			{
				g_result += QString("%1: flags[%2] is NOT correct.\n").arg(block1.blockNumber()+1).arg(i);
				QString f = "m_charFlags[] = {";
				int len = f.size();
				for(int k = 0; k < flags.size(); ++k) {
					f += g_flag_char[data->m_charFlags[k]];
				}
				g_result += f + "}\n";
				g_result += QString(len + i, QChar(0x00a0)) + "^\n";
				++g_failed_count;
			}
		}
		block1 = block1.next();
	}
}
const QStringList QA_CONTEXT_AT = {
	"# title",
	"text",
	"",
};
struct TestCaseContextAt {
	int		m_position;
	QChar	m_anchorChar;
	int		m_offset;
	int		m_nth;
} g_testCaseContextAt[] = {
	{0, u't', 0, 1},	//	[#] title
	{1, u't', 0, 1},
	{2, u't', 0, 1},	//	# [t]itle
	{3, u'i', 0, 1},
	{4, u't', 0, 2},	//	# title		２つめの t
	{5, u'l', 0, 1},	//	# title
	{6, u'e', 0, 1},	//	# title
	{7, ETX, 0, 1},		//	# title
};
void MainWindow::test_contextAt(DocWidget *docWidget) {
	QString buf;
	for(int i = 0; i < QA_CONTEXT_AT.size(); ++i)
		buf += QA_CONTEXT_AT[i] + "\n";
	docWidget->m_editor->setPlainText(buf);
	QTextCursor cursor = docWidget->m_editor->textCursor();
	for(auto tc: g_testCaseContextAt) {
		cursor.setPosition(tc.m_position);
		PosContext pc = docWidget->m_editor->contextAt(tc.m_position);
		ASSERT_EQ( tc.m_anchorChar, pc.m_anchorChar, cursor.block().blockNumber() );
	}
}
void MainWindow::onAction_DumpBlockUserData() {
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;
	QTextBlock block = docWidget->m_editor->document()->firstBlock();
	if( !docWidget->m_diffMode ) {
		//QString txt = "\n# Dump charFlags[]\n\n```\n";
		do_output("\n# Dump charFlags[]\n\n");
		while( block.isValid() ) {
			do_output(QString::number((int)block.blockNumber()) + ": '" + block.text() + "' ");
			const BlockData* data = getBlockData(block);
			for(int i = 0; i < data->m_charFlags.size(); ++i) {
				//txt += QString::number((int)data->m_charFlags[i]) + u' ';
				do_output(QString::number((int)data->m_charFlags[i]) + u' ');
			}
			do_output("\n");
			block = block.next();
		}
	} else {
		do_output("\n# Dump editor userData\n\n");
		while( block.isValid() ) {
			do_output(QString::number((int)block.blockNumber()) + ": '" + block.text() + "' ");
			const DiffBlockUserData *userData = dynamic_cast<const DiffBlockUserData*>(block.userData());
			if( userData != nullptr ) {
				const QList<DiffRange> &ranges = userData->ranges;
				QString txt;
				for(const auto dr: ranges) {
					do_output(QString("(%1 %2) ").arg(dr.start).arg(dr.length));
				}
			}
			do_output("\n");
			block = block.next();
		}
		block = docWidget->m_diffview->document()->firstBlock();
		do_output("\n# Dump diffview userData\n\n");
		while( block.isValid() ) {
			do_output(QString::number((int)block.blockNumber()) + ": '" + block.text() + "' ");
			const DiffBlockUserData *userData = dynamic_cast<const DiffBlockUserData*>(block.userData());
			if( userData != nullptr ) {
				const QList<DiffRange> &ranges = userData->ranges;
				QString txt;
				for(const auto dr: ranges) {
					do_output(QString("(%1 %2) ").arg(dr.start).arg(dr.length));
				}
			}
			do_output("\n");
			block = block.next();
		}
	}
	//txt += "```\n";
	//QTextCursor cursor = docWidget->m_editor->textCursor();
	//cursor.movePosition(QTextCursor::End);
	//cursor.insertText(txt);
	//docWidget->m_editor->setTextCursor(cursor);
}
void MainWindow::onAction_DumpBlockUserStates() {
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;
	//QString txt = "\n## userStates of Editor Blocks\n\n```\n";
	do_output("\n## userStates of Editor Blocks\n\n");
	QTextBlock block = docWidget->m_editor->document()->firstBlock();
	while( block.isValid() ) {
		do_output(QString("%1: 0x%2 '%3'\n").arg(block.blockNumber()+1).arg((unsigned int)block.userState(), 4, 16, QChar('0')).arg(block.text()));
		block = block.next();
	}
	//txt += "```\n";
	//txt += "\n## userStates of Preview Blocks\n\n```\n";
	if( !docWidget->m_diffMode && docWidget->m_docType == DocType::Markdown ) {
		do_output("\n## userStates of Preview Blocks\n\n");
		block = docWidget->m_preview->document()->firstBlock();
		while( block.isValid() ) {
			do_output(QString("%1: 0x%2 '%3'\n").arg(block.blockNumber()+1).arg((unsigned int)block.userState(), 4, 16, QChar('0')).arg(block.text()));
			block = block.next();
		}
	}
	if( docWidget->m_diffMode ) {
		do_output("\n## userStates of Diffview Blocks\n\n");
		QTextBlock block = docWidget->m_diffview->document()->firstBlock();
		while( block.isValid() ) {
			do_output(QString("%1: 0x%2 '%3'\n").arg(block.blockNumber()+1).arg((unsigned int)block.userState(), 4, 16, QChar('0')).arg(block.text()));
			block = block.next();
		}
	}
	//txt += "```\n";

	//QTextCursor cursor = docWidget->m_editor->textCursor();
	//cursor.movePosition(QTextCursor::End);
	//cursor.insertText(txt);
	//docWidget->m_editor->setTextCursor(cursor);
}
static QString g_script_1 = R"(
	TYPE "hoge"
	#CRLF
	#TYPE "xyzzzz"
	#CRLF
	#UP
)";
void MainWindow::onAction_RunPTS() {
	run_previewTestScript(g_script_1);
}
void MainWindow::run_previewTestScript(const QString &script) {
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;
	docWidget->m_preview->setFocus();
	QStringList lst = script.split('\n', Qt::SkipEmptyParts);
	for (const QString& line : lst) {
		QCoreApplication::processEvents();		//	溜まっているイベント処理
		QTextCursor cursor = docWidget->m_preview->textCursor();
		int pos = cursor.position();
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith("//") || trimmed.startsWith("#")) continue; // 空行やコメントをスキップ

        // コマンドと引数に分割 (最初のスペースで分ける)
        int ix = trimmed.indexOf(' ');
        QString cmd = trimmed.left(ix).toUpper();
        QString arg = (ix != -1) ? trimmed.mid(ix + 1) : "";

        // 引数のダブルクォーテーションを外す (もしあれば)
        if (arg.startsWith('"') && arg.endsWith('"')) {
            arg = arg.mid(1, arg.length() - 2);
        }
        if( cmd == "TYPE" ) {
        	if( !arg.isEmpty() ) cursor.insertText(arg);
        } else if( cmd == "CRLF" ) {
        	cursor.insertText("\n");
        } else if( cmd == "UP" ) {
        	cursor.movePosition(QTextCursor::Up);
        }
		docWidget->m_preview->setTextCursor(cursor);
	}
}
//----------------------------------------------------------------------
// viコマンド自動テスト用のテストケース構造体
struct ViTestCase {
    QString m_name;			// テスト名
    QString m_initialText;	// 初期テキスト（'|' がカーソル位置）
    QStringList m_steps;	//	{viコマンド列, コマンド実行後テキスト（'|' がカーソル位置）}...
};

const QList<ViTestCase> viTestCases = {
#if 0
    { "Basic i command",
        "┃\n",
        {
            "iabc", "ab┃c\n",
        }
    },
#endif
#if 0
    { "Insert with count ([num]i)", "┃\n",
        {
            "3ix", "xx┃x\n", // 1文字の複数回挿入（'xxx' を挿入後、Escで2番目の 'x' にスナップ）
            "u", "┃\n", // undoして元に戻す
            "3iabc", "abcabcab┃c\n", // 複数文字の複数回挿入（'abcabcabc' を挿入後、Escで最後の 'c' にスナップ）
            "u", "┃\n", // undoして元に戻す
            "1iabc", "ab┃c\n" // カウントが 1 の場合の挙動（通常の i と同一）
        }
    },
#endif
#if 0
    { "Basic ex command",
        "li┃ne1\nline2\nline3\n",
        {
            ":2", "line1\n┃line2\nline3\n", // 2行目行頭に移動
        }
    },
#endif
	//{ "Move cursor right",	"h┃ello\n", {"l", "he┃llo\n", "l", "hel┃lo\n", "l", "hell┃o\n", } },
	//{ "Move cursor left",	"h┃ello\n", {"h", "┃hello\n", "h", "┃hello\n", } },
	//{ "Visual mode",		"h┃ello\n", {"v", "h《┃e》llo\n", "l", "h《e┃l》lo\n", } },
#if 1
#if 1		//	h j k l
	// 下移動 (j) の基本動作と最終行での境界制御
    { "Move cursor down (j)",
        "a┃bc\ndef\nghi\n",
        {
            "j", "abc\nd┃ef\nghi\n", // 2行目の同じ列に移動
            "j", "abc\ndef\ng┃hi\n", // 3行目の同じ列に移動
            "j", "abc\ndef\nghi\n┃"  // 改行によって作られた4行目（空行）に移動
        }
    },
    // 上移動 (k) の基本動作と先頭行での境界制御
    { "Move cursor up (k)",
        "abc\ndef\ng┃hi\n",
        {
            "k", "abc\nd┃ef\nghi\n", // 2行目の同じ列に移動
            "k", "a┃bc\ndef\nghi\n", // 1行目の同じ列に移動
            "k", "a┃bc\ndef\nghi\n"  // 先頭行のため上移動しない
        }
    },
    // 短い行を通過する際の「列位置の記憶（カラムメモリ）」の検証
    // ※ 1行目の4文字目（index 3）からスタートし、短い2行目を通過して、3行目で元の4文字目に復帰するかをテストします。
    { "Move vertically through short lines (Column Memory)",
        "lin┃e1\nab\nline3\n",
        {
            "j", "line1\na┃b\nline3\n",  // 2行目が短いため、一時的に2行目の末尾（index 1）に移動
            "j", "line1\nab\nlin┃e3\n",  // 3行目に降りると、元の列位置（index 3）に復帰する
            "k", "line1\na┃b\nline3\n",  // 再び2行目の末尾に制限される
            "k", "lin┃e1\nab\nline3\n"   // 1行目に戻ると、元の列位置（index 3）に完全復帰する
        }
    },
    // h と l が行境界（改行）を越えて回り込まないかの検証
    { "h and l boundaries (No wrapping)",
        "abc\n┃def\nghi\n",
        {
            "h", "abc\n┃def\nghi\n", // 行頭のため、h を押しても前の行の末尾に回り込まない
            "l", "abc\nd┃ef\nghi\n",
            "l", "abc\nde┃f\nghi\n", // 'f' はこの行の実質的な末尾（改行 \n の直前）
            "l", "abc\nde┃f\nghi\n"  // 行末のため、l を押しても次の行の先頭に回り込まない
        }
    },
    // カウント付き右移動 (nl) と行末でのクランプ処理
    { "Move right with count (nl)",
        "h┃ello world\n", // 'e' (index 1) からスタート
        {
            "3l", "hell┃o world\n", // 3文字右へ -> 'o' (index 4)
            "5l", "hello wor┃ld\n", // 5文字右へ -> 'l' (index 9)
            "5l", "hello worl┃d\n"  // 5文字右へ -> 行末の 'd' (index 10) でクランプされて止まる
        }
    },
    // カウント付き左移動 (nh) と行頭でのクランプ処理
    { "Move left with count (nh)",
        "hello wor┃ld\n", // 'l' (index 9) からスタート
        {
            "4h", "hello┃ world\n", // 4文字左へ -> 半角スペース (index 5)
            "10h", "┃hello world\n"  // 10文字左へ -> 行頭の 'h' (index 0) でクランプされて止まる
        }
    },
    // カウント付き下移動 (nj) と最終行でのクランプ処理
    { "Move down with count (nj)",
        "a┃bc\ndef\nghi\njkl\n", // 1行目の 'b' (line 0, index 1) からスタート
        {
            "2j", "abc\ndef\ng┃hi\njkl\n", // 2行下へ -> 3行目の 'h' (line 2, index 1)
            "5j", "abc\ndef\nghi\njkl\n┃"  // 5行下へ -> 末尾改行による5行目（空行）へ移動して止まる
        }
    },
    // カウント付き上移動 (nk) と先頭行でのクランプ処理
    { "Move up with count (nk)",
        "abc\ndef\nghi\nj┃kl\n", // 4行目の 'k' (line 3, index 1) からスタート
        {
            "2k", "abc\nd┃ef\nghi\njkl\n", // 2行上へ -> 2行目の 'e' (line 1, index 1)
            "5k", "a┃bc\ndef\nghi\njkl\n"  // 5行上へ -> 先頭行の 'b' (line 0, index 1) でクランプされて止まる
        }
    },
#endif
#if 1		//	w
    { "Move forward word (w) - Basic spacing",
        "┃abc def ghi",
        {
            "w", "abc ┃def ghi", // 次の単語 "def" の先頭へ移動
            "w", "abc def ┃ghi", // 次の単語 "ghi" の先頭へ移動
            "w", "abc def gh┃i"  // これ以上単語がないため、ファイル末尾の文字 'i' で停止
        }
    },
    { "Move forward word (w) - Across lines and empty lines",
        "┃abc\n  def\n\nghi",
        {
            "w", "abc\n  ┃def\n\nghi",
            "w", "abc\n  def\n┃\nghi", // ← 2回目は空行の先頭で止まる（Vimの正しい挙動）
            "w", "abc\n  def\n\n┃ghi", // ← 3回目で ghi の先頭へ移動
            "w", "abc\n  def\n\ngh┃i"  // ← 4回目で末尾クランプ
        }
    },
    { "Move forward word (w) - Across lines and empty lines(2)",
        "┃abc\n\n\ndef\n",
        {
            "w", "abc\n┃\n\ndef\n", // ← 1回目は空行の先頭で止まる（Vimの正しい挙動）
            "w", "abc\n\n┃\ndef\n", // ← 2回目も空行の先頭で止まる（Vimの正しい挙動）
            "w", "abc\n\n\n┃def\n", // ← 3回目で def の先頭へ移動
        }
    },
    { "Move forward word (w) - Punctuation boundaries",
        "┃abc.def!ghi",
        {
            "w", "abc┃.def!ghi", // 記号 "." の先頭で停止
            "w", "abc.┃def!ghi", // 次の英数字単語 "def" の先頭へ
            "w", "abc.def┃!ghi", // 記号 "!" の先頭で停止
            "w", "abc.def!┃ghi", // 次の英数字単語 "ghi" の先頭へ
            "w", "abc.def!gh┃i"  // ファイル末尾の文字 'i' で停止
        }
    },
    { "Move forward multiple words (num w) - Basic counts",
        "┃abc def ghi jkl mno",
        {
            "2w", "abc def ┃ghi jkl mno", // 2単語分移動して "ghi" の先頭へ
            "2w", "abc def ghi jkl ┃mno", // さらに2単語分移動して "mno" の先頭へ
            "3w", "abc def ghi jkl mn┃o"  // 3単語分移動（足りないためファイル末尾の文字 'o' で停止）
        }
    },
    { "Word motion and gg (2w, gg, 3w)",
        "┃abc def\nxyz hoge\n",
        {
            "2w", "abc def\n┃xyz hoge\n", // 'a' から 2w 移動して 'x' へ
            "gg", "┃abc def\nxyz hoge\n", // gg でファイルの先頭 ('a') へ戻る
            "3w", "abc def\nxyz ┃hoge\n"  // 'a' から 3w 移動して 'h' (hoge の頭) へ
        }
    },
    { "Move forward word (w) - Japanese word classes",
        "┃日本語ひらがなカタカナ。漢字",
        {
            "w", "日本語┃ひらがなカタカナ。漢字", // 漢字「日本語」を飛び越え、ひらがな「ひ」の先頭へ
            "w", "日本語ひらがな┃カタカナ。漢字", // ひらがな「ひらがな」を飛び越え、カタカナ「カ」の先頭へ
            "w", "日本語ひらがなカタカナ┃。漢字", // カタカナ「カタカナ」を飛び越え、記号「。」の先頭へ
            "w", "日本語ひらがなカタカナ。┃漢字", // 記号「。」を飛び越え、次の漢字「漢」の先頭へ
            "w", "日本語ひらがなカタカナ。漢┃字"  // 次の単語がないため、ファイル末尾の文字 '字' で停止
        }
    },
    { "Move forward word (w) - Japanese mixed with alphanumeric and spaces",
        "┃日本語 abc カタカナ 123。ひらがな",
        {
            "w", "日本語 ┃abc カタカナ 123。ひらがな", // スペースをスキップし、半角英字 "abc" の先頭へ
            "w", "日本語 abc ┃カタカナ 123。ひらがな", // スペースをスキップし、カタカナ "カタカナ" の先頭へ
            "w", "日本語 abc カタカナ ┃123。ひらがな", // スペースをスキップし、半角数字 "123" の先頭へ
            "w", "日本語 abc カタカナ 123┃。ひらがな", // スペース無しでの文字種変更を検知し、記号 "。" の先頭へ
            "w", "日本語 abc カタカナ 123。┃ひらがな", // スペース無しでの文字種変更を検知し、ひらがな "ひらがな" の先頭へ
            "w", "日本語 abc カタカナ 123。ひらが┃な"  // 次の単語がないため、末尾の文字 'な' で停止
        }
    },
#endif
#if 1		//	e
    { "Move to end of word (e) - Basic spacing",
        "┃abc def ghi",
        {
            "e", "ab┃c def ghi", // 現在の単語 "abc" の末尾 "c" へ移動
            "e", "abc de┃f ghi", // 次の単語 "def" の末尾 "f" へ移動
            "e", "abc def gh┃i"  // 次の単語 "ghi" の末尾 "i" へ移動
        }
    },
    { "Move to end of word (e) - Across lines and empty lines",
        "┃abc\n  def\n\nghi",
        {
            "e", "ab┃c\n  def\n\nghi", // 現在の単語 "abc" の末尾 "c" へ移動
            "e", "abc\n  de┃f\n\nghi", // 改行とインデントをスキップし、"def" の末尾 "f" へ
            "e", "abc\n  def\n\ngh┃i"  // 空行をスキップし、"ghi" の末尾 "i" へ
        }
    },
    { "Move to end of word (e) - Punctuation boundaries",
        "┃abc.def!ghi",
        {
            "e", "ab┃c.def!ghi", // 現在の単語 "abc" の末尾 "c" へ移動
            "e", "abc┃.def!ghi", // 記号 "." の末尾（記号自身）へ移動
            "e", "abc.de┃f!ghi", // 次の単語 "def" の末尾 "f" へ移動
            "e", "abc.def┃!ghi", // 記号 "!" の末尾（記号自身）へ移動
            "e", "abc.def!gh┃i"  // 次の単語 "ghi" の末尾 "i" へ移動
        }
    },
    { "Move to end of multiple words (num e) - Basic counts",
        "┃abc def ghi jkl mno",
        {
            "2e", "abc de┃f ghi jkl mno", // 2単語分移動して "def" の末尾 "f" へ
            "2e", "abc def ghi jk┃l mno", // さらに2単語分移動して "jkl" の末尾 "l" へ
            "3e", "abc def ghi jkl mn┃o"  // 3単語分移動（足りないためファイル末尾の文字 'o' で停止）
        }
    },
    { "Move to end of word (e) - Japanese word classes",
        "┃日本語ひらがなカタカナ。漢字",
        {
            "e", "日本┃語ひらがなカタカナ。漢字", // 漢字「日本語」の末尾「語」へ
            "e", "日本語ひらが┃なカタカナ。漢字", // ひらがな「ひらがな」の末尾「な」へ
            "e", "日本語ひらがなカタカ┃ナ。漢字", // カタカナ「カタカナ」の末尾「ナ」へ
            "e", "日本語ひらがなカタカナ┃。漢字", // 記号「。」の末尾（記号自身）へ
            "e", "日本語ひらがなカタカナ。漢┃字"  // 漢字「漢字」の末尾「字」へ
        }
    },
    { "Move to end of word (e) - Japanese mixed with alphanumeric and spaces",
        "┃日本語 abc カタカナ 123。ひらがな",
        {
            "e", "日本┃語 abc カタカナ 123。ひらがな", // 漢字「日本語」の末尾「語」へ
            "e", "日本語 ab┃c カタカナ 123。ひらがな", // 半角英字 "abc" の末尾 "c" へ
            "e", "日本語 abc カタカ┃ナ 123。ひらがな", // カタカナ "カタカナ" の末尾 "ナ" へ
            "e", "日本語 abc カタカナ 12┃3。ひらがな", // 半角数字 "123" の末尾 "3" へ
            "e", "日本語 abc カタカナ 123┃。ひらがな", // 記号 "。" の末尾（記号自身）へ
            "e", "日本語 abc カタカナ 123。ひらが┃な"  // ひらがな "ひらがな" の末尾 "な" へ
        }
    },
#endif
#if 1
    { "Move backward word (b) - Basic spacing",
        "abc def gh┃i",
        {
            "b", "abc def ┃ghi", // 現在の単語 "ghi" の先頭へ移動
            "b", "abc ┃def ghi", // 前の単語 "def" の先頭へ移動
            "b", "┃abc def ghi"  // 前の単語 "abc" の先頭へ移動
        }
    },
    { "Move backward word (b) - Across lines and empty lines",
        "abc\n  def\n\ngh┃i",
        {
            "b", "abc\n  def\n\n┃ghi", // 現在の単語 "ghi" の先頭へ移動
            "b", "abc\n  def\n┃\nghi", // 空行をスキップしない
            "b", "abc\n  ┃def\n\nghi", // 前の単語 "def" の先頭へ移動
            "b", "┃abc\n  def\n\nghi"  // インデントと改行をスキップし、前の単語 "abc" の先頭へ移動
        }
    },
    { "Move backward word (b) - Punctuation boundaries",
        "abc.def!gh┃i",
        {
            "b", "abc.def!┃ghi", // 現在の単語 "ghi" の先頭へ移動
            "b", "abc.def┃!ghi", // 記号 "!" の先頭（記号自身）へ移動
            "b", "abc.┃def!ghi", // 前の単語 "def" の先頭へ移動
            "b", "abc┃.def!ghi", // 記号 "." の先頭（記号自身）へ移動
            "b", "┃abc.def!ghi"  // 前の単語 "abc" の先頭へ移動
        }
    },
    { "Move backward multiple words (num b) - Basic counts",
        "abc def ghi jkl mn┃o",
        {
            "2b", "abc def ghi ┃jkl mno", // 2単語分戻って "jkl" の先頭へ（1b目で "mno" の先頭、2b目で "jkl" の先頭）
            "2b", "abc ┃def ghi jkl mno", // さらに2単語分戻って "def" の先頭へ
            "3b", "┃abc def ghi jkl mno"  // 3単語分戻る（足りないためファイル先頭の文字 'a' で停止）
        }
    },
    { "Move backward word (b) - Japanese word classes",
        "日本語ひらがなカタカナ。漢┃字",
        {
            "b", "日本語ひらがなカタカナ。┃漢字", // 漢字「漢字」の先頭「漢」へ
            "b", "日本語ひらがなカタカナ┃。漢字", // 記号「。」の先頭（記号自身）へ
            "b", "日本語ひらがな┃カタカナ。漢字", // カタカナ「カタカナ」の先頭「カ」へ
            "b", "日本語┃ひらがなカタカナ。漢字", // ひらがな「ひらがな」の先頭「ひ」へ
            "b", "┃日本語ひらがなカタカナ。漢字"  // 漢字「日本語」の先頭「日」へ
        }
    },
    { "Move backward word (b) - Japanese mixed with alphanumeric and spaces",
        "日本語 abc カタカナ 123。ひらが┃な",
        {
            "b", "日本語 abc カタカナ 123。┃ひらがな", // ひらがな「ひらがな」の先頭「ひ」へ
            "b", "日本語 abc カタカナ 123┃。ひらがな", // 記号 "。" の先頭（記号自身）へ
            "b", "日本語 abc カタカナ ┃123。ひらがな", // 半角数字 "123" の先頭 "1" へ
            "b", "日本語 abc ┃カタカナ 123。ひらがな", // カタカナ "カタカナ" の先頭 "カ" へ
            "b", "日本語 ┃abc カタカナ 123。ひらがな", // 半角英字 "abc" の先頭 "a" へ
            "b", "┃日本語 abc カタカナ 123。ひらがな"  // 漢字「日本語」の先頭「日」へ
        }
    },
#endif
#if 1
    // 基本的な空白区切り（wと同様の動作をするケース）
    { "Move forward WORD (W) - Basic spacing",
        "┃abc def ghi",
        {
            "W", "abc ┃def ghi", // 次のWORD "def" の先頭へ移動
            "W", "abc def ┃ghi", // 次のWORD "ghi" の先頭へ移動
            "W", "abc def gh┃i"  // これ以上WORDがないため、ファイル末尾の文字 'i' で停止
        }
    },
    // 記号ハンドリング（Wコマンドの真価：記号をスキップせずWORDの一部とする）
    { "Move forward WORD (W) - Punctuation handling",
        "┃foo-bar.baz qux",
        {
            // w コマンドなら「-」や「.」の場所で止まりますが、W は空白まで一気にジャンプします
            "W", "foo-bar.baz ┃qux", // 記号を含む "foo-bar.baz" を1つのWORDとして扱い、次の "qux" の先頭へ
            "W", "foo-bar.baz qu┃x"  // これ以上WORDがないため、末尾の文字 'x' で停止
        }
    },
    // 連続する空白と記号が混在するケース
    { "Move forward WORD (W) - Mixed punctuation and spacing",
        "┃a.b   c,d.e!  f",
        {
            "W", "a.b   ┃c,d.e!  f", // 複数の空白をスキップし、記号混じりの "c,d.e!" の先頭へ
            "W", "a.b   c,d.e!  ┃f", // "f" の先頭へ
            "W", "a.b   c,d.e!  ┃f"  // これ以上WORDがないため、末尾の文字 'f' で停止
        }
    },
    // 基本的な空白区切り（eと同様の動作をするケース）
    { "Move to end of WORD (E) - Basic spacing",
        "┃abc def ghi",
        {
            "E", "ab┃c def ghi", // 現在のWORD "abc" の末尾 'c' へ移動
            "E", "abc de┃f ghi", // 次のWORD "def" の末尾 'f' へ移動
            "E", "abc def gh┃i", // 次のWORD "ghi" の末尾 'i' へ移動
            "E", "abc def gh┃i"  // これ以上WORDがないため、末尾の文字 'i' で停止
        }
    },
    // 記号ハンドリング（Eコマンドの真価：記号をスキップして全体の末尾へ）
    { "Move to end of WORD (E) - Punctuation handling",
        "┃foo-bar.baz qux",
        {
            // e コマンドなら "foo" の末尾 'o' や "bar" の末尾 'r' で止まりますが、E は一気に 'z' まで進みます
            "E", "foo-bar.ba┃z qux", // 記号を含む "foo-bar.baz" 全体の末尾 'z' へ移動
            "E", "foo-bar.baz qu┃x"  // 次のWORD "qux" の末尾 'x' へ移動
        }
    },
    // WORDの途中から開始するケース
    { "Move to end of WORD (E) - Starting inside a WORD",
        "fo┃o-bar.baz   qux",
        {
            "E", "foo-bar.ba┃z   qux", // 現在のWORD "foo-bar.baz" の末尾 'z' へ移動
            "E", "foo-bar.baz   qu┃x"  // 空白をスキップし、次のWORD "qux" の末尾 'x' へ移動
        }
    },
    // 基本的な空白区切り（bと同様の動作をするケース）
    { "Move backward WORD (B) - Basic spacing",
        "abc def gh┃i",
        {
            "B", "abc def ┃ghi", // 現在のWORD "ghi" の先頭 'g' へ移動
            "B", "abc ┃def ghi", // 前のWORD "def" の先頭 'd' へ移動
            "B", "┃abc def ghi", // 前のWORD "abc" の先頭 'a' へ移動
            "B", "┃abc def ghi"  // これ以上戻れないため、先頭の文字 'a' で停止
        }
    },
    // 記号ハンドリング（Bコマンドの真価：記号をスキップして全体の先頭へ）
    { "Move backward WORD (B) - Punctuation handling",
        "foo-bar.baz qu┃x",
        {
            // b コマンドなら "baz" や "bar" の先頭や記号部分で細かく止まりますが、B は一気に 'f' まで戻ります
            "B", "foo-bar.baz ┃qux", // 現在のWORD "qux" の先頭 'q' へ移動
            "B", "┃foo-bar.baz qux"  // 空白をスキップし、前のWORD "foo-bar.baz" 全体の先頭 'f' へ移動
        }
    },
    // すでにWORDの先頭にいる状態から開始するケース
    { "Move backward WORD (B) - Starting at the start of a WORD",
        "foo-bar.baz   ┃qux",
        {
            "B", "┃foo-bar.baz   qux" // すでに "qux" の先頭にいるため、空白をスキップして前のWORD "foo-bar.baz" の先頭 'f' へ移動
        }
    },
#endif
	{ "Move to start of line (0)",
        "abc d┃ef\n  ghi\n\n",
        {
            "0", "┃abc def\n  ghi\n\n", // 1行目の絶対行頭（カラム0）に移動
            "j", "abc def\n┃  ghi\n\n", // 2行目の絶対行頭（カラム0）へ移動（縦移動のカラムメモリ検証）
            "l", "abc def\n ┃ ghi\n\n", // 右に1文字移動（1つ目のスペース）
            "l", "abc def\n  ┃ghi\n\n", // 右に1文字移動（2つ目のスペース）
            "0", "abc def\n┃  ghi\n\n", // インデントを無視して絶対行頭（カラム0）に戻る
            "j", "abc def\n  ghi\n┃\n",  // 3行目の空行に移動
            "0", "abc def\n  ghi\n┃\n"   // 空行で実行しても安全にその場にとどまる
        }
    },
	{ "Move to first non-blank character (^)",
        "abc d┃ef\n  ghi\n\n",
        {
            "^", "┃abc def\n  ghi\n\n", // 1行目の先頭文字（スペース無しの場合は絶対行頭）に移動
            "j", "abc def\n┃  ghi\n\n", // 2行目の絶対行頭に移動（カラム0）
            "^", "abc def\n  ┃ghi\n\n", // 2行目のインデントを考慮した最初の非空白文字 'g' に移動
            "j", "abc def\n  ghi\n┃\n",  // 3行目の空行に移動
            "^", "abc def\n  ghi\n┃\n"   // 空行で実行しても安全にとどまる
        }
    },
	{ "Move to end of line ($) and EOL memory",
        "a┃bc\n  defgh\n\nij\n",
        {
            "$", "ab┃c\n  defgh\n\nij\n", // 1行目の末尾 'c' に移動
            "j", "abc\n  defg┃h\n\nij\n", // 2行目の末尾 'h' に移動（スナップ先の長さが変わっても末尾を維持）
            "j", "abc\n  defgh\n┃\nij\n",  // 3行目の空行に移動
            "j", "abc\n  defgh\n\ni┃j\n",  // 4行目の末尾 'j' に移動（空行を跨いでも行末維持メモリが継続しているか）
            "0", "abc\n  defgh\n\n┃ij\n",  // 4行目の絶対行頭 'i' に移動（カラムメモリが 0 に上書きされる）
            "k", "abc\n  defgh\n┃\nij\n",  // 3行目の空行に戻る
            "k", "abc\n┃  defgh\n\nij\n"   // 2行目のカラム0（1つ目のスペース）に戻る（行末維持が解除されているか）
        }
    },
	{ "Move to end of line with count (<num>$) and EOL memory",
        "a┃bc\n  defgh\n\nij\n",
        {
            "2$", "abc\n  defg┃h\n\nij\n", // 2$: 1行下（2行目）の末尾 'h' に移動
            "k", "ab┃c\n  defgh\n\nij\n",  // k: 1行目の末尾 'c' に戻る（行末維持メモリが機能しているか）
            "4$", "abc\n  defgh\n\ni┃j\n",  // 4$: 3行下（4行目）の末尾 'j' に移動（空行を跨ぐ）
            "0", "abc\n  defgh\n\n┃ij\n",  // 0: 4行目の絶対行頭 'i' に移動（行末維持メモリが解除される）
            "k", "abc\n  defgh\n┃\nij\n",  // k: 3行目の空行に戻る
            "k", "abc\n┃  defgh\n\nij\n",  // k: 2行目のカラム0（1つ目のスペース）に戻る（行末維持が解除されていることの確認）
            "1$", "abc\n  defg┃h\n\nij\n", // 1$: 現在行（2行目）の末尾 'h' に移動（$ と同等）
            "10$", "abc\n  defgh\n\nij\n┃", // 10$: 範囲外の大きなカウントは最終行（4行目）の末尾 'j' で止まる？
        }
    },
	{ "Jump to matching bracket (%) - Basic and Search",
        "┃(abc [def] {ghi})\n",
        {
            "%", "(abc [def] {ghi}┃)\n", // 外側の '(' から対応する ')' にジャンプ
            "%", "┃(abc [def] {ghi})\n", // ')' から対応する '(' に戻る
            "l", "(┃abc [def] {ghi})\n", // カーソルを1文字右（'a'）に移動
            "%", "(abc [def┃] {ghi})\n", // 右側にある最初の括弧 '[' を自動検知し、そのペア ']' にジャンプ
            "%", "(abc ┃[def] {ghi})\n", // ']' からペアである '[' に戻る
            "l", "(abc [┃def] {ghi})\n", // カーソルを 'd' に移動
            "%", "(abc ┃[def] {ghi})\n", // 右側で最初に見つかる括弧 ']' を検知し、そのペアである '[' にジャンプ（Vim標準挙動）
            "%", "(abc [def┃] {ghi})\n"  // '[' から再びペアである ']' に戻る
        }
    },
	{ "Jump to matching bracket (%) - Nested",
        "┃(abc (def) ghi)\n",
        {
            "%", "(abc (def) ghi┃)\n", // 外側の '(' から、正しく「外側の ')'」にジャンプできるか
            "%", "┃(abc (def) ghi)\n", // 外側の ')' から、正しく「外側の '('」に戻れるか
        }
    },
    { "Jump to matching bracket (%) - Inner Nested",
        "(abc ┃(def) ghi)\n",
        {
            "%", "(abc (def┃) ghi)\n", // 内側の '(' から、正しく「内側の ')'」にジャンプできるか（外側を無視）
            "%", "(abc ┃(def) ghi)\n"  // 内側の ')' から、正しく「内側の '('」に戻れるか
        }
    },
    { "Find character (f and F) - Basic and Boundary",
        "┃abc def abc def\n",
        {
            "fa", "abc def ┃abc def\n", // 現在地 'a' の次の 'a'（カラム8）にジャンプ
            "fd", "abc def abc ┃def\n", // カムラ8より右側にある最初の 'd'（カラム12）にジャンプ
            "Fa", "abc def ┃abc def\n", // カラム12より左側にある最初の 'a'（カラム8）にジャンプ
            "Fb", "a┃bc def abc def\n", // カラム8より左側にある最初の 'b'（カラム1）にジャンプ
            "fx", "a┃bc def abc def\n", // 'x' は存在しないのでカーソルは移動しない
            "Fy", "a┃bc def abc def\n"  // 'y' は存在しないのでカーソルは移動しない
        }
    },
    { "Find character (f and F) - Line limitation",
        "┃abc\ndef\n",
        {
            "fd", "┃abc\ndef\n", // 'd' は次の行にあるため、f コマンドは移動しない（行を跨がない）
            "j",  "abc\n┃def\n", // 2行目の先頭 'd' に移動
            "ff", "abc\nde┃f\n", // 同一行内の 'f' にジャンプ
            "Fa", "abc\nde┃f\n"  // 'a' は前の行にあるため、F コマンドは移動しない（行を跨がない）
        }
    },
    { "Find character with count ([num]f and [num]F)",
        "┃abc abc abc abc\n",
        {
            "2fa", "abc abc ┃abc abc\n", // カラム0から、2番目の 'a'（カラム8）にジャンプ
            "2fa", "abc abc ┃abc abc\n", // カラム8から右側には 'a' が1つ（カラム12）しか無いため、2回に満たず「移動しない」
            "1fa", "abc abc abc ┃abc\n", // カラム8から、1番目の 'a'（カラム12）にジャンプ
            "3Fa", "┃abc abc abc abc\n", // カラム12から、左に向かって3番目の 'a'（カラム0）にジャンプ
            "4Fa", "┃abc abc abc abc\n"  // カラム0から左側には 'a' が存在しないため「移動しない」
        }
    },
    { "Find till character (t and T) - Basic and Count",
        "┃abc def abc def\n",
        {
            "td",  "abc┃ def abc def\n", // 次の 'd'（カラム4）の「手前」（カラム3：スペース）にジャンプ
            "tb",  "abc def ┃abc def\n", // 次の 'b'（カラム9）の「手前」（カラム8：'a'）にジャンプ
            "2td", "abc def ┃abc def\n", // カラム8から右側に 'd' は1つしかないため、2回に満たず「移動しない」
            "1td", "abc def abc┃ def\n", // カラム8から、1番目の 'd'（カラム12）の「手前」（カラム11：スペース）にジャンプ
            "Ta",  "abc def a┃bc def\n", // カラム11から、左の 'a'（カラム8）の「直後」（カラム9：'b'）にジャンプ
            "2Ta", "a┃bc def abc def\n", // カラム9から、左に2番目の 'a'（カラム0）の「直後」（カラム1：'b'）にジャンプ
            "tx",  "a┃bc def abc def\n", // 'x' は存在しないので移動しない
            "Ty",  "a┃bc def abc def\n"  // 'y' は存在しないので移動しない
        }
    },
    { "Find till character (t and T) - Line limitation",
        "┃abc\ndef\n",
        {
            "td", "┃abc\ndef\n", // 'd' は次の行にあるため、t コマンドは移動しない
            "j",  "abc\n┃def\n", // 2行目の先頭 'd' に移動
            "tf", "abc\nd┃ef\n", // 同一行内の 'f' の「手前」である 'e' にジャンプ
            "Ta", "abc\nd┃ef\n"  // 'a' は前の行にあるため、T コマンドは移動しない
        }
    },
    { "Repeat find character (; and ,)",
        "┃abc def abc def\n",
        {
            "fd", "abc ┃def abc def\n", // まず 'd' を前方検索（カラム4へ移動。これが「最後の検索」になる）
            ";",  "abc def abc ┃def\n", // 正方向（前方）に繰り返し検索（カラム12へ移動）
            ",",  "abc ┃def abc def\n", // 逆方向（後方）に繰り返し検索（カラム4へ移動）
            ";",  "abc def abc ┃def\n", // もう一度正方向に検索（カラム12へ移動）
            "2,", "abc def abc ┃def\n", // 逆方向に2回繰り返し（カラム12から左に 'd' は1つしかないため「移動しない」）
            "1,", "abc ┃def abc def\n", // 逆方向に1回繰り返し（カラム4へ移動）
            "Fa", "┃abc def abc def\n", // 今度は 'a' を後方検索（カラム0へ移動。これで「最後の検索」が上書きされる）
            "l",  "a┃bc def abc def\n", // 通常移動 'l' を実行（これで検索レジスタが壊れないことを確認）
            ";",  "┃abc def abc def\n", // 最後の検索 'Fa' を同じ方向（後方）に繰り返し（カラム0へ移動）
            ",",  "abc def ┃abc def\n", // 最後の検索 'Fa' を逆方向（前方）に繰り返し（カラム8へ移動）
            "2;", "abc def ┃abc def\n"  // 同じ方向（後方）に2回繰り返し（カラム8から左に 'a' は1つしかないため「移動しない」）
        }
    },
#if 1
    { "Delete character under cursor (x) - Basic",
        "a┃bc\n",
        {
            "x", "a┃c\n", // 'b' を削除、カーソルは右隣の文字 'c' へ移動
            "x", "┃a\n"   // 'c' を削除。行末の文字だったため、カーソルは左の 'a' に後退（\nには乗らない）
        }
    },
    { "Delete multiple characters (num x) - With count",
        "┃abcdef\n",
        {
            "3x", "┃def\n", // 3文字（a, b, c）を削除、カーソルは 'd' へ
            "5x", "┃\n"     // 残り3文字に対して5文字削除を指定。行を越えて削除せず、空行（┃\n）に
        }
    },
    { "Anchor test",
        "┃abcdef\n",
        {
            "vl", "《a┃b》cdef\n",	// a, b 選択、カーソルは b 位置
            "x", "┃cdef\n",     		// 被選択 a, b が削除される
            "u", "┃abcdef\n",
            "3lvh", "ab《┃cd》ef\n",	// d まで移動し cd 選択、カーソルは c 位置
            "x", "ab┃ef\n",     		// 被選択 cd が削除される
        }
    },
    { "Delete character before cursor (X) - Basic",
        "abc┃d\n",
        {
            "X", "ab┃d\n", // 'c' を削除、カーソルはそのまま 'd' を維持
            "X", "a┃d\n",  // 'b' を削除、カーソルはそのまま 'd' を維持
            "X", "┃d\n",   // 'a' を削除。これ以上左に文字がないため、カーソルは 'd'（行頭）へ
            "X", "┃d\n"    // 行頭での実行。削除されず、状態とカーソル位置を維持（0158の検証）
        }
    },
    { "Delete character before cursor (X) - Japanese",
        "あいう┃え\n",
        {
            "X", "あい┃え\n", // 'う' を削除、カーソルはそのまま 'え' を維持
            "X", "あ┃え\n",  // 'い' を削除、カーソルはそのまま 'え' を維持
            "X", "┃え\n",   // 'あ' を削除。行頭になったため、カーソルは 'え'（行頭）へ
            "X", "┃え\n"    // 行頭での実行。日本語でも何も起こらないことを確認
        }
    },
    { "Delete character before cursor (X) - Count",
        "abcde┃f\n",
        {
            "2X", "abc┃f\n", // 'd', 'e' の2文字を削除、カーソルはそのまま 'f' を維持
            "5X", "┃f\n"     // 5文字の削除を試みるが、存在する3文字のみ削除して行頭に留まる
        }
    },
    { "Delete character under cursor (x) - Japanese",
	    "あ┃いうえ\n",
	    {
	        "x", "あ┃うえ\n",  // 日本語1文字削除
	        "2x", "┃あ\n"      // count付き日本語削除
	    }
	},
	{ "Delete character under cursor (x) - Empty line",
	    "abc\n┃\ndef\n",
	    {
	        "x", "abc\n┃\ndef\n"  // 空行でxは何もしない（またはSPR #174相当）
	    }
	},
#endif
#endif
	{ "Basic i command", "┃\n",
        {
            "iabc", "ab┃c\n", // 空行での基本挿入（Escにより末尾の 'c' から1文字左にスナップ）
        }
    },
    { "Insert in middle of line", "ab┃c\n",
        {
            "ixyz", "abxy┃zc\n", // 文字の間での挿入（'c' の手前に 'xyz' を挿入し、Escで 'z' の上にスナップ）
        }
    },
    { "Insert with count ([num]i)", "┃\n",
        {
            "3ix", "xx┃x\n", // 1文字の複数回挿入（'xxx' を挿入後、Escで2番目の 'x' にスナップ）
            "u", "┃\n", // undoして元に戻す
            "3iabc", "abcabcab┃c\n", // 複数文字の複数回挿入（'abcabcabc' を挿入後、Escで最後の 'c' にスナップ）
            "u", "┃\n", // undoして元に戻す
            "1iabc", "ab┃c\n" // カウントが 1 の場合の挙動（通常の i と同一）
        }
    },
    { "Insert with count in middle of line", "de┃f\n",
        {
            "2ix", "dex┃xf\n", // 文字の間での複数回挿入（'f' の手前に 'xx' を挿入し、Escで2番目の 'x' にスナップ）
        }
    },
    { "Basic a command", "┃\n",
        {
            "aabc", "ab┃c\n", // 空行での追加（右側に文字がないためiと同様の挙動、Escで 'c' から1文字左にスナップ）
        }
    },
    { "Append in middle of line", "ab┃c\n",
        {
            "axyz", "abcxy┃z\n", // 文字の後ろへの追加（'c' の後ろに 'xyz' を追加し、Escで 'z' の上にスナップ）
        }
    },
    { "Append with count ([num]a)", "┃\n",
        {
            "3ax", "xx┃x\n", // 空行での複数回追加
            "u", "┃\n", // undoして元に戻す
            "2axyz", "xyzxy┃z\n" // 空行での複数文字の複数回追加（'xyzxyz' を追加後、Escで最後の 'z' にスナップ）
        }
    },
    { "Append with count in middle of line", "a┃b\n",
        {
            "2axyz", "abxyzxy┃z\n", // 文字の後ろへの複数回追加（'b' の後ろに 'xyzxyz' を追加し、Escで最後の 'z' にスナップ）
        }
    },
    { "Basic I command", "  abc┃ def\n",
        {
            "Ixyz", "  xy┃zabc def\n", // インデントを考慮し、最初の非空白文字 'a' の手前に 'xyz' を挿入（Escで 'z' にスナップ）
        }
    },
    { "I command without leading spaces", "abc┃ def\n",
        {
            "Ixyz", "xy┃zabc def\n", // 先頭にスペースがない場合は絶対行頭に挿入
        }
    },
    { "I command with count ([num]I)", "  ab┃c\n",
        {
            "3Ix", "  xx┃xabc\n", // インデント手前に 'xxx' を挿入後、Escで3番目の 'x' にスナップ
            "u", "  ┃abc\n", // undoして「編集開始位置である 'a'（カラム2）」にカーソルが戻る（Vim標準挙動！）
            "2Ixyz", "  xyzxy┃zabc\n" // インデント手前に 'xyzxyz' を挿入後、Escで最後の 'z' にスナップ
        }
    },
    { "Basic A command", "ab┃c\n",
        {
            "Axyz", "abcxy┃z\n", // 行末の後ろに 'xyz' を追加（Escで 'z' にスナップ）
        }
    },
    { "A command on empty line", "┃\n",
        {
            "Axyz", "xy┃z\n", // 空行での挙動（i や a と同様に絶対行頭に挿入される）
        }
    },
    { "A command with count ([num]A)", "ab┃c\n",
        {
            "3Ax", "abcxx┃x\n", // 行末の後ろに 'xxx' を追加後、Escで3番目の 'x' にスナップ
            "u", "ab┃c\n", // undoして元のテキスト（"abc"）に戻り、カーソルは行末の 'c' にスナップ（Vim標準挙動）
            "2Axyz", "abcxyzxy┃z\n" // 行末の後ろに 'xyzxyz' を追加後、Escで最後の 'z' にスナップ
        }
    },
    { "Basic s command",
        "a┃bc\n",
        {
            "sxyz", "axy┃zc\n", // 1文字置換して挿入（Escにより末尾の 'z' から1文字左にスナップ）
        }
    },
    { "s command with count",
        "a┃bc\n",
        {
            "2sxyz", "axy┃z\n",  // カウント指定：2文字（bc）を置換して挿入
        }
    },
    { "s command with large count",
        "a┃bc\n",
        {
            "5sxyz", "axy┃z\n",  // 範囲外のカウント：行末を超える場合は行末まで削除して置換
        }
    },
    { "s command at end of line",
        "ab┃c\n",
        {
            "sxyz", "abxy┃z\n", // 行末の1文字を置換して挿入（削除後に末尾へ挿入）
        }
    },
    { "s command on empty line",
        "┃\n",
        {
            "sxyz", "xy┃z\n",   // 空行での置換（削除するものがないため、実質的に i コマンドと同様の挙動）
        }
    },
    { "Basic C command",
        "a┃bc\n",
        {
            "Cxyz", "axy┃z\n", // カーソル位置（b）から行末までを置換して挿入
        }
    },
    { "C command at start of line",
        "┃abc\n",
        {
            "Cxyz", "xy┃z\n", // 行頭での挙動（行全体の置換）
        }
    },
    { "C command at end of line",
        "ab┃c\n",
        {
            "Cxyz", "abxy┃z\n", // 行末1文字での挙動
        }
    },
    { "C command on empty line",
        "┃\n",
        {
            "Cxyz", "xy┃z\n", // 空行での挙動（削除なしで挿入モードに移行）
        }
    },
    { "C command with count",
        "a┃bc\ndef\nghi\n",
        {
            "2Cxyz", "axy┃z\nghi\n", // 2行分（現在の行のカーソルから次の行の末尾まで）を削除して置換
        }
    },
    
	// 1. 単純な dw
    { "Delete word (dw) - Basic",
        "┃abc def ghi\n",
        {
            "dw", "┃def ghi\n", // "abc " を削除、カーソルは "def" の先頭へ
            "dw", "┃ghi\n"     // "def " を削除、カーソルは "ghi" の先頭へ
        }
    },

    // 2. <num>dw, d<num>w, <num>d<num>w
    { "Delete word with counts (2dw, d2w, 2d2w)",
        "┃w1 w2 w3 w4 w5 w6 w7 w8 w9\n",
        {
            "2dw",  "┃w3 w4 w5 w6 w7 w8 w9\n", // 2単語 ("w1 w2 ") を削除
            "d2w",  "┃w5 w6 w7 w8 w9\n",       // 2単語 ("w3 w4 ") を削除
            "2d2w", "┃w9\n"                   // 2×2=4単語 ("w5 w6 w7 w8 ") を削除
        }
    },

    // 3. 行末単語の dw
    { "Delete word at end of line (dw)",
        "hello ┃world\n",
        {
            "dw", "hello┃ \n" // "world" を削除。改行(\n)は削除されないが、カーソルはひとつ左に移動
        }
    },

    // 4. 改行にカーソルがある場合の dw
    { "Delete word on newline (dw)",
        "first\n┃\nsecond\n",
        {
            "dw", "first\n┃second\n" // 改行文字(\n)自体が削除され、下の行と連結される
        }
    },

    // 5. 行をまたぐ <num>dw
    { "Delete words across lines (<num>dw)",
        "┃foo bar\nbaz qux\n",
        {
            "3dw", "┃qux\n" // 1:"foo ", 2:"bar\n", 3:"baz " の計3単語分を削除して改行を跨ぐ
        }
    },
    { "Basic dd command",
        "first\n"
        "sec┃ond\n"
        "third\n",
        {
            "dd", "first\n"
                  "┃third\n",         // カレント行を削除し、次行の行頭へ移動
        }
    },
    { "dd at the last line",
        "first\n"
        "second\n"
        "thi┃rd\n",
        {
            "dd", "first\n"
                  "┃second\n",        // 最終行削除時は直前行の行頭へ移動
        }
    },
    { "dd at the last line without trailing newline",
        "first\n"
        "second\n"
        "thi┃rd",
        {
            "dd", "first\n"
                  "┃second\n",          // 末尾改行がない最終行の削除
        }
    },
    { "dd on single line document",
        "hel┃lo\n",
        {
            "dd", "┃",                // 1行のみの文書で dd した場合は空文書
        }
    },
    { "dd with count (3dd)",
        "┃line1\n"
        "line2\n"
        "line3\n"
        "line4\n",
        {
            "3dd", "┃line4\n",        // 3行削除（※本体の SPR 0404 修正でパスするようになります）
        }
    },
    { "dd with count exceeding remaining lines",
        "line1\n"
        "li┃ne2\n"
        "line3\n",
        {
            "5dd", "┃line1\n",        // 残り行数以上は末尾まで削除し直前行へ
        }
    },
    { "dd repeat with dot",
        "┃line1\n"
        "line2\n"
        "line3\n",
        {
            "dd", "┃line2\n"
                  "line3\n",
            ".",  "┃line3\n",         // '.' でカレント行の削除を繰り返し
        }
    },
    { "dd undo",
        "first\n"
        "sec┃ond\n"
        "third\n",
        {
            "dd", "first\n"
                  "┃third\n",
            "u",  "first\n"
                  "┃second\n"
                  "third\n",          // Undo で元の行・先頭位置が復元される
        }
    },
    { "dd with count undo",
        "┃line1\n"
        "line2\n"
        "line3\n"
        "line4\n",
        {
            "3dd", "┃line4\n",
            "u",   "┃line1\n"
                   "line2\n"
                   "line3\n"
                   "line4\n",
        }
    },
    { "dd and put (paste linewise)",
        "fi┃rst\n"
        "second\n"
        "third\n",
        {
            "dd", "┃second\n"
                  "third\n",
            "p",  "second\n"
                  "┃first\n"
                  "third\n",          // 削除行を下に行単位でペースト
        }
    },
    //
    { "dw and put (characterwise p)",
        "a ┃word in line\n",
        {
            "dw", "a ┃in line\n",
            "p",  "a iword┃ n line\n",
        }
    },
    { "dw and Put (characterwise P)",
        "a ┃word in line\n",
        {
            "dw", "a ┃in line\n",
            "P",  "a word┃ in line\n",
        }
    },
    { "de and put (characterwise p)",
        "┃abc def\n",
        {
            "de", "┃ def\n",
            "l",  " ┃def\n",
            "p",  " dab┃cef\n",          // 'd' の後ろに "abc" が入り、'c' の上にカーソル
        }
    },
    // =========================================================================
    //  dh (左方向・文字単位削除)
    // =========================================================================

    { "Basic dh command",
        "a┃bc\n",
        {
            "dh", "┃bc\n",              // カーソル手前の 'a' を削除（カーソルは 'b' の上）
        }
    },
    { "dh at the beginning of line",
        "┃abc\n",
        {
            "dh", "┃abc\n",             // 行頭では何も削除されない
        }
    },
    { "dh with count (3dh)",
        "abcd┃efg\n",
        {
            "3dh", "a┃efg\n",           // 手前の 'b', 'c', 'd' の3文字を削除
        }
    },
    { "dh with double count (2d2h = 4 characters)",
        "abcdef┃g\n",
        {
            "2d2h", "ab┃g\n",           // 手前の 2x2=4 文字 ('c', 'd', 'e', 'f') を削除
        }
    },

    // =========================================================================
    //  dl (右方向・文字単位削除 / x と同等)
    // =========================================================================

    { "Basic dl command",
        "a┃bc\n",
        {
            "dl", "a┃c\n",              // カレント文字 'b' を削除
        }
    },
    { "dl with count (3dl)",
        "a┃bcdefg\n",
        {
            "3dl", "a┃efg\n",           // 'b', 'c', 'd' の3文字を削除
        }
    },
    { "dl with double count (3d2l = 6 characters)",
        "a┃bcdefghijk\n",
        {
            "3d2l", "a┃hijk\n",         // 3x2=6 文字 ('b'〜'g') を削除
        }
    },
    { "dl at the end of line",
        "ab┃c\n",
        {
            "dl", "a┃b\n",              // 行末文字 'c' を削除（カーソルは1つ左の 'b' にスナップ）
        }
    },

    // =========================================================================
    //  dj (下方向・行単位削除: カレント行 + 下の行)
    // =========================================================================

    { "Basic dj command (deletes 2 lines: current and below)",
        "first\n"
        "sec┃ond\n"
        "third\n"
        "fourth\n",
        {
            "dj", "first\n"
                  "┃fourth\n",          // second と third の計2行を削除
        }
    },
    { "dj with count (d2j = deletes 3 lines)",
        "first\n"
        "sec┃ond\n"
        "third\n"
        "fourth\n"
        "fifth\n",
        {
            "d2j", "first\n"
                   "┃fifth\n",          // second, third, fourth の計3行（カレント+2行）を削除
        }
    },
    { "dj with double count (2d2j = 4 lines down, deletes 5 lines)",
        "line1\n"
        "li┃ne2\n"
        "line3\n"
        "line4\n"
        "line5\n"
        "line6\n"
        "line7\n",
        {
            "2d2j", "line1\n"
                    "┃line7\n",         // line2〜line6 の計5行（カレント+4行）を削除
        }
    },
    { "dj at the last line",
        "first\n"
        "sec┃ond",
        {
            "dj", "first\n"
                  "sec┃ond",          // 下に行がないため何も削除されない
        }
    },

    // =========================================================================
    //  dk (上方向・行単位削除: カレント行 + 上の行)
    // =========================================================================

    { "Basic dk command (deletes 2 lines: current and above)",
        "first\n"
        "second\n"
        "thi┃rd\n"
        "fourth\n",
        {
            "dk", "first\n"
                  "┃fourth\n",          // third と second の計2行を削除（カーソルは繰り上がった fourth の行頭）
        }
    },
    { "dk with count (d2k = deletes 3 lines)",
        "first\n"
        "second\n"
        "third\n"
        "fou┃rth\n"
        "fifth\n",
        {
            "d2k", "first\n"
                   "┃fifth\n",          // fourth, third, second の計3行（カレント+上2行）を削除
        }
    },
    { "dk at the first line",
        "fi┃rst\n"
        "second\n",
        {
            "dk", "fi┃rst\n"
                  "second\n",           // 上に行がないため何も削除されない
        }
    },

    // =========================================================================
    //  d{h,j,k,l} の Undo テスト
    // =========================================================================

    { "dj undo",
        "first\n"
        "sec┃ond\n"
        "third\n"
        "fourth\n",
        {
            "dj", "first\n"
                  "┃fourth\n",
            "u",  "first\n"
                  "┃second\n"          // Undo で2行とも一度に復元
                  "third\n"
                  "fourth\n",
        }
    },
    { "3d2l undo",
        "a┃bcdefghijk\n",
        {
            "3d2l", "a┃hijk\n",
            "u",    "a┃bcdefghijk\n",   // Undo で6文字まとめて復元
        }
    },

    // --- 行単位削除（dd）後の P（上に行挿入） ---

    { "dd and Put (paste linewise above)",
        "first\n"
        "sec┃ond\n"
        "third\n",
        {
            "dd", "first\n"
                  "┃third\n",
            "P",  "first\n"
                  "┃second\n"          // 挿入された second 行の行頭
                  "third\n",
        }
    },
    { "2dd and Put (paste linewise above)",
        "first\n"
        "sec┃ond\n"
        "third\n"
        "fourth\n",
        {
            "2dd", "first\n"
                   "┃fourth\n",          // "second\nthird\n" の2行を削除（カーソルは fourth の行頭）
            "P",   "first\n"
                   "┃second\n"          // fourth の上に2行ペースト（カーソルは挿入先頭行の行頭）
                   "third\n"
                   "fourth\n",
        }
    },
    { "2dd and put (paste linewise below)",
        "first\n"
        "sec┃ond\n"
        "third\n"
        "fourth\n",
        {
            "2dd", "first\n"
                   "┃fourth\n",          // "second\nthird\n" の2行を削除
            "p",   "first\n"
                   "fourth\n"
                   "┃second\n"          // fourth の下に2行ペースト（カーソルは挿入先頭行の行頭）
                   "third\n",
        }
    },
    // --- カウント指定の文字単位ペースト (<num>p / <num>P) ---

    { "characterwise put with count (3p)",
        "a┃bc\n",
        {
            "x",  "a┃c\n",              // 'b' を削除
            "3p", "acbb┃b\n",           // 'c' の後ろに 'b' を3回ペースト（期待値を修正）
        }
    },
    { "characterwise Put with count (3P)",
        "a┃bc\n",
        {
            "x",  "a┃c\n",              // 'b' を削除
            "3P", "abb┃bc\n",           // 'c' の手前に 'b' を3回ペースト。最後の 'b' の上
        }
    },

    // --- カウント指定の行単位ペースト (<num>p / <num>P) ---

    { "linewise put with count (3p)",
        "fi┃rst\n"
        "second\n",
        {
            "dd", "┃second\n",
            "3p", "second\n"
                  "┃first\n"           // 最初に挿入された行の行頭
                  "first\n"
                  "first\n",
        }
    },
    { "linewise Put with count (3P)",
        "first\n"
        "sec┃ond\n",
        {
            "dd", "┃first\n",
            "3P", "┃second\n"          // 最初に挿入された行の行頭
                  "second\n"
                  "second\n"
                  "first\n",
        }
    },
    { "Basic r command",
        "a┃bc\n",
        {
            "rx", "a┃xc\n",         // 'b' を 'x' に置換（カーソルは置換した文字の上）
        }
    },
    { "r command at the end of line",
        "ab┃c\n",
        {
            "rx", "ab┃x\n",         // 行末文字 'c' を 'x' に置換
        }
    },
    { "r command with count",
        "a┃bcde\n",
        {
            "3rx", "axx┃xe\n",      // 'b', 'c', 'd' の3文字を 'x' に置換（カーソルは置換した最後の文字の上）
        }
    },
    { "r command with Enter (replace with newline)",
        "a┃bc\n",
        {
            "r\n", "a\n┃c\n",       // 'b' を改行に置換
        }
    },
    { "r command repeat with dot",
        "┃abc\n",
        {
            "rx",  "┃xbc\n",        // 1文字目を 'x' に置換
            "l.",  "x┃xc\n",        // 右に移動して '.' で同じ置換を繰り返し
        }
    },
    { "r command undo",
        "a┃bc\n",
        {
            "rx", "a┃xc\n",         // 置換
            "u",  "a┃bc\n",         // Undo で元に戻る
        }
    },
    { "r command with count undo",
        "a┃bcde\n",
        {
            "3rx", "axx┃xe\n",      // 3文字置換
            "u",   "a┃bcde\n",      // Undo で3文字とも一度に戻る
        }
    },
    { "r command multibyte character",
        "あ┃いう\n",
        {
            "rえ", "あ┃えう\n",     // マルチバイト文字の置換
        }
    },
//	ex commands
#if 0
    { "Ex Range - Absolute Line Number (:num)",
        "line 1\nli┃ne 2\nline 3\nline 4\n",
        {
            ":3", "line 1\nline 2\n┃line 3\nline 4\n", // 3行目の行頭へジャンプ
            ":1", "┃line 1\nline 2\nline 3\nline 4\n"  // 1行目の行頭へジャンプ
        }
    },
    { "Ex Range - Current Line (:.)",
        "line 1\nli┃ne 2\nline 3\n",
        {
            ":.", "line 1\n┃line 2\nline 3\n" // カレント行（2行目）の行頭へジャンプ
        }
    },
    { "Ex Range - End of File (:$ and :%)",
        "line 1\nli┃ne 2\nline 3\n",
        {
            ":$", "line 1\nline 2\n┃line 3\n", // 最終行（3行目）へジャンプ
            ":%", "line 1\nline 2\n┃line 3\n"  // 全行。範囲のみ指定時は、最後の行（3行目）へジャンプ
        }
    },
    { "Ex Range - Relative Offsets (:+-)",
        "line 1\nline 2\nli┃ne 3\nline 4\nline 5\n",
        {
            ":+2", "line 1\nline 2\nline 3\nline 4\n┃line 5\n", // '.' 省略の相対移動。現在行(3) + 2 = 5行目へ
            ":-3", "line 1\n┃line 2\nline 3\nline 4\nline 5\n", // 現在行(5) - 3 = 2行目へ
            ":-",  "┃line 1\nline 2\nline 3\nline 4\nline 5\n"  // 数値省略時は -1。現在行(2) - 1 = 1行目へ
        }
    },
    { "Ex Range - Forward Pattern Search (:/pat/)",
        "line 1\nline 2 (TODO)\nli┃ne 3\nline 4 (TODO)\nline 5\n",
        {
            // 3行目以降で、最初にマッチする行（4行目）へジャンプ
            ":/TODO/", "line 1\nline 2 (TODO)\nline 3\n┃line 4 (TODO)\nline 5\n" 
        }
    },
    { "Ex Range - Backward Pattern Search (:?pat?)",
        "line 1\nline 2 (TODO)\nline 3\nli┃ne 4 (TODO)\nline 5\n",
        {
            // 4行目より前で、最初にマッチする行（2行目）へジャンプ
            ":?TODO?", "line 1\n┃line 2 (TODO)\nline 3\nline 4 (TODO)\nline 5\n" 
        }
    },
    { "Ex Range - Semicolon Separator (:;)",
        "line 1\nli┃ne 2\nline 3\nline 4\nline 5\n",
        {
            // セミコロン区切り：'1' で一度カレント行が1行目に更新され、そこから '+2' されて3行目へジャンプ
            ":1;+2", "line 1\nline 2\n┃line 3\nline 4\nline 5\n" 
        }
    },
#endif
#if 0
    { "Ex Delete - Current Line (:d)",
        "line 1\nli┃ne 2\nline 3\n",
        {
            ":d", "line 1\n┃line 3\n" // 範囲省略時はカレント行（2行目）を削除。カーソルは次の行の先頭へ
        }
    },
    { "Ex Delete - Specific Line (:1d)",
        "line 1\nli┃ne 2\nline 3\n",
        {
            ":1d", "┃line 2\nline 3\n" // 指定した1行目を削除。カーソルは新しい1行目（元2行目）の先頭へ
        }
    },
    { "Ex Delete - Range (:1,2d)",
        "line 1\nline 2\nli┃ne 3\n",
        {
            ":1,2d", "┃line 3\n" // 1〜2行目を一括削除。カーソルは残った3行目の先頭へ
        }
    },
    { "Ex Delete - Whole Document (:%d)",
        "line 1\nli┃ne 2\nline 3\n",
        {
            ":%d", "┃\n" // バッファ全体のすべての行を削除。Vimの仕様として、中身のない空行が1行だけ残る
        }
    },
    { "Ex Delete - To EOF (:.,$d)",
        "line 1\nli┃ne 2\nline 3\n",
        {
            ":.,$d", "┃line 1\n" // カレント行（2行目）から最終行（3行目）まで削除。下に行がないため、カーソルは1行目へ
        }
    },
#endif
};
QString removeCursor(const QString &src, int &pos, int &anchor) {
	QString dst;
	int p = 0;
	anchor = -1;
	for(auto ch: src) {
		if( ch == u'┃' ) {
			pos = p;
			if( anchor == pos ) anchor = -1;
		} else if( ch == u'《' ) {
			anchor = p;
		} else if( ch == u'》' ) {
			if( anchor < 0 )
				anchor = p - 1;
		} else {
			dst += ch;
			++p;
		}
	}
	return dst;
}
QString actualText(const QTextCursor& cursor) {
	QString act = cursor.document()->toPlainText();
	act.insert(cursor.position(), u'┃');
	if( gvi.m_vMode == u'v' ) {
		if( gvi.m_vAnchor <= cursor.position() ) {
			act.insert(gvi.m_vAnchor, u'《');
			act.insert(cursor.position() + 3, u'》');
		} else {
			act.insert(cursor.position(), u'《');
			act.insert(gvi.m_vAnchor + 3, u'》');
		}
	}
	act.replace('\n', "\\n");
	return act;
}
void MainWindow::onAction_TestViCommands() {
	qDebug() << "MainWindow::onAction_TestViCommands()";
	addTab(QString("QA-%1").arg(++m_QA_tab_number));
	DocWidget *docWidget = getCurDocWidget();;
	if( docWidget == nullptr ) return;
	MarkdownEditor *editor = docWidget->m_editor;
	gvi.m_currentMode = ViMode::Normal;
	int total_tested = 0;
	int total_failed = 0;
	do_output("\n");
	do_output("Test vi commands...\n\n");
	int pos, anchor;
	for(int i = 0; i < viTestCases.size(); ++i) {
		gvi.m_currentMode = ViMode::Normal;
		gvi.m_vMode = u' ';
		do_output(viTestCases[i].m_name + ": ");
		QTextCursor cursor = editor->textCursor();
		cursor.movePosition(QTextCursor::Start);
		cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		cursor.insertText(removeCursor(viTestCases[i].m_initialText, pos, anchor));
		cursor.setPosition(pos);
		cursor.clearSelection();
		editor->setTextCursor(cursor);
		editor->savePrefferedX(cursor);
		QCoreApplication::processEvents();		//	溜まっているイベント処理
		const QStringList &steps = viTestCases[i].m_steps;
		for(int k = 0; k < steps.size(); k+=2) {
			++total_tested;
			do_output(".");
			QString before = cursor.document()->toPlainText();
			before.insert(cursor.position(), u'┃');
			before.replace('\n', "\\n");
			const QString cmd_text = steps[k];
			for(int i = 0; i < cmd_text.size(); ++i) {
				do_viCmd(cmd_text[i], cursor);
				if( gvi.m_currentMode == ViMode::Insert ) {
					auto txt = gvi.m_insertedText = cmd_text.mid(i+1);
					//if( gvi.m_insRepCount > 1 ) {
					//	//qDebug() << "gvi.m_insRepCount = " << gvi.m_insRepCount;
					//	txt = txt.repeated(gvi.m_insRepCount-1);
					//}
					//cursor.insertText(txt);
					editor->openUndoBlock();
					editor->do_insertText(cursor, txt);
					exitInsertMode(cursor);
					//if( cursor.position() > cursor.block().position())
					//	cursor.movePosition(QTextCursor::Left);
					//gvi.m_currentMode = ViMode::Normal;
					break;
				} else if( gvi.m_currentMode == ViMode::CommandLine ) {
					if( cmd_text[i] == u':' )
						do_exCmd(cmd_text.mid(i), cursor);
					break;
				}
			}
			editor->setTextCursor(cursor);
			QCoreApplication::processEvents();		//	溜まっているイベント処理
			cursor = editor->textCursor();
			int cpos1 = cursor.position();
			QString exp = removeCursor(steps[k+1], pos, anchor);
			//cursor = editor->textCursor();
			//int cpos2 = cursor.position();
			if( cursor.document()->toPlainText() != exp ) {
				QString exp = steps[k+1];
				exp.replace('\n', "\\n");
				QString act = actualText(cursor);
				++total_failed;
				do_output(QString("\n[FAILED #%1] wrong document text.\n").arg(total_failed));
				do_output(QString("Before:   '%1'\n").arg(before));
				do_output("Commands: '" + cmd_text + "'\n");
				do_output(QString("Expected: '%1'\nActual:   '%2'\n").arg(exp).arg(act));
				//do_output("expected:\n'" + exp + "', but:\n'" + cursor.document()->toPlainText() + "'\n");
				break;
			}
			if( cursor.position() != pos ) {
				//do_output(QString("wrong cursor position. pos = %1 expected, but %2\n").arg(pos).arg(cursor.position()));
				QString exp = steps[k+1];
				exp.replace('\n', "\\n");
				QString act = actualText(cursor);
				++total_failed;
				do_output(QString("\n[FAILED #%1] wrong cursor position.\n").arg(total_failed));
				do_output(QString("Before:   '%1'\n").arg(before));
				do_output("Commands: '" + cmd_text + "'\n");
				do_output(QString("Expected: '%1'\nActual:   '%2'\n").arg(exp).arg(act));
				//cursor.setPosition(pos);
				//editor->setTextCursor(cursor);
				break;
			}
		}
		do_output("\n");
	}
	QString mess = QString("\nTotal: %1 failed / %2 tested. (Success:%3%)")
					.arg(total_failed).arg(total_tested).arg(100.0 - total_failed*100.0/total_tested, 0, 'f', 1);
	statusBar()->showMessage(mess);
	do_output(mess);
}
