#include "GapBufferTest.h"
#include "MainWindow.h"
#include "gap_buffer.h"

#include <random>

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
	{
		GBuf buf;
		verify(buf.isEmpty() && buf.size() == 0, "empty buffer");
		checkContent(buf, "", "empty content");
	}
	//	push_back / front / back / push_front
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
	{
		GBuf buf;
		buf.insert(0, QChar('b'));
		buf.insert(0, QChar('a'));			//	先頭
		buf.insert(buf.size(), QChar('d'));	//	末尾
		buf.insert(2, QChar('c'));			//	中間
		checkContent(buf, "abcd", "insert char");
	}
	//	範囲 insert
	{
		GBuf buf;
		QString init = "ad";
		buf.insert(0, init.constData(), init.size());
		QString mid = "bc";
		buf.insert(1, mid.constData(), mid.size());
		checkContent(buf, "abcd", "insert range");
	}
	//	erase（単一・範囲）/ pop_back / pop_front
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
	{
		GBuf buf;
		QString init = "abc";
		buf.insert(0, init.constData(), init.size());
		buf.setAt(1, QChar('X'));
		checkContent(buf, "aXc", "setAt");
	}
	//	resize（拡大はフィル文字、縮小は切り捨て）
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
	{
		GBuf buf;
		QString init = "hello world";
		buf.insert(0, init.constData(), init.size());
		QString pat = "o w";
		verify(buf.indexOf(pat.constData(), pat.size()) == 4, "indexOf middle");
		QString np = "xyz";
		verify(buf.indexOf(np.constData(), np.size()) == -1, "indexOf not found");
	}
}

//----------------------------------------------------------------------
//	ランダムテスト（QString を参照モデルにする）
static void test_random(uint64_t seed, int nOps)
{
	std::mt19937_64 rng(seed);
	static const QChar alphabet[] = {
		QChar('a'), QChar('b'), QChar('c'), QChar('X'), QChar(' '),
		QChar(u'\n'), QChar(u'あ'), QChar(u'い'),
	};
	const int nAlpha = (int)std::size(alphabet);
	auto randChar = [&]() { return alphabet[rng() % nAlpha]; };
	auto randPos = [&](pos_t size) { return size == 0 ? (pos_t)0 : (pos_t)(rng() % (uint64_t)(size + 1)); };

	GBuf buf;
	QString model;
	for( int op = 0; op < nOps; ++op ) {
		QString desc;
		int kind = (int)(rng() % 100);
		if( model.size() > 5000 ) kind = 40 + (int)(rng() % 20);	//	大きくなりすぎたら削除寄りにする
		if( kind < 30 ) {			//	範囲 insert
			pos_t pos = randPos((pos_t)model.size());
			int len = 1 + (int)(rng() % 16);
			QString chunk;
			for( int i = 0; i < len; ++i ) chunk += randChar();
			buf.insert(pos, chunk.constData(), chunk.size());
			model.insert((qsizetype)pos, chunk);
			desc = QString("insert range len=%1 at %2").arg(len).arg(pos);
		} else if( kind < 50 ) {	//	単一文字 insert
			pos_t pos = randPos((pos_t)model.size());
			QChar ch = randChar();
			buf.insert(pos, ch);
			model.insert((qsizetype)pos, ch);
			desc = QString("insert char at %1").arg(pos);
		} else if( kind < 70 ) {	//	範囲 erase
			if( model.isEmpty() ) continue;
			pos_t pos = (pos_t)(rng() % (uint64_t)model.size());
			pos_t len = 1 + (pos_t)(rng() % 16);
			if( pos + len > model.size() ) len = (pos_t)model.size() - pos;
			buf.erase((size_t)pos, (size_t)len);
			model.remove((qsizetype)pos, (qsizetype)len);
			desc = QString("erase len=%1 at %2").arg(len).arg(pos);
		} else if( kind < 80 ) {	//	setAt
			if( model.isEmpty() ) continue;
			pos_t pos = (pos_t)(rng() % (uint64_t)model.size());
			QChar ch = randChar();
			buf.setAt(pos, ch);
			model[(qsizetype)pos] = ch;
			desc = QString("setAt at %1").arg(pos);
		} else if( kind < 90 ) {	//	push_back / pop_back
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
		} else {					//	ギャップ移動のみ（内容は不変のはず）
			pos_t pos = randPos((pos_t)model.size());
			buf.setGapIndex(pos);
			desc = QString("move gap to %1").arg(pos);
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
	const uint64_t seeds[] = {1, 42, 20261110};
	for( uint64_t seed : seeds )
		test_random(seed, 3000);
	output(QString("\nTest gap_buffer: %1 failed / %2 tested.\n\n")
		.arg(g_failed_count).arg(g_tested_count));
}
