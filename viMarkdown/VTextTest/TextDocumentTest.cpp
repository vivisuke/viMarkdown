#include "TextDocumentTest.h"
#include "MainWindow.h"
#include "TextDocument.h"

#include <random>

//----------------------------------------------------------------------
//	TextDocument の動作確認テスト
//	・固定ケース（setPlainText の往復、改行位置の characterAt、
//	  行を跨ぐ eraseRaw、'\n' の erase(pos)、clear、サロゲートペア）
//	・QString を参照モデルにしたランダムテスト（seed 固定）
//	各操作の後に、全文・文字数・全位置の characterAt・論理行数を照合する
//----------------------------------------------------------------------

typedef TextDocument::pos_t pos_t;

static MainWindow *g_mw = nullptr;
static int g_tested_count = 0;
static int g_failed_count = 0;

static void output(const QString& txt)
{
	g_mw->do_output(txt);
}
static void verify(bool cond, const QString& what)
{
	++g_tested_count;
	if( !cond ) {
		++g_failed_count;
		output(QString("FAILED: %1\n").arg(what));
	}
}
//	全文・文字数・全位置の characterAt・論理行数がモデルと一致するか（1回の照合で1カウント）
static bool checkDoc(const TextDocument& doc, const QString& model, const QString& what)
{
	++g_tested_count;
	QString ng;		//	最初に見つかった不一致
	if( doc.toPlainText() != model )
		ng = "text differs";
	if( ng.isEmpty() && doc.characterCount() != model.size() )
		ng = QString("characterCount: %1 expected %2").arg(doc.characterCount()).arg(model.size());
	for( qsizetype i = 0; ng.isEmpty() && i < model.size(); ++i ) {
		if( doc.characterAt(i) != model[i] )
			ng = QString("characterAt(%1)").arg(i);
	}
	if( ng.isEmpty() && doc.characterAt(model.size()) != QChar() )
		ng = "characterAt(size) is not null";
	int lines = (int)model.count(QChar('\n')) + 1;
	if( ng.isEmpty() && doc.lineMgr().lineCount() != lines )
		ng = QString("lineCount: %1 expected %2").arg(doc.lineMgr().lineCount()).arg(lines);
	if( !ng.isEmpty() ) {
		++g_failed_count;
		output(QString("FAILED: %1 (%2)\n").arg(what).arg(ng));
		return false;
	}
	return true;
}

static void test_fixed()
{
	{	//	空文書
		TextDocument doc;
		verify(doc.characterCount() == 0, "empty: characterCount");
		verify(doc.toPlainText().isEmpty(), "empty: toPlainText");
		verify(doc.lineMgr().lineCount() == 1, "empty: lineCount");
		verify(doc.characterAt(0) == QChar(), "empty: characterAt(0)");
	}
	{	//	setPlainText の往復（末尾改行あり・なし）
		TextDocument doc;
		doc.setPlainText(QStringLiteral("abc\ndef\n"));
		checkDoc(doc, QStringLiteral("abc\ndef\n"), "setPlainText trailing NL");
		verify(doc.characterAt(3) == QChar('\n'), "characterAt at NL");
		doc.setPlainText(QStringLiteral("abc\ndef"));
		checkDoc(doc, QStringLiteral("abc\ndef"), "setPlainText no trailing NL");
		doc.setPlainText(QString());
		checkDoc(doc, QString(), "setPlainText empty");
	}
	{	//	insertRaw（先頭・途中・末尾、複数行）
		TextDocument doc;
		doc.setPlainText(QStringLiteral("world"));
		doc.insertRaw(0, QStringLiteral("hello "));
		checkDoc(doc, QStringLiteral("hello world"), "insertRaw head");
		doc.insertRaw(5, QStringLiteral("\n"));
		checkDoc(doc, QStringLiteral("hello\n world"), "insertRaw newline");
		doc.insertRaw(doc.characterCount(), QStringLiteral("\n!"));
		checkDoc(doc, QStringLiteral("hello\n world\n!"), "insertRaw tail");
	}
	{	//	eraseRaw（行を跨ぐ削除、空範囲）
		TextDocument doc;
		doc.setPlainText(QStringLiteral("abc\ndef\nghi"));
		doc.eraseRaw(2, 6);	//	"c\nde" を削除
		checkDoc(doc, QStringLiteral("abf\nghi"), "eraseRaw across lines");
		doc.eraseRaw(1, 1);	//	空範囲は無変化
		checkDoc(doc, QStringLiteral("abf\nghi"), "eraseRaw empty range");
	}
	{	//	erase(pos)（1文字削除。'\n' を消すと行が連結する）
		TextDocument doc;
		doc.setPlainText(QStringLiteral("abc\ndef"));
		doc.erase(1);
		checkDoc(doc, QStringLiteral("ac\ndef"), "erase(pos) char");
		doc.erase(2);	//	'\n'
		checkDoc(doc, QStringLiteral("acdef"), "erase(pos) newline");
	}
	{	//	insert / erase（undo 対象側。今は Raw と同じ動作）
		TextDocument doc;
		doc.setPlainText(QStringLiteral("middle"));
		doc.insert(0, QStringLiteral("start-"));
		doc.insert(doc.characterCount(), QStringLiteral("-end"));
		checkDoc(doc, QStringLiteral("start-middle-end"), "insert");
		doc.erase(0, 6);
		checkDoc(doc, QStringLiteral("middle-end"), "erase range");
	}
	{	//	clear
		TextDocument doc;
		doc.setPlainText(QStringLiteral("abc\ndef\n"));
		doc.clear();
		checkDoc(doc, QString(), "clear");
	}
	{	//	サロゲートペア（素通し。characterAt は各ハーフを返す）
		TextDocument doc;
		QString s = QStringLiteral("a") + QChar(0xD83D) + QChar(0xDC31) + QStringLiteral("b");
		doc.setPlainText(s);
		checkDoc(doc, s, "surrogate pair");
		verify(doc.characterCount() == 4, "surrogate: count in UTF-16 units");
	}
}

