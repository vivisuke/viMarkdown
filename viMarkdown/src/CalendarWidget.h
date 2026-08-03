#pragma once

#include <QCalendarWidget>

class CalendarWidget  : public QCalendarWidget
{
	Q_OBJECT

public:
	CalendarWidget(QWidget *parent);
	~CalendarWidget();
};

