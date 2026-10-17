// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_REGISTDIALOG_H
#define CHIAKI_REGISTDIALOG_H

#include <chiaki/regist.h>

#include "host.h"

#include "moderndialog.h"

class Settings;

class QAbstractButton;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class RegistDialog : public ModernDialog
{
	Q_OBJECT

	private:
		Settings *settings;

		QLineEdit *host_edit;
		QAbstractButton *broadcast_toggle;
		QComboBox *target_combo_box;
		QLineEdit *psn_online_id_edit;
		QLineEdit *psn_account_id_edit;
		QLineEdit *pin_edit;
		QPushButton *register_button;

		RegisteredHost registered_host;

		bool NeedAccountId();

	private slots:
		void ValidateInput();

	public:
		explicit RegistDialog(Settings *settings, const QString &host = QString(), QWidget *parent = nullptr);
		~RegistDialog();

		RegisteredHost GetRegisteredHost() { return registered_host; }

	public slots:
		void accept() override;
};

class RegistExecuteDialog: public ModernDialog
{
	Q_OBJECT

	friend class RegistExecuteDialogPrivate;

	private:
		Settings *settings;

		ChiakiLog log;
		ChiakiRegist regist;

		QPlainTextEdit *log_edit;
		QPushButton *cancel_button;
		bool finished;

		RegisteredHost registered_host;

		void Finished();

	private slots:
		void Log(ChiakiLogLevel level, QString msg);
		void Success(RegisteredHost host);
		void Failed();

	public:
		explicit RegistExecuteDialog(Settings *settings, const ChiakiRegistInfo &regist_info, QWidget *parent = nullptr);
		~RegistExecuteDialog();

		RegisteredHost GetRegisteredHost()	{ return registered_host; }
};

Q_DECLARE_METATYPE(ChiakiRegistEventType)

#endif // CHIAKI_REGISTDIALOG_H
