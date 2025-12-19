#pragma once

#include <QString>
#include <vector>

class MarkdownToHtmlConvertor
{
public:
	explicit MarkdownToHtmlConvertor(const QString& markdownText = "")
        : m_markdownText(markdownText)
    {}

    void	setMarkdownText(const QString& txt) { m_markdownText = txt; }
	const QString&	getMarkdownText() const { return m_markdownText; }
	const QString&	getHtmlText() const { return m_htmlText; }
    const QString&	convert();
    const QStringList&	getHeadings() const { return m_headingList; }
    const std::vector<char>	getBlockType() const { return m_blockType; }

private:
    void	do_heading(const QString&);
    void	do_list(const QString&);
    void	do_olist(const QString&);
    void	do_paragraph(const QString&);

    QString	parceInline(const QString&);

    void	open_ul(int lvl);
    void	close_ul(int lvl=0);
    void	open_ol(int lvl);
    void	close_ol(int lvl=0);

private:
	QString		m_markdownText;
	QString		m_htmlText;
	QStringList	m_headingList;		//	見出しレベル（1～9）＋見出し文字列
	bool		m_isParagraphOpen = true;
	bool		m_isInsideUl = false;
	bool		m_isInsideOl = false;
	int			m_curUlLevel = 0;
	int			m_curOlLevel = 0;
	int			m_nSpace = 0;
	std::vector<int>	m_headingLine;
	std::vector<char>	m_blockType;	//	' ': body text, '#': headings
};

