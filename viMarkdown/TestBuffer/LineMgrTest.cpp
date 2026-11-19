#include "LineMgrTest.h"
#include "MainWindow.h"
#include "LineMgr.h"

#include <random>

//----------------------------------------------------------------------
//	LineMgr の動作確認テスト
//	・固定ケース（基本操作・行境界・複数行に跨る編集）
//	・QString を参照モデルにしたランダムテスト（seed 固定）
//	各操作の後に、全文・全行の開始位置と長さ・全位置の行番号を照合する
//----------------------------------------------------------------------

typedef LineMgr::pos_t pos_t;

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
//	全文・全行情報・全位置の行番号がモデルと一致するか（1回の照合で1カウント）
static bool checkAll(const LineMgr& lm, const QString& model, const QString& what)
{
	++g_tested_count;
	//	期待する行開始位置列
	QVector<pos_t> starts;
	starts.push_back(0);
	for( qsizetype i = 0; i < model.size(); ++i )
		if( model[i] == QChar('\n') ) starts.push_back(i + 1);
	QString ng;		//	最初に見つかった不一致
	if( lm.size() != model.size() )
		ng = QString("size: %1 expected %2").arg(lm.size()).arg(model.size());
	QString text;
	for( pos_t i = 0; ng.isEmpty() && i < lm.size(); ++i )
		text += lm[i];
	if( ng.isEmpty() && text != model ) {
		qsizetype i = 0;
		qsizetype lim = std::min<qsizetype>(text.size(), model.size());
		while( i < lim && text[i] == model[i] ) ++i;
		ng = QString("text differs at %1").arg(i);
	}
	if( ng.isEmpty() && lm.lineCount() != starts.size() )
		ng = QString("lineCount: %1 expected %2").arg(lm.lineCount()).arg(starts.size());
	for( int ln = 0; ng.isEmpty() && ln < starts.size(); ++ln ) {
		pos_t len = (ln + 1 < starts.size()) ? starts[ln + 1] - 1 - starts[ln]
											 : (pos_t)model.size() - starts[ln];
		if( lm.lineStartPos(ln) != starts[ln] )
			ng = QString("lineStartPos(%1): %2 expected %3").arg(ln).arg(lm.lineStartPos(ln)).arg(starts[ln]);
		else if( lm.lineLength(ln) != len )
			ng = QString("lineLength(%1): %2 expected %3").arg(ln).arg(lm.lineLength(ln)).arg(len);
		else if( lm.lineEndPos(ln) != starts[ln] + len )
			ng = QString("lineEndPos(%1)").arg(ln);
	}
	for( pos_t p = 0; ng.isEmpty() && p <= model.size(); ++p ) {
		int expect = 0;
		for( int ln = 0; ln < starts.size(); ++ln )
			if( starts[ln] <= p ) expect = ln;
		if( lm.lineAtPos(p) != expect )
			ng = QString("lineAtPos(%1): %2 expected %3").arg(p).arg(lm.lineAtPos(p)).arg(expect);
	}
	if( !ng.isEmpty() ) {
		++g_failed_count;
		output(QString("FAILED: %1 (%2)\n").arg(what).arg(ng));
		return false;
	}
	return true;
}

