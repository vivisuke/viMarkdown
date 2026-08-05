#pragma once

#include <QMap>
#include <QDate>

#include <QCalendarWidget>

struct DayInfo {
    QDateTime m_lastModified;               // 最終更新日時（QFileInfo::lastModified()）
    int m_totalTodoCount = 0;               // ToDo総数
    int m_openTodoCount = 0;                // 未完了ToDo数
};


class CalendarWidget  : public QCalendarWidget
{
	Q_OBJECT

public:
	CalendarWidget(QWidget *parent);
	~CalendarWidget();

public:
	void setDayInfoMap(const QDate &date, const DayInfo& di) {
		m_dayInfoMap[date] = di;
	}

protected:
    void paintCell(QPainter *painter, const QRect &rect, QDate date) const override;

private:
	QMap<QDate, DayInfo>	m_dayInfoMap;
};

