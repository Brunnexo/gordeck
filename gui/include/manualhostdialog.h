// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_MANUALHOSTDIALOG_H
#define CHIAKI_MANUALHOSTDIALOG_H

#include "moderndialog.h"

class Settings;
class QLineEdit;
class QComboBox;
class QPushButton;

class ManualHostDialog: public ModernDialog
{
	Q_OBJECT

	private:
		Settings *settings;
		int host_id;

		QLineEdit *host_edit;
		QComboBox *registered_host_combo_box;
		QPushButton *save_button;

	private slots:
		void ValidateInput();

	public:
		explicit ManualHostDialog(Settings *settings, int id, QWidget *parent = nullptr);

	public slots:
		void accept() override;
};

#endif // CHIAKI_MANUALHOSTDIALOG_H
