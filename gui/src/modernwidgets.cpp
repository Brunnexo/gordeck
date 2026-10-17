// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <modernwidgets.h>
#include <theme.h>

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

#define CONTROL_HEIGHT 44

ToggleSwitch::ToggleSwitch(QWidget *parent) : QAbstractButton(parent)
{
	setCheckable(true);
	setFocusPolicy(Qt::StrongFocus);
	setCursor(Qt::PointingHandCursor);
}

QSize ToggleSwitch::sizeHint() const
{
	return QSize(64, 34);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	QRectF track = QRectF(rect()).adjusted(2, 2, -2, -2);
	qreal radius = track.height() / 2.0;
	if(!isEnabled())
		painter.setOpacity(0.4);
	painter.setPen(hasFocus() ? QPen(Theme::accent_light, 2.0) : QPen(QColor(0x2b, 0x38, 0x58), 1.5));
	painter.setBrush(isChecked() ? Theme::accent : QColor(0x0c, 0x13, 0x22));
	painter.drawRoundedRect(track, radius, radius);
	qreal knob = track.height() - 8.0;
	qreal x = isChecked() ? track.right() - 4.0 - knob : track.left() + 4.0;
	painter.setPen(Qt::NoPen);
	painter.setBrush(isChecked() ? QColor(Qt::white) : Theme::text_muted);
	painter.drawEllipse(QRectF(x, track.top() + 4.0, knob, knob));
}

void ToggleSwitch::keyPressEvent(QKeyEvent *event)
{
	if(event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)
	{
		bool checked = event->key() == Qt::Key_Right;
		if(checked != isChecked())
			setChecked(checked);
		return;
	}
	QAbstractButton::keyPressEvent(event);
}

static QFrame *FindRow(QWidget *widget)
{
	for(QWidget *w = widget; w; w = w->parentWidget())
	{
		if(w->objectName() == "settingRow")
			return qobject_cast<QFrame *>(w);
	}
	return nullptr;
}

static void SetRowFocused(QWidget *widget, bool focused)
{
	QFrame *row = FindRow(widget);
	if(!row || row->property("focused").toBool() == focused)
		return;
	row->setProperty("focused", focused);
	row->style()->unpolish(row);
	row->style()->polish(row);
}

static void InstallRowHighlight()
{
	static bool installed = false;
	if(installed)
		return;
	installed = true;
	QObject::connect(qApp, &QApplication::focusChanged, qApp, [](QWidget *old, QWidget *now) {
		if(old)
			SetRowFocused(old, false);
		if(!now)
			return;
		SetRowFocused(now, true);
		// keep the focused control visible in scrollable pages
		for(QWidget *w = now->parentWidget(); w; w = w->parentWidget())
		{
			if(auto scroll = qobject_cast<QScrollArea *>(w))
			{
				scroll->ensureWidgetVisible(now, 20, 60);
				break;
			}
		}
	});
}

QFrame *ModernWidgets::CreateRow(QWidget *parent, const QString &title, const QString &description,
		QWidget *control, int control_width)
{
	InstallRowHighlight();

	auto row = new QFrame(parent);
	row->setObjectName("settingRow");
	row->setProperty("focused", false);

	auto layout = new QHBoxLayout(row);
	layout->setContentsMargins(20, 12, 16, 12);
	layout->setSpacing(18);

	auto text_layout = new QVBoxLayout();
	text_layout->setSpacing(2);
	auto title_label = new QLabel(title, row);
	title_label->setStyleSheet("font-size: 13pt; color: #ecf1fa; background: transparent;");
	text_layout->addWidget(title_label);
	if(!description.isEmpty())
	{
		auto description_label = new QLabel(description, row);
		description_label->setWordWrap(true);
		description_label->setStyleSheet("font-size: 10.5pt; color: #8e9bb4; background: transparent;");
		text_layout->addWidget(description_label);
	}
	layout->addLayout(text_layout, 1);

	control->setParent(row);
	if(qobject_cast<ToggleSwitch *>(control))
		control->setFixedSize(control->sizeHint());
	else
	{
		control->setFixedWidth(control_width);
		if(!qobject_cast<QLabel *>(control))
			control->setMinimumHeight(CONTROL_HEIGHT);
	}
	layout->addWidget(control, 0, Qt::AlignVCenter | Qt::AlignRight);
	return row;
}

void ModernWidgets::SetGridNavigation(QWidget *container)
{
	container->setProperty("gridNavigation", true);
}

bool ModernWidgets::HasGridNavigation(QWidget *widget)
{
	return widget && widget->parentWidget() && widget->parentWidget()->property("gridNavigation").toBool();
}
