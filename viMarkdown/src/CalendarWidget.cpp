#include <QPainter>
#include "CalendarWidget.h"
#include "MainWindow.h"

extern Global g;

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
			painter->fillRect(rect, g.m_completedColor /*QColor("#c0ffc0")*/);
        else {
			painter->fillRect(rect, g.m_pendingColor /*QColor("#ffc0c0")*/);
			int closedCount = di.m_totalTodoCount - di.m_openTodoCount;
	        //double ratio = (double)closedCount / di.m_totalTodoCount;
#if 0
			int fillWidth = rect.width() * closedCount / di.m_totalTodoCount;
			QRect fillRect(rect.left(), rect.top(), fillWidth, rect.height());
#else
			int fillHeight = rect.height() * closedCount / di.m_totalTodoCount;
			QRect fillRect(rect.left(), rect.top() + rect.height() - fillHeight, rect.width(), fillHeight);
#endif
			painter->fillRect(fillRect, g.m_completedColor /*QColor("#c0ffc0")*/);
        }
        if( di.m_totalTodoCount >= g.m_numOfLowFire ) {
        	static const QPixmap firePixmap(":/MainWindow/images/fire-2.png"); 
        	static const QPixmap starPixmap(":/MainWindow/images/star.png"); 
        	if( !firePixmap.isNull() && !starPixmap.isNull() ) {
				// セルサイズに合わせてアスペクト比を維持したまま縮小（※余白を入れるなら rect.size() * 0.8）
				const auto& pm = di.m_openTodoCount == 0 ? starPixmap : firePixmap;
				QPixmap scaled = pm.scaled(rect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
				// 中央揃えの座標（x, y）を計算
				int x = rect.left() + (rect.width() - scaled.width()) / 2;
				int y = rect.top() + (rect.height() - scaled.height()) / 2;
				if( di.m_totalTodoCount < g.m_numOfHighFire )
					 painter->setOpacity(0.3); 
				if( g.m_allCompletedStar || di.m_openTodoCount != 0 )
					painter->drawPixmap(x, y, scaled);
			}
        }
	} else
		painter->fillRect(rect, g.m_notesOnlyColor /*QColor("#c0c0ff")*/);
	painter->restore();
	QCalendarWidget::paintCell(painter, rect, date);
}
