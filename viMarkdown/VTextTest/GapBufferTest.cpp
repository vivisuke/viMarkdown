#include "GapBufferTest.h"
#include "MainWindow.h"
#include "gap_buffer.h"

#include <random>
#include <QElapsedTimer>

//----------------------------------------------------------------------
//	gap_buffer<QChar> の動作確認テスト
//	・固定ケース（基本操作・境界・コピー・ギャップ移動の不変性）
//	・QString を参照モデルにしたランダムテスト（seed 固定）
//	読み出しは get_data（一括）と operator[]（要素ごと）の両経路を検証する
//----------------------------------------------------------------------

typedef gap_buffer<QChar> GBuf;
typedef GBuf::pos_t pos_t;

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
//	get_data で全内容を取り出す
static QString contentByBulk(const GBuf& buf)
{
	QString s;
	QChar chunk[1024];
	pos_t ix = 0;
	while( ix < buf.size() ) {
		int n = (int)std::min<pos_t>(1024, buf.size() - ix);
		auto got = buf.get_data(ix, chunk, n);
		if( got <= 0 ) break;
		s.append(chunk, (qsizetype)got);
		ix += got;
	}
	return s;
}
//	operator[] で全内容を取り出す
static QString contentByIndex(const GBuf& buf)
{
	QString s;
	s.reserve((qsizetype)buf.size());
	for( pos_t i = 0; i < buf.size(); ++i )
		s += buf[i];
	return s;
}
//	サイズと全内容（両経路）が期待値と一致するか
static bool checkContent(const GBuf& buf, const QString& expected, const QString& what)
{
	++g_tested_count;
	QString bulk = contentByBulk(buf);
	QString idx = contentByIndex(buf);
	if( buf.size() == expected.size() && bulk == expected && idx == expected )
		return true;
	++g_failed_count;
	qsizetype i = 0;
	qsizetype lim = std::min<qsizetype>(bulk.size(), expected.size());
	while( i < lim && bulk[i] == expected[i] ) ++i;
	output(QString("FAILED: %1 (size: %2 expected %3, first diff at %4)\n")
		.arg(what).arg(buf.size()).arg(expected.size()).arg(i));
	return false;
}