//----------------------------------------------------------------------
//	固定ケース
static void test_fixed()
{
	//	空テキスト（1行・長さ0）
	output("  test: empty\n");
	{
		LineMgr lm;
		verify(lm.lineCount() == 1 && lm.lineStartPos(0) == 0 && lm.lineLength(0) == 0, "empty line info");
		checkAll(lm, "", "empty");
	}
	//	改行なしの挿入
	output("  test: insert without newline\n");
	{
		LineMgr lm;
		QString model = "abc";
		lm.insert(0, model.constData(), model.size());
		checkAll(lm, model, "insert without newline");
	}
	//	複数行ブロックの挿入（末尾の改行で最終行は空行になる）
	output("  test: insert block\n");
	{
		LineMgr lm;
		QString model = "ab\ncd\n";
		lm.insert(0, model.constData(), model.size());
		checkAll(lm, model, "insert block");
	}
	//	行の途中への改行挿入（行が分割される）
	output("  test: insert newline mid line\n");
	{
		LineMgr lm;
		QString model = "abcd";
		lm.insert(0, model.constData(), model.size());
		lm.insert(2, QChar('\n'));
		model.insert(2, QChar('\n'));
		checkAll(lm, model, "insert newline mid line");
	}
	//	行の途中への複数行挿入
	output("  test: insert lines mid line\n");
	{
		LineMgr lm;
		QString model = "ad";
		lm.insert(0, model.constData(), model.size());
		QString ins = "b\nc\n";
		lm.insert(1, ins.constData(), ins.size());
		model.insert(1, ins);
		checkAll(lm, model, "insert lines mid line");
	}
	//	行内の削除
	output("  test: erase in line\n");
	{
		LineMgr lm;
		QString model = "abcdef";
		lm.insert(0, model.constData(), model.size());
		lm.erase(2, 2);
		model.remove(2, 2);
		checkAll(lm, model, "erase in line");
	}
	//	改行を跨ぐ削除（行が結合する）
	output("  test: erase across newline\n");
	{
		LineMgr lm;
		QString model = "ab\ncd\n";
		lm.insert(0, model.constData(), model.size());
		lm.erase(1, 2);		//	"b\n" を削除 -> "acd\n"
		model.remove(1, 2);
		checkAll(lm, model, "erase across newline");
	}
	//	複数行に跨る削除
	output("  test: erase across lines\n");
	{
		LineMgr lm;
		QString model = "a\nb\nc\nd";
		lm.insert(0, model.constData(), model.size());
		lm.erase(1, 4);		//	"\nb\nc" を削除 -> "a\nd"
		model.remove(1, 4);
		checkAll(lm, model, "erase across lines");
	}
	//	末尾改行の直後（最終空行）への挿入
	output("  test: insert into last empty line\n");
	{
		LineMgr lm;
		QString model = "ab\n";
		lm.insert(0, model.constData(), model.size());
		QString tail = "cd";
		lm.insert(lm.size(), tail.constData(), tail.size());
		model += tail;
		checkAll(lm, model, "insert into last empty line");
	}
	//	全削除と再利用
	output("  test: erase all and reuse\n");
	{
		LineMgr lm;
		QString model = "ab\ncd";
		lm.insert(0, model.constData(), model.size());
		lm.erase(0, lm.size());
		model.clear();
		checkAll(lm, model, "erase all");
		lm.insert(0, QChar('x'));
		model = "x";
		checkAll(lm, model, "reuse after erase all");
	}
	//	行頭・行末・改行位置の lineAtPos（checkAll が全位置を照合する）
	output("  test: lineAtPos boundaries\n");
	{
		LineMgr lm;
		QString model = "ab\ncd\n";
		lm.insert(0, model.constData(), model.size());
		verify(lm.lineAtPos(2) == 0, "lineAtPos on newline char");
		verify(lm.lineAtPos(3) == 1, "lineAtPos on line start");
		verify(lm.lineAtPos(6) == 2, "lineAtPos on size");
		checkAll(lm, model, "lineAtPos boundaries");
	}
}

//----------------------------------------------------------------------
//	ランダムテスト（QString を参照モデルにする）
static void test_random(uint64_t seed, int nOps)
{
	output(QString("  test: random (seed = %1, %2 ops)\n").arg(seed).arg(nOps));
	std::mt19937_64 rng(seed);
	static const QChar alphabet[] = {
		QChar('a'), QChar('b'), QChar('\n'), QChar('c'), QChar('\n'), QChar('x'),
	};
	const int nAlpha = (int)std::size(alphabet);
	LineMgr lm;
	QString model;
	for( int op = 0; op < nOps; ++op ) {
		int kind = (int)(rng() % 100);
		if( model.size() > 1500 ) kind = 60;	//	大きくなりすぎたら削除に倒す
		QString desc;
		if( kind < 55 || model.isEmpty() ) {	//	範囲 insert
			pos_t pos = model.isEmpty() ? 0 : (pos_t)(rng() % (uint64_t)(model.size() + 1));
			int len = 1 + (int)(rng() % 12);
			QString chunk;
			for( int i = 0; i < len; ++i ) chunk += alphabet[rng() % nAlpha];
			lm.insert(pos, chunk.constData(), chunk.size());
			model.insert((qsizetype)pos, chunk);
			desc = QString("insert len=%1 at %2").arg(len).arg(pos);
		} else {								//	範囲 erase
			pos_t pos = (pos_t)(rng() % (uint64_t)model.size());
			pos_t len = 1 + (pos_t)(rng() % 12);
			if( pos + len > model.size() ) len = (pos_t)model.size() - pos;
			lm.erase(pos, len);
			model.remove((qsizetype)pos, (qsizetype)len);
			desc = QString("erase len=%1 at %2").arg(len).arg(pos);
		}
		if( !checkAll(lm, model, QString("random seed=%1 op=%2 (%3)").arg(seed).arg(op).arg(desc)) ) {
			output("random test aborted.\n");
			return;
		}
	}
}

void do_test_lineMgr(MainWindow *mw)
{
	g_mw = mw;
	g_tested_count = 0;
	g_failed_count = 0;
	output("\n# Test LineMgr\n\n");
	test_fixed();
	output(QString("  fixed cases: %1 failed / %2 tested.\n").arg(g_failed_count).arg(g_tested_count));
	const uint64_t seeds[] = {1, 42, 20261110};
	for( uint64_t seed : seeds )
		test_random(seed, 3000);
	output(QString("\nTest LineMgr: %1 failed / %2 tested.\n\n")
		.arg(g_failed_count).arg(g_tested_count));
}
