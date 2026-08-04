#include "CalendarWidget.h"

CalendarWidget::CalendarWidget(QWidget *parent)
	: QCalendarWidget(parent)
{
	qDebug() << "CalendarWidget::CalendarWidget(QWidget *parent)";
}

CalendarWidget::~CalendarWidget()
{}