static void test_random(unsigned int seed, int nOps)
{
	std::mt19937 rng(seed);
	TextDocument doc;
	QString model;
	static const QString pieces[] = {
		QStringLiteral("a"), QStringLiteral("XYZ"), QStringLiteral("\n"),
		QStringLiteral("ab\ncd"), QStringLiteral("\n\n"),
		QString(QChar(0x3042)), QStringLiteral("p\n"),
		QString() + QChar(0xD83D) + QChar(0xDC31),
	};
	const int nPieces = (int)(sizeof(pieces) / sizeof(pieces[0]));
	for( int op = 0; op < nOps; ++op ) {
		int kind = (int)(rng() % 100);
		if( model.size() > 1200 ) kind = 60;	//	大きくなりすぎたら削除に倒す
		QString desc;
		if( kind < 40 || model.isEmpty() ) {	//	insertRaw
			pos_t pos = model.isEmpty() ? 0 : (pos_t)(rng() % (uint64_t)(model.size() + 1));
			const QString& chunk = pieces[rng() % nPieces];
			doc.insertRaw(pos, chunk);
			model.insert((qsizetype)pos, chunk);
			desc = QString("insertRaw at %1").arg(pos);
		} else if( kind < 65 ) {				//	eraseRaw
			pos_t p1 = (pos_t)(rng() % (uint64_t)(model.size() + 1));
			pos_t p2 = p1 + (pos_t)(rng() % 8);
			if( p2 > model.size() ) p2 = model.size();
			doc.eraseRaw(p1, p2);
			model.remove((qsizetype)p1, (qsizetype)(p2 - p1));
			desc = QString("eraseRaw [%1,%2)").arg(p1).arg(p2);
		} else if( kind < 80 ) {				//	erase(pos)
			pos_t pos = (pos_t)(rng() % (uint64_t)model.size());
			doc.erase(pos);
			model.remove((qsizetype)pos, 1);
			desc = QString("erase at %1").arg(pos);
		} else if( kind < 90 ) {				//	insert（undo 対象側）
			pos_t pos = (pos_t)(rng() % (uint64_t)(model.size() + 1));
			const QString& chunk = pieces[rng() % nPieces];
			doc.insert(pos, chunk);
			model.insert((qsizetype)pos, chunk);
			desc = QString("insert at %1").arg(pos);
		} else if( kind < 95 ) {				//	setPlainText
			const QString& chunk = pieces[rng() % nPieces];
			doc.setPlainText(chunk);
			model = chunk;
			desc = "setPlainText";
		} else {								//	clear
			doc.clear();
			model.clear();
			desc = "clear";
		}
		if( !checkDoc(doc, model, QString("random seed=%1 op=%2 (%3)").arg(seed).arg(op).arg(desc)) ) {
			output("random test aborted.\n");
			return;
		}
	}
}

void do_test_textDocument(MainWindow *mw)
{
	g_mw = mw;
	g_tested_count = 0;
	g_failed_count = 0;
	output("\n# Test TextDocument\n\n");
	test_fixed();
	output(QString("  fixed cases: %1 failed / %2 tested.\n").arg(g_failed_count).arg(g_tested_count));
	test_random(1, 3000);
	test_random(42, 3000);
	test_random(20261112, 3000);
	output(QString("\nTest TextDocument: %1 failed / %2 tested.\n\n")
		.arg(g_failed_count).arg(g_tested_count));
}
