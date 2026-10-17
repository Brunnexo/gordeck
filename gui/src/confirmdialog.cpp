// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <confirmdialog.h>
#include <theme.h>

#include <QDialogButtonBox>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#define DIALOG_WIDTH 560
#define DIALOG_RADIUS 22.0

ConfirmDialog::ConfirmDialog(const QString &title, const QString &text,
		const QString &confirm_text, const QString &cancel_text,
		HintStyle style, bool destructive, QWidget *parent)
	: QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
	setAttribute(Qt::WA_TranslucentBackground);
	setWindowTitle(title);
	setFixedWidth(DIALOG_WIDTH);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(36, 30, 36, 18);
	layout->setSpacing(14);

	auto title_label = new QLabel(title, this);
	title_label->setAlignment(Qt::AlignCenter);
	title_label->setStyleSheet("font-size: 18pt; font-weight: bold; color: #ecf1fa;");
	layout->addWidget(title_label);

	auto text_label = new QLabel(text, this);
	text_label->setAlignment(Qt::AlignCenter);
	text_label->setWordWrap(true);
	text_label->setStyleSheet("font-size: 13pt; color: #8e9bb4;");
	layout->addWidget(text_label);

	layout->addSpacing(10);

	auto button_box = new QDialogButtonBox(Qt::Horizontal, this);
	button_box->setCenterButtons(true);
	confirm_button = button_box->addButton(confirm_text, QDialogButtonBox::AcceptRole);
	cancel_button = button_box->addButton(cancel_text, QDialogButtonBox::RejectRole);
	QString focus_color = destructive ? "#ff5c6c" : "#2f8cff";
	for(auto button : { confirm_button, cancel_button })
	{
		button->setMinimumSize(190, 54);
		button->setStyleSheet(QString(
			"QPushButton { font-size: 14pt; font-weight: bold; border-radius: 12px; border: 2px solid #2b3858; outline: none; }"
			"QPushButton:focus { background-color: %1; border-color: %1; color: #ffffff; }").arg(focus_color));
	}
	layout->addWidget(button_box);
	connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	layout->addSpacing(4);

	// button hints, like on the main screen
	auto hints = new HintBar(this);
	hints->AddStretch();
	auto hint_select = hints->AddHint(HintGlyph::A, "Enter");
	hint_select->setText(tr("Select"));
	auto hint_back = hints->AddHint(HintGlyph::B, "Esc");
	hint_back->setText(tr("Back"));
	hints->AddStretch();
	hints->SetStyle(style);
	layout->addWidget(hints);
	connect(hint_select, &QAbstractButton::clicked, this, [this]() {
		if(cancel_button->hasFocus())
			reject();
		else
			accept();
	});
	connect(hint_back, &QAbstractButton::clicked, this, &QDialog::reject);

	// destructive actions start on the safe choice
	(destructive ? cancel_button : confirm_button)->setFocus();
	(destructive ? cancel_button : confirm_button)->setDefault(true);
}

void ConfirmDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	if(parentWidget())
	{
		QWidget *window = parentWidget()->window();
		QPoint center = window->mapToGlobal(window->rect().center());
		move(center - QPoint(width() / 2, height() / 2));
	}
}

void ConfirmDialog::paintEvent(QPaintEvent *)
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

bool ConfirmDialog::Ask(QWidget *parent, HintStyle style, const QString &title, const QString &text,
		const QString &confirm_text, const QString &cancel_text, bool destructive)
{
	// dim the window behind the dialog
	QWidget *dim = nullptr;
	if(parent)
	{
		QWidget *window = parent->window();
		dim = new QWidget(window);
		dim->setAttribute(Qt::WA_StyledBackground);
		dim->setStyleSheet("background-color: rgba(0, 0, 0, 150);");
		dim->setGeometry(window->rect());
		dim->show();
		dim->raise();
	}

	ConfirmDialog dialog(title, text, confirm_text, cancel_text, style, destructive, parent);
	bool confirmed = dialog.exec() == QDialog::Accepted;

	delete dim;
	return confirmed;
}
