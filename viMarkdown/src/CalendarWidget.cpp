#include <QPainter>
#include "CalendarWidget.h"

CalendarWidget::CalendarWidget(QWidget *parent)
	: QCalendarWidget(parent)
{
	qDebug() << "CalendarWidget::CalendarWidget(QWidget *parent)";
}

CalendarWidget::~CalendarWidget()
{}

void CalendarWidget::paintCell(QPainter *painter, const QRect &rect, QDate date) const {
	if( !m_dayInfoMap.contains(date) ) {
		QCalendarWidget::paintCell(painter, rect, date);
		return;
	}
	const DayInfo &di = m_dayInfoMap[date];
	qDebug() << "date = " << date << ", opened = " << di.m_openTodoCount << ", total = " << di.m_totalTodoCount;
	painter->save();
	if( di.m_totalTodoCount > 0 ) {
        if( di.m_openTodoCount == 0 )
			painter->fillRect(rect, QColor("#c0ffc0"));
        else {
			painter->fillRect(rect, QColor("#ffc0c0"));
			int closedCount = di.m_totalTodoCount - di.m_openTodoCount;
	        //double ratio = (double)closedCount / di.m_totalTodoCount;
			//int fillWidth = rect.width() * closedCount / di.m_totalTodoCount;
			//QRect fillRect(rect.left(), rect.top(), fillWidth, rect.height());
			int fillHeight = rect.height() * closedCount / di.m_totalTodoCount;
			QRect fillRect(rect.left(), rect.top() + rect.height() - fillHeight, rect.width(), fillHeight);
			painter->fillRect(fillRect, QColor("#c0ffc0"));
        }
	} else
		painter->fillRect(rect, QColor("#c0c0ff"));
	painter->restore();
	QCalendarWidget::paintCell(painter, rect, date);
}