//----------------------------------------------------------------------
//	固定ケース
static void test_fixed()
{
	//	空バッファ
	output("  test: empty buffer\n");
	{
		GBuf buf;
		verify(buf.isEmpty() && buf.size() == 0, "empty buffer");
		checkContent(buf, "", "empty content");
	}
	//	push_back / front / back / push_front
	output("  test: push_back / push_front\n");
	{
		GBuf buf;
		buf.push_back(QChar('a'));
		buf.push_back(QChar('b'));
		buf.push_back(QChar('c'));
		checkContent(buf, "abc", "push_back");
		verify(buf.front() == QChar('a') && buf.back() == QChar('c'), "front/back");
		buf.push_front(QChar('z'));
		checkContent(buf, "zabc", "push_front");
	}
	//	単一文字 insert（先頭・中間・末尾）
	output("  test: insert char\n");
	{
		GBuf buf;
		buf.insert(0, QChar('b'));
		buf.insert(0, QChar('a'));			//	先頭
		buf.insert(buf.size(), QChar('d'));	//	末尾
		buf.insert(2, QChar('c'));			//	中間
		checkContent(buf, "abcd", "insert char");
	}
	//	範囲 insert
	output("  test: insert range\n");
	{
		GBuf buf;
		QString init = "ad";
		buf.insert(0, init.constData(), init.size());
		QString mid = "bc";
		buf.insert(1, mid.constData(), mid.size());
		checkContent(buf, "abcd", "insert range");
	}
	//	erase（単一・範囲）/ pop_back / pop_front
	output("  test: erase / pop\n");
	{
		GBuf buf;
		QString init = "abcdef";
		buf.insert(0, init.constData(), init.size());
		buf.erase(0);						//	'a' を削除 -> "bcdef"
		checkContent(buf, "bcdef", "erase single");
		buf.erase(1, 2);					//	"de" を削除 -> "bef"
		checkContent(buf, "bef", "erase range");
		buf.pop_back();						//	-> "be"
		buf.pop_front();					//	-> "e"
		checkContent(buf, "e", "pop_back/pop_front");
	}
	//	setAt
	output("  test: setAt\n");
	{
		GBuf buf;
		QString init = "abc";
		buf.insert(0, init.constData(), init.size());
		buf.setAt(1, QChar('X'));
		checkContent(buf, "aXc", "setAt");
	}
	//	resize（拡大はフィル文字、縮小は切り捨て）
	output("  test: resize\n");
	{
		GBuf buf;
		QString init = "ab";
		buf.insert(0, init.constData(), init.size());
		buf.resize(5, QChar('-'));
		checkContent(buf, "ab---", "resize grow");
		buf.resize(1);
		checkContent(buf, "a", "resize shrink");
	}
	//	clear 後の再利用
	output("  test: clear and reuse\n");
	{
		GBuf buf;
		QString init = "abc";
		buf.insert(0, init.constData(), init.size());
		buf.clear();
		verify(buf.isEmpty() && buf.size() == 0, "clear");
		buf.push_back(QChar('x'));
		checkContent(buf, "x", "reuse after clear");
	}
	//	大量挿入（reserve の再確保を跨いでも内容が壊れない）
	output("  test: growth\n");
	{
		GBuf buf;
		QString model;
		for( int i = 0; i < 10000; ++i ) {
			QChar ch = QChar('a' + (i % 26));
			buf.push_back(ch);
			model += ch;
		}
		checkContent(buf, model, "growth 10000 push_back");
		//	文頭への大量挿入（ギャップ大移動＋再確保）
		QString head;
		for( int i = 0; i < 5000; ++i )
			head += QChar('0' + (i % 10));
		buf.insert(0, head.constData(), head.size());
		model = head + model;
		checkContent(buf, model, "growth insert at head");
	}
	//	コピーコンストラクタ（コピー側・元側の変更が互いに影響しない）
	output("  test: copy ctor\n");
	{
		GBuf buf;
		QString init = "hello";
		buf.insert(0, init.constData(), init.size());
		GBuf copy(buf);
		checkContent(copy, "hello", "copy ctor");
		buf.push_back(QChar('!'));
		checkContent(copy, "hello", "copy independent of original");
		copy.erase(0);
		checkContent(buf, "hello!", "original independent of copy");
	}
	//	ギャップ移動の不変性
	output("  test: gap invariance\n");
	{
		GBuf buf;
		QString model;
		for( int i = 0; i < 1000; ++i ) {
			QChar ch = QChar('A' + (i % 26));
			buf.push_back(ch);
			model += ch;
		}
		const pos_t gaps[] = {0, 1, 2, 499, 500, 999, 1000};
		for( pos_t g : gaps ) {
			buf.setGapIndex(g);
			checkContent(buf, model, QString("gap invariance at %1").arg(g));
		}
	}
	//	indexOf（中間一致と不一致のみ。末尾一致は仕様確認中のため対象外）
	output("  test: indexOf\n");
	{
		GBuf buf;
		QString init = "hello world";
		buf.insert(0, init.constData(), init.size());
		QString pat = "o w";
		verify(buf.indexOf(pat.constData(), pat.size()) == 4, "indexOf middle");
		QString np = "xyz";
		verify(buf.indexOf(np.constData(), np.size()) == -1, "indexOf not found");
	}
	//	ギャップが両端・中央にある状態での挿入・削除
	output("  test: gap boundary edit\n");
	{
		GBuf buf;
		QString model = "abcd";
		buf.insert(0, model.constData(), model.size());
		buf.setGapIndex(0);					//	ギャップを先頭へ
		buf.insert(0, QChar('z'));
		model.insert(0, QChar('z'));
		checkContent(buf, model, "insert with gap at head");
		buf.setGapIndex(buf.size());		//	ギャップを末尾へ
		buf.insert(buf.size(), QChar('e'));
		model += QChar('e');
		checkContent(buf, model, "insert with gap at tail");
		buf.setGapIndex(3);					//	ギャップを中央へ
		buf.insert(3, QChar('X'));
		model.insert(3, QChar('X'));
		checkContent(buf, model, "insert at gap position");
		buf.erase(3);
		model.remove(3, 1);
		checkContent(buf, model, "erase at gap position");
	}
	//	ギャップを跨ぐ範囲削除
	output("  test: erase across gap\n");
	{
		GBuf buf;
		QString model = "0123456789";
		buf.insert(0, model.constData(), model.size());
		buf.setGapIndex(5);
		buf.erase(3, 4);
		model.remove(3, 4);
		checkContent(buf, model, "erase across gap");
		buf.setGapIndex(0);
		buf.erase(0, 2);
		model.remove(0, 2);
		checkContent(buf, model, "erase at head with gap at head");
		buf.setGapIndex(buf.size());
		buf.erase((size_t)(buf.size() - 2), 2);
		model.chop(2);
		checkContent(buf, model, "erase at tail with gap at tail");
	}
	//	満杯（ギャップ長0）での操作と、容量を大きく超える範囲挿入
	output("  test: full buffer\n");
	{
		GBuf buf;
		QString model;
		buf.push_back(QChar('a'));
		model += QChar('a');
		while( buf.size() < buf.capacity() ) {	//	空きが無くなるまで埋める
			QChar ch = QChar('a' + (int)(model.size() % 26));
			buf.push_back(ch);
			model += ch;
		}
		checkContent(buf, model, "buffer filled to capacity");
		buf.push_back(QChar('!'));				//	満杯状態からの挿入（拡張が走る）
		model += QChar('!');
		checkContent(buf, model, "insert on full buffer");
		QString big((qsizetype)buf.capacity() * 2, QChar('Z'));	//	容量の2倍の範囲挿入
		buf.insert(1, big.constData(), big.size());
		model = model.left(1) + big + model.mid(1);
		checkContent(buf, model, "insert range larger than capacity");
	}
	//	長さ0の挿入・削除
	output("  test: zero-length operations\n");
	{
		GBuf buf;
		QString model = "abc";
		buf.insert(0, model.constData(), model.size());
		qsizetype zeroLen = 0;
		buf.insert(1, model.constData(), zeroLen);
		checkContent(buf, model, "insert zero-length range");
		buf.erase(1, 0);
		checkContent(buf, model, "erase zero-length at middle");
		buf.erase((size_t)buf.size(), 0);
		checkContent(buf, model, "erase zero-length at end");
	}
	//	末尾までの削除・全削除
	output("  test: erase to end / erase all\n");
	{
		GBuf buf;
		QString model = "abcdefgh";
		buf.insert(0, model.constData(), model.size());
		buf.erase(2, (size_t)(buf.size() - 2));
		model.resize(2);
		checkContent(buf, model, "erase to end");
		buf.erase(0, (size_t)buf.size());
		model.clear();
		checkContent(buf, model, "erase all");
		verify(buf.isEmpty(), "empty after erase all");
	}
	//	削除直後の、ギャップが残る位置への挿入
	output("  test: insert after erase\n");
	{
		GBuf buf;
		QString model = "0123456789";
		buf.insert(0, model.constData(), model.size());
		buf.erase(4, 3);
		model.remove(4, 3);
		checkContent(buf, model, "erase range (setup for insert)");
		QString ins = "XYZ";
		buf.insert(4, ins.constData(), ins.size());
		model.insert(4, ins);
		checkContent(buf, model, "insert after erase");
		buf.erase(4);
		model.remove(4, 1);
		buf.insert(4, QChar('q'));
		model.insert(4, QChar('q'));
		checkContent(buf, model, "insert char after erase char");
	}
	//	ピンポン操作（先頭と末尾へ交互に挿入し、ギャップを往復させる）
	output("  test: ping-pong insert\n");
	{
		GBuf buf;
		QString model;
		for( int i = 0; i < 100; ++i ) {
			QChar ch = QChar('a' + (i % 26));
			if( i % 2 == 0 ) {
				buf.push_front(ch);
				model.prepend(ch);
			} else {
				buf.push_back(ch);
				model += ch;
			}
		}
		checkContent(buf, model, "ping-pong insert x100");
	}
	//	indexOf（大小無視）。末尾一致は既知の対象外（修正パッチ待ち）のため、末尾にかからないパターンで確認する
	output("  test: indexOf ignore case\n");
	{
		GBuf buf;
		QString init = "Hello World!";
		buf.insert(0, init.constData(), init.size());
		QString p1 = "HELLO";
		verify(buf.indexOf(p1.constData(), p1.size(), 0, true) == 0, "indexOf ic at head");
		QString p2 = "o w";
		verify(buf.indexOf(p2.constData(), p2.size(), 0, true) == 4, "indexOf ic middle");
		QString p3 = "WORLD";
		verify(buf.indexOf(p3.constData(), p3.size(), 0, true) == 6, "indexOf ic word");
		QString p4 = "XYZ";
		verify(buf.indexOf(p4.constData(), p4.size(), 0, true) == -1, "indexOf ic not found");
	}
	//	サロゲートペア（コード単位の素通り確認：ペアの途中に挿入・削除しても壊れない）
	output("  test: surrogate pair passthrough\n");
	{
		GBuf buf;
		QString model;
		model += QChar('a');
		model += QChar(0xD83D);		//	U+1F600 の上位サロゲート
		model += QChar(0xDE00);		//	下位サロゲート
		model += QChar('b');
		buf.insert(0, model.constData(), model.size());
		buf.insert(2, QChar('x'));	//	ペアの途中へ挿入
		model.insert(2, QChar('x'));
		checkContent(buf, model, "insert inside surrogate pair");
		buf.erase(2);
		model.remove(2, 1);
		checkContent(buf, model, "erase inside surrogate pair");
	}
	//	大規模バッファでの編集（100万文字：先頭・中央・末尾の編集と大量削除）
	output("  test: large buffer\n");
	{
		GBuf buf;
		QString model;
		QString line(99, QChar('x'));
		line += QChar('\n');
		for( int i = 0; i < 10000; ++i ) {
			buf.insert(buf.size(), line.constData(), line.size());
			model += line;
		}
		buf.insert(0, QChar('H'));
		model.insert(0, QChar('H'));
		pos_t mid = buf.size() / 2;
		buf.insert(mid, QChar('M'));
		model.insert((qsizetype)mid, QChar('M'));
		buf.insert(buf.size(), QChar('T'));
		model += QChar('T');
		checkContent(buf, model, "large buffer edits at head/middle/tail");
		buf.erase(100, 500000);
		model.remove(100, 500000);
		checkContent(buf, model, "large buffer erase 500000");
	}
}

