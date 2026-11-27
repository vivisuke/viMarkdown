#include <QPainter>
#include "CalendarWidget.h"
#include "MainWindow.h"

extern Global g;

CalendarWidget::CalendarWidget(QWidget *parent)
	: QCalendarWidget(parent)
	, m_firePixmap(QStringLiteral(":/MainWindow/images/fire-2.png"))
	, m_starPixmap(QStringLiteral(":/MainWindow/images/star.png"))
{
}

void CalendarWidget::paintCell(QPainter *painter, const QRect &rect, QDate date) const {
	const auto it = m_dayInfoMap.constFind(date);
	if( it == m_dayInfoMap.constEnd() ) {
		QCalendarWidget::paintCell(painter, rect, date);
		return;
	}
	const DayInfo &di = it.value();
	painter->save();
	if( di.m_totalTodoCount > 0 ) {
		if( di.m_openTodoCount <= 0 ) {
			painter->fillRect(rect, g.m_completedColor);
		} else {
			painter->fillRect(rect, g.m_pendingColor);
			const int closedCount = qBound(0, di.m_totalTodoCount - di.m_openTodoCount, di.m_totalTodoCount);
			const int fillHeight = rect.height() * closedCount / di.m_totalTodoCount;
			if( fillHeight > 0 ) {
				const QRect fillRect(rect.left(), rect.top() + rect.height() - fillHeight, rect.width(), fillHeight);
				painter->fillRect(fillRect, g.m_completedColor);
			}
		}
		const bool showIcon = (di.m_openTodoCount > 0) || g.m_allCompletedStar;
		if( showIcon && di.m_totalTodoCount >= g.m_numOfLowFire ) {
			const QPixmap &pm = (di.m_openTodoCount <= 0) ? m_starPixmap : m_firePixmap;
			if( !pm.isNull() ) {
				const QSize targetSize = pm.size().scaled(rect.size(), Qt::KeepAspectRatio);
				const QRect targetRect(
					rect.left() + (rect.width() - targetSize.width()) / 2,
					rect.top() + (rect.height() - targetSize.height()) / 2,
					targetSize.width(),
					targetSize.height()
				);
				if( di.m_totalTodoCount < g.m_numOfHighFire )
					painter->setOpacity(0.3);
				painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
				painter->drawPixmap(targetRect, pm);
			}
		}
	} else {
		painter->fillRect(rect, g.m_notesOnlyColor);
	}
	painter->restore();
	QCalendarWidget::paintCell(painter, rect, date);
}
