#include <QTextBlock>
#include "OutputView.h"
#include "MainWindow.h"

extern Global g;

OutputView::OutputView(QWidget *parent)
	: QPlainTextEdit(parent)
{
	qDebug() << "OutputView::OutputView()";
	QFont font = this->font();
    font.setPointSize(14);
    setFont(font);
}
OutputView::~OutputView()
{}

void OutputView::highlightSearchText(const QString &searchText) {
	QList<QTextEdit::ExtraSelection> extraSelections;
	if (searchText.isEmpty()) {
		setExtraSelections(extraSelections);
		return;
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
		// 次のヒットを検索
		// 引数に FindFlags (大文字小文字区別など) を指定可能
		QTextDocument::FindFlags flags;
		if( !g.m_ignoreCase )
			flags |= QTextDocument::FindCaseSensitively;
		cursor = doc->find(searchText, cursor, flags);

		if (!cursor.isNull()) {
			QTextEdit::ExtraSelection selection;
			selection.format = format;
			selection.cursor = cursor;
			extraSelections.append(selection);
		}
	}
	setExtraSelections(extraSelections);	// エディタに適用
}
void OutputView::mouseDoubleClickEvent(QMouseEvent *e) {
	QTextCursor cursor = cursorForPosition(e->pos());
	QTextBlock block = cursor.block();
	qDebug() << "text = " << block.text();
	int ln = block.text().section(':', 0, 0).trimmed().toInt();
	qDebug() << "ln = " << ln;
	if( ln <= 0 ) return;
	do {
		if( !(block = block.previous()).isValid() ) return;
	} while( !block.text().startsWith('"') );
	QString path = block.text();
	path = path.mid(1, path.size() - 2);		//	前後の " を削除
	qDebug() << "path = " << path;
	emit do_open(path, ln-1);
}
bool OutputView::event(QEvent *e)
{
    if (e->type() == QEvent::ShortcutOverride) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(e);
        if (ke->matches(QKeySequence::Copy)) {
            e->accept(); // ★「このショートカットは私が処理します」とQtに宣言
            return true;
        }
    }
    return QPlainTextEdit::event(e);
}
void OutputView::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_C && (e->modifiers() & Qt::ControlModifier) != 0 ) {
		copy();
		return;
	}
	QPlainTextEdit::keyPressEvent(e);
}