//----------------------------------------------------------------------
//	ランダムテスト（QString を参照モデルにする）
static void test_random(uint64_t seed, int nOps)
{
	output(QString("  test: random (seed = %1, %2 ops)\n").arg(seed).arg(nOps));
	std::mt19937_64 rng(seed);
	static const QChar alphabet[] = {
		QChar('a'), QChar('b'), QChar('c'), QChar('X'), QChar(' '),
		QChar(u'\n'), QChar(u'あ'), QChar(u'い'),
	};
	const int nAlpha = (int)std::size(alphabet);
	auto randChar = [&]() { return alphabet[rng() % nAlpha]; };
	auto randPos = [&](pos_t size) {
		if( size == 0 ) return (pos_t)0;
		if( rng() % 8 == 0 ) return (rng() & 1) ? (pos_t)0 : size;	//	端に偏らせる回
		return (pos_t)(rng() % (uint64_t)(size + 1));
	};

	GBuf buf;
	QString model;
	for( int op = 0; op < nOps; ++op ) {
		QString desc;
		int kind = (int)(rng() % 100);
		if( model.size() > 5000 ) kind = 40 + (int)(rng() % 20);	//	大きくなりすぎたら削除寄りにする
		if( kind < 28 ) {			//	範囲 insert
			pos_t pos = randPos((pos_t)model.size());
			int len = 1 + (int)(rng() % 16);
			QString chunk;
			for( int i = 0; i < len; ++i ) chunk += randChar();
			buf.insert(pos, chunk.constData(), chunk.size());
			model.insert((qsizetype)pos, chunk);
			desc = QString("insert range len=%1 at %2").arg(len).arg(pos);
		} else if( kind < 46 ) {	//	単一文字 insert
			pos_t pos = randPos((pos_t)model.size());
			QChar ch = randChar();
			buf.insert(pos, ch);
			model.insert((qsizetype)pos, ch);
			desc = QString("insert char at %1").arg(pos);
		} else if( kind < 64 ) {	//	範囲 erase
			if( model.isEmpty() ) continue;
			pos_t pos = (pos_t)(rng() % (uint64_t)model.size());
			pos_t len = 1 + (pos_t)(rng() % 16);
			if( pos + len > model.size() ) len = (pos_t)model.size() - pos;
			buf.erase((size_t)pos, (size_t)len);
			model.remove((qsizetype)pos, (qsizetype)len);
			desc = QString("erase len=%1 at %2").arg(len).arg(pos);
		} else if( kind < 72 ) {	//	setAt
			if( model.isEmpty() ) continue;
			pos_t pos = (pos_t)(rng() % (uint64_t)model.size());
			QChar ch = randChar();
			buf.setAt(pos, ch);
			model[(qsizetype)pos] = ch;
			desc = QString("setAt at %1").arg(pos);
		} else if( kind < 80 ) {	//	push_back / pop_back
			if( (rng() & 1) || model.isEmpty() ) {
				QChar ch = randChar();
				buf.push_back(ch);
				model += ch;
				desc = "push_back";
			} else {
				buf.pop_back();
				model.chop(1);
				desc = "pop_back";
			}
		} else if( kind < 88 ) {	//	ギャップ移動のみ（内容は不変のはず）
			pos_t pos = randPos((pos_t)model.size());
			buf.setGapIndex(pos);
			desc = QString("move gap to %1").arg(pos);
		} else if( kind < 96 ) {	//	resize
			pos_t newSize = (pos_t)(rng() % 6000);
			QChar ch = randChar();
			buf.resize(newSize, ch);
			if( newSize < model.size() ) model.resize((qsizetype)newSize);
			else if( newSize > model.size() ) model += QString((qsizetype)(newSize - model.size()), ch);
			desc = QString("resize to %1").arg(newSize);
		} else {					//	clear
			buf.clear();
			model.clear();
			desc = "clear";
		}
		if( !checkContent(buf, model,
				QString("random seed=%1 op=%2 (%3)").arg(seed).arg(op).arg(desc)) ) {
			output("random test aborted.\n");
			return;
		}
	}
}

