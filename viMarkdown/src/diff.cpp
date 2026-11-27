//----------------------------------------------------------------------
//
//			File:			"diff.cpp"
//			Created:		04-7-2026
//			Author:			vivisuke
//			Description:
//
//----------------------------------------------------------------------

#include <QTextDocument>
#include <QTextBlock>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QClipboard>
#include <QStringDecoder>
#include <QPointer>
#include <QSaveFile>
#include <QScopeGuard>
#include <vector>
#include <utility>
#include <algorithm>
#include <assert.h>
#include "dtl/dtl.hpp"
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "DocWidget.h"
#include "MarkdownEditor.h"
#include "MarkdownPreview.h"
#include "diff.h"

CharType getCharType(QChar ch);

// 1. ダミー行（高さを揃えるための空行）かどうかの判定
#if 1
bool isDummyLine(MarkdownEditor* editor, const QTextBlock &block) {
    const auto &vbn = editor->diffBlockNumbers();   // ← editor 自身の対応表を引く
    int bn = block.blockNumber();
    return bn >= 0 && bn < (int)vbn.size() && vbn[bn] < 0;
}
#else
bool isDummyLine(MarkdownEditor* editor, const QTextBlock &block) {
    const auto &vbn = editor->diffBlockNumbers();
    int bn = block.blockNumber();
    return bn < vbn.size() && vbn[bn] < 0;
    //return block.userState() == 0;
}
#endif
#if 1
// 2. 行番号（1オリジン）取得（2ビット右シフトするだけ）
int lineNumber(MarkdownEditor* editor, const QTextBlock &block) {
    const auto &vbn = editor->diffBlockNumbers();
    auto ix = block.blockNumber();
    if( ix >= 0 && ix < (int)vbn.size() )
	    return vbn[ix] + 1;
    else
    	return 0;
    //return (unsigned)block.userState() >> 2;
}

// 3. 差分の有無（下位2ビットが 0なら差分なし）
int getDiff(MarkdownEditor*editor, const QTextBlock &block) {
    const auto &flgs = editor->diffFlags();
    int bn = block.blockNumber();
    if( bn >= 0 && bn < (int)flgs.size() )
	    return flgs[bn];
    else
    	return 0;
    //return (block.userState() & 0x03);
}
bool hasDiff(MarkdownEditor*editor, const QTextBlock &block) {
	return getDiff(editor, block) != 0;
}

// ダミー行をセットする場合
void setDummyLine(MarkdownEditor* editor, QTextBlock block) {
    int bn = block.blockNumber();
    if( bn < 0 ) return;
    auto &vbn = editor->diffBlockNumbers();
    auto &flgs = editor->diffFlags();
    if( (int)vbn.size() < bn + 1 )
        vbn.resize(bn + 1, 0);
    if( (int)flgs.size() < bn + 1 )
        flgs.resize(bn + 1, 0);
    vbn[bn] = DUMMY_LINE_NUM;
    flgs[bn] = 0;
    //block.setUserState(0);
}

// 物理的な行をセットする場合
void setPhysicalLine(MarkdownEditor* editor, QTextBlock &block, int ln, uchar flag) {
    editor->diffBlockNumbers().push_back(block.blockNumber() - editor->nDummyLines());
    editor->diffFlags().push_back(flag);
    //int state = (ln << 2) | flag;
    //block.setUserState(state);
}
#else
// 2. 行番号（1オリジン）取得（1ビット右シフトするだけ）
int lineNumber(const QTextBlock &block) {
    return (unsigned)block.userState() >> 1;
}

// 3. 差分の有無（最下位ビットが 1 なら差分あり / 0 なら差分なし）
bool hasDiff(const QTextBlock &block) {
    return (block.userState() & 0x01) != 0;
}

// ダミー行をセットする場合
void setDummyLine(QTextBlock block) {
    block.setUserState(0);
}

// 物理的な行をセットする場合
void setPhysicalLine(QTextBlock &block, int ln, bool changed) {
    int state = (ln << 1) | (changed ? 1 : 0);
    block.setUserState(state);
}
#endif
int visualLineCount(const QTextBlock &block, MarkdownEditor* editor) {
	assert( block.isValid() );
	if( !block.isValid() ) {
		return 1;
	}
#if 0
	int vc = 1;			//	表示行数
	QTextLayout *layout = block.layout();
	if( layout != 0 ) vc = layout->lineCount();
	return vc;
#else
	QTextLayout *layout = block.layout();
	//if( layout != 0 ) vc = layout->lineCount();
	int vc = layout == nullptr ? 0 : layout->lineCount();
	if( vc == 0 ) {
#if 1
		int ls = qMax(1, editor->fontMetrics().lineSpacing());
		vc = block.document()->documentLayout()->blockBoundingRect(block).height() / ls;
#else
		qreal width = block.document()->textWidth();
        if (width <= 0) {
            return 1; // 幅が未定なら 1行扱い
        }
        layout->beginLayout();
        while (true) {
            QTextLine line = layout->createLine();
            if (!line.isValid()) break;
            line.setLineWidth(width);
        }
        layout->endLayout();
        vc = layout->lineCount();
#endif
	}
	return qMax(1, vc);
#endif
}
int visualLineCount(QTextDocument *doc, MarkdownEditor *editor) {
	int vc = 0;
	QTextBlock block = doc->begin();
	while( block.isValid() ) {
		vc += visualLineCount(block, editor);
		block = block.next();
	}
	return vc;
}
//
void MarkdownEditor::removeAllDummyLines() {
    QTextDocument *doc = document();
    if (!doc) return;
    if (!m_dummyInserted) return;
    //	文字挿入などで do_diff() より先にダミー行が除去される場合、除去前のスクロール位置を退避する（v0.3 #1390）
    if (m_docWidget && m_docWidget->m_diffMode && !m_docWidget->m_hasSavedDiffScroll) {
        m_docWidget->m_savedDiffScroll1 = m_docWidget->m_editor->verticalScrollBar()->value();
        m_docWidget->m_savedDiffScroll2 = m_docWidget->m_diffview->verticalScrollBar()->value();
        m_docWidget->m_hasSavedDiffScroll = true;
    }
    qDebug() << "removeAllDummyLines()";
    // シグナルを一時的に止めて、途中で do_diff が呼ばれないようにする
    QSignalBlocker blocker(this); 
    bool isModified = doc->isModified();

    QTextCursor cursor(doc);
    // 描画更新を一時停止し、処理を高速化
    cursor.beginEditBlock();

    // 実際に削除されたブロック番号を記録（非空ガード発動時に m_diffBlockNumbers と行数がずれないようにする）
    std::vector<bool> removed(m_diffBlockNumbers.size(), false);

    // 末尾のブロックから先頭に向かって逆順に巡回します
    // （空の prevBlock との結合で QTextBlock ハンドルが無効化されるのを防ぐため、ブロック番号で辿る）
    for (int bn = doc->blockCount() - 1; bn >= 0; --bn) {
        QTextBlock block = doc->findBlockByNumber(bn);
        if (!block.isValid()) continue;
        //if (isDummyLine(this, block))
        if (isDummyLine(this, block) && block.text().isEmpty())
        	//	ダミー行は常に空行。テキストがある場合は diffBlockNumbers がずれている（#1255）ので、
        	//	実テキストを誤って削除しないためのガード
        {
        	qDebug() << "bn = " << block.blockNumber() << " will be deleted";
            cursor.setPosition(block.position());

            if (block.next().isValid()) {
                // パターンA: 末尾以外のブロックを削除する場合
                // 「自ブロックの先頭」から「次のブロックの先頭」までを選択（これで改行コードも含まれます）
                cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                if (bn < (int)removed.size()) removed[bn] = true;
            } else {
                // パターンB: 末尾のブロックを削除する場合
                if (bn > 0) {
                    // 前の行が存在する場合、手前の改行コードも含めて後方から選択・削除します
                    cursor.movePosition(QTextCursor::EndOfBlock);
                    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
                    cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor); // 手前の改行を巻き込む
                    cursor.removeSelectedText();
                    if (bn < (int)removed.size()) removed[bn] = true;
                } else {
                    // ドキュメントにこの1行しか残っていない場合は、プレーンにテキストのみクリア（ブロック自体は残る）
                    cursor.select(QTextCursor::BlockUnderCursor);
                    cursor.removeSelectedText();
                }
            }
        }
    }

    cursor.endEditBlock(); // レイアウトを再計算して画面を更新
    {
        std::vector<int> newBlockNumbers;
        std::vector<uchar> newFlags;
        newBlockNumbers.reserve(doc->blockCount());
        newFlags.reserve(doc->blockCount());
        for (size_t i = 0; i < m_diffBlockNumbers.size(); ++i) {
            if (removed[i]) continue; // 実際に削除されたダミー行分だけ捨てる
            newFlags.push_back(i < m_diffFlags.size() ? m_diffFlags[i] : uchar(0));
            newBlockNumbers.push_back((int)newBlockNumbers.size()); // 実在行の連番に振り直し
        }
        m_diffBlockNumbers = std::move(newBlockNumbers);
        m_diffFlags = std::move(newFlags);
    }
    m_nDummyLines = 0;
    m_dummyInserted = false;
    doc->setModified(isModified);
}
// -------------------------------------------------------------
const QColor COLOR_DIFF_MINIMAP_DELETED   = QColor("#e8e8e8"); // 明るいグレイ（削除）
const QColor COLOR_DIFF_MINIMAP_ADDED   = QColor("#ffa0a0"); // 明るい赤（挿入）
//const QColor COLOR_DIFF_MINIMAP_MODIFIED = QColor("#ffffa0"); // 明るい黄色（変更）
//const QColor COLOR_DIFF_MINIMAP_MODIFIED = QColor("#ffff00"); // 黄色（変更）
const QColor COLOR_DIFF_MINIMAP_MODIFIED = QColor("#e0e000"); // 少し暗い黄色（変更）
#if 0
void updateMapSub(QPainter &p, int x, int width, QTextDocument* doc, int totalLines, int mapHeight) {
	if (totalLines <= 0) return;

	QTextBlock block = doc->begin();
	int currentVLine = 0; // 現在のビジュアル行インデックス

	while (block.isValid()) {
		QColor col = Qt::white;
		if (isDummyLine(block)) {
			col = QColor("#e8e8e8");
		} else {
			auto d = getDiff(block);
			if (d == ADDED_LINE) col = QColor("#ffa0a0");
			else if (d == CHANGED_LINE) col = QColor("#ffffa0");
		}

		int vc = visualLineCount(block);

		// このブロック（行）の MiniMap 上での Y 座標と高さを比率計算
		int y1 = currentVLine * mapHeight / totalLines;
		int y2 = (currentVLine + vc) * mapHeight / totalLines;
		int h = qMax(1, y2 - y1); // 最低でも1pxは描画

		p.fillRect(x, y1, width, h, col);

		currentVLine += vc;
		block = block.next();
	}
}

