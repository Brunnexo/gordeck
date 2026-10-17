// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <loginpindialog.h>
#include <modernwidgets.h>

#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>

#define PIN_LENGTH 4

LoginPINDialog::LoginPINDialog(bool incorrect, QWidget *parent)
	: ModernDialog(tr("Console Login PIN"), parent, incorrect ? Icon::Error : Icon::None, 540)
{
	SetText(incorrect ? tr("Entered PIN was incorrect!") : tr("Enter the login PIN of your PlayStation user."));

	pin_label = new QLabel(this);
	pin_label->setAlignment(Qt::AlignCenter);
	pin_label->setStyleSheet("font-size: 28pt; font-weight: bold; letter-spacing: 12px; color: #ecf1fa;"
			"background-color: #0c1322; border: 1px solid #2b3858; border-radius: 12px; padding: 8px;");
	GetContentLayout()->addWidget(pin_label);

	// number pad
	auto pad = new QWidget(this);
	ModernWidgets::SetGridNavigation(pad);
	auto grid = new QGridLayout(pad);
	grid->setContentsMargins(40, 6, 40, 0);
	grid->setSpacing(8);
	auto AddKey = [this, pad, grid](const QString &text, int row, int col, std::function<void()> action) {
		auto button = new QPushButton(text, pad);
		button->setAutoDefault(false);
		button->setMinimumHeight(52);
		button->setStyleSheet("QPushButton { font-size: 16pt; font-weight: bold; border-radius: 12px; outline: none; }"
				"QPushButton:focus { background-color: #2f8cff; border-color: #2f8cff; color: #ffffff; }");
		connect(button, &QPushButton::clicked, this, action);
		grid->addWidget(button, row, col);
		return button;
	};
	QPushButton *first = nullptr;
	for(int i=1; i<=9; i++)
	{
		auto b = AddKey(QString::number(i), (i - 1) / 3, (i - 1) % 3, [this, i]() {
			if(pin.length() < PIN_LENGTH)
				pin += QString::number(i);
			UpdatePIN();
		});
		if(i == 1)
			first = b;
	}
	AddKey(QString::fromUtf8("⌫"), 3, 0, [this]() {
		pin.chop(1);
		UpdatePIN();
	});
	AddKey("0", 3, 1, [this]() {
		if(pin.length() < PIN_LENGTH)
			pin += "0";
		UpdatePIN();
	});
	AddKey("C", 3, 2, [this]() {
		pin.clear();
		UpdatePIN();
	});
	GetContentLayout()->addWidget(pad);

	ok_button = AddButton(tr("OK"), ButtonRole::Accept);
	AddButton(tr("Cancel"), ButtonRole::Reject);
	SetInitialFocus(first);

	UpdatePIN();
}

void LoginPINDialog::UpdatePIN()
{
	QString display;
	for(int i=0; i<PIN_LENGTH; i++)
		display += i < pin.length() ? QString::fromUtf8("●") : QString::fromUtf8("○");
	pin_label->setText(display);
	ok_button->setEnabled(pin.length() == PIN_LENGTH);
	// jump to OK once the PIN is complete
	if(pin.length() == PIN_LENGTH)
		ok_button->setFocus();
}

void LoginPINDialog::keyPressEvent(QKeyEvent *event)
{
	// typing on a keyboard works as well
	QString text = event->text();
	if(text.length() == 1 && text[0].isDigit())
	{
		if(pin.length() < PIN_LENGTH)
			pin += text;
		UpdatePIN();
		return;
	}
	if(event->key() == Qt::Key_Backspace)
	{
		pin.chop(1);
		UpdatePIN();
		return;
	}
	ModernDialog::keyPressEvent(event);
}
