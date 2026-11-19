#include "TextDocumentTest.h"
#include "MainWindow.h"
#include "TextDocument.h"
#include "TextCursor.h"

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

//----------------------------------------------------------------------
//	LineBlock / VLineBlock のテスト
//----------------------------------------------------------------------
static void test_blocks_fixed()
{
	{	//	無効ハンドル
		LineBlock b;
		verify(!b.isValid(), "block: default invalid");
		verify(b.position() == -1 && b.length() == 0 && b.text().isEmpty(), "block: invalid values");
		verify(b.next().blockNumber() == -1, "block: next from -1 stays -1");
		VLineBlock v;
		verify(!v.isValid() && v.position() == -1 && v.length() == 0 && v.text().isEmpty(), "vline: invalid values");
	}
	{	//	LineBlock の走査・位置・長さ・等価性
		TextDocument doc;
		doc.setPlainText(QStringLiteral("ab\ncde\n\nf"));
		QStringList want = {QStringLiteral("ab\n"), QStringLiteral("cde\n"), QStringLiteral("\n"), QStringLiteral("f")};
		int i = 0;
		for( LineBlock b = doc.firstBlock(); b.isValid(); b = b.next(), ++i ) {
			verify(b.blockNumber() == i, "block: number");
			verify(b.text() == want[i], "block: text with NL");
			verify(b.length() == (int)want[i].size(), "block: length incl NL");
		}
		verify(i == 4, "block: count by iteration");
		verify(doc.end() == doc.findBlockByNumber(4), "block: end sentinel");
		verify(!doc.end().isValid(), "block: end invalid");
		verify(doc.end().previous() == doc.lastBlock(), "block: previous from end");
		verify(doc.firstBlock().previous().blockNumber() == -1, "block: previous from 0");
		verify(doc.firstBlock().position() == 0, "block: pos 0");
		verify(doc.findBlockByNumber(1).position() == 3, "block: pos line1");
		verify(doc.findBlock(4) == doc.findBlockByNumber(1), "block: findBlock in line1");
		verify(doc.findBlock(8) == doc.findBlockByNumber(3), "block: findBlock last line");
		verify(doc.findBlockByNumber(-1) != doc.firstBlock(), "block: neq");
		TextDocument doc2;
		doc2.setPlainText(QStringLiteral("ab\ncde\n\nf"));
		verify(doc2.firstBlock() != doc.firstBlock(), "block: different mgr neq");
	}
	{	//	VLineBlock（行幅3で折り返し）
		TextDocument doc;
		doc.setPlainText(QStringLiteral("abcdefg\nhi"));
		doc.lineMgr().setLineWidth(3);
		QStringList want = {QStringLiteral("abc"), QStringLiteral("def"), QStringLiteral("g\n"), QStringLiteral("hi")};
		int i = 0;
		QString cat;
		for( VLineBlock v = doc.firstVLine(); v.isValid(); v = v.next(), ++i ) {
			verify(v.text() == want[i], "vline: text");
			verify(v.length() == (int)want[i].size(), "vline: length");
			cat += v.text();
		}
		verify(i == 4, "vline: count");
		verify(cat == doc.toPlainText(), "vline: concatenation == document");
		verify(doc.findVLineByNumber(2).line() == 0, "vline: line of vline2");
		verify(doc.findVLineByNumber(3).line() == 1, "vline: line of vline3");
		verify(doc.findVLine(5) == doc.findVLineByNumber(1), "vline: findVLine");
		verify(doc.findBlockByLineNumber(2) == doc.findBlockByNumber(0), "block: findBlockByLineNumber");
		verify(doc.findBlockByLineNumber(99).isValid() == false, "block: findBlockByLineNumber oob");
	}
	{	//	VLineBlock（全角、行幅4）
		TextDocument doc;
		doc.setPlainText(QString(QChar(0x3042)) + QChar(0x3044) + QChar(0x3046) + QChar(0x3048) + QChar(0x304a));
		doc.lineMgr().setLineWidth(4);
		verify(doc.lineMgr().vlineCount() == 3, "vline wide: count");
		verify(doc.findVLineByNumber(0).length() == 2, "vline wide: len 0");
		verify(doc.findVLineByNumber(2).text() == QString(QChar(0x304a)), "vline wide: last text");
	}
}

