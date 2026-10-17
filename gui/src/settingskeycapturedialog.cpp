// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include "settingskeycapturedialog.h"

#include <QKeyEvent>
#include <QPushButton>

SettingsKeyCaptureDialog::SettingsKeyCaptureDialog(QWidget *parent)
	: ModernDialog(tr("Key Capture"), parent, Icon::Info)
{
	SetText(tr("Press any key on the keyboard to configure the button."));
	auto button = AddButton(tr("Cancel"), ButtonRole::Reject);
	// keyboard keys must not activate the button, every key is captured
	button->setFocusPolicy(Qt::NoFocus);
}

void SettingsKeyCaptureDialog::keyPressEvent(QKeyEvent *event)
{
	// handled on release, don't let the dialog close on Escape or Enter
	event->accept();
}

void SettingsKeyCaptureDialog::keyReleaseEvent(QKeyEvent *event)
{
	if(event->isAutoRepeat())
		return;
	emit KeyCaptured(Qt::Key(event->key()));
	accept();
}