void MiniMap::updateMap(QTextDocument* doc1, QTextDocument* doc2) {
	auto ht = rect().height();
	m_mapPixmap = QPixmap(MINMAP_WIDTH, ht);
	m_mapPixmap.fill(QColor("#e8e8e8")); // 背景をデフォルト色で初期化

	int totalLines1 = visualLineCount(doc1);
	int totalLines2 = visualLineCount(doc2);
	// 左右で大きい方の行数に合わせる（通常diffアライメントで同一のはず）
	m_totalLines = qMax(totalLines1, totalLines2);

	if (m_totalLines <= 0) return;

	QPainter p(&m_mapPixmap);
	
	int halfW = MINMAP_WIDTH / 2;
	// 左ペイン用 (0 〜 halfW)
	updateMapSub(p, 0, halfW, doc1, m_totalLines, ht);
	// 右ペイン用 (halfW 〜 MINMAP_WIDTH)
	updateMapSub(p, halfW, MINMAP_WIDTH - halfW, doc2, m_totalLines, ht);

	// ※末尾の余白塗りつぶしは不要になります（全体にスケーリングされるため）
}
#else
#if 0
void updateMapSub(QPainter &p, int x, QTextDocument* doc, MarkdownEditor* editor, int totalLines, int mapHeight) {
	if( totalLines <= 0 ) return;
	QTextBlock block = doc->begin();
	int curVLine = 0;
	for(; block.isValid(); block=block.next()) {
		QColor col = Qt::white;
		if( isDummyLine(editor, block) ) col = COLOR_DIFF_MINIMAP_DELETED;
		else {
			auto d = getDiff(editor, block);
			if( d == ADDED_LINE ) col = COLOR_DIFF_MINIMAP_ADDED;
			else if( d == MODIFIED_LINE ) col = COLOR_DIFF_MINIMAP_MODIFIED;
		}
		int vc = visualLineCount(block, editor);
		int y1 = curVLine * mapHeight / totalLines;
		int y2 = (curVLine + vc) * mapHeight / totalLines;
		p.fillRect(x, y1, MINMAP_WIDTH/2, qMax(1, y2 - y1), col);
		curVLine += vc;
	}
}
void MiniMap::updateMap(QTextDocument* doc1, MarkdownEditor* editor1, QTextDocument* doc2, MarkdownEditor* editor2) {
	auto ht = rect().height();
	m_mapPixmap = QPixmap(MINMAP_WIDTH, ht);
	m_mapPixmap.fill(COLOR_DIFF_MINIMAP_DELETED);	//	背景（ダミー行色）
	m_totalLines = qMax(visualLineCount(doc1, editor1), visualLineCount(doc2, editor2));
	if( m_totalLines <= 0 ) return;
	QPainter p(&m_mapPixmap);
	updateMapSub(p, 0, doc1, editor1, m_totalLines, ht);
	updateMapSub(p, MINMAP_WIDTH/2, doc2, editor2, m_totalLines, ht);
}
#else
//void updateMapSub(QPainter &p, int x, QTextDocument* doc, MarkdownEditor* editor)
void updateMapSub(QPainter &p, int x, QTextDocument* doc, MarkdownEditor* editor, int totalLines, int mapHeight)
{
	if( totalLines > mapHeight ) {	//	帯の高さに収まらない場合のみ比例圧縮し、差分行が潰れないよう優先度付きで集約する
		std::vector<QColor> rowColor(mapHeight, Qt::white);
		std::vector<int> rowPrio(mapHeight, -1);
		int curVLine = 0;
		for(QTextBlock block = doc->begin(); block.isValid(); block=block.next()) {
			QColor col = Qt::white;
			int prio = 0;	//	通常行
			if( isDummyLine(editor, block) ) { col = COLOR_DIFF_MINIMAP_DELETED; prio = 1; }
			else {
				auto d = getDiff(editor, block);
				if( d == ADDED_LINE ) { col = COLOR_DIFF_MINIMAP_ADDED; prio = 2; }
				else if( d == MODIFIED_LINE ) { col = COLOR_DIFF_MINIMAP_MODIFIED; prio = 3; }
			}
			int vc = visualLineCount(block, editor);
			int y1 = (int)((qint64)curVLine * mapHeight / totalLines);
			int y2 = (int)((qint64)(curVLine + vc) * mapHeight / totalLines);
			if( y2 <= y1 ) y2 = y1 + 1;
			if( y2 > mapHeight ) y2 = mapHeight;
			for(int y = y1; y < y2; ++y) {
				if( prio > rowPrio[y] ) { rowPrio[y] = prio; rowColor[y] = col; }
			}
			curVLine += vc;
		}
		for(int y = 0; y < mapHeight; ++y)
			p.fillRect(x, y, MINMAP_WIDTH/2, 1, rowColor[y]);
		return;
	}
	QTextBlock block = doc->begin();
	for(int y = 0; /*y < doc->blockCount() &&*/ block.isValid(); block=block.next()) {
		QColor col = Qt::white;		//QColor("#808080");
		if( isDummyLine(editor, block) ) col = COLOR_DIFF_MINIMAP_DELETED;
		//else if( hasDiff(block) ) col = QColor("#ffa0a0");	//QColor("#ccffcc");	//QColor("#ffecec");
		else {
			auto d = getDiff(editor, block);
			if( d == ADDED_LINE ) col = COLOR_DIFF_MINIMAP_ADDED;
			else if( d == MODIFIED_LINE ) col = COLOR_DIFF_MINIMAP_MODIFIED;
		}
		p.setPen(col);
		int vc = visualLineCount(block, editor);
		for(int k = 0; k < vc; ++k) {
			p.drawLine(x, y, x+MINMAP_WIDTH/2-1, y);
			++y;
		}
	}
}
void MiniMap::updateMap(QTextDocument* doc1, MarkdownEditor* editor1, QTextDocument* doc2, MarkdownEditor* editor2) {
	//m_mapPixmap = QPixmap(MINMAP_WIDTH, doc1->blockCount());
	m_mapDoc1 = doc1; m_mapEditor1 = editor1; m_mapDoc2 = doc2; m_mapEditor2 = editor2;	//	resizeEvent での再描画用に保持
	auto ht = rect().height();
	m_totalLines = qMax(visualLineCount(doc1, editor1), visualLineCount(doc2, editor2));	//	paintEvent・マウス逆換算用
	if( ht <= 0 || m_totalLines <= 0 ) {
		m_mapPixmap = QPixmap();
		return;
	}
	m_mapPixmap = QPixmap(MINMAP_WIDTH, ht);
	m_mapPixmap.fill(COLOR_DIFF_MINIMAP_DELETED);
	QPainter p(&m_mapPixmap);
	int x = 0;
#if 1
	updateMapSub(p, 0, doc1, editor1, m_totalLines, ht);
	updateMapSub(p, MINMAP_WIDTH/2, doc2, editor2, m_totalLines, ht);
	if( m_totalLines <= ht ) {	//	圧縮時は全面描画済みのため余白塗りは不要
		p.fillRect(0, m_totalLines, MINMAP_WIDTH, ht - m_totalLines, COLOR_DIFF_MINIMAP_DELETED);
	}
#else
	updateMapSub(p, 0, doc1, editor1);
	updateMapSub(p, MINMAP_WIDTH/2, doc2, editor2);
	p.setBrush(COLOR_DIFF_MINIMAP_DELETED);
	//p.drawRect(0, doc1->blockCount(), MINMAP_WIDTH, ht - doc1->blockCount());
	p.drawRect(0, visualLineCount(doc1, editor1), MINMAP_WIDTH, ht - visualLineCount(doc1, editor1));
#endif
}
#endif
#endif
// -------------------------------------------------------------
std::vector<QString> extractLinesFromDocument(const QTextDocument *doc) {
    std::vector<QString> lines;
    if (!doc) return lines;

    // メモリ確保のオーバーヘッドを減らすため大まかにリザーブ
    lines.reserve(doc->blockCount());

    // QTextBlock を走査することで、巨大なテキストでも分割時のメモリ消費を最小限に抑えます
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        lines.push_back(block.text() /*+u'\n'*/);
    }
    //lines.back() += QChar(0xffff);
    if (!lines.empty())
        lines.back() += QString("\n")+ QChar(0xffff);		//	改行＋EOF
    return lines;
}
static bool readDiffPeerFile(const QString &fullPath, QString &content, QStringConverter::Encoding &encoding, bool &withBOM) {
	QFile file(fullPath);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	QByteArray raw = file.readAll();
	file.close();
	withBOM = false;
	auto bomEnc = QStringConverter::encodingForData(raw);
	if (bomEnc.has_value()) {
		encoding = bomEnc.value();
		withBOM = true;
		QStringDecoder dec(encoding);
		content = dec(raw);
	} else {
		QStringDecoder utf8Dec(QStringConverter::Utf8);
		QString decoded = utf8Dec(raw);
		if (!utf8Dec.hasError()) {
			encoding = QStringConverter::Utf8;
			content = decoded;
		} else {
			encoding = QStringConverter::System;
			QStringDecoder sysDec(encoding);
			content = sysDec(raw);
		}
	}
	content.replace("\r\n", "\n");
	content.replace('\r', '\n');
	return true;
}
bool MainWindow::confirmSaveDiffPeer(DocWidget *docWidget) {
	if( docWidget == nullptr || docWidget->m_diffview == nullptr )
		return true;
	if( !docWidget->m_diffview->document()->isModified() )
		return true;
	QString path = docWidget->m_diffPeerFullPath;
	QString name = path.isEmpty() ? docWidget->m_diffPeerTitle : path;
	QString mess = QString(tr("The document '%1' has been modified.\nDo you want to save your changes ?")).arg(name);
	QMessageBox::StandardButton reply = QMessageBox::warning(
		this,
		tr("Confirm save"),
		mess,
		QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
		QMessageBox::Save
	);
	if( reply == QMessageBox::Save ) {
		if( path.isEmpty() ) {
			path = QFileDialog::getSaveFileName(
				this,
				"Save File",
				QString(),
				"markdown (*.md);;all (*.*)",
				nullptr,
				QFileDialog::DontUseNativeDialog
			);
			if( path.isEmpty() ) return false;
			if( QFileInfo(path).suffix().isEmpty() ) path += ".md";
		}
		QSaveFile file(path);
		QIODevice::OpenMode mode = QIODevice::WriteOnly;
		if( docWidget->m_diffPeerEncoding != QStringConverter::Utf16
		 && docWidget->m_diffPeerEncoding != QStringConverter::Utf16LE
		 && docWidget->m_diffPeerEncoding != QStringConverter::Utf16BE ) {
			mode |= QIODevice::Text;
		}
		if( !file.open(mode) ) {
			QMessageBox::warning(this, tr("Error"), tr("Cannot write file:\n%1").arg(path));
			return false;
		}
		QTextStream out(&file);
		out.setEncoding(docWidget->m_diffPeerEncoding);
		out.setGenerateByteOrderMark(docWidget->m_diffPeerWithBOM);
		MarkdownEditor *diffView = docWidget->m_diffview;
		if( diffView->dummyInserted() ) {
			QStringList lines;
			for( QTextBlock block = diffView->document()->firstBlock(); block.isValid(); block = block.next() ) {
				if( isDummyLine(diffView, block) && block.text().isEmpty() ) continue;
				lines << block.text();
			}
			out << lines.join(u'\n');
		} else {
			out << diffView->toPlainText();
		}
		out.flush();
		if( out.status() != QTextStream::Ok || !file.commit() ) {
			QMessageBox::warning(this, tr("Error"), tr("Cannot write file:\n%1").arg(path));
			return false;
		}
		docWidget->m_diffPeerFullPath = path;
		docWidget->m_diffPeerTitle = QFileInfo(path).fileName();
		docWidget->m_diffviewLabel->setText(docWidget->m_diffPeerTitle);
		diffView->document()->setModified(false);
		return true;
	} else if( reply == QMessageBox::Discard ) {
		return true;
	}
	return false;
}
//	比較先ファイルを読み込み、docWidget->m_diffview と付随状態を初期化する共通ヘルパ
static bool loadDiffPeerIntoWidget(QWidget *parent, DocWidget *docWidget, const QString &fullPath) {
	QString content;
	QStringConverter::Encoding peerEnc = QStringConverter::Utf8;
	bool peerBOM = false;
	if (!readDiffPeerFile(fullPath, content, peerEnc, peerBOM)) {
		QMessageBox::warning(parent, QObject::tr("Error"), QObject::tr("Cannot open file:\n%1").arg(fullPath));
		return false;
	}
	docWidget->m_diffPeerEncoding = peerEnc;
	docWidget->m_diffPeerWithBOM = peerBOM;
	docWidget->m_diffPeerFullPath = fullPath;
	QFileInfo fi2(fullPath);
	docWidget->m_diffviewLabel->setText(docWidget->m_diffPeerTitle = fi2.fileName());
	docWidget->m_diffview->setDummyInserted(false);
	docWidget->m_diffview->diffBlockNumbers().clear();
	docWidget->m_diffview->diffFlags().clear();
	docWidget->m_diffview->clearNDummyLines();
	docWidget->m_diffview->setPlainText(content);
	docWidget->m_diffview->document()->setModified(false);
	docWidget->m_diffview->initUndoMgr();
	return true;
}
void MainWindow::diffview_open() {		//	文書を選択して比較
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr || !docWidget->m_diffMode )
		return;
	if( !confirmSaveDiffPeer(docWidget) )
		return;
	QString fullPath = QFileDialog::getOpenFileName(
		this,
		"select diff file",			// ダイアログのタイトル
		QDir::currentPath(),		// 初期ディレクトリ
		"markdown file (*.md *.markdown);;text file(*.txt);;all(*.*)"	// フィルター
	);
	if( fullPath.isEmpty() ) return;
	if( !loadDiffPeerIntoWidget(this, docWidget, fullPath) ) return;
	updateUndoRedoEnabled();
	do_diff();
}
void MainWindow::onAction_DiffWithFile() {
	//DocWidget *docWidget = getCurDocWidget();
	//if( docWidget == nullptr ) return;
	QString fullPath = QFileDialog::getOpenFileName(
		this,
		"select diff file",			// ダイアログのタイトル
		QDir::currentPath(),		// 初期ディレクトリ
		"markdown file (*.md *.markdown);;text file(*.txt);;all(*.*)"	// フィルター
	);
	if( fullPath.isEmpty() ) return;
	DocWidget *base = getCurDocWidget();	//	diff 元文書
	if( base == nullptr ) return;
	int ix = tabIndexOf("", fullPath);
	if( ix >= 0 ) {
		qDebug() << fullPath << " is already open.";
		if( ui->tabWidget->widget(ix) == base ) {
			QMessageBox::warning(this, tr("diff with file"),
				tr("Cannot diff a file with itself:\n%1").arg(fullPath));
			return;
		}
		QMessageBox msgBox(this);
		msgBox.setWindowTitle(tr("diff with file"));
		msgBox.setText(tr("The file is already open in another tab:\n%1\nClose that tab and continue with diff?").arg(fullPath));
		QAbstractButton *closeButton = msgBox.addButton(tr("Close tab and diff"), QMessageBox::AcceptRole);
		msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
		msgBox.exec();
		if( msgBox.clickedButton() != closeButton ) return;	//	diff を中止
		ui->tabWidget->setCurrentIndex(ix);
		do_close();		//	未保存なら保存確認ダイアログが出る（Cancel の場合は閉じない）
		if( tabIndexOf("", fullPath) >= 0 ) return;	//	閉じられなかった → diff を中止
		int bix = ui->tabWidget->indexOf(base);
		if( bix < 0 ) return;
		ui->tabWidget->setCurrentIndex(bix);	//	diff 元文書のタブをカレントに戻す
	}
	do_diff(fullPath);
}
void MainWindow::do_diff(const QString &diffPeerFullPath) {
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;
	if( docWidget->m_diffMode && !confirmSaveDiffPeer(docWidget) )
		return;
	if( !loadDiffPeerIntoWidget(this, docWidget, diffPeerFullPath) )
		return;
	updateUndoRedoEnabled();
	//
	docWidget->m_diffMode = true;
	docWidget->m_editor->setDiffMode(true);
	docWidget->m_diffview->setDiffMode(true);
	docWidget->m_editor->expandAll();
	docWidget->m_editor->setHighlightDiff(true);
	docWidget->m_diffview->setHighlightDiff(true);
	docWidget->m_editor->setHighlightMarkdown(false);
	//docWidget->m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
	docWidget->m_editor->rehighlight();
	//##docWidget->m_editor->document()->adjustSize();
	//##docWidget->m_diffview->document()->adjustSize();
	docWidget->updatePanes();
	//docWidget->m_editor->updateLnAreaRaw();
    if( ui->action_DiffMode->isChecked() ) {
        do_diff();
    } else {
        ui->action_DiffMode->setChecked(true);
    }
}
void DocWidget::setupDiffMode() {
	this->m_diffMode = true;
	this->m_editor->setDiffMode(true);
	this->m_diffview->setDiffMode(true);
	QScrollBar *bar1 = this->m_editor->horizontalScrollBar();
	QScrollBar *bar2 = this->m_diffview->horizontalScrollBar();
	this->m_editor->expandAll();
	this->m_editor->setHighlightDiff(true);
	this->m_diffview->setHighlightDiff(true);
	this->m_editor->setHighlightMarkdown(false);
	//this->m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
	//##do_diff();
	connect(this->m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
        this, &DocWidget::syncScrollFromLeft, Qt::UniqueConnection);
    connect(this->m_diffview->verticalScrollBar(), &QScrollBar::valueChanged,
        this, &DocWidget::syncScrollFromRight, Qt::UniqueConnection);
    this->m_diffview->verticalScrollBar()->setValue(this->m_diffview->verticalScrollBar()->value());
    int fvl = this->m_editor->verticalScrollBar()->value();
    int vl = this->m_editor->getVisibleLineCount();
    this->setMiniMapCurPos(fvl, vl);
    connect(this->m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
	        this, &DocWidget::syncMinimapWithEditor, Qt::UniqueConnection);
    connect(bar1, &QScrollBar::valueChanged, bar2, &QScrollBar::setValue, Qt::UniqueConnection);
	connect(bar2, &QScrollBar::valueChanged, bar1, &QScrollBar::setValue, Qt::UniqueConnection);
}
void MainWindow::onAction_DiffMode(bool checked) {
	QPointer<DocWidget> docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;
	//docWidget->m_diffMode = checked;
	//docWidget->m_editor->setDiffMode(checked);
	//QScrollBar *bar1 = docWidget->m_editor->horizontalScrollBar();
	//QScrollBar *bar2 = docWidget->m_diffview->horizontalScrollBar();
	++m_doing_diff;
	auto doingDiffGuard = qScopeGuard([this]() { --m_doing_diff; });
	if (checked) {
		docWidget->setupDiffMode();
		QCoreApplication::processEvents();
		if( !docWidget ) return;
		do_diff();
		if( !docWidget ) return;
		docWidget->m_editor->syncDiffViewCursorFromEditor();	//	diff 進入時に比較先カーソルをエディタ位置へ同期（#1365）
		QCoreApplication::processEvents();
		if( !docWidget ) return;
		//QTimer::singleShot(0, this, [this]() {
        //    do_diff();
        //});
		//bool b = docWidget->m_editor->document()->isModified();
		//qDebug() << "modified = " << b;
	} else {
		if( docWidget->m_diffMode && !confirmSaveDiffPeer(docWidget) ) {
			QSignalBlocker blocker(ui->action_DiffMode);
			ui->action_DiffMode->setChecked(true);
			return;
		}
		docWidget->m_diffview->document()->setModified(false);
		docWidget->m_diffMode = false;
		docWidget->m_hasSavedDiffScroll = false;
		docWidget->m_diffPeerFullPath.clear();
		docWidget->m_editor->setDiffMode(false);
		docWidget->m_diffview->setDiffMode(false);
		QScrollBar *bar1 = docWidget->m_editor->horizontalScrollBar();
		QScrollBar *bar2 = docWidget->m_diffview->horizontalScrollBar();
		if( docWidget->m_docType == DocType::Markdown )
			docWidget->m_editor->setHighlightMarkdown(true);
		docWidget->m_editor->setHighlightDiff(false);
		docWidget->m_diffview->setHighlightDiff(false);
		//docWidget->m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		QTextDocument *doc1 = docWidget->m_editor->document();
		QTextDocument *doc2 = docWidget->m_diffview->document();
		bool modified1 = doc1->isModified();
#if 0
		if( docWidget->m_editor->dummyInserted() )
			doc1->undo();
		if( docWidget->m_diffview->dummyInserted() )
			doc2->undo();
#else
		if( docWidget->m_editor->dummyInserted() )
            docWidget->m_editor->removeAllDummyLines();
		if( docWidget->m_diffview->dummyInserted() )
            docWidget->m_diffview->removeAllDummyLines();
#endif
	    docWidget->m_editor->setDummyInserted(false);
	    docWidget->m_diffview->setDummyInserted(false);
		doc1->setModified(modified1);
		doc2->setModified(false);
		disconnect(docWidget->m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
            docWidget, &DocWidget::syncScrollFromLeft);
	    disconnect(docWidget->m_diffview->verticalScrollBar(), &QScrollBar::valueChanged,
            docWidget, &DocWidget::syncScrollFromRight);
	    disconnect(docWidget->m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
            docWidget, &DocWidget::syncMinimapWithEditor);
        disconnect(bar1, &QScrollBar::valueChanged, bar2, &QScrollBar::setValue);
		disconnect(bar2, &QScrollBar::valueChanged, bar1, &QScrollBar::setValue);
	}
	//bool b = docWidget->m_editor->document()->isModified();
	//qDebug() << "modified = " << b;
	docWidget->m_editor->rehighlight();
	docWidget->updatePanes();
	//b = docWidget->m_editor->document()->isModified();
	//qDebug() << "modified = " << b;
}
void MainWindow::onDiffViewChanged() {	//	比較先エディタが編集された場合
	//##qDebug() << "MainWindow::onDiffViewChanged()";
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;
	if( docWidget->m_diffMode && !m_doing_diff) {
		m_diffTimer->start(100); 
	}
#if 0
    if (m_processing!=0) return;
    ++m_processing;
    do_diff();
    --m_processing;
#endif
}
#if 0
void applyInlineHighlight(QTextBlock &block, const DiffBlockUserData *userData, bool isLeft = true) {
    if (!block.isValid()) return;

    // データが空、または差分範囲がない場合はフォーマットを空にしてリセット
    if (!userData || userData->ranges.isEmpty()) {
        block.layout()->setFormats(QList<QTextLayout::FormatRange>()); // 空リストでリセット [1.1]
        return;
    }

    QList<QTextLayout::FormatRange> formats;

    // 1. 強調したい色を設定（ダミー行よりも一段階「濃い」パステルカラー）
    QTextCharFormat format;
    if (isLeft) {
        format.setBackground(QColor("#ffc1c1")); // 削除された文字：少し濃い目のパステル赤
    } else {
        format.setBackground(QColor("#b2f0b2")); // 挿入された文字：少し濃い目のパステル緑
    }

    // 2. 自前の範囲データ（ranges）を、Qtが描画に使う FormatRange 構造体に変換して追加します [1.1, 1.2]
    for (const auto &range : userData->ranges) {
        QTextLayout::FormatRange fRange;
        fRange.start = range.start;
        fRange.length = range.length;
        fRange.format = format;
        formats.append(fRange);
    }

    // 3. 【マジック】レイアウトに対してフォーマットを直接登録します [1.1, 1.2]
    block.layout()->setFormats(formats);

    // 4. 【重要】この行（ブロック）の再描画が必要であることをQtの描画エンジンに通知します [1.1, 1.2]
    if (block.document()) {
        const_cast<QTextDocument*>(block.document())->markContentsDirty(block.position(), block.length());
    }
}
#endif






