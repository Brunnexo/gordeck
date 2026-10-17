// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_LOGINPINDIALOG_H
#define CHIAKI_LOGINPINDIALOG_H

#include "moderndialog.h"

class QLabel;
class QPushButton;

/**
 * Asks for the console's 4 digit login PIN with an on-screen number pad,
 * so it can be entered with a gamepad as well.
 */
class LoginPINDialog : public ModernDialog
{
	Q_OBJECT

	private:
		QString pin;

		QLabel *pin_label;
		QPushButton *ok_button;

		void UpdatePIN();

	protected:
		void keyPressEvent(QKeyEvent *event) override;

	public:
		explicit LoginPINDialog(bool incorrect, QWidget *parent = nullptr);

		QString GetPIN()	{ return pin; }
};

#endif // CHIAKI_LOGINPINDIALOG_H