//	ランダム文書での全ブロック・全表示行スイープ
static void test_blocks_random(unsigned int seed)
{
	std::mt19937 rng(seed);
	TextDocument doc;
	QString model;
	static const QString pieces[] = {
		QStringLiteral("a"), QStringLiteral("XYZ"), QStringLiteral("\n"),
		QStringLiteral("ab\ncd"), QString(QChar(0x3042)), QStringLiteral("\n\n"),
	};
	const int nPieces = (int)(sizeof(pieces) / sizeof(pieces[0]));
	for( int op = 0; op < 300; ++op ) {
		TextDocument::pos_t pos = model.isEmpty() ? 0 : (TextDocument::pos_t)(rng() % (uint64_t)(model.size() + 1));
		const QString& chunk = pieces[rng() % nPieces];
		doc.insertRaw(pos, chunk);
		model.insert((qsizetype)pos, chunk);
	}
	//	論理行スイープ
	QStringList lines = model.split(QChar('\n'));
	QString cat;
	TextDocument::pos_t expectPos = 0;
	for( int i = 0; i < lines.size(); ++i ) {
		LineBlock b = doc.findBlockByNumber(i);
		QString want = lines[i];
		if( i + 1 < lines.size() ) want += QChar('\n');
		verify(b.isValid() && b.position() == expectPos, "rblock: position");
		verify(b.text() == want, "rblock: text");
		verify(b.length() == (int)want.size(), "rblock: length");
		verify(doc.findBlock(expectPos) == b, "rblock: findBlock");
		cat += b.text();
		expectPos += want.size();
	}
	verify(cat == model, "rblock: concatenation == document");
	//	表示行スイープ（複数行幅）
	static const int widths[] = {1, 3, 10, 70};
	for( int w : widths ) {
		doc.lineMgr().setLineWidth(w);
		QString vcat;
		TextDocument::pos_t prevPos = -1;
		int prevLine = -1;
		for( VLineBlock v = doc.firstVLine(); v.isValid(); v = v.next() ) {
			verify(v.position() > prevPos, "rvline: positions increasing");
			verify(v.line() >= prevLine, "rvline: lines non-decreasing");
			verify(v.length() == (int)v.text().size(), "rvline: length == text size");
			verify(doc.findVLine(v.position()) == v, "rvline: findVLine");
			prevPos = v.position();
			prevLine = v.line();
			vcat += v.text();
		}
		verify(vcat == model, "rvline: concatenation == document");
	}
}