std::vector<WordToken> tokenize(const QString &text) {
    std::vector<WordToken> tokens;
    int i = 0;
    int len = text.length();

    while (i < len) {
        if (text[i] == u'\n') {
            // 改行は単独トークンとして分離（複数行ハンクで次行インデントと結合しないようにする）
            tokens.push_back({text.mid(i, 1), i});
            i++;
        }
        else if (text[i].isSpace()) {
            // 空白セグメント（改行を除くスペースやタブの連続）
            int start = i;
            while (i < len && text[i].isSpace() && text[i] != u'\n') {
                i++;
            }
            tokens.push_back({text.mid(start, i - start), start});
        } 
        else if (text[i].isLetterOrNumber()) {
        	auto t = getCharType(text[i]);
            // 英数字・日本語ワードセグメント
            int start = i;
            while (i < len && text[i].isLetterOrNumber() && getCharType(text[i]) == t) {
                i++;
            }
            tokens.push_back({text.mid(start, i - start), start});
        } 
        else {
            // 記号列（カンマ、ピリオド、ブラケット等の連続）はひとつの単語としてトークン化（v0.3 #1406）
            int start = i;
            while (i < len && !text[i].isSpace() && !text[i].isLetterOrNumber()) {
                i++;
            }
            tokens.push_back({text.mid(start, i - start), start});
        }
    }
    return tokens;
}
void calculateAndSetWordDiff(QTextBlock block1, QTextBlock block2, const QString& text1, const QString& text2) {
	std::vector<WordToken> tokens1 = tokenize(text1);
    std::vector<WordToken> tokens2 = tokenize(text2);
    // 2. dtl::Diff<単語の型, ベクターの型> で単語単位の比較を実行！
    dtl::Diff<WordToken, std::vector<WordToken>> d(tokens1, tokens2);
    d.compose();
    auto ses = d.getSes().getSequence();

    // 各ブロックのテキスト内オフセット表を構築（単語範囲と相手側対応位置の正確な紐づけ用。v0.3 #1395, #1457）
    struct BlockSpan {
        QTextBlock block;
        int textStart;
        int textLen;
    };
    auto buildSpans = [](QTextBlock b, int totalLen) {
        std::vector<BlockSpan> spans;
        int off = 0;
        while (b.isValid() && off < totalLen) {
            int len = b.text().size();
            spans.push_back({b, off, len});
            off += len + 1;
            b = b.next();
        }
        return spans;
    };
    std::vector<BlockSpan> spans1 = buildSpans(block1, (int)text1.size());
    std::vector<BlockSpan> spans2 = buildSpans(block2, (int)text2.size());

    auto locatePos = [](const std::vector<BlockSpan> &spans, int pos, int &outBn, int &outCol) {
        outBn = -1;
        outCol = -1;
        for (const auto &sp : spans) {
            if (pos <= sp.textStart + sp.textLen) {
                outBn = sp.block.blockNumber();
                outCol = qBound(0, pos - sp.textStart, sp.textLen);
                return;
            }
        }
        // pos が相手側最終行の改行より後ろ（パディング対向の余剰行など）にある場合は未設定（-1）のまま返し、
        // reflectWordToPeer 側の既存フォールバック（#1447）に委ねる
    };
    auto appendRange = [](std::vector<BlockSpan> &spans, int pos, int len, int peerBn, int peerCol, int peerLen) {
        for (auto &sp : spans) {
            if (pos <= sp.textStart + sp.textLen) {
                auto *ud = dynamic_cast<DiffBlockUserData*>(sp.block.userData());
                if (!ud) {
                    ud = new DiffBlockUserData();
                    sp.block.setUserData(ud);
                }
                ud->ranges.append(DiffRange(pos - sp.textStart, len, peerBn, peerCol, peerLen));
                return;
            }
        }
    };

    // 変更区間（COMMON を挟まない削除・追加の連続）単位で単語範囲・相手側対応先・欠落位置を記録する（v0.3 #1363, #1457）
    std::vector<int> gaps1, gaps2;
    {
        int pos1 = 0, pos2 = 0;
        int hunkStart1 = 0, hunkStart2 = 0;
        std::vector<WordToken> delToks, addToks;
        int runDelNL = 0, runAddNL = 0;
        auto flushRun = [&]() {
            const int runDel = (int)delToks.size();
            const int runAdd = (int)addToks.size();
            if (runDel == 0 && runAdd == 0) return;

            const int endPos1 = delToks.empty() ? hunkStart1 : (delToks.back().start + (int)delToks.back().text.size());
            const int endPos2 = addToks.empty() ? hunkStart2 : (addToks.back().start + (int)addToks.back().text.size());

            for (int i = 0; i < runDel; ++i) {
                int pBn = -1, pCol = -1, pLen = 0;
                if (i < runAdd) {
                    locatePos(spans2, addToks[i].start, pBn, pCol);
                    pLen = (int)addToks[i].text.size();
                } else if (runAdd == 0 || runDelNL == runAddNL) {
                    locatePos(spans2, endPos2, pBn, pCol);
                    pLen = 0;
                }
                appendRange(spans1, delToks[i].start, (int)delToks[i].text.size(), pBn, pCol, pLen);
            }
            for (int i = 0; i < runAdd; ++i) {
                int pBn = -1, pCol = -1, pLen = 0;
                if (i < runDel) {
                    locatePos(spans1, delToks[i].start, pBn, pCol);
                    pLen = (int)delToks[i].text.size();
                } else if (runDel == 0 || runDelNL == runAddNL) {
                    locatePos(spans1, endPos1, pBn, pCol);
                    pLen = 0;
                }
                appendRange(spans2, addToks[i].start, (int)addToks[i].text.size(), pBn, pCol, pLen);
            }

            // 対になる分は置換であり欠落ではない（同一行内の置換でトークン数が食い違うだけの場合は欠落とはしない）
            // 欠落位置とするのは、片側が空の純挿入・純削除か、行構造（改行数）が食い違う場合に限る。
            if (runDel > runAdd && (runAdd == 0 || runDelNL != runAddNL)) gaps2.push_back(pos2);		// 削除が余る → doc2 側の現在位置が欠落位置
            else if (runAdd > runDel && (runDel == 0 || runDelNL != runAddNL)) gaps1.push_back(pos1);	// 追加が余る → doc1 側の現在位置が欠落位置
            delToks.clear();
            addToks.clear();
            runDelNL = runAddNL = 0;
        };
        for (const auto &item : ses) {
            const WordToken &token = item.first;
            dtl::elemInfo info = item.second;
            int sz = (int)token.text.size();
            switch (info.type) {
            case dtl::SES_COMMON:
                flushRun();
                pos1 += sz; pos2 += sz;
                hunkStart1 = pos1;
                hunkStart2 = pos2;
                break;
            case dtl::SES_DELETE:	// doc1 にだけある単語
                if (delToks.empty() && addToks.empty()) { hunkStart1 = pos1; hunkStart2 = pos2; }
                delToks.push_back(token);
                runDelNL += (int)token.text.count(u'\n');
                pos1 += sz;
                break;
            case dtl::SES_ADD:		// doc2 にだけある単語
                if (delToks.empty() && addToks.empty()) { hunkStart1 = pos1; hunkStart2 = pos2; }
                addToks.push_back(token);
                runAddNL += (int)token.text.count(u'\n');
                pos2 += sz;
                break;
            }
        }
        flushRun();
    }
    auto distributeGaps = [](QTextBlock block, const std::vector<int> &gaps, int textLen) {
        const int maxPos = qMax(0, textLen - 1);
        int start = 0;	// block のテキスト内開始位置
        size_t gi = 0;
        while (block.isValid() && gi < gaps.size()) {
            int bsize = block.text().size();
            while (gi < gaps.size() && qMin(gaps[gi], maxPos) - start <= bsize) {
                auto *ud = dynamic_cast<DiffBlockUserData*>(block.userData());
                if (!ud) {
                    ud = new DiffBlockUserData();
                    block.setUserData(ud);
                }
                ud->gaps.append(qBound(0, qMin(gaps[gi], maxPos) - start, bsize));
                ++gi;
            }
            start += bsize + 1;
            block = block.next();
        }
    };
    distributeGaps(block1, gaps1, (int)text1.size());
    distributeGaps(block2, gaps2, (int)text2.size());
}
//
#if 0
std::vector<int>	g_vc1, g_vc2;
void buildVcTable(QTextDocument *doc, std::vector<int>& vc) {
	vc.clear();
	QTextBlock block = doc->begin();
	while( block.isValid() ) {
		vc.push_back(visualLineCount(block));
		block = block.next();
	}
}
#endif
void MainWindow::insertDummyLines(MarkdownEditor* editor, QTextCursor &cur, QTextBlock &block, int count, bool deleted) {
	if (count <= 0) return;
#if 1
    QTextDocument* doc = cur.document();
	int insertPos;
    bool atEnd = !block.isValid();

    if (atEnd) {
        // block が無効（ドキュメント末尾）の場合は、文書の終端位置に挿入
        insertPos = doc->characterCount() - 1;
        cur.setPosition(insertPos);
        // 末尾に改行を入れてダミー行を作成
        for (int i = 0; i < count; ++i) {
            cur.insertText("\n");
        }
    } else {
        // 通常は block の手前に挿入
        insertPos = block.position();
        cur.setPosition(insertPos);
        for (int i = 0; i < count; ++i) {
            cur.insertText("\n");
        }
    }

    // 挿入された空ブロック群をダミー行に設定
    QTextBlock dummy = doc->findBlock(insertPos);
    if (!atEnd) {
        for (int i = 0; i < count && dummy.isValid(); ++i) {
            editor->incNDummyLines();
            editor->diffBlockNumbers().push_back(DUMMY_LINE_NUM);
            editor->diffFlags().push_back(deleted ? DELETED_LINE : PADDING_LINE);
            dummy = dummy.next();
        }
        block = dummy; // 押し出された元のテキストブロックを指すように更新
    } else {
        // 末尾に追加した場合は、新しく末尾にできたダミー行群を設定
        dummy = dummy.next(); // 挿入した改行の次のブロックからがダミー
        for (int i = 0; i < count && dummy.isValid(); ++i) {
            editor->incNDummyLines();
            editor->diffBlockNumbers().push_back(DUMMY_LINE_NUM);
            editor->diffFlags().push_back(deleted ? DELETED_LINE : PADDING_LINE);
            dummy = dummy.next();
        }
        block = doc->end(); // 末尾のまま
    }
#else
    int insertPos = block.position();
    cur.setPosition(insertPos);
    for (int i = 0; i < count; ++i) {
        cur.insertText("\n");
    }
    // 挿入された空ブロック群をダミーに設定
    QTextBlock dummy = cur.document()->findBlock(insertPos);
    for (int i = 0; i < count; ++i) {
        setDummyLine(dummy);
        dummy = dummy.next();
    }
    block = dummy; // 元のテキストブロックを指すように更新
#endif
}
// 左側のみ存在（右側で削除）
void MainWindow::applyDeleteHunk(DocWidget* docWidget,
    int diffLn, int endLn,
    //int nAdd,
    int &ln1,
    QTextBlock &block1, QTextBlock &block2,
    QTextCursor &cur2, const std::vector<QString> &lines1) 
{
	//int endLn = diffLn + nAdd;
    for (int ln = diffLn; ln < endLn && block1.isValid(); ++ln) {
        if (ln - 1 >= (int)lines1.size()) break;
        //if (ln - 1 < lines1.size())
        //    do_output(QString("- %1 0 '%2'\n").arg(ln).arg(lines1[ln - 1]));

        setPhysicalLine(docWidget->m_editor, block1, ++ln1, ADDED_LINE);
        int vc = qMax(1, visualLineCount(block1, docWidget->m_editor));
        block1 = block1.next();

        // 右側にダミー行を挿入
        insertDummyLines(docWidget->m_diffview, cur2, block2, vc);
    }
}

