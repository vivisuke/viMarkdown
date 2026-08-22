#include <QTextCursor>
#include <QTextBlock>
#include <QTextDocument>
#include <QElapsedTimer>
#include "MainWindow.h"

const int US_HEADING = 1;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowClass())
{
    ui->setupUi(this);
    connect(ui->action_Gen1000lines, &QAction::triggered, this, &MainWindow::onAction_gen1000lines);
    connect(ui->action_Gen2000lines, &QAction::triggered, this, &MainWindow::onAction_gen2000lines);
    connect(ui->action_Gen5000lines, &QAction::triggered, this, &MainWindow::onAction_gen5000lines);
    connect(ui->action_Gen10000lines, &QAction::triggered, this, &MainWindow::onAction_gen10000lines);
    connect(ui->action_Gen20000lines, &QAction::triggered, this, &MainWindow::onAction_gen20000lines);
    connect(ui->action_SetMarkdown, &QAction::triggered, this, &MainWindow::onAction_SetMarkdown);

    connect(ui->plainTextEdit->document(), &QTextDocument::contentsChange, this, &MainWindow::onEditorContentsChange);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::gen_lines(int n) {
	m_processing = true;
	ui->plainTextEdit->clear();
	QTextCursor cursor = ui->plainTextEdit->textCursor();
	cursor.beginEditBlock();
	const int UNIT = 25;
	for(int k = 0; k < n/UNIT; ++k) {
		cursor.insertText(QString("## %1. heading").arg(k+1));
		cursor.block().setUserState(US_HEADING);
		cursor.insertText("\n");
		for(int i = 0; i < UNIT - 1; ++i) {
			cursor.insertText("body text body text body text body text body text body text body text  \n");
		}
	}
	cursor.endEditBlock();
	ui->plainTextEdit->setTextCursor(cursor);
	m_processing = false;
}

void MainWindow::onAction_gen1000lines() {
	gen_lines(1000);
}
void MainWindow::onAction_gen2000lines() {
	gen_lines(2000);
}
void MainWindow::onAction_gen5000lines() {
	gen_lines(5000);
}
void MainWindow::onAction_gen10000lines() {
	gen_lines(10000);
}
void MainWindow::onAction_gen20000lines() {
	gen_lines(20000);
}
void MainWindow::onAction_SetMarkdown() {
	QElapsedTimer timer;
    timer.start();
#if 1
    QTextBlock block = ui->plainTextEdit->document()->begin();
    ui->textEdit->clear();
    QTextCursor cursor = ui->textEdit->textCursor();
    cursor.beginEditBlock();
    while( block.isValid() ) {
    	cursor.insertMarkdown(block.text());
    	if( block.userState() == US_HEADING )
    		cursor.block().setUserState(US_HEADING);
		cursor.insertText("\n");
		block = block.next();
    }
    cursor.endEditBlock();
#else
	auto md = ui->plainTextEdit->toPlainText();
	ui->textEdit->setMarkdown(md);
	// ※ QTextDocumentの内部レイアウト計算を強制完了させて描画遅延も含める場合
    //ui->textEdit->document()->documentLayout()->requestUpdate();
#endif
    qint64 elapsedMs = timer.elapsed();
    qDebug() << "[Benchmark] setMarkdown completed:" << elapsedMs << "ms (" 
             << timer.nsecsElapsed() / 1000.0 << "μs)";
}
void MainWindow::onEditorContentsChange(int position, int charsRemoved, int charsAdded) {
	if( m_processing ) return;
	qDebug() << "pos = " << position << ", removed = " << charsRemoved << ", added = " << charsAdded;
	QTextBlock block = ui->plainTextEdit->document()->findBlock(position);
	if( !block.isValid() ) return;
	while( block.userState() != US_HEADING ) {		//	文書先頭に向かって編集されたブロックの見出し行を探す
		if( !block.previous().isValid() ) break;
		block = block.previous();
	}
	qDebug() << "block.position() = " << block.position();
	QTextBlock block2 = ui->textEdit->document()->begin();		//	block に対応するプレビューのブロックのための QTextBlock
	if( block.position() != 0 ) {
		QTextBlock block1 = ui->plainTextEdit->document()->begin();
		while( block1 != block ) {
			do {
				block1 = block1.next();
				//if( !block1.isValid() ) return;
			} while( block1.userState() != US_HEADING );	//	見出し行までスキップ
			do {
				block2 = block2.next();
				//if( !block2.isValid() ) return;
			} while( block2.userState() != US_HEADING );	//	見出し行までスキップ
		}
	}
	qDebug() << "block2.position() = " << block2.position();
	// 先にプレビュー側の古いセクション範囲を特定して【削除】する
    QTextBlock oldEndBlock = block2.next(); // 次のブロックから探索開始
    while( oldEndBlock.isValid() && oldEndBlock.userState() != US_HEADING ) {
        oldEndBlock = oldEndBlock.next();
    }
	QTextCursor cursor2(block2);
	cursor2.beginEditBlock();
	if( oldEndBlock.isValid() ) {
        cursor2.setPosition(oldEndBlock.position(), QTextCursor::KeepAnchor);
    } else {
        cursor2.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    }
    cursor2.removeSelectedText();
    int insertPos = cursor2.position();
    //QString sectionText;
    bool init = true;
    while( block.isValid() ) {
        bool hdg = !block.text().isEmpty() && block.text()[0] == '#';
        if( hdg && !init ) break;
        init = false;
        cursor2.insertMarkdown(block.text() + "\n");
        //sectionText += block.text() + "\n";
        block = block.next();
    }
#if 0
    // 5. プレビュー側に Markdown を一括【挿入】
    int insertPos = cursor2.position();
    cursor2.insertMarkdown(sectionText.trimmed());
    cursor2.insertText("\n");
#endif

    // 挿入した見出しブロックの userState を設定
    QTextBlock newHeadBlock = ui->textEdit->document()->findBlock(insertPos);
    if( newHeadBlock.isValid() ) {
        newHeadBlock.setUserState(US_HEADING);
    }

    cursor2.endEditBlock();
    ui->textEdit->setTextCursor(cursor2);

    m_processing = false;
}