//----------------------------------------------------------------------
//	TextCursor のテスト
//----------------------------------------------------------------------
static void test_cursor_fixed()
{
	{	//	移動とクランプ
		TextDocument doc;
		doc.setPlainText(QStringLiteral("ab\ncde\nf"));	//	サイズ8
		TextCursor c(&doc);
		verify(c.position() == 0 && !c.hasSelection(), "cur: init");
		c.setPosition(100);
		verify(c.position() == 8, "cur: clamp to size");
		verify(!c.movePosition(TextCursor::Right), "cur: right at end fails");
		c.setPosition(4);	//	'd'
		verify(c.blockNumber() == 1 && c.positionInBlock() == 1, "cur: block/column");
		verify(c.block().text() == QStringLiteral("cde\n"), "cur: block text");
		c.movePosition(TextCursor::StartOfLine);
		verify(c.position() == 3, "cur: start of line");
		c.movePosition(TextCursor::EndOfLine);
		verify(c.position() == 6, "cur: end of line");
		c.movePosition(TextCursor::Left);
		verify(c.position() == 5, "cur: left");
		c.movePosition(TextCursor::End);
		verify(c.position() == 8, "cur: end");
		c.movePosition(TextCursor::Start);
		verify(c.position() == 0, "cur: start");
	}
	{	//	選択と insertText（置換は1つの undo 単位）
		TextDocument doc;
		doc.setPlainText(QStringLiteral("abcdef"));
		TextCursor c(&doc);
		c.setPosition(1);
		c.setPosition(4, TextCursor::KeepAnchor);
		verify(c.hasSelection() && c.selectionStart() == 1 && c.selectionEnd() == 4, "cur: selection range");
		verify(c.selectedText() == QStringLiteral("bcd"), "cur: selectedText");
		c.insertText(QStringLiteral("X"));
		verify(doc.toPlainText() == QStringLiteral("aXef"), "cur: insertText replaces selection");
		verify(c.position() == 2 && !c.hasSelection(), "cur: after insert");
		doc.undo();
		verify(doc.toPlainText() == QStringLiteral("abcdef"), "cur: replace is one undo unit");
	}
	{	//	編集操作
		TextDocument doc;
		doc.setPlainText(QStringLiteral("abc"));
		TextCursor c(&doc, 1);
		c.deleteChar();
		verify(doc.toPlainText() == QStringLiteral("ac") && c.position() == 1, "cur: deleteChar");
		c.deletePreviousChar();
		verify(doc.toPlainText() == QStringLiteral("c") && c.position() == 0, "cur: deletePreviousChar");
		c.insertText(QStringLiteral("xy\nz"));
		verify(doc.toPlainText() == QStringLiteral("xy\nzc") && c.position() == 4, "cur: insertText multiline");
	}
	{	//	外部編集による自動調整
		TextDocument doc;
		doc.setPlainText(QStringLiteral("hello world"));
		TextCursor a(&doc, 8);
		TextCursor b(&doc, 2);
		doc.insertRaw(0, QStringLiteral(">>"));
		verify(a.position() == 10 && b.position() == 4, "cur: adjust insert before");
		doc.insertRaw(10, QStringLiteral("!"));	//	a の位置ちょうど → 動かない
		verify(a.position() == 10, "cur: insert at pos keeps pos");
		doc.eraseRaw(0, 2);
		verify(a.position() == 8 && b.position() == 2, "cur: adjust erase before");
		TextCursor d(&doc, 5);
		doc.eraseRaw(3, 9);	//	範囲内 → 先頭へクランプ
		verify(d.position() == 3, "cur: erase clamps inside");
		verify(a.position() == 3, "cur: erase clamps at end boundary");
		doc.setPlainText(QStringLiteral("new"));
		verify(a.position() == 0 && b.position() == 0, "cur: setPlainText resets");
	}
	{	//	コピーも登録される／破棄で解除される
		TextDocument doc;
		doc.setPlainText(QStringLiteral("abcdef"));
		TextCursor a(&doc, 3);
		{
			TextCursor copy = a;
			doc.insertRaw(0, QStringLiteral("zz"));
			verify(copy.position() == 5 && a.position() == 5, "cur: copy adjusts");
		}
		doc.insertRaw(0, QStringLiteral("q"));	//	copy 破棄後も落ちない
		verify(a.position() == 6, "cur: after copy destroyed");
	}
	{	//	Up/Down（折り返し、行幅3）
		TextDocument doc;
		doc.setPlainText(QStringLiteral("abcdefg\nhi"));
		doc.lineMgr().setLineWidth(3);
		TextCursor c(&doc, 5);	//	表示行1 "def" のカラム2
		c.movePosition(TextCursor::Down);
		verify(c.position() == 7, "cur: down to NL pos");	//	表示行2 "g"、目標2 → 末尾の '\n' 位置
		c.movePosition(TextCursor::Down);
		verify(c.position() == 10, "cur: down keeps goal");	//	表示行3 "hi" の末尾
		c.movePosition(TextCursor::Up);
		verify(c.position() == 7, "cur: up keeps goal");
		c.movePosition(TextCursor::Left);	//	目標カラムリセット（位置7→6、カラム0）
		c.movePosition(TextCursor::Down);
		verify(c.position() == 8, "cur: goal reset by left");
	}
	{	//	Up/Down（全角）
		TextDocument doc;
		doc.setPlainText(QString(QChar(0x3042)) + QChar(0x3044) + QChar(0x3046));
		doc.lineMgr().setLineWidth(4);	//	"あい" / "う"
		TextCursor c(&doc, 1);	//	カラム2（あ の後）
		c.movePosition(TextCursor::Down);
		verify(c.position() == 3, "cur: down wide chars");	//	"う" を通って末尾
	}
	{	//	UpLine/DownLine（論理行単位、文字カラム保持）
		TextDocument doc;
		doc.setPlainText(QStringLiteral("abcdefg\nhi\nx"));	//	行開始 0 / 8 / 11、サイズ12
		doc.lineMgr().setLineWidth(3);
		TextCursor c(&doc, 5);	//	論理行0、文字カラム5
		verify(c.movePosition(TextCursor::DownLine), "curline: down 1");
		verify(c.position() == 10, "curline: clamped to line1 length");	//	8 + min(5, 2)
		verify(c.movePosition(TextCursor::DownLine), "curline: down 2");
		verify(c.position() == 12, "curline: clamped to line2 length");	//	11 + min(5, 1)
		verify(!c.movePosition(TextCursor::DownLine), "curline: down at last line fails");
		verify(c.position() == 12, "curline: stays");
		verify(c.movePosition(TextCursor::UpLine), "curline: up 1");
		verify(c.position() == 10, "curline: goal column kept");	//	8 + min(5, 2)
		verify(c.movePosition(TextCursor::UpLine), "curline: up 2");
		verify(c.position() == 5, "curline: back to column 5");
		verify(!c.movePosition(TextCursor::UpLine), "curline: up at first line fails");
		//	表示単位 Down との違い（同じ開始位置から）
		c.setPosition(5);
		c.movePosition(TextCursor::Down);
		verify(c.position() == 7, "curline: visual down differs");	//	表示行2の末尾
		//	他の移動で目標カラムがリセットされる
		c.setPosition(9);	//	論理行1、カラム1
		c.movePosition(TextCursor::DownLine);
		verify(c.position() == 12, "curline: down with column 1");	//	11 + 1
		c.movePosition(TextCursor::Left);	//	リセット（位置12→11、カラム0）
		c.movePosition(TextCursor::UpLine);
		verify(c.position() == 8, "curline: goal recomputed after left");	//	カラム0 → 行1の先頭
	}
}

