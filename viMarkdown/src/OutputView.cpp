#include <QTextBlock>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QRegularExpression>
#include "OutputView.h"
#include "MainWindow.h"

extern Global g;

OutputView::OutputView(QWidget *parent)
	: QPlainTextEdit(parent)
{
	QFont font = this->font();
	font.setPointSize(14);
	setFont(font);
}

OutputView::~OutputView() = default;

void OutputView::highlightSearchText(const QString &searchText) {
	QList<QTextEdit::ExtraSelection> extraSelections;
	if (searchText.isEmpty()) {
		setExtraSelections(extraSelections);
		return;
	}

	QRegularExpression re;
	if (g.m_regexp) {
		re = QRegularExpression(
			searchText,
			g.m_ignoreCase ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption
		);
		if (!re.isValid()) {
			setExtraSelections(extraSelections);
			return;
		}
	}

	QTextDocument::FindFlags flags;
	if (!g.m_ignoreCase) {
		flags |= QTextDocument::FindCaseSensitively;
	}

	// 検索時の書式設定
	QTextCharFormat format;
	format.setBackground(Qt::yellow);		// 背景を設定色に
	//format.setBackground(g.m_matchColor);		// 背景を設定色に
	format.setForeground(Qt::black);		// 文字を黒に（必要に応じて）

	// ドキュメント全体から検索
	QTextDocument *doc = document();
	QTextCursor cursor(doc);

	while (!cursor.isNull() && !cursor.atEnd()) {
		cursor = g.m_regexp ? doc->find(re, cursor, flags) : doc->find(searchText, cursor, flags);
		if (cursor.isNull()) {
			break;
		}
		if (cursor.hasSelection()) {
			QTextEdit::ExtraSelection selection;
			selection.format = format;
			selection.cursor = cursor;
			extraSelections.append(selection);
		} else {
			// ゼロ幅マッチ（^ や .* 等）での無限ループ防止
			if (!cursor.movePosition(QTextCursor::NextCharacter)) {
				break;
			}
		}
	}
	setExtraSelections(extraSelections);	// エディタに適用
}

void OutputView::mouseDoubleClickEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) {
		QPlainTextEdit::mouseDoubleClickEvent(e);
		return;
	}
	QTextCursor cursor = cursorForPosition(e->pos());
	QTextBlock block = cursor.block();
	const QString line = block.text();
	const int colon = line.indexOf(u':');
	bool ok = false;
	const int ln = (colon > 0) ? line.left(colon).trimmed().toInt(&ok) : 0;
	if (!ok || ln <= 0) {
		QPlainTextEdit::mouseDoubleClickEvent(e);
		return;
	}
	QTextBlock pathBlock = block.previous();
	while (pathBlock.isValid()) {
		const QString t = pathBlock.text().trimmed();
		if (t.size() >= 2 && t.startsWith(u'"') && t.endsWith(u'"')) {
			const QString path = t.mid(1, t.size() - 2);		//	前後の " を削除
			emit do_open(path, ln - 1);
			return;
		}
		pathBlock = pathBlock.previous();
	}
	QPlainTextEdit::mouseDoubleClickEvent(e);
}

bool OutputView::event(QEvent *e)
{
	if (e->type() == QEvent::ShortcutOverride) {
		QKeyEvent *ke = static_cast<QKeyEvent*>(e);
		if (ke->matches(QKeySequence::Copy) || ke->matches(QKeySequence::SelectAll)) {
			e->accept(); // ★「このショートカットは私が処理します」とQtに宣言
			return true;
		}
	}
	return QPlainTextEdit::event(e);
}

void OutputView::keyPressEvent(QKeyEvent *e) {
	if (e->matches(QKeySequence::Copy)) {
		copy();
		return;
	}
	if (e->matches(QKeySequence::SelectAll)) {
		selectAll();
		return;
	}
	if (e->key() == Qt::Key_Escape) {
		QTextCursor cursor = textCursor();
		cursor.clearSelection();
		setTextCursor(cursor);
		return;
	}
	QPlainTextEdit::keyPressEvent(e);
}
