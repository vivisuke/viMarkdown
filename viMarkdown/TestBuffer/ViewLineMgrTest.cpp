#include "ViewLineMgrTest.h"
#include "MainWindow.h"
#include "ViewLineMgr.h"

#include <QVector>
#include <random>

//----------------------------------------------------------------------
//	ViewLineMgr の動作確認テスト
//	・固定ケース（折り返し・全角の跨ぎ・タブ幅・空行・行幅変更）
//	・QString＋仕様からの再計算を参照モデルにしたランダムテスト（seed 固定）
//	各操作の後に、全表示行の開始位置と長さ・行対応・全位置の表示行と桁を照合する
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
//	参照モデル用の文字幅（仕様：タブ=次のタブストップまで、全角=2、それ以外=1。BMP外は2）
static int refCharWidth(const QString& s, qsizetype i, qsizetype& adv, int col, int tabWidth)
{
	adv = 1;
	QChar ch = s[i];
	if( ch.isHighSurrogate() && i + 1 < s.size() && s[i + 1].isLowSurrogate() ) {
		adv = 2;
		return 2;
	}
	if( ch == QChar('\t') ) return tabWidth - (col % tabWidth);
	uint u = ch.unicode();
	bool wide = (u >= 0x1100 && u <= 0x115F) || (u >= 0x2E80 && u <= 0x303E)
		|| (u >= 0x3041 && u <= 0x33FF) || (u >= 0x3400 && u <= 0x4DBF)
		|| (u >= 0x4E00 && u <= 0x9FFF) || (u >= 0xA000 && u <= 0xA4CF)
		|| (u >= 0xAC00 && u <= 0xD7A3) || (u >= 0xF900 && u <= 0xFAFF)
		|| (u >= 0xFE10 && u <= 0xFE19) || (u >= 0xFE30 && u <= 0xFE6B)
		|| (u >= 0xFF00 && u <= 0xFF60) || (u >= 0xFFE0 && u <= 0xFFE6);
	return wide ? 2 : 1;
}
//	全文・全表示行情報・全位置の表示行と桁がモデルと一致するか（1回の照合で1カウント）
static bool checkAll(const ViewLineMgr& vm, const QString& model, int width, const QString& what)
{
	++g_tested_count;
	QString ng;		//	最初に見つかった不一致
	QString text;
	for( pos_t i = 0; i < vm.size(); ++i )
		text += vm[i];
	if( text != model )
		ng = "text differs";
	//	モデルの論理行
	QVector<pos_t> lstarts;
	lstarts.push_back(0);
	for( qsizetype i = 0; i < model.size(); ++i )
		if( model[i] == QChar('\n') ) lstarts.push_back(i + 1);
	if( ng.isEmpty() && vm.lineCount() != lstarts.size() )
		ng = QString("lineCount: %1 expected %2").arg(vm.lineCount()).arg(lstarts.size());
	//	仕様どおりに表示行を再計算する
	const int tabWidth = vm.tabWidth();
	QVector<pos_t> vstarts, vlens;
	QVector<int> viewLineOfLine, lineOfViewLine;
	for( int ln = 0; ng.isEmpty() && ln < lstarts.size(); ++ln ) {
		viewLineOfLine.push_back((int)vstarts.size());
		pos_t ls = lstarts[ln];
		pos_t llen = (ln + 1 < lstarts.size()) ? lstarts[ln + 1] - 1 - ls
											   : (pos_t)model.size() - ls;
		if( vm.lineToViewLine(ln) != viewLineOfLine[ln] )
			ng = QString("lineToViewLine(%1)").arg(ln);
		int expectInLine = 1;
		if( width > 0 && llen > 0 ) {
			pos_t segStart = 0;
			int w = 0;
			for( pos_t i = 0; i < llen; ) {
				qsizetype adv;
				int cw = refCharWidth(model, ls + i, adv, w, tabWidth);
				if( w > 0 && w + cw > width ) {
					vstarts.push_back(ls + segStart);
					vlens.push_back(i - segStart);
					lineOfViewLine.push_back(ln);
					segStart = i;
					w = 0;
					if( model[ls + i] == QChar('\t') )
						cw = refCharWidth(model, ls + i, adv, 0, tabWidth);
					++expectInLine;
				}
				w += cw;
				i += adv;
			}
			vstarts.push_back(ls + segStart);
			vlens.push_back(llen - segStart);
			lineOfViewLine.push_back(ln);
		} else {
			vstarts.push_back(ls);
			vlens.push_back(llen);
			lineOfViewLine.push_back(ln);
		}
		if( ng.isEmpty() && vm.viewLineCountIn(ln) != expectInLine )
			ng = QString("viewLineCountIn(%1): %2 expected %3").arg(ln).arg(vm.viewLineCountIn(ln)).arg(expectInLine);
	}
	if( ng.isEmpty() && vm.viewLineCount() != vstarts.size() )
		ng = QString("viewLineCount: %1 expected %2").arg(vm.viewLineCount()).arg(vstarts.size());
	for( int v = 0; ng.isEmpty() && v < vstarts.size(); ++v ) {
		if( vm.viewLineStartPos(v) != vstarts[v] )
			ng = QString("viewLineStartPos(%1): %2 expected %3").arg(v).arg(vm.viewLineStartPos(v)).arg(vstarts[v]);
		else if( vm.viewLineLength(v) != vlens[v] )
			ng = QString("viewLineLength(%1): %2 expected %3").arg(v).arg(vm.viewLineLength(v)).arg(vlens[v]);
		else if( vm.viewLineToLine(v) != lineOfViewLine[v] )
			ng = QString("viewLineToLine(%1)").arg(v);
	}
	for( pos_t p = 0; ng.isEmpty() && p <= model.size(); ++p ) {
		int expect = 0;
		for( int v = 0; v < vstarts.size(); ++v )
			if( vstarts[v] <= p ) expect = v;
		if( vm.viewLineAtPos(p) != expect ) {
			ng = QString("viewLineAtPos(%1): %2 expected %3").arg(p).arg(vm.viewLineAtPos(p)).arg(expect);
			break;
		}
		int col = 0;
		for( pos_t i = vstarts[expect]; i < p; ) {
			qsizetype adv;
			col += refCharWidth(model, i, adv, col, tabWidth);
			i += adv;
		}
		if( vm.viewLineColumnAtPos(p) != col )
			ng = QString("viewLineColumnAtPos(%1): %2 expected %3").arg(p).arg(vm.viewLineColumnAtPos(p)).arg(col);
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
	//	空テキスト（表示行1行・幅デフォルト70）
	output("  test: empty\n");
	{
		ViewLineMgr vm;
		verify(vm.lineWidth() == 70, "default line width");
		verify(vm.viewLineCount() == 1 && vm.viewLineStartPos(0) == 0 && vm.viewLineLength(0) == 0, "empty viewLine info");
		checkAll(vm, "", 70, "empty");
	}
	//	折り返しなしの短い行
	output("  test: short line\n");
	{
		ViewLineMgr vm;
		QString model = "abcde";
		vm.insert(0, model.constData(), model.size());
		verify(vm.viewLineCount() == 1, "short line viewLineCount");
		checkAll(vm, model, 70, "short line");
	}
	//	半角の折り返し
	output("  test: wrap ascii\n");
	{
		ViewLineMgr vm;
		vm.setLineWidth(5);
		QString model = "abcdefgh";
		vm.insert(0, model.constData(), model.size());
		verify(vm.viewLineCount() == 2 && vm.viewLineLength(0) == 5 && vm.viewLineLength(1) == 3, "wrap ascii counts");
		checkAll(vm, model, 5, "wrap ascii");
	}
	//	全角の折り返し
	output("  test: wrap wide chars\n");
	{
		ViewLineMgr vm;
		vm.setLineWidth(5);
		QString model = "あいうえお";
		vm.insert(0, model.constData(), model.size());
		verify(vm.viewLineCount() == 3, "wrap wide count");	//	2字（幅4）・2字（幅4）・1字
		checkAll(vm, model, 5, "wrap wide chars");
	}
	//	全角文字が境界を跨ぐ（手前で切る）
	output("  test: wide char straddle\n");
	{
		ViewLineMgr vm;
		vm.setLineWidth(3);
		QString model = "abあc";
		vm.insert(0, model.constData(), model.size());
		verify(vm.viewLineCount() == 2 && vm.viewLineLength(0) == 2, "straddle counts");	//	"ab" / "あc"
		verify(vm.viewLineAtPos(2) == 1, "straddle viewLineAtPos at wide char");
		verify(vm.viewLineColumnAtPos(3) == 2, "straddle column after wide char");
		checkAll(vm, model, 3, "wide char straddle");
	}
	//	タブ幅（デフォルト4、タブストップ方式）
	output("  test: tab width\n");
	{
		ViewLineMgr vm;
		verify(vm.tabWidth() == 4, "default tab width is 4");
		vm.setLineWidth(5);
		QString model = "a\tb";	//	'a'(col 0->1) + '\t'(col 1->4) + 'b'(col 4->5) = 幅 5
		vm.insert(0, model.constData(), model.size());
		verify(vm.viewLineCount() == 1, "tab fits in width 5");
		verify(vm.viewLineColumnAtPos(2) == 4, "tab advances to column 4");
		checkAll(vm, model, 5, "tab width");
		vm.setLineWidth(4);
		verify(vm.viewLineCount() == 2, "tab+b wraps at width 4");
		checkAll(vm, model, 4, "tab width narrowed to 4");
		vm.setLineWidth(3);
		verify(vm.viewLineCount() == 2, "tab wraps onto next line at width 3");
		checkAll(vm, model, 3, "tab width narrowed to 3");
		vm.setTabWidth(2);
		vm.setLineWidth(4);
		verify(vm.viewLineCount() == 1, "tabWidth 2 fits in width 4");	//	'a'(1)+'\t'(1)+'b'(1) = 3
		verify(vm.viewLineColumnAtPos(2) == 2, "tabWidth 2 advances to column 2");
		checkAll(vm, model, 4, "tabWidth changed to 2");
	}
	//	空行も表示行1行
	output("  test: empty line\n");
	{
		ViewLineMgr vm;
		vm.setLineWidth(4);
		QString model = "あいう\n\nab";
		vm.insert(0, model.constData(), model.size());
		verify(vm.viewLineCount() == 4, "empty line counts");	//	2行＋空行1行＋1行
		verify(vm.viewLineToLine(2) == 1 && vm.viewLineLength(2) == 0, "empty line is a viewLine");
		checkAll(vm, model, 4, "empty line");
	}
	//	行幅1（全角は1字で1行を占める）
	output("  test: width one\n");
	{
		ViewLineMgr vm;
		vm.setLineWidth(1);
		QString model = "aあb";
		vm.insert(0, model.constData(), model.size());
		verify(vm.viewLineCount() == 3, "width one count");
		checkAll(vm, model, 1, "width one");
	}
	//	折り返しなし（幅0）
	output("  test: no wrap\n");
	{
		ViewLineMgr vm;
		vm.setLineWidth(0);
		QString model = "abcdefghij\nkl";
		vm.insert(0, model.constData(), model.size());
		verify(vm.viewLineCount() == 2, "no wrap count");
		checkAll(vm, model, 0, "no wrap");
	}
	//	行幅変更（全再ラップ）
	output("  test: change line width\n");
	{
		ViewLineMgr vm;
		QString model = "あいうえおかきくけこ";
		vm.insert(0, model.constData(), model.size());
		checkAll(vm, model, 70, "width 70 before change");
		vm.setLineWidth(6);
		verify(vm.viewLineCount() == 4, "width 6 count");	//	3字（幅6）×3＋1字
		checkAll(vm, model, 6, "width changed to 6");
		vm.setLineWidth(70);
		verify(vm.viewLineCount() == 1, "width back to 70");
		checkAll(vm, model, 70, "width back to 70");
	}
	//	折り返した行への編集（挿入で表示行数が変わる・表示行を跨ぐ削除）
	output("  test: edit wrapped line\n");
	{
		ViewLineMgr vm;
		vm.setLineWidth(4);
		QString model = "abcd";
		vm.insert(0, model.constData(), model.size());
		verify(vm.viewLineCount() == 1, "before insert one viewLine");
		QString ins = "ef";
		vm.insert(4, ins.constData(), ins.size());
		model += ins;
		verify(vm.viewLineCount() == 2, "after insert two viewLines");
		checkAll(vm, model, 4, "insert into wrapped line");
		vm.erase(2, 3);
		model.remove(2, 3);
		checkAll(vm, model, 4, "erase across viewLines");
	}
	//	clear と再利用
	output("  test: clear and reuse\n");
	{
		ViewLineMgr vm;
		vm.setLineWidth(3);
		QString model = "あいうえお";
		vm.insert(0, model.constData(), model.size());
		vm.clear();
		model.clear();
		verify(vm.viewLineCount() == 1, "clear viewLineCount");
		checkAll(vm, model, 3, "clear");
		QString s = "ab";
		vm.insert(0, s.constData(), s.size());
		model = s;
		checkAll(vm, model, 3, "reuse after clear");
	}
}

//----------------------------------------------------------------------
//	ランダムテスト（QString＋仕様再計算を参照モデルにする）
static void test_random(uint64_t seed, int width, int nOps)
{
	output(QString("  test: random (seed = %1, width = %2, %3 ops)\n").arg(seed).arg(width).arg(nOps));
	std::mt19937_64 rng(seed);
	static const QString pieces[] = {
		"abc", "あいう", "x", "\n", "あ", "ab\ncd", "\t", "0123456789",
		QString::fromUtf8("😀"), "日本語テスト", "\n\n", "ｱ", "ＡＢ",
	};
	const int nPieces = (int)std::size(pieces);
	ViewLineMgr vm;
	vm.setLineWidth(width);
	QString model;
	int curWidth = width;
	for( int op = 0; op < nOps; ++op ) {
		if( op == nOps / 2 && width > 1 ) {	//	途中で行幅を変えて全再ラップも混ぜる
			curWidth = width + 7;
			vm.setLineWidth(curWidth);
		}
		int kind = (int)(rng() % 100);
		if( model.size() > 1200 ) kind = 60;	//	大きくなりすぎたら削除に倒す
		QString desc;
		if( kind < 55 || model.isEmpty() ) {	//	挿入
			pos_t pos = model.isEmpty() ? 0 : (pos_t)(rng() % (uint64_t)(model.size() + 1));
			const QString& chunk = pieces[rng() % nPieces];
			vm.insert(pos, chunk.constData(), chunk.size());
			model.insert((qsizetype)pos, chunk);
			desc = QString("insert at %1").arg(pos);
		} else {								//	削除
			pos_t pos = (pos_t)(rng() % (uint64_t)model.size());
			pos_t len = 1 + (pos_t)(rng() % 10);
			if( pos + len > model.size() ) len = (pos_t)model.size() - pos;
			vm.erase(pos, len);
			model.remove((qsizetype)pos, (qsizetype)len);
			desc = QString("erase len=%1 at %2").arg(len).arg(pos);
		}
		if( !checkAll(vm, model, curWidth, QString("random seed=%1 op=%2 (%3)").arg(seed).arg(op).arg(desc)) ) {
			output("random test aborted.\n");
			return;
		}
	}
}

void do_test_viewLineMgr(MainWindow *mw)
{
	g_mw = mw;
	g_tested_count = 0;
	g_failed_count = 0;
	output("\n# Test ViewLineMgr\n\n");
	test_fixed();
	output(QString("  fixed cases: %1 failed / %2 tested.\n").arg(g_failed_count).arg(g_tested_count));
	test_random(1, 10, 3000);
	test_random(42, 10, 3000);
	test_random(20261110, 10, 3000);
	test_random(42, 1, 1500);
	test_random(7, 0, 1500);
	output(QString("\nTest ViewLineMgr: %1 failed / %2 tested.\n\n")
		.arg(g_failed_count).arg(g_tested_count));
}