//	モデル（QString + 位置）と突き合わせるランダムテスト
static void test_cursor_random(unsigned int seed)
{
	std::mt19937 rng(seed);
	TextDocument doc;
	QString model;
	TextCursor cur(&doc);
	TextDocument::pos_t mpos = 0, manchor = 0;
	int mgoalLine = -1;	//	論理行移動の目標カラム（文字単位）
	auto clampM = [&]() {
		if( mpos < 0 ) mpos = 0;
		if( mpos > model.size() ) mpos = model.size();
		if( manchor < 0 ) manchor = 0;
		if( manchor > model.size() ) manchor = model.size();
	};
	auto adjIns = [&](TextDocument::pos_t p, TextDocument::pos_t len) {
		if( mpos > p ) mpos += len;
		if( manchor > p ) manchor += len;
		mgoalLine = -1;
	};
	auto adjEra = [&](TextDocument::pos_t p1, TextDocument::pos_t p2) {
		auto one = [&](TextDocument::pos_t p) {
			if( p <= p1 ) return p;
			if( p >= p2 ) return p - (p2 - p1);
			return p1;
		};
		mpos = one(mpos); manchor = one(manchor);
		mgoalLine = -1;
	};
	static const QString pieces[] = {
		QStringLiteral("a"), QStringLiteral("XYZ"), QStringLiteral("\n"),
		QStringLiteral("ab\ncd"), QStringLiteral(" "),
	};
	const int nPieces = (int)(sizeof(pieces) / sizeof(pieces[0]));
	for( int op = 0; op < 1500; ++op ) {
		int what = (int)(rng() % 10);
		if( what <= 1 ) {	//	カーソル移動
			int mv = (int)(rng() % 8);
			bool keep = (rng() % 2) != 0;
			TextCursor::MoveMode mode = keep ? TextCursor::KeepAnchor : TextCursor::MoveAnchor;
			TextCursor::MoveOperation mop = TextCursor::Left;
			switch( mv ) {
			case 0: mop = TextCursor::Left; if( mpos > 0 ) --mpos; break;
			case 1: mop = TextCursor::Right; if( mpos < model.size() ) ++mpos; break;
			case 2: mop = TextCursor::StartOfLine;
				while( mpos > 0 && model[(int)mpos - 1] != QChar('\n') ) --mpos; break;
			case 3: mop = TextCursor::EndOfLine;
				while( mpos < model.size() && model[(int)mpos] != QChar('\n') ) ++mpos; break;
			case 4: mop = TextCursor::Start; mpos = 0; break;
			case 6: {	//	UpLine
				mop = TextCursor::UpLine;
				int ls = (int)mpos;
				while( ls > 0 && model[ls - 1] != QChar('\n') ) --ls;
				if( ls > 0 ) {
					if( mgoalLine < 0 ) mgoalLine = (int)mpos - ls;
					int pls = ls - 1;
					while( pls > 0 && model[pls - 1] != QChar('\n') ) --pls;
					int plen = (ls - 1) - pls;
					mpos = pls + (mgoalLine < plen ? mgoalLine : plen);
				}
				break;
			}
			case 7: {	//	DownLine
				mop = TextCursor::DownLine;
				int ls = (int)mpos;
				while( ls > 0 && model[ls - 1] != QChar('\n') ) --ls;
				int le = (int)mpos;
				while( le < model.size() && model[le] != QChar('\n') ) ++le;
				if( le < model.size() ) {
					if( mgoalLine < 0 ) mgoalLine = (int)mpos - ls;
					int ns = le + 1;
					int ne = ns;
					while( ne < model.size() && model[ne] != QChar('\n') ) ++ne;
					int nlen = ne - ns;
					mpos = ns + (mgoalLine < nlen ? mgoalLine : nlen);
				}
				break;
			}
			default: mop = TextCursor::End; mpos = model.size(); break;
			}
			if( mv != 6 && mv != 7 ) mgoalLine = -1;
			cur.movePosition(mop, mode);
			if( !keep ) manchor = mpos;
		} else if( what == 2 ) {	//	setPosition
			TextDocument::pos_t p = model.isEmpty() ? 0 : (TextDocument::pos_t)(rng() % (uint64_t)(model.size() + 3));
			bool keep = (rng() % 2) != 0;
			cur.setPosition(p, keep ? TextCursor::KeepAnchor : TextCursor::MoveAnchor);
			mpos = p; clampM();
			mgoalLine = -1;
			if( !keep ) manchor = mpos;
		} else if( what <= 4 ) {	//	カーソル経由の挿入
			const QString& chunk = pieces[rng() % nPieces];
			TextDocument::pos_t s = mpos < manchor ? mpos : manchor;
			TextDocument::pos_t e = mpos < manchor ? manchor : mpos;
			model.remove((qsizetype)s, (qsizetype)(e - s));
			model.insert((qsizetype)s, chunk);
			cur.insertText(chunk);
			mpos = manchor = s + chunk.size();
			mgoalLine = -1;
		} else if( what == 5 ) {	//	deleteChar
			TextDocument::pos_t s = mpos < manchor ? mpos : manchor;
			TextDocument::pos_t e = mpos < manchor ? manchor : mpos;
			if( s != e ) { model.remove((qsizetype)s, (qsizetype)(e - s)); mpos = manchor = s; }
			else if( mpos < model.size() ) model.remove((qsizetype)mpos, 1);
			cur.deleteChar();
			mgoalLine = -1;
		} else if( what == 6 ) {	//	deletePreviousChar
			TextDocument::pos_t s = mpos < manchor ? mpos : manchor;
			TextDocument::pos_t e = mpos < manchor ? manchor : mpos;
			if( s != e ) { model.remove((qsizetype)s, (qsizetype)(e - s)); mpos = manchor = s; }
			else if( mpos > 0 ) { model.remove((qsizetype)(mpos - 1), 1); --mpos; manchor = mpos; }
			cur.deletePreviousChar();
			mgoalLine = -1;
		} else if( what <= 8 ) {	//	外部挿入
			TextDocument::pos_t p = model.isEmpty() ? 0 : (TextDocument::pos_t)(rng() % (uint64_t)(model.size() + 1));
			const QString& chunk = pieces[rng() % nPieces];
			doc.insertRaw(p, chunk);
			model.insert((qsizetype)p, chunk);
			adjIns(p, chunk.size());
		} else {	//	外部削除
			if( !model.isEmpty() ) {
				TextDocument::pos_t p1 = (TextDocument::pos_t)(rng() % (uint64_t)model.size());
				TextDocument::pos_t p2 = p1 + 1 + (TextDocument::pos_t)(rng() % 4);
				if( p2 > model.size() ) p2 = model.size();
				doc.eraseRaw(p1, p2);
				model.remove((qsizetype)p1, (qsizetype)(p2 - p1));
				adjEra(p1, p2);
			}
		}
		clampM();
		verify(doc.toPlainText() == model, "rcur: text");
		verify(cur.position() == mpos && cur.anchor() == manchor, "rcur: position/anchor");
		if( g_failed_count > 20 ) return;
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
	test_blocks_fixed();
	test_blocks_random(5);
	test_cursor_fixed();
	test_cursor_random(7);
	test_cursor_random(1234);
	output(QString("  fixed cases (incl. blocks): %1 failed / %2 tested.\n").arg(g_failed_count).arg(g_tested_count));
	output(QString("  fixed cases (incl. cursor): %1 failed / %2 tested.\n").arg(g_failed_count).arg(g_tested_count));
	output(QString("\nTest TextDocument: %1 failed / %2 tested.\n\n")
		.arg(g_failed_count).arg(g_tested_count));
}
