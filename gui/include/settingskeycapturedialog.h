// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_SETTINGSKEYCAPTUREDIALOG_H
#define CHIAKI_SETTINGSKEYCAPTUREDIALOG_H

#include "moderndialog.h"

class SettingsKeyCaptureDialog : public ModernDialog
{
	Q_OBJECT

	signals:
		void KeyCaptured(Qt::Key);

	protected:
		void keyPressEvent(QKeyEvent *event) override;
		void keyReleaseEvent(QKeyEvent *event) override;

	public:
		explicit SettingsKeyCaptureDialog(QWidget *parent = nullptr);
};

#endif // CHIAKI_SETTINGSKEYCAPTUREDIALOG_H