void do_test_gapBuffer(MainWindow *mw)
{
	g_mw = mw;
	g_tested_count = 0;
	g_failed_count = 0;
	output("\n# Test gap_buffer\n\n");
	test_fixed();
	output(QString("  fixed cases: %1 failed / %2 tested.\n").arg(g_failed_count).arg(g_tested_count));
	const uint64_t seeds[] = {1, 42, 20261110};
	for( uint64_t seed : seeds )
		test_random(seed, 3000);
	output(QString("\nTest gap_buffer: %1 failed / %2 tested.\n\n")
		.arg(g_failed_count).arg(g_tested_count));
}

//----------------------------------------------------------------------
//	gap_buffer<QChar> の性能計測
//	目標規模：1行約100文字×100,000行＝約1,000万文字
//----------------------------------------------------------------------
void do_perf_gapBuffer(MainWindow *mw)
{
	g_mw = mw;
	output("\n# Performance gap_buffer\n\n");
	QElapsedTimer timer;

	//	1行＝99文字＋改行＝100文字の行データ
	QString line(99, QChar('x'));
	line += QChar('\n');
	const int nLines = 100000;

	//	構築：末尾へ1行ずつ挿入
	GBuf buf;
	timer.start();
	for( int i = 0; i < nLines; ++i )
		buf.insert(buf.size(), line.constData(), line.size());
	output(QString("build (insert %1 lines at end): %2 ms, size = %3\n")
		.arg(nLines).arg(timer.elapsed()).arg(buf.size()));

	//	全文逐次読み：operator[]
	timer.restart();
	uint64_t sum = 0;
	for( pos_t i = 0; i < buf.size(); ++i )
		sum += buf[i].unicode();
	output(QString("sequential read (operator[]): %1 ms (checksum %2)\n")
		.arg(timer.elapsed()).arg(sum));

	//	全文一括読み：get_data（64K文字ずつ）
	timer.restart();
	{
		QChar chunk[65536];
		sum = 0;
		pos_t ix = 0;
		while( ix < buf.size() ) {
			int n = (int)std::min<pos_t>(65536, buf.size() - ix);
			auto got = buf.get_data(ix, chunk, n);
			if( got <= 0 ) break;
			for( int i = 0; i < got; ++i ) sum += chunk[i].unicode();
			ix += got;
		}
	}
	output(QString("bulk read (get_data): %1 ms (checksum %2)\n")
		.arg(timer.elapsed()).arg(sum));

	//	ランダムアクセス読み
	std::mt19937_64 rng(20261111);
	std::uniform_int_distribution<pos_t> posDist(0, buf.size() - 1);
	timer.restart();
	sum = 0;
	const int nRandRead = 1000000;
	for( int i = 0; i < nRandRead; ++i )
		sum += buf[posDist(rng)].unicode();
	output(QString("random read (%1 times): %2 ms (checksum %3)\n")
		.arg(nRandRead).arg(timer.elapsed()).arg(sum));

	//	カーソル位置への連続挿入（タイピング想定：中央の固定位置へ1文字ずつ）
	pos_t cur = buf.size() / 2;
	timer.restart();
	const int nType = 100000;
	for( int i = 0; i < nType; ++i )
		buf.insert(cur++, QChar('y'));
	output(QString("typing at cursor (%1 chars): %2 ms\n")
		.arg(nType).arg(timer.elapsed()));
	buf.erase(cur - nType, (size_t)nType);

	//	ランダム位置への挿入・削除（1行＝100文字単位）
	timer.restart();
	const int nRandEdit = 1000;
	for( int i = 0; i < nRandEdit; ++i ) {
		pos_t pos = posDist(rng);
		buf.insert(pos, line.constData(), line.size());
		buf.erase(pos, (size_t)line.size());
	}
	output(QString("random insert+erase (%1 times x %2 chars): %3 ms\n")
		.arg(nRandEdit).arg(line.size()).arg(timer.elapsed()));

	output("\nPerformance gap_buffer: done.\n\n");
}
