#include "ViTest.h"
#include "MainWindow.h"
#include "TextEdit.h"
#include "ViEngine.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QStringList>
#include <QVector>

namespace {
struct ViTestCase {
	QString m_name;
	QString m_initialText;
	QStringList m_steps;
};

const QVector<ViTestCase> kViTestCases = {
	// --- h / j / k / l ---
	{ "Move cursor down (j)",
		"a┃bc\ndef\nghi\n",
		{
			"j", "abc\nd┃ef\nghi\n",
			"j", "abc\ndef\ng┃hi\n",
			"j", "abc\ndef\nghi\n┃",
		}
	},
	{ "Move cursor up (k)",
		"abc\ndef\ng┃hi\n",
		{
			"k", "abc\nd┃ef\nghi\n",
			"k", "a┃bc\ndef\nghi\n",
			"k", "a┃bc\ndef\nghi\n",
		}
	},
	{ "h and l boundaries (No wrapping)",
		"abc\n┃def\nghi\n",
		{
			"h", "abc\n┃def\nghi\n",
			"l", "abc\nd┃ef\nghi\n",
			"l", "abc\nde┃f\nghi\n",
			"l", "abc\nde┃f\nghi\n",
		}
	},
	{ "Move right with count (nl)",
		"h┃ello world\n",
		{
			"3l", "hell┃o world\n",
			"5l", "hello wor┃ld\n",
			"5l", "hello worl┃d\n",
		}
	},
	{ "Move left with count (nh)",
		"hello wor┃ld\n",
		{
			"4h", "hello┃ world\n",
			"10h", "┃hello world\n",
		}
	},
	{ "Move down with count (nj)",
		"a┃bc\ndef\nghi\njkl\n",
		{
			"2j", "abc\ndef\ng┃hi\njkl\n",
			"5j", "abc\ndef\nghi\njkl\n┃",
		}
	},
	{ "Move up with count (nk)",
		"abc\ndef\nghi\nj┃kl\n",
		{
			"2k", "abc\nd┃ef\nghi\njkl\n",
			"5k", "a┃bc\ndef\nghi\njkl\n",
		}
	},

	// --- w / b / e / W / B / E ---
	{ "Move forward word (w) - Basic spacing",
		"┃abc def ghi",
		{
			"w", "abc ┃def ghi",
			"w", "abc def ┃ghi",
		}
	},
	{ "Move forward word (w) - Punctuation boundaries",
		"┃abc.def!ghi",
		{
			"w", "abc┃.def!ghi",
			"w", "abc.┃def!ghi",
			"w", "abc.def┃!ghi",
			"w", "abc.def!┃ghi",
		}
	},
	{ "Move forward word (w) - Japanese word classes",
		"┃日本語ひらがなカタカナ。漢字",
		{
			"w", "日本語┃ひらがなカタカナ。漢字",
			"w", "日本語ひらがな┃カタカナ。漢字",
			"w", "日本語ひらがなカタカナ┃。漢字",
			"w", "日本語ひらがなカタカナ。┃漢字",
		}
	},
	{ "Word motion and gg (2w, gg, 3w)",
		"┃abc def\nxyz hoge\n",
		{
			"2w", "abc def\n┃xyz hoge\n",
			"gg", "┃abc def\nxyz hoge\n",
			"3w", "abc def\nxyz ┃hoge\n",
		}
	},
	{ "Move to end of word (e) - Basic",
		"┃abc def ghi\n",
		{
			"e", "ab┃c def ghi\n",
			"e", "abc de┃f ghi\n",
			"e", "abc def gh┃i\n",
		}
	},
	{ "Move backward word (b) - Basic",
		"abc def ┃ghi\n",
		{
			"b", "abc ┃def ghi\n",
			"b", "┃abc def ghi\n",
		}
	},

	// --- 0 / ^ / $ / + / - / G / gg / f / F / t / T / ; / , / % ---
	{ "Line start/end (0, ^, $)",
		"  ab┃cd ef\n",
		{
			"0", "┃  abcd ef\n",
			"^", "  ┃abcd ef\n",
			"$", "  abcd e┃f\n",
		}
	},
	{ "Basic + and - commands",
		"  first ┃line\n  second line\n    third line\n",
		{
			"+", "  first line\n  ┃second line\n    third line\n",
			"+", "  first line\n  second line\n    ┃third line\n",
			"2-", "  ┃first line\n  second line\n    third line\n",
		}
	},
	{ "G / gg / [num]G / [num]gg",
		"┃first\n  second\nthird\nfourth\n",
		{
			"2G", "first\n  ┃second\nthird\nfourth\n",
			"3gg", "first\n  second\n┃third\nfourth\n",
			"gg", "┃first\n  second\nthird\nfourth\n",
			"1G", "┃first\n  second\nthird\nfourth\n",
		}
	},
	{ "Find char (f, F, t, T, ;, ,)",
		"┃abc def abc def\n",
		{
			"fa", "abc def ┃abc def\n",
			"td", "abc def abc┃ def\n",
			"Fa", "abc def ┃abc def\n",
			";",  "┃abc def abc def\n",
			",",  "abc def ┃abc def\n",
		}
	},
	{ "Matching bracket (%)",
		"foo┃(a + [b]) bar\n",
		{
			"%", "foo(a + [b]┃) bar\n",
			"%", "foo┃(a + [b]) bar\n",
		}
	},

	// --- Insert / x / X / r / ~ / J / >> / << ---
	{ "Insert mode (i, a, I, A)",
		"  ┃bc\n",
		{
			"ia", "  ┃abc\n",
			"Ad", "  abc┃d\n",
			"I0", "  ┃0abcd\n",
		}
	},
	{ "Open line (o / O)",
		"first\n┃second\n",
		{
			"oX", "first\nsecond\n┃X\n",
			"OY", "first\nsecond\n┃Y\nX\n",
		}
	},
	{ "Delete char (x / X / count)",
		"a┃bcdef\n",
		{
			"x",  "a┃cdef\n",
			"2x", "a┃ef\n",
			"X",  "┃ef\n",
			"X",  "┃ef\n",
		}
	},
	{ "Replace char (r)",
		"a┃bcde\n",
		{
			"rZ",  "a┃Zcde\n",
			"2rX", "aX┃Xde\n",
		}
	},
	{ "Join lines (J) - Basic and Undo",
		"┃foo\n  bar\nbaz\n",
		{
			"J", "foo┃ bar\nbaz\n",
			"J", "foo bar┃ baz\n",
			"u", "foo ba┃r\nbaz\n",
		}
	},
	{ "Indent (>>) and Unindent (<<)",
		"he┃llo\nworld\n",
		{
			">>", "  ┃hello\nworld\n",
			">>", "    ┃hello\nworld\n",
			"<<", "  ┃hello\nworld\n",
			"<<", "┃hello\nworld\n",
		}
	},

	// --- Operators: dd / dw / de / db / dW / dE / dB / dh / dl / dj / dk / D / cw / cW / C / yy / p / P ---
	{ "Basic dd, count, undo, put",
		"fi┃rst\nsecond\nthird\n",
		{
			"dd", "┃second\nthird\n",
			"p",  "second\n┃first\nthird\n",
			"u",  "┃second\nthird\n",
			"u",  "┃first\nsecond\nthird\n",
		}
	},
	{ "dd with count (3dd) and undo",
		"┃line1\nline2\nline3\nline4\n",
		{
			"3dd", "┃line4\n",
			"u",   "┃line1\nline2\nline3\nline4\n",
		}
	},
	{ "Delete word (dw, 2dw, de, db)",
		"┃w1 w2 w3 w4 w5\n",
		{
			"dw",  "┃w2 w3 w4 w5\n",
			"2dw", "┃w4 w5\n",
			"de",  "┃ w5\n",
		}
	},
	{ "Delete WORD (dW, dE, dB)",
		"┃foo-bar.baz qux xyz\n",
		{
			"dW", "┃qux xyz\n",
			"w",  "qux ┃xyz\n",
			"dB", "┃xyz\n",
		}
	},
	{ "Delete word backwards (db)",
		"abc def ┃ghi\n",
		{
			"db", "abc ┃ghi\n",
			"db", "┃ghi\n",
		}
	},
	{ "Delete char motions (dh, dl, D)",
		"abcd┃efgh\n",
		{
			"2dh", "ab┃efgh\n",
			"2dl", "ab┃gh\n",
			"D",   "a┃b\n",
		}
	},
	{ "Delete lines (dj, dk) and undo",
		"first\nsec┃ond\nthird\nfourth\n",
		{
			"dj", "first\n┃fourth\n",
			"u",  "first\n┃second\nthird\nfourth\n",
			"dk", "┃third\nfourth\n",
		}
	},
	{ "Change word (cw, cW, C)",
		"┃abc def ghi\n",
		{
			"cwXY", "X┃Y def ghi\n",
			"w",    "XY ┃def ghi\n",
			"CZ",   "XY ┃Z\n",
		}
	},
	{ "Yank and put (yy, p, P)",
		"┃one\ntwo\n",
		{
			"yy", "┃one\ntwo\n",
			"p",  "one\n┃one\ntwo\n",
			"P",  "one\n┃one\none\ntwo\n",
		}
	},

	// --- Visual Mode (v / V) ---
	{ "Visual mode - select and delete (vl, l, d)",
		"┃abcdef\n",
		{
			"vl", "《a┃b》cdef\n",
			"l",  "《ab┃c》def\n",
			"d",  "┃def\n",
		}
	},
	{ "Visual - word select (ve) and delete",
		"┃foo bar baz\n",
		{
			"ve", "《fo┃o》 bar baz\n",
			"d",  "┃ bar baz\n",
		}
	},
	{ "Visual - backward char (vh) and delete",
		"abc┃def\n",
		{
			"vh", "ab《┃cd》ef\n",
			"d",  "ab┃ef\n",
		}
	},
	{ "Visual - backward word (vb) and delete",
		"foo bar ┃baz\n",
		{
			"vb", "foo 《┃bar b》az\n",
			"d",  "foo ┃az\n",
		}
	},
	{ "Visual - change (c) and undo",
		"┃abcdef\n",
		{
			"v2lcXY", "X┃Ydef\n",
			"u",      "┃abcdef\n",
		}
	},

	// --- Search (/, ?, n, N, *, #) ---
	{ "Search forward/backward (/, ?, n, N)",
		"┃alpha beta alpha gamma alpha\n",
		{
			"/alpha", "alpha beta ┃alpha gamma alpha\n",
			"n",      "alpha beta alpha gamma ┃alpha\n",
			"N",      "alpha beta ┃alpha gamma alpha\n",
			"?beta",  "alpha ┃beta alpha gamma alpha\n",
		}
	},

	// --- Ex commands ---
	{ "Ex Range - Absolute / Current / Relative (:num, :., :+2, :-)",
		"line 1\nli┃ne 2\nline 3\nline 4\nline 5\n",
		{
			":3",  "line 1\nline 2\n┃line 3\nline 4\nline 5\n",
			":+2", "line 1\nline 2\nline 3\nline 4\n┃line 5\n",
			":-3", "line 1\n┃line 2\nline 3\nline 4\nline 5\n",
			":1",  "┃line 1\nline 2\nline 3\nline 4\nline 5\n",
		}
	},
	{ "Ex Range - Semicolon (:1;+2)",
		"line 1\nli┃ne 2\nline 3\nline 4\n",
		{
			":1;+2", "line 1\nline 2\n┃line 3\nline 4\n",
		}
	},
	{ "Ex Delete (:d, :1d, :1,2d)",
		"line 1\nli┃ne 2\nline 3\nline 4\n",
		{
			":d",    "line 1\n┃line 3\nline 4\n",
			":1,2d", "┃line 4\n",
		}
	},
	{ "Ex Substitute (:s, :s/g, :%s, :s#/#_#)",
		"┃foo foo\nfoo a/b\n",
		{
			":s/foo/baz/",   "┃baz foo\nfoo a/b\n",
			":%s/foo/bar/g", "┃baz bar\nbar a/b\n",
			":2s#/#_#",      "baz bar\n┃bar a_b\n",
		}
	},
	{ "Ex Global Delete (:g/pat/d, :2,3g/pat/d)",
		"┃keep\nTODO a\nkeep2\nTODO b\n",
		{
			":g/TODO/d", "┃keep\nkeep2\n",
		}
	},
	{ "Ex Range - Trailing backslash / not found (#1449, #1450)",
		"line 1\nli┃ne 2\nline 3\n",
		{
			":/\\",            "line 1\nli┃ne 2\nline 3\n",
			":/NO_SUCH_PAT/d", "line 1\nli┃ne 2\nline 3\n",
		}
	},
	{ "Ex Substitute - Undo via UndoMgr (#1451)",
		"┃hello world\nhello again\n",
		{
			"iX",                    "┃Xhello world\nhello again\n",
			":%s/hello/goodbye/",    "┃Xgoodbye world\ngoodbye again\n",
			"u",                     "┃Xhello world\nhello again\n",
			"u",                     "┃hello world\nhello again\n",
		}
	},
};

QString removeCursor(const QString &src, int &pos, int &anchor)
{
	QString dst;
	int p = 0;
	pos = 0;
	anchor = -1;
	for( const QChar ch : src ) {
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

QString formatActual(TextEdit *edit)
{
	QString act = edit->toPlainText();
	const ViEngine *vi = edit->viEngine();
	const int pos = (vi && (vi->mode() == ViMode::Visual || vi->mode() == ViMode::VisualLine))
		? static_cast<int>(vi->visualCursor())
		: static_cast<int>(edit->textCursor().position());
	act.insert(qBound(0, pos, static_cast<int>(act.size())), u'┃');
	if( vi && vi->mode() == ViMode::Visual ) {
		const int anchor = static_cast<int>(vi->visualAnchor());
		if( anchor <= pos ) {
			act.insert(qBound(0, anchor, static_cast<int>(act.size())), u'《');
			act.insert(qBound(0, pos + 3, static_cast<int>(act.size())), u'》');
		} else {
			act.insert(qBound(0, pos, static_cast<int>(act.size())), u'《');
			act.insert(qBound(0, anchor + 3, static_cast<int>(act.size())), u'》');
		}
	}
	act.replace(QChar('\n'), QStringLiteral("\\n"));
	return act;
}

void sendViKey(TextEdit *edit, QChar ch)
{
	const int key = (ch == QChar('\n')) ? Qt::Key_Return : ch.toUpper().unicode();
	QKeyEvent ev(QEvent::KeyPress, key, Qt::NoModifier, QString(ch));
	edit->viEngine()->handleKeyPress(edit, &ev);
}

void sendEsc(TextEdit *edit)
{
	QKeyEvent ev(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
	edit->viEngine()->handleKeyPress(edit, &ev);
}
}

void do_test_vi(MainWindow *mw)
{
	if( !mw ) return;
	TextEdit edit;
	edit.resize(600, 400);
	edit.setViEnabled(true);

	int totalTested = 0;
	int totalFailed = 0;
	mw->do_output(QStringLiteral("\nTest vi commands...\n\n"));

	for( const ViTestCase &tc : kViTestCases ) {
		mw->do_output(tc.m_name + QStringLiteral(": "));
		int pos = 0, anchor = -1;
		const QString initText = removeCursor(tc.m_initialText, pos, anchor);
		edit.setPlainText(initText);
		edit.document()->setModified(false);
		edit.viEngine()->setEnabled(true);
		{
			TextCursor c = edit.textCursor();
			c.setPosition(pos);
			edit.setTextCursor(c);
		}

		for( qsizetype k = 0; k + 1 < tc.m_steps.size(); k += 2 ) {
			++totalTested;
			mw->do_output(QStringLiteral("."));
			const QString before = formatActual(&edit);
			const QString &cmd = tc.m_steps[k];

			for( qsizetype i = 0; i < cmd.size(); ++i ) {
				const QChar ch = cmd[i];
				if( edit.viEngine()->mode() == ViMode::Normal && (ch == QChar(':') || ch == QChar('/') || ch == QChar('?')) ) {
					const QString body = cmd.mid(i + 1);
					if( ch == QChar(':') ) {
						ViEngine::ExCallbacks cb;
						edit.viEngine()->executeExCommand(&edit, body, cb);
					} else {
						edit.viEngine()->search(&edit, body, ch == QChar('/'));
					}
					break;
				}
				sendViKey(&edit, ch);
				if( edit.viEngine()->mode() == ViMode::Insert ) {
					const QString ins = cmd.mid(i + 1);
					if( !ins.isEmpty() ) {
						const int p = static_cast<int>(edit.textCursor().position());
						edit.applyPreviewEdit(p, p, ins);
					}
					sendEsc(&edit);
					break;
				}
			}

			int expPos = 0, expAnchor = -1;
			const QString expText = removeCursor(tc.m_steps[k + 1], expPos, expAnchor);
			QString expDisp = tc.m_steps[k + 1];
			expDisp.replace(QChar('\n'), QStringLiteral("\\n"));
			const QString actDisp = formatActual(&edit);

			const int actPos = (edit.viEngine()->mode() == ViMode::Visual || edit.viEngine()->mode() == ViMode::VisualLine)
				? static_cast<int>(edit.viEngine()->visualCursor())
				: static_cast<int>(edit.textCursor().position());
			const bool textOk = (edit.toPlainText() == expText);
			const bool posOk = (actPos == expPos);
			bool anchorOk = true;
			if( expAnchor >= 0 ) {
				anchorOk = (edit.viEngine()->mode() == ViMode::Visual
					&& static_cast<int>(edit.viEngine()->visualAnchor()) == expAnchor);
			}

			if( !textOk || !posOk || !anchorOk ) {
				++totalFailed;
				const QString reason = !textOk ? QStringLiteral("wrong document text")
					: (!posOk ? QStringLiteral("wrong cursor position") : QStringLiteral("wrong visual anchor"));
				mw->do_output(QStringLiteral("\n[FAILED #%1] %2.\n").arg(totalFailed).arg(reason));
				mw->do_output(QStringLiteral("Before:   '%1'\n").arg(before));
				mw->do_output(QStringLiteral("Commands: '%1'\n").arg(cmd));
				mw->do_output(QStringLiteral("Expected: '%1'\nActual:   '%2'\n").arg(expDisp, actDisp));
				break;
			}
		}
		mw->do_output(QStringLiteral("\n"));
	}

	const int passed = totalTested - totalFailed;
	const double pct = (totalTested > 0) ? (100.0 * passed / totalTested) : 100.0;
	mw->do_output(QStringLiteral("\n=== Vi Test Summary: %1 / %2 PASS (%3%), %4 FAILED ===\n")
		.arg(passed).arg(totalTested).arg(QString::number(pct, 'f', 1)).arg(totalFailed));
}
