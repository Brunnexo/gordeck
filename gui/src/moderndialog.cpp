// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <moderndialog.h>
#include <hintbar.h>
#include <theme.h>
#include <modernwidgets.h>

#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#define DIALOG_RADIUS 22.0
#define ICON_SIZE 52

static QString IconFile(ModernDialog::Icon icon)
{
	switch(icon)
	{
		case ModernDialog::Icon::Info: return ":/icons/ui-info.svg";
		case ModernDialog::Icon::Question: return ":/icons/ui-help.svg";
		case ModernDialog::Icon::Warning: return ":/icons/ui-warning.svg";
		case ModernDialog::Icon::Error: return ":/icons/ui-error.svg";
		case ModernDialog::Icon::Success: return ":/icons/ui-check.svg";
		default: return QString();
	}
}

static QColor IconColor(ModernDialog::Icon icon)
{
	switch(icon)
	{
		case ModernDialog::Icon::Warning: return Theme::state_standby;
		case ModernDialog::Icon::Error: return Theme::danger;
		case ModernDialog::Icon::Success: return Theme::state_ready;
		default: return Theme::accent_light;
	}
}

ModernDialog::ModernDialog(const QString &title, QWidget *parent, Icon icon, int width)
	: QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint),
	vertical_buttons(false),
	clicked_index(-1),
	dim_overlay(nullptr)
{
	setAttribute(Qt::WA_TranslucentBackground);
	setWindowTitle(title);
	setFixedWidth(width);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(36, 28, 36, 18);
	layout->setSpacing(12);

	icon_label = new QLabel(this);
	icon_label->setAlignment(Qt::AlignCenter);
	icon_label->setVisible(icon != Icon::None);
	if(icon != Icon::None)
		icon_label->setPixmap(Theme::RenderIcon(IconFile(icon), ICON_SIZE, IconColor(icon), devicePixelRatioF()));
	layout->addWidget(icon_label);

	title_label = new QLabel(title, this);
	title_label->setAlignment(Qt::AlignCenter);
	title_label->setWordWrap(true);
	title_label->setStyleSheet("font-size: 18pt; font-weight: bold; color: #ecf1fa;");
	layout->addWidget(title_label);

	text_label = new QLabel(this);
	text_label->setAlignment(Qt::AlignCenter);
	text_label->setWordWrap(true);
	text_label->setTextInteractionFlags(Qt::NoTextInteraction);
	text_label->setStyleSheet("font-size: 13pt; color: #8e9bb4;");
	text_label->hide();
	layout->addWidget(text_label);

	content_layout = new QVBoxLayout();
	content_layout->setSpacing(10);
	layout->addLayout(content_layout);

	layout->addSpacing(6);

	button_container = new QWidget(this);
	button_layout = new QHBoxLayout(button_container);
	button_layout->setContentsMargins(0, 0, 0, 0);
	button_layout->setSpacing(12);
	ModernWidgets::SetGridNavigation(button_container);
	layout->addWidget(button_container);

	layout->addSpacing(2);

	auto hints = new HintBar(this);
	hints->AddStretch();
	auto hint_select = hints->AddHint(HintGlyph::A, "Enter");
	hint_select->setText(tr("Select"));
	hint_back = hints->AddHint(HintGlyph::B, "Esc");
	hint_back->setText(tr("Back"));
	hints->AddStretch();
	layout->addWidget(hints);
	connect(hint_select, &QAbstractButton::clicked, this, [this]() {
		if(auto button = qobject_cast<QAbstractButton *>(focusWidget()))
			button->animateClick();
	});
	connect(hint_back, &QAbstractButton::clicked, this, &QDialog::reject);
}

ModernDialog::~ModernDialog()
{
	delete dim_overlay;
}

void ModernDialog::SetText(const QString &text)
{
	text_label->setText(text);
	text_label->setVisible(!text.isEmpty());
}

void ModernDialog::SetTitle(const QString &title)
{
	setWindowTitle(title);
	title_label->setText(title);
}

void ModernDialog::SetBackHintText(const QString &text)
{
	hint_back->setText(text);
}

void ModernDialog::SetVerticalButtons(bool vertical)
{
	if(vertical == vertical_buttons || !buttons.isEmpty())
		return;
	vertical_buttons = vertical;
	delete button_layout;
	if(vertical)
		button_layout = new QVBoxLayout(button_container);
	else
		button_layout = new QHBoxLayout(button_container);
	button_layout->setContentsMargins(0, 0, 0, 0);
	button_layout->setSpacing(vertical ? 8 : 12);
}

QPushButton *ModernDialog::AddButton(const QString &text, ButtonRole role, const QString &icon)
{
	auto button = new QPushButton(text, button_container);
	button->setAutoDefault(false);
	button->setMinimumHeight(54);
	if(!icon.isEmpty())
	{
		QIcon qicon;
		qicon.addPixmap(Theme::RenderIcon(icon, 48, Theme::text, 1.0), QIcon::Normal);
		qicon.addPixmap(Theme::RenderIcon(icon, 48, QColor(0x5d, 0x6a, 0x82), 1.0), QIcon::Disabled);
		button->setIcon(qicon);
		button->setIconSize(QSize(24, 24));
	}

	QString focus_color = role == ButtonRole::Destructive ? "#ff5c6c" : "#2f8cff";
	button->setStyleSheet(QString(
		"QPushButton { font-size: 14pt; font-weight: bold; border-radius: 12px; border: 2px solid #2b3858;"
			" outline: none; padding: 8px 22px; %2 }"
		"QPushButton:focus { background-color: %1; border-color: %1; color: #ffffff; }"
		"QPushButton:disabled { color: #5d6a82; }")
		.arg(focus_color, vertical_buttons ? "text-align: left;" : "min-width: 150px;"));

	int index = buttons.count();
	buttons.append(button);
	if(vertical_buttons)
		button_layout->addWidget(button);
	else
	{
		// center the buttons
		if(index == 0)
		{
			button_layout->addStretch(1);
			button_layout->addWidget(button);
			button_layout->addStretch(1);
		}
		else
			button_layout->insertWidget(button_layout->count() - 1, button);
	}

	connect(button, &QPushButton::clicked, this, [this, index, role]() {
		clicked_index = index;
		switch(role)
		{
			case ButtonRole::Accept:
			case ButtonRole::Destructive:
				accept();
				break;
			case ButtonRole::Reject:
				reject();
				break;
			case ButtonRole::Other:
				done(QDialog::Accepted);
				break;
		}
	});

	if(!initial_focus)
		initial_focus = button;
	return button;
}

void ModernDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);

	// dim the window behind the dialog
	if(parentWidget() && !dim_overlay)
	{
		QWidget *window = parentWidget()->window();
		dim_overlay = new QWidget(window);
		dim_overlay->setAttribute(Qt::WA_StyledBackground);
		dim_overlay->setStyleSheet("background-color: rgba(0, 0, 0, 150);");
		dim_overlay->setGeometry(window->rect());
		dim_overlay->show();
		dim_overlay->raise();

		adjustSize();
		QPoint center = window->mapToGlobal(window->rect().center());
		move(center - QPoint(width() / 2, height() / 2));
	}

	// e.g. the stream window grabs the keyboard, which would make the dialog unusable
	QWidget *grabber = QWidget::keyboardGrabber();
	if(grabber && grabber != this)
	{
		keyboard_grabber = grabber;
		grabber->releaseKeyboard();
	}

	if(initial_focus)
		initial_focus->setFocus(Qt::OtherFocusReason);
	activateWindow();
	raise();
}

void ModernDialog::hideEvent(QHideEvent *event)
{
	delete dim_overlay;
	dim_overlay = nullptr;
	if(keyboard_grabber && keyboard_grabber->isVisible())
		keyboard_grabber->grabKeyboard();
	keyboard_grabber = nullptr;
	QDialog::hideEvent(event);
}

void ModernDialog::keyPressEvent(QKeyEvent *event)
{
	if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
	{
		// buttons are not auto default, Enter activates the focused control instead
		QWidget *focus = focusWidget();
		if(auto button = qobject_cast<QAbstractButton *>(focus))
			button->animateClick();
		else if(auto combo = qobject_cast<QComboBox *>(focus))
			combo->showPopup();
		else if(qobject_cast<QLineEdit *>(focus) && !buttons.isEmpty() && buttons.first()->isEnabled())
			buttons.first()->animateClick(); // confirm forms from text fields
		return;
	}
	QDialog::keyPressEvent(event);
}

void ModernDialog::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
	QLinearGradient bg(r.topLeft(), r.bottomLeft());
	bg.setColorAt(0.0, Theme::surface_light);
	bg.setColorAt(1.0, Theme::surface);
	painter.setBrush(bg);
	painter.setPen(QPen(Theme::accent, 2.0));
	painter.drawRoundedRect(r, DIALOG_RADIUS, DIALOG_RADIUS);
}

void ModernDialog::Message(QWidget *parent, Icon icon, const QString &title, const QString &text)
{
	ModernDialog dialog(title, parent, icon);
	dialog.SetText(text);
	dialog.AddButton(tr("OK"));
	dialog.exec();
}

int ModernDialog::Choice(QWidget *parent, Icon icon, const QString &title, const QString &text,
		const QStringList &button_texts, int default_index, int destructive_index)
{
	ModernDialog dialog(title, parent, icon, button_texts.count() > 2 ? 860 : 580);
	dialog.SetText(text);
	for(int i=0; i<button_texts.count(); i++)
	{
		auto button = dialog.AddButton(button_texts[i], i == destructive_index ? ButtonRole::Destructive : ButtonRole::Other);
		if(i == default_index)
			dialog.SetInitialFocus(button);
	}
	if(dialog.exec() != QDialog::Accepted)
		return -1;
	return dialog.GetClickedIndex();
}

bool ModernDialog::Confirm(QWidget *parent, const QString &title, const QString &text,
		const QString &confirm_text, const QString &cancel_text, bool destructive)
{
	ModernDialog dialog(title, parent, destructive ? Icon::Warning : Icon::Question);
	dialog.SetText(text);
	dialog.AddButton(confirm_text, destructive ? ButtonRole::Destructive : ButtonRole::Accept);
	auto cancel = dialog.AddButton(cancel_text, ButtonRole::Reject);
	// destructive actions start on the safe choice
	if(destructive)
		dialog.SetInitialFocus(cancel);
	return dialog.exec() == QDialog::Accepted;
}

int ModernDialog::Menu(QWidget *parent, const QString &title, const QString &subtitle, const QList<MenuEntry> &entries)
{
	ModernDialog dialog(title, parent, Icon::None, 520);
	dialog.SetText(subtitle);
	dialog.SetVerticalButtons(true);
	QPushButton *first_enabled = nullptr;
	for(const auto &entry : entries)
	{
		auto button = dialog.AddButton(entry.text, ButtonRole::Other, entry.icon);
		button->setEnabled(entry.enabled);
		if(entry.enabled && !first_enabled)
			first_enabled = button;
	}
	if(first_enabled)
		dialog.SetInitialFocus(first_enabled);
	if(dialog.exec() != QDialog::Accepted)
		return -1;
	return dialog.GetClickedIndex();
}
