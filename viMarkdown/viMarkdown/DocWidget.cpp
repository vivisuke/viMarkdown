#include "MarkdownEditor.h"
#include "DocWidget.h"

DocWidget::DocWidget(const QString& title, const QString& fullPath, QWidget *parent)
	: m_title(title)
	, m_fullPath(fullPath)
	, QWidget(parent)
{
}
bool DocWidget::isModified() const {
	return m_mdEditor->document()->isModified();
}
void DocWidget::setModified(bool b) {
	m_mdEditor->document()->setModified(b);
}
