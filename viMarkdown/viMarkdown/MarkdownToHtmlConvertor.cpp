#include <QList>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include "markdowntohtmlconvertor.h"

using namespace std;

const QString& MarkdownToHtmlConvertor::convert(const QString& markdownText) {
	m_markdownText = markdownText;
	m_htmlText.clear();
	m_htmlText += "<html>\n";
	m_htmlText += "<head>\n";
    m_htmlText += "<meta charset=\"UTF-8\">\n";
	m_htmlText += "<style>\n";
    m_htmlText += "    blockquote {\n";
    m_htmlText += "        border-left: 10px solid blue;\n";
    m_htmlText += "        background-color: #ffffe0;\n";
    m_htmlText += "        padding: 20px;\n";
    //m_htmlText += "        border-radius: 4px;\n";
    m_htmlText += "    }\n";
    m_htmlText += "</style>\n";
	m_htmlText += "</head>\n";
	m_htmlText += "<body>\n";
	m_headingList.clear();
	m_headingLineNum.clear();
	m_blockNumTohtmlLineNum.clear();
	m_isParagraphOpen = true;
	m_curUlLevel = 0;
	m_curOlLevel = 0;
	auto lst = m_markdownText.split('\n');
	m_blockType.resize(lst.size());
	int htmlLn = 0;
	m_ln = 0;
	//for(int ln = 0; ln != lst.size(); ++ln) {
	while(  m_ln < lst.size() ) {
		auto lnStr = lst[m_ln];
		m_blockNumTohtmlLineNum.push_back(htmlLn);
		m_nSpace = 0;
		m_blockType[m_ln] = ' ';
	  	while( m_nSpace < lnStr.size() && lnStr[m_nSpace] == ' ' ) ++m_nSpace;
	  	if( m_nSpace != 0 ) lnStr = lnStr.mid(m_nSpace);
		if( lnStr.isEmpty() ) {
			m_isParagraphOpen = true;
			++m_ln;
        } else if( lnStr.startsWith('#') ) {
			m_blockType[m_ln] = '#';
			do_heading(lnStr, m_ln);
		} else if( lnStr.startsWith("- ") ) {
			do_list(lnStr);
		} else if( lnStr[0].isNumber() && lnStr.mid(1).startsWith(". ") ) {
			do_olist(lnStr);
		} else if( lnStr.startsWith("> ") ) {
			do_quote(lnStr);
		} else if( lnStr == "---" ) {
			m_htmlText += "<hr>\n";
			m_isParagraphOpen = true;
			++m_ln;
		} else {
			do_paragraph(lnStr);
		}
	}
	close_quote();
	close_ul();
	close_ol();
	m_htmlText += "</body>\n";
	m_htmlText += "</html>\n";
	return m_htmlText;
}
void MarkdownToHtmlConvertor::open_ul(int lvl) {
	while( m_curUlLevel < lvl ) {
		m_htmlText += "<ul>\n";
		++m_curUlLevel;
	}
}
void MarkdownToHtmlConvertor::close_ul(int lvl) {
	while( m_curUlLevel > lvl) {
		m_htmlText += "</ul>\n";
		--m_curUlLevel;
	}
}
void MarkdownToHtmlConvertor::open_ol(int lvl) {
	while( m_curOlLevel < lvl ) {
		m_htmlText += "<ol>\n";
		++m_curOlLevel;
	}
}
void MarkdownToHtmlConvertor::close_ol(int lvl) {
	while( m_curOlLevel > lvl) {
		m_htmlText += "</ol>\n";
		--m_curOlLevel;
	}
}
void MarkdownToHtmlConvertor::close_quote() {
	if( m_isInsideQuote ) {
		m_htmlText += "</blockquote>\n";
		m_isInsideQuote = false;
	}
}
QString MarkdownToHtmlConvertor::parceInline(const QString& lnStr) {
	QString result = lnStr;

	//QRegularExpression italicRe("\\*(.+?)\\*");
	//result.replace(italicRe, "<em>\\1</em>");

    static QRegularExpression re_bold(R"((?<![\\])(\*\*|__))");  // 直前が \ でない ** or __ とマッチ
	QRegularExpressionMatch match;
	while ((match = re_bold.match(result)).hasMatch()) {
        int s = match.capturedStart();  // 最初のマッチ位置
        match = re_bold.match(result, s+2);
		if( !match.hasMatch() ) break;
        int e = match.capturedStart();  // ２番目のマッチ位置 
		result = result.left(s) + "<b>" + result.mid(s+2, e - s - 2) + "</b>" + result.mid(e+2);
	}
    static QRegularExpression re_italic(R"((?<![\\])(\*|_))");  // 直前が \ でない * or _ とマッチ
	while ((match = re_italic.match(result)).hasMatch()) {
        int s = match.capturedStart();  // 最初のマッチ位置 
		match = re_italic.match(result, s+1);
		if( !match.hasMatch() ) break;
        int e = match.capturedStart();  // ２番目のマッチ位置 
        if( e == s + 1 ) break;
		result = result.left(s) + "<i>" + result.mid(s+1, e - s - 1) + "</i>" + result.mid(e+1);
	}
    static QRegularExpression re_del(R"((?<![\\])~~)");  // 直前が \ でない ~~ とマッチ
	while ((match = re_del.match(result)).hasMatch()) {
        int s = match.capturedStart();  // 最初のマッチ位置
        match = re_del.match(result, s+2);
		if( !match.hasMatch() ) break;
        int e = match.capturedStart();  // ２番目のマッチ位置 
		result = result.left(s) + "<s>" + result.mid(s+2, e - s - 2) + "</s>" + result.mid(e+2);
	}
    static QRegularExpression re_check(R"((?<![\\])(\[ \]))");  // 直前が \ でない [ ] とマッチ
	while ((match = re_check.match(result)).hasMatch()) {
        int s = match.capturedStart();  // 最初のマッチ位置 
        QString left = result.left(s);
        QString nbsp;
        if( left.isEmpty() && m_nSpace != 0 ) {
	        for(int i = 0; i < m_nSpace; ++i)
	        	nbsp += "&nbsp;&nbsp;";
        }
		result = left + nbsp + " □ " + result.mid(s+3);
		//result = left + nbsp + " <input disabled=\"\" type=\"checkbox\"> " + result.mid(s+3);
	}
    static QRegularExpression re_checked(R"((?<![\\])(\[[xX]\]))");  // 直前が \ でない [x] [X]とマッチ
	while ((match = re_checked.match(result)).hasMatch()) {
        int s = match.capturedStart();  // 最初のマッチ位置 
        QString left = result.left(s);
        QString nbsp;
        if( left.isEmpty() && m_nSpace != 0 ) {
	        for(int i = 0; i < m_nSpace; ++i)
	        	nbsp += "&nbsp;&nbsp;";
        }
		result = left + nbsp + " ☑ " + result.mid(s+3);
	}
	static QRegularExpression re_link(R"((?<!!)\[([^\]]*)\]\(([^)]*)\))");		//	[title](URL)
	while ((match = re_link.match(result)).hasMatch()) {
        int s = match.capturedStart();  // 最初のマッチ位置 
        int length = match.capturedLength();	//	マッチ長
        QString title = match.captured(1); // グループ1: タイトル
        QString url   = match.captured(2); // グループ2: URL
        result = result.left(s) + "<a href=\"" + url + "\">" + title + "</a>" + result.mid(s+length);
	}
	int s = 0;
	while( (s = result.indexOf('\\', s)) >= 0 && s+1 < result.size() ) {
        QString ch = result[s+1];
        if( ch == "<" )
        	ch = "&lt;";
        else if( ch == ">" )
        	ch = "&gt;";
        result = result.left(s) + ch + result.mid(s+2);
        s += 1;
	}
#if 0
	static QRegularExpression re_esc("\\");		//
	while ((match = re_esc.match(result)).hasMatch()) {
        int s = match.capturedStart();  // 最初のマッチ位置 
        //QString ch = match.captured(1); // エスケープされた文字
        QString ch = result[s+1];
        if( ch == "<" )
        	ch = "&lt;";
        else if( ch == ">" )
        	ch = "&gt;";
        result = result.left(s) + ch + result.mid(s+2);
	}
#endif

    result.replace("\\*", "*");
    result.replace("\\_", "_");
    //##result.replace("--", "-");		//	この変換は拡張仕様？

	return result;
}
void MarkdownToHtmlConvertor::do_heading(const QString& lnStr, int lineNum) {
	close_ul();
	close_ol();
	int i = 1;
	while( i < lnStr.size() && lnStr[i] == '#' ) ++i;
	int h = i;
	while( i < lnStr.size() && lnStr[i] == ' ' ) ++i;
	QString t = lnStr.mid(i);
	if( h == 1 )
		m_htmlText += QString("<h1 align=center>") + t + "</h1>\n";
	else
		m_htmlText += QString("<h%1>").arg(h) + t + QString("</h%1>\n").arg(h);
	m_headingList.push_back(QString("%1 ").arg(i) + t);
	m_headingLineNum.push_back(lineNum);
	m_isParagraphOpen = true;
	++m_ln;
}
bool isCheckboxLine(const QString& lnStr) {		//	"- [ ]" or "- [x]" or "- [X]" か？
	return lnStr.size() >= 6 && lnStr[2] == '[' && lnStr[4] == ']' && lnStr[5] == ' ' &&
			(lnStr[3] == ' ' || lnStr[3] == 'x' || lnStr[3] == 'X');
}
void MarkdownToHtmlConvertor::do_list(const QString& lnStr) {
	if( isCheckboxLine(lnStr) ) {
		if( m_isParagraphOpen ) {
			m_htmlText += "<p>\n";
			m_isParagraphOpen = false;
		}
		m_nSpace += 2;
		m_htmlText += parceInline(lnStr.mid(2)) + "<br/>\n";
		++m_ln;
		return;
	}
    if( !m_isInsideUl ) {
	    if( m_curUlLevel > 0 )
	    	close_ol();
	    m_isInsideUl = true;
    }
    m_isInsideOl = false;
    const int lvl = m_nSpace/2 + 1;
    if( lvl < m_curUlLevel )
	    close_ul(lvl);
    else
	    open_ul(lvl);
	m_htmlText += "<li>" + parceInline(lnStr.mid(2)) + "\n";
	++m_ln;
}
void MarkdownToHtmlConvertor::do_olist(const QString& lnStr) {
    if( !m_isInsideOl ) {
	    if( m_curOlLevel > 0 )
	    	close_ul();
	    m_isInsideOl = true;
    }
    m_isInsideUl = false;
    const int lvl = m_nSpace/2 + 1;
    if( lvl < m_curOlLevel )
	    close_ol(lvl);
    else
	    open_ol(lvl);
	m_htmlText += "<li>" + parceInline(lnStr.mid(2)) + "\n";
	++m_ln;
}
void MarkdownToHtmlConvertor::do_quote(const QString& lnStr) {
	if( !m_isInsideQuote ) {
		m_isInsideQuote = true;
		m_htmlText += "<blockquote>\n";
	}
	m_htmlText += parceInline(lnStr.mid(2)) + "<br/>\n";
	++m_ln;
}
void MarkdownToHtmlConvertor::do_paragraph(const QString& lnStr) {
	close_quote();
	close_ul();
	close_ol();
    m_isInsideUl = false;
    m_isInsideOl = false;
	if( m_isParagraphOpen ) {
		m_htmlText += "<p>";
		m_isParagraphOpen = false;
	}
	m_htmlText += parceInline(lnStr);
	if( lnStr.endsWith("  ") )
		m_htmlText += "<br/>\n";
	else
		m_htmlText += "\n";
	++m_ln;
}