// 右側のみ存在（右側で追加）
void MainWindow::applyAddHunk(DocWidget* docWidget,
    int diffLn, int endLn,
    //int nDelete,
    int &ln2,
    QTextBlock &block1, QTextBlock &block2,
    QTextCursor &cur1, const std::vector<QString> &lines2) 
{
	//int endLn = diffLn + nDelete;
    for (int ln = diffLn; ln < endLn && block2.isValid(); ++ln) {
        if (ln - 1 >= (int)lines2.size()) break;
        //do_output(QString("+ 0 %1 '%2'\n").arg(ln).arg(lines2[ln - 1]));

        setPhysicalLine(docWidget->m_diffview, block2, ++ln2, ADDED_LINE);
        int vc = qMax(1, visualLineCount(block2, docWidget->m_diffview));
        block2 = block2.next();

        // 左側にダミー行を挿入
        insertDummyLines(docWidget->m_editor, cur1, block1, vc);
    }
}

// 変更行（両側で異なる）
void MainWindow::applyModifyHunk(DocWidget* docWidget,
    int diffLn1, int endLn1,
    int diffLn2, int endLn2,
    int nDelete, int nAdd, int &ln1, int &ln2,
    QTextBlock &block1, QTextBlock &block2,
    QTextCursor &cur1, QTextCursor &cur2) 
{
	endLn1 = diffLn1 + nDelete;
	endLn2 = diffLn2 + nAdd;
	assert( endLn1 - diffLn1 == nDelete );
	assert( endLn2 - diffLn2 == nAdd );
    // 単語差分用テキストの抽出
    QString text1, text2;
    auto b1 = block1, b2 = block2;
    for (int i = 0; i < nDelete && b1.isValid(); ++i, b1 = b1.next())
        text1 += b1.text() + "\n";
    for (int i = 0; i < nAdd && b2.isValid(); ++i, b2 = b2.next())
        text2 += b2.text() + "\n";

    calculateAndSetWordDiff(block1, block2, text1, text2);

    // 左側の行属性設定と表示行数カウント
    int totalVc1 = 0;
    for (int ln = diffLn1; ln < endLn1 && block1.isValid(); ++ln) {
        setPhysicalLine(docWidget->m_editor, block1, ++ln1, MODIFIED_LINE);
        totalVc1 += qMax(1, visualLineCount(block1, docWidget->m_editor));
        block1 = block1.next();
    }

    // 右側の行属性設定と表示行数カウント
    int totalVc2 = 0;
    for (int ln = diffLn2; ln < endLn2 && block2.isValid(); ++ln) {
        setPhysicalLine(docWidget->m_diffview, block2, ++ln2, MODIFIED_LINE);
        totalVc2 += qMax(1, visualLineCount(block2, docWidget->m_diffview));
        block2 = block2.next();
    }
	qDebug() << "totalVc1 = " << totalVc1 << ", totalVc2 = " << totalVc2;
    // 高さの差をダミー行で埋める
    int d = totalVc1 - totalVc2;
    if (d > 0) {
        insertDummyLines(docWidget->m_diffview, cur2, block2, d, false);
    } else if (d < 0) {
        insertDummyLines(docWidget->m_editor, cur1, block1, -d, false);
    }
}
void MainWindow::applyDiffToDocuments(
    DocWidget *docWidget,
    const std::vector<QString> &lines1,
    const std::vector<QString> &lines2,
    const dtl::Ses<QString> &ses) 
{
	docWidget->m_editor->diffFlags().clear();
	docWidget->m_editor->diffBlockNumbers().clear();
	docWidget->m_editor->clearNDummyLines();
	docWidget->m_diffview->diffFlags().clear();
	docWidget->m_diffview->diffBlockNumbers().clear();
	docWidget->m_diffview->clearNDummyLines();
    QTextDocument *doc1 = docWidget->m_editor->document();
    QTextDocument *doc2 = docWidget->m_diffview->document();
    QTextBlock block1 = doc1->begin();
    QTextBlock block2 = doc2->begin();
    QTextCursor cur1 = docWidget->m_editor->textCursor();
    QTextCursor cur2 = docWidget->m_diffview->textCursor();

    cur1.beginEditBlock();
    cur2.beginEditBlock();

    int ln1 = 0, ln2 = 0;
    int diffLn1 = INT_MAX, diffLn2 = INT_MAX;
    int nDelete = 0, nAdd = 0;

    auto flush = [&](int endLn1, int endLn2) {
        if (nDelete == 0 && nAdd == 0) return;
        if (nAdd == 0) {
            applyDeleteHunk(docWidget, diffLn1,
            				diffLn1 + nDelete,
            				ln1, block1, block2, cur2, lines1);
        } else if (nDelete == 0) {
            applyAddHunk(docWidget, diffLn2, diffLn2 + nAdd,
            				ln2, block1, block2, cur1, lines2);
        } else {
            applyModifyHunk(docWidget, diffLn1, endLn1,
                                diffLn2, endLn2,
            					nDelete, nAdd, ln1, ln2, block1, block2, cur1, cur2);
        }
        nDelete = nAdd = 0;
        diffLn1 = diffLn2 = INT_MAX;
    };

    for (const auto &item : ses.getSequence()) {
        const QString &line = item.first;
        dtl::elemInfo info = item.second;
        switch (info.type) {
        case dtl::SES_COMMON:
            flush(info.beforeIdx, info.afterIdx);
            //do_output(QString("= %1 %2 '%3'\n").arg(info.beforeIdx).arg(info.afterIdx).arg(line));
            setPhysicalLine(docWidget->m_editor, block1, ++ln1, 0);
            block1 = block1.next();
            setPhysicalLine(docWidget->m_diffview, block2, ++ln2, 0);
            block2 = block2.next();
            break;
        case dtl::SES_DELETE:
            diffLn1 = qMin(diffLn1, info.beforeIdx);
            nDelete++;
            break;
        case dtl::SES_ADD:
            diffLn2 = qMin(diffLn2, info.afterIdx);
            nAdd++;
            break;
        }
    }
    flush((int)lines1.size() + 1, (int)lines2.size() + 1);

    cur1.endEditBlock();
    cur2.endEditBlock();
}
void MainWindow::do_diff() {
	if (m_processing != 0) return;
    DocWidget *docWidget = getCurDocWidget();
    if (!docWidget || !docWidget->m_diffMode) return;
    ++m_doing_diff;
    auto doingDiffGuard = qScopeGuard([this]() { --m_doing_diff; });
    QSignalBlocker blocker(this); 
    QTextCursor cursor0 = docWidget->m_editor->textCursor();
    QTextCursor cursor2 = docWidget->m_diffview->textCursor();	//	右ビューのカーソルも保存する（v0.3 #1393）
    //	diff 再計算でスクロール位置を変えないための保存（v0.3 #1390）。
    //	再計算中のダミー行の抜き差しで Qt がスクロールバーを一時的に動かし、
    //	末尾のカーソル復元時の最小スクロールでは元の位置に戻らないことがある。
    QScrollBar *vbar1 = docWidget->m_editor->verticalScrollBar();
    QScrollBar *vbar2 = docWidget->m_diffview->verticalScrollBar();
    const int vscroll1 = docWidget->m_hasSavedDiffScroll ? docWidget->m_savedDiffScroll1 : vbar1->value();
    const int vscroll2 = docWidget->m_hasSavedDiffScroll ? docWidget->m_savedDiffScroll2 : vbar2->value();
    //int pos0 = cursor.position();
    //qDebug() << "cursor.position() = " << pos0 << ", blockNumber() = " << cursor.blockNumber();;

    ++m_processing;
    auto processingGuard = qScopeGuard([this]() { --m_processing; });
    //m_diffBlockNumbers.clear();
    // --- 1. 前処理 ---
    QTextDocument *doc1 = docWidget->m_editor->document();
    QTextDocument *doc2 = docWidget->m_diffview->document();
    //qreal width2 = docWidget->m_diffview->viewport()->width();
    //doc2->setTextWidth(width2);
    //doc2->adjustSize();
    //buildVcTable(doc1, g_vc1);
    //buildVcTable(doc2, g_vc2);
    bool modified1 = doc1->isModified();
    bool modified2 = doc2->isModified();

    if (docWidget->m_editor->dummyInserted())   docWidget->m_editor->removeAllDummyLines();
    if (docWidget->m_diffview->dummyInserted()) docWidget->m_diffview->removeAllDummyLines();
    //	ダミー行除去後の実テキスト基準でカーソル位置（物理行＋列）を保存する（v0.3 #1426）。
    //	末尾へのダミー行挿入（atEnd）で QTextCursor が最終ダミー行へ押し出されるのを防ぐため。
    struct SavedCursorPos {
        int anchorLine = 0, anchorCol = 0;
        int posLine = 0, posCol = 0;
    };
    auto saveCursorPos = [](const QTextCursor &c, const QTextDocument *doc) -> SavedCursorPos {
        QTextBlock ab = doc->findBlock(c.anchor());
        QTextBlock pb = doc->findBlock(c.position());
        return {
            ab.isValid() ? ab.blockNumber() : 0,
            ab.isValid() ? c.anchor() - ab.position() : 0,
            pb.isValid() ? pb.blockNumber() : 0,
            pb.isValid() ? c.position() - pb.position() : 0
        };
    };
    const SavedCursorPos savedCur0 = saveCursorPos(cursor0, doc1);
    const SavedCursorPos savedCur2 = saveCursorPos(cursor2, doc2);
	//	前回の単語差分強調 (DiffBlockUserData) を全てクリアする。
	//	「変更→共通」になったブロックは calculateAndSet* を通らないため、
	//	クリアしないと古い強調が残り続ける (SPR #1182)
	for (QTextBlock b = doc1->begin(); b.isValid(); b = b.next()) b.setUserData(nullptr);
	for (QTextBlock b = doc2->begin(); b.isValid(); b = b.next()) b.setUserData(nullptr);

    std::vector<QString> lines1 = extractLinesFromDocument(doc1);
    std::vector<QString> lines2 = extractLinesFromDocument(doc2);

    // --- 2. diff 計算 ---
    dtl::Diff<QString, std::vector<QString>> d(lines1, lines2);
    d.compose();

    // --- 3. ドキュメント反映 ---
    applyDiffToDocuments(docWidget, lines1, lines2, d.getSes());

    // --- 4. 後処理・UI更新 ---
    docWidget->m_editor->setDummyInserted(docWidget->m_editor->nDummyLines() != 0);
    docWidget->m_diffview->setDummyInserted(docWidget->m_diffview->nDummyLines() != 0);
    docWidget->m_minimap->updateMap(doc1, docWidget->m_editor, doc2, docWidget->m_diffview);
    docWidget->m_editor->rehighlight();
    docWidget->m_diffview->rehighlight();
    //	diffFlags を書き換えただけでは行番号エリアは再描画されないため、明示的に更新する（#1299）
    docWidget->m_editor->updateLnAreaRaw();
    docWidget->m_diffview->updateLnAreaRaw();
    doc1->setModified(modified1);
    doc2->setModified(modified2);

    const auto vbn1 = docWidget->m_editor->diffBlockNumbers();
    const auto flg1 = docWidget->m_editor->diffFlags();
    const auto vbn2 = docWidget->m_diffview->diffBlockNumbers();
    const auto flg2 = docWidget->m_diffview->diffFlags();
    if( vbn1.size() != flg1.size() ) {
    	qDebug() << "vbn1.size(): " << vbn1.size() << ", flg1.size(): " << flg1.size();
    }
    assert( vbn1.size() == flg1.size() );
    if( vbn2.size() != flg2.size() ) {
    	qDebug() << "vbn2.size(): " << vbn2.size() << ", flg2.size(): " << flg2.size();
    }
    assert( vbn2.size() == flg2.size() );

    processingGuard.dismiss();
    --m_processing;

    //##//	カーソル復元に伴う「カーソルを表示範囲に入れる」最小スクロールが、ダミー行の抜き差しで
    //##//	レイアウトが未確定な状態のときに誤作動しないよう、先にカーソル位置のレイアウトを確定させる（v0.3 #1390）
    //##(void)docWidget->m_editor->cursorRect(cursor0);
    // 例：直前位置が正しい前提なら、シグナルを止めてから setTextCursor → 直後復元
	//	カーソル復元に伴う同期（逆流）で復元結果が上書きされないよう、両ビューのシグナルを止めてから復元する（v0.3 #1393）
    {
        auto restoreCursorPos = [](MarkdownEditor *ed, QTextDocument *doc, const SavedCursorPos &sc) -> QTextCursor {
            const auto &vbn = ed->diffBlockNumbers();
            auto toDocPos = [&](int physLine, int col) -> int {
                int targetBn = -1;
                for (size_t bn = 0; bn < vbn.size(); ++bn) {
                    if (vbn[bn] == physLine) {
                        targetBn = (int)bn;
                        break;
                    }
                }
                QTextBlock b = (targetBn >= 0) ? doc->findBlockByNumber(targetBn) : doc->lastBlock();
                if (!b.isValid()) return 0;
                return b.position() + qBound(0, col, b.text().size());
            };
            QTextCursor c(doc);
            int apos = toDocPos(sc.anchorLine, sc.anchorCol);
            int ppos = toDocPos(sc.posLine, sc.posCol);
            c.setPosition(apos);
            if (ppos != apos)
                c.setPosition(ppos, QTextCursor::KeepAnchor);
            return c;
        };
        QSignalBlocker sbEditor(docWidget->m_editor);
        QSignalBlocker sbDiffView(docWidget->m_diffview);
        docWidget->m_editor->setTextCursor(restoreCursorPos(docWidget->m_editor, doc1, savedCur0));
        docWidget->m_diffview->setTextCursor(restoreCursorPos(docWidget->m_diffview, doc2, savedCur2));
    }
    //	スクロール位置を復元する（v0.3 #1390）。同期スロット経由で互いに
    //	上書きし合わないよう、両バーのシグナルを止めて個別に戻す。
    {
        QSignalBlocker vblocker1(vbar1);
        QSignalBlocker vblocker2(vbar2);
        vbar1->setValue(qMin(vscroll1, vbar1->maximum()));
        vbar2->setValue(qMin(vscroll2, vbar2->maximum()));
    }
    docWidget->m_hasSavedDiffScroll = false;
    //cursor = docWidget->m_editor->textCursor();
    //pos0 = cursor.position();
    //qDebug() << "cursor.position() = " << pos0 << ", blockNumber() = " << cursor.blockNumber();;
    //	この時点の文書 revision を記録する。単語差分データ（DiffBlockUserData）は
    //	この revision のテキストに対するスナップショットになる（v0.3 #1394）
    docWidget->m_diffRev1 = doc1->revision();
    docWidget->m_diffRev2 = doc2->revision();
}
//----------------------------------------------------------------------
//	--- の前に現れてよい既知の前置き行かどうか
static bool isPatchPreambleLine(const QString &line) {
	if( line.isEmpty() || line == "---" ) return true;
	static const char *prefixes[] = {
		"index ", "new file mode ", "deleted file mode ", "old mode ", "new mode ",
		"rename from ", "rename to ", "copy from ", "copy to ",
		"similarity index ", "dissimilarity index ", "Binary files ",
	};
	for( const char *p : prefixes )
		if( line.startsWith(QLatin1String(p)) ) return true;
	return false;
}
#if 1
void MainWindow::dumpLinesToOutput(const QString &caption, const QStringList &lines) {
	do_output("\n" + caption + "\n");
	for( int i = 0; i < lines.size(); ++i )
		do_output(QString("%1: %2\n").arg(i + 1, 4).arg(lines[i]));
}
#else
auto dumpLines = [&](const QString &caption, const QStringList &ls) {
	do_output("\n" + caption + "\n");
	for( int i = 0; i < ls.size(); ++i )
		do_output(QString("%1: %2\n").arg(i + 1, 4).arg(ls[i]));
};
#endif
//	成功すれば true、失敗時は error に行番号つきの理由を入れる
bool parseUnifiedDiff(const QString &text, QVector<DiffFile> &files, QString *error = nullptr) {
	files.clear();
	static const QRegularExpression reHunk(R"(^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@(.*)$)");
	QStringList lines = text.split(u'\n');
	int fi = -1;		//	現在の DiffFile（-1: なし）
	int hi = -1;		//	現在の DiffHunk
	int needOld = 0, needNew = 0;	//	現ハンクの残り行数
	auto fail = [&](int ln, const QString &msg) {
		if( error != nullptr )
			*error = QString("line %1: %2: ").arg(ln + 1).arg(msg) + lines[ln];
		return false;
	};
	for( int ln = 0; ln < lines.size(); ++ln ) {
		QString line = lines[ln];
		if( line.endsWith(u'\r') ) line.chop(1);
		if( fi >= 0 && hi >= 0 && ((needOld > 0 || needNew > 0) || line.startsWith(u'\\')) ) {
			//	ハンク本体
			DiffHunk &hunk = files[fi].m_hunks[hi];
			if( line.startsWith(u'\\') ) {		//	"\ No newline at end of file"
				if( !hunk.m_lines.isEmpty() ) {
					QChar t = hunk.m_lines.back().m_type;
					if( t == u'-' || t == u' ' ) hunk.m_oldNoNewline = true;
					if( t == u'+' || t == u' ' ) hunk.m_newNoNewline = true;
				}
				continue;
			}
			QChar type = line.isEmpty() ? u' ' : line[0];	//	空行は文脈行とみなす（空白が落ちたパッチ対策）
			if( type != u' ' && type != u'-' && type != u'+' )
				return fail(ln, "invalid hunk line");
			hunk.m_lines.push_back({type, line.isEmpty() ? QString() : line.mid(1)});
			if( type == u' ' ) { --needOld; --needNew; }
			else if( type == u'-' ) --needOld;
			else --needNew;
			if( needOld < 0 || needNew < 0 )
				return fail(ln, "hunk line count mismatch");
			continue;
		}
		hi = -1;
		if( line.startsWith("diff --git ") ) {
			files.push_back(DiffFile());
			fi = (int)files.size() - 1;
		} else if( line.startsWith("--- ") ) {
			if( fi < 0 || !files[fi].m_hunks.isEmpty() || !files[fi].m_oldPath.isEmpty() ) {
				files.push_back(DiffFile());
				fi = (int)files.size() - 1;
			}
			QString p = line.mid(4);
			int tab = p.indexOf(u'\t');		//	古典形式のタイムスタンプを除く
			if( tab >= 0 ) p = p.left(tab);
			files[fi].m_oldPath = stripDiffPrefix(p.trimmed());
			qDebug() << "oldPath = " << files[fi].m_oldPath;
		} else if( line.startsWith("+++ ") ) {
			if( fi < 0 ) return fail(ln, "+++ without ---");
			QString p = line.mid(4);
			int tab = p.indexOf(u'\t');
			if( tab >= 0 ) p = p.left(tab);
			files[fi].m_newPath = stripDiffPrefix(p.trimmed());
			qDebug() << "newPath = " << files[fi].m_newPath;
		} else if( line.startsWith("@@ ") ) {
			if( fi < 0 ) return fail(ln, "@@ without file header");
			auto m = reHunk.match(line);
			if( !m.hasMatch() ) return fail(ln, "invalid hunk header");
			files[fi].m_hunks.push_back(DiffHunk());
			hi = (int)files[fi].m_hunks.size() - 1;
			DiffHunk &hunk = files[fi].m_hunks[hi];
			hunk.m_oldStart = m.captured(1).toInt();
			hunk.m_oldCount = m.captured(2).isEmpty() ? 1 : m.captured(2).toInt();
			hunk.m_newStart = m.captured(3).toInt();
			hunk.m_newCount = m.captured(4).isEmpty() ? 1 : m.captured(4).toInt();
			hunk.m_section = m.captured(5).trimmed();
			needOld = hunk.m_oldCount;
			needNew = hunk.m_newCount;
		} else if( !isPatchPreambleLine(line) ) {
			return fail(ln, "unexpected line");	
		}
		//	パッチのプリアンブル行（index 行、モード変更行等）は無視
	}
	if( hi >= 0 && (needOld > 0 || needNew > 0) )
		return fail((int)lines.size() - 1, "hunk is truncated");
	//	ハンクを持たないファイル（モード変更のみ等）を除外し、有効なハンクが1つも無ければ失敗とする
	files.erase(std::remove_if(files.begin(), files.end(),
		[](const DiffFile &f) { return f.m_hunks.isEmpty(); }), files.end());
	if( files.isEmpty() ) {
		if( error != nullptr ) *error = "no hunk found";
		return false;
	}
	return true;
}
//	oldLines に file のハンクを適用し、結果を newLines に返す（失敗時は error に理由）
bool applyDiffFile(const QStringList &oldLines, const DiffFile &file, QStringList &newLines, QString *error = nullptr, QString *detail = nullptr) {
	newLines.clear();
	int srcPos = 0;		//	oldLines の読み進み位置
	int fuzzDelta = 0;	//	前ハンクのファジー検索による位置ズレ（後続の純挿入ハンクにも引き継ぐ）
	for( int hi = 0; hi < file.m_hunks.size(); ++hi ) {
		const DiffHunk &hunk = file.m_hunks[hi];
		QStringList oldSide, newSide;
		for( const DiffLine &dl : hunk.m_lines ) {
			if( dl.m_type != u'+' ) oldSide << dl.m_text;
			if( dl.m_type != u'-' ) newSide << dl.m_text;
		}
		//	適用位置（oldStart は1始まり。oldCount==0 のときは oldStart 行の直後）
		int basePos = hunk.m_oldCount == 0 ? hunk.m_oldStart : hunk.m_oldStart - 1;
		int pos = basePos + fuzzDelta;
		if( oldSide.isEmpty() ) {
			if( pos < srcPos || pos > oldLines.size() ) {
				if( error != nullptr ) *error = QString("hunk %1: out of range").arg(hi + 1);
				return false;
			}
		} else {
			//	旧側テキストが一致するか検証し、ずれている場合は最も近い一致位置を探す
			auto matchesAt = [&](int p) {
				if( p < srcPos || p + oldSide.size() > oldLines.size() ) return false;
				for( int i = 0; i < oldSide.size(); ++i )
					if( oldLines[p + i] != oldSide[i] ) return false;
				return true;
			};
			if( !matchesAt(pos) ) {
				int found = -1;
				for( int p = srcPos; p + oldSide.size() <= oldLines.size(); ++p )
					if( matchesAt(p) && (found < 0 || qAbs(p - pos) < qAbs(found - pos)) ) found = p;
				if( found < 0 ) {
					if( error != nullptr ) *error = QString("hunk %1: context mismatch").arg(hi + 1);
					if( detail != nullptr ) {
						QStringList d;
						d << QString("hunk %1: patch expected (at line %2):").arg(hi + 1).arg(pos + 1);
						for( const QString &s : oldSide ) d << s;
						d << "current:";
						int end = qMin(pos + (int)oldSide.size(), (int)oldLines.size());
						for( int i = qMax(0, pos); i < end; ++i ) d << oldLines[i];
						if( end <= qMax(0, pos) ) d << "(no lines: beyond end of file)";
						*detail = d.join('\n');
					}
					return false;
				}
				pos = found;
				fuzzDelta = pos - basePos;
			}
		}
		for( int i = srcPos; i < pos; ++i ) newLines << oldLines[i];	//	ハンク間の無変更部分
		newLines << newSide;
		srcPos = pos + (int)oldSide.size();
	}
	for( int i = srcPos; i < oldLines.size(); ++i ) newLines << oldLines[i];
	return true;
}
void MainWindow::do_merge(const QString &arg) {
	//	クリップボードのパッチを対象ファイルへ適用し、新規タブで diff 表示する
	//	-wrapright 指定時のみ右端で折り返す（デフォルトは折り返さない）
	bool wrapRight = false;
	for( const QString &tok: arg.split(u' ', Qt::SkipEmptyParts) ) {
		if( tok == "-wrapright" )
			wrapRight = true;
		else {
			QString msg = tr("Merge: unknown option (%1)").arg(tok);
			statusBar()->showMessage(msg, 5000);
			do_output(msg + "\n");
			return;
		}
	}
	QString text = QGuiApplication::clipboard()->text();
	do_output("clipboard text:\n" + text + "\n");
	QVector<DiffFile> files;
	QString error;
	if( !parseUnifiedDiff(text, files, &error) ) {
		QString msg = tr("Merge: cannot parse as a patch (%1)").arg(error);
		statusBar()->showMessage(msg, 5000);
		do_output(msg + "\n");
		return;
	}
	for( const DiffFile &file : files )
		do_merge_file(file, wrapRight);
}
//	パッチ内の1ファイルを対象ファイルへ適用し、新規タブで diff 表示する
void MainWindow::do_merge_file(const DiffFile &file, bool wrapRight) {
	QString error;
	if( file.m_oldPath == "/dev/null" ) {
		QString msg = tr("Merge: creating a new file is not supported");
		statusBar()->showMessage(msg, 5000);
		do_output(msg + "\n");
		return;
	}

	//	対象ファイルのフルパスを解決する：現在の文書のディレクトリを優先し、次にそのままのパス
	//	（parseUnifiedDiff() で stripDiffPrefix() 適用済みのため二重適用しない）
	QString relPath = file.m_oldPath;
	QString fullPath;
	DocWidget *cur = getCurDocWidget();
	if( cur != nullptr && !cur->m_fullPath.isEmpty() ) {
		QString cand = QDir(QFileInfo(cur->m_fullPath).absolutePath()).filePath(relPath);
		if( QFileInfo::exists(cand) ) fullPath = QFileInfo(cand).absoluteFilePath();
	}
	if( fullPath.isEmpty() && QFileInfo::exists(relPath) )
		fullPath = QFileInfo(relPath).absoluteFilePath();
	if( fullPath.isEmpty() ) {
		QString msg = tr("Merge: target file not found (%1)").arg(relPath);
		statusBar()->showMessage(msg, 5000);
		do_output(msg + "\n");
		return;
	}

	//	対象ファイルが既に開いているか（開く前に調べる。無題タブが置き換わる場合があるため）
	bool alreadyOpen = false;
	for( int i = 0; i < ui->tabWidget->count(); ++i ) {
		//DocWidget *w = qobject_cast<DocWidget*>(ui->tabWidget->widget(i));
		DocWidget *w = static_cast<DocWidget*>(ui->tabWidget->widget(i));
		if( w != nullptr && w->m_fullPath == fullPath ) { alreadyOpen = true; break; }
	}

	//	対象ファイルを先に開く（既に開いていればそのタブがアクティブになる）

	do_open("", fullPath);
	DocWidget *srcWidget = getCurDocWidget();
	if( srcWidget == nullptr || srcWidget->m_fullPath != fullPath ) {
		QString msg = tr("Merge: cannot open the target file (%1)").arg(relPath);
		statusBar()->showMessage(msg, 5000);
		do_output(msg + "\n");
		return;
	}
	QString oldText = srcWidget->m_editor->document()->toPlainText();
	auto srcEncoding = srcWidget->m_encoding;
	bool srcWithBOM = srcWidget->m_withBOM;
	if( !alreadyOpen ) do_close();	//	:merge が開いたタブは取り込み後に閉じる


	//	タブ作成前にパッチ適用を検証・実行する（失敗時にゴミタブを残さない）
	QStringList oldLines = oldText.split('\n');
	if( oldText.endsWith('\n') ) oldLines.removeLast();
	QStringList newLines;
	QString detail;
	if( !applyDiffFile(oldLines, file, newLines, &error, &detail) ) {
		QString msg = tr("Merge: cannot apply patch (%1)").arg(error);
		statusBar()->showMessage(msg, 5000);
		do_output(msg + "\n");
		if( !detail.isEmpty() ) do_output(detail + "\n");
		//dumpLinesToOutput("buffer:", oldLines);
		return;
	}
	QString newText = newLines.join('\n');
	bool trailingNewline = oldText.isEmpty() || oldText.endsWith('\n');
	if( !file.m_hunks.isEmpty() ) {
		const DiffHunk &last = file.m_hunks.last();
		if( last.m_newNoNewline ) trailingNewline = false;
		else if( last.m_oldNoNewline ) trailingNewline = true;
	}
	if( trailingNewline && !newText.isEmpty() ) newText += '\n';

	//	新規タブを開き、左＝適用結果、右＝元のソースで diff 表示にする
	QString title = QFileInfo(fullPath).fileName();
	title.remove(QRegularExpression("\\.md$"));
	//addTab(title + " (merge)", fullPath, oldText, srcWidget->m_withBOM, srcWidget->m_encoding);
	addTab(title  /*+ " (merge)"*/, fullPath, oldText, srcWithBOM, srcEncoding);
	DocWidget *docWidget = getCurDocWidget();
	if( docWidget == nullptr ) return;

	docWidget->m_editor->setPlainText(newText);
	docWidget->m_editor->document()->setModified(true);
	docWidget->m_diffPeerTitle = QFileInfo(fullPath).fileName() + " (before)";		//	右側は適用前の元のソース
	docWidget->m_diffviewLabel->setText(docWidget->m_diffPeerTitle);
	docWidget->m_diffview->setDummyInserted(false);
	docWidget->m_diffview->diffBlockNumbers().clear();
	docWidget->m_diffview->diffFlags().clear();
	docWidget->m_diffview->clearNDummyLines();
	docWidget->m_diffview->setPlainText(oldText);
	docWidget->m_diffview->document()->setModified(false);
	docWidget->m_diffview->initUndoMgr();
	updateUndoRedoEnabled();
	docWidget->m_diffMode = true;
	docWidget->m_diffPeerFullPath = fullPath;
	docWidget->m_diffPeerEncoding = srcEncoding;
	docWidget->m_diffPeerWithBOM = srcWithBOM;
	docWidget->m_editor->setDiffMode(true);
	docWidget->m_diffview->setDiffMode(true);
	docWidget->m_editor->setHighlightDiff(true);
	docWidget->m_diffview->setHighlightDiff(true);
	docWidget->m_editor->setHighlightMarkdown(false);
	docWidget->m_diffview->setHighlightMarkdown(false);
	docWidget->m_editor->expandAll();
	docWidget->m_diffview->expandAll();
	//	-wrapright 指定時のみ右端で折り返す（デフォルトは折り返さない）
	QPlainTextEdit::LineWrapMode wrapMode = wrapRight ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap;
	docWidget->m_editor->setLineWrapMode(wrapMode);
	docWidget->m_diffview->setLineWrapMode(wrapMode);
	docWidget->updatePanes();
	if( ui->action_DiffMode->isChecked() ) {
		onAction_DiffMode(true);
	} else {
		ui->action_DiffMode->setChecked(true);
	}
}
