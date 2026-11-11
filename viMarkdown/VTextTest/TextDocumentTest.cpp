#include "TextDocumentTest.h"
#include "MainWindow.h"
#include "TextDocument.h"

#include <QVector>
#include <random>

//----------------------------------------------------------------------
//	TextDocument の動作確認テスト
//	・固定ケース（setPlainText の往復、改行位置の characterAt、
//	  行を跨ぐ eraseRaw、'\n' の erase(pos)、clear、サロゲートペア）
//	・QString を参照モデルにしたランダムテスト（seed 固定）
//	各操作の後に、全文・文字数・全位置の characterAt・論理行数を照合する
//----------------------------------------------------------------------



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

//----------------------------------------------------------------------
//	undo / redo のテスト（TextDocument に統合された UndoMgr）
//----------------------------------------------------------------------
static void test_undo_fixed()
{
	{	//	挿入の undo / redo と canUndo / canRedo
		TextDocument doc;
		verify(!doc.canUndo() && !doc.canRedo(), "undo: initial state");
		doc.insert(0, QStringLiteral("\nabc"));
		verify(doc.canUndo() && !doc.canRedo(), "undo: after insert");
		doc.undo();
		checkDoc(doc, QString(), "undo: insert undone");
		verify(!doc.canUndo() && doc.canRedo(), "undo: after undo");
		doc.redo();
		checkDoc(doc, QStringLiteral("\nabc"), "undo: insert redone");
	}
	{	//	連続挿入はマージされる（改行で始まる挿入はマージされない）
		TextDocument doc;
		doc.insert(0, QStringLiteral("a"));
		doc.insert(1, QStringLiteral("b"));
		doc.insert(2, QStringLiteral("c"));
		doc.undo();	//	3回の挿入が1アクションにマージされ、一括で戻る
		checkDoc(doc, QString(), "undo: merged inserts");
		doc.redo();
		checkDoc(doc, QStringLiteral("abc"), "undo: merged inserts redone");
		doc.insert(3, QStringLiteral("\n"));
		doc.insert(4, QStringLiteral("x"));
		//	改行の挿入自体はマージされないが、直後の連続挿入は改行のアクションにマージされる
		doc.undo();
		checkDoc(doc, QStringLiteral("abc"), "undo: newline + following insert");
		doc.undo();
		checkDoc(doc, QString(), "undo: all undone");
	}
	{	//	削除の undo / redo
		TextDocument doc;
		doc.insert(0, QStringLiteral("hello"));
		doc.erase(1, 3);	//	"el" を削除
		checkDoc(doc, QStringLiteral("hlo"), "undo: erased");
		doc.undo();
		checkDoc(doc, QStringLiteral("hello"), "undo: erase undone");
		doc.redo();
		checkDoc(doc, QStringLiteral("hlo"), "undo: erase redone");
		doc.erase(0);	//	1文字削除
		checkDoc(doc, QStringLiteral("lo"), "undo: erase(pos)");
		doc.undo();
		checkDoc(doc, QStringLiteral("hlo"), "undo: erase(pos) undone");
	}
	{	//	ブロック（openBlock / closeBlock）で複数操作を一括 undo
		TextDocument doc;
		doc.undoMgr().openBlock();
		doc.insert(0, QStringLiteral("\nfoo"));
		doc.insert(4, QStringLiteral("\nbar"));
		doc.undoMgr().closeBlock();
		checkDoc(doc, QStringLiteral("\nfoo\nbar"), "undo: block content");
		doc.undo();
		checkDoc(doc, QString(), "undo: block undone at once");
		doc.redo();
		checkDoc(doc, QStringLiteral("\nfoo\nbar"), "undo: block redone");
	}
	{	//	変更状態と保存ポイント
		TextDocument doc;
		verify(!doc.isModified(), "modified: initial");
		doc.insert(0, QStringLiteral("x"));
		verify(doc.isModified(), "modified: after insert");
		doc.onSaved();
		verify(!doc.isModified(), "modified: after onSaved");
		doc.undo();
		verify(doc.isModified(), "modified: undo from save point");
		doc.redo();
		verify(!doc.isModified(), "modified: redo to save point");
	}
	{	//	setPlainText / clear は undo 履歴を破棄する
		TextDocument doc;
		doc.insert(0, QStringLiteral("abc"));
		doc.setPlainText(QStringLiteral("new"));
		verify(!doc.canUndo(), "undo: cleared by setPlainText");
		doc.insert(0, QStringLiteral("y"));
		doc.clear();
		verify(!doc.canUndo(), "undo: cleared by clear");
	}
}

//	状態スナップショット列をモデルにしたランダム undo / redo テスト。
//	挿入チャンクは必ず '\n' で始める（マージが起きず、1操作＝1アクションになる）
static void test_undo_random(unsigned int seed, int nOps)
{
	std::mt19937 rng(seed);
	TextDocument doc;
	QVector<QString> states;
	states.push_back(QString());
	int cur = 0;
	static const QString chunks[] = {
		QStringLiteral("\na"), QStringLiteral("\n"), QStringLiteral("\nxyz"),
		QStringLiteral("\n") + QChar(0x3042),
	};
	const int nChunks = (int)(sizeof(chunks) / sizeof(chunks[0]));
	for( int op = 0; op < nOps; ++op ) {
		const QString& model = states[cur];
		int kind = (int)(rng() % 100);
		QString desc;
		if( kind < 45 || model.isEmpty() ) {	//	挿入
			TextDocument::pos_t pos = model.isEmpty() ? 0 : (TextDocument::pos_t)(rng() % (uint64_t)(model.size() + 1));
			const QString& chunk = chunks[rng() % nChunks];
			doc.insert(pos, chunk);
			QString next = model;
			next.insert((qsizetype)pos, chunk);
			states.resize(cur + 1);
			states.push_back(next);
			++cur;
			desc = QString("insert at %1").arg(pos);
		} else if( kind < 70 ) {				//	削除
			TextDocument::pos_t p1 = (TextDocument::pos_t)(rng() % (uint64_t)model.size());
			TextDocument::pos_t p2 = p1 + 1 + (TextDocument::pos_t)(rng() % 6);
			if( p2 > model.size() ) p2 = model.size();
			doc.erase(p1, p2);
			QString next = model;
			next.remove((qsizetype)p1, (qsizetype)(p2 - p1));
			states.resize(cur + 1);
			states.push_back(next);
			++cur;
			desc = QString("erase [%1,%2)").arg(p1).arg(p2);
		} else if( kind < 85 ) {				//	undo
			if( cur <= 0 ) continue;
			doc.undo();
			--cur;
			desc = "undo";
		} else {								//	redo
			if( cur + 1 >= states.size() ) continue;
			doc.redo();
			++cur;
			desc = "redo";
		}
		bool ok = doc.toPlainText() == states[cur]
			&& doc.canUndo() == (cur > 0)
			&& doc.canRedo() == (cur + 1 < states.size());
		++g_tested_count;
		if( !ok ) {
			++g_failed_count;
			output(QString("FAILED: undo random seed=%1 op=%2 (%3)\n").arg(seed).arg(op).arg(desc));
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
	test_undo_fixed();
	output(QString("  fixed cases (incl. undo): %1 failed / %2 tested.\n").arg(g_failed_count).arg(g_tested_count));
	test_undo_random(3, 2000);
	test_undo_random(99, 2000);
	output(QString("\nTest TextDocument: %1 failed / %2 tested.\n\n")
		.arg(g_failed_count).arg(g_tested_count));
}
