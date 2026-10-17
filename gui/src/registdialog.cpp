// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <registdialog.h>
#include <modernwidgets.h>
#include <settings.h>

#include <QComboBox>
#include <QFontDatabase>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QVBoxLayout>

Q_DECLARE_METATYPE(ChiakiLogLevel)

#define PIN_LENGTH 8

static const QRegularExpression pin_re(QString("[0-9]").repeated(PIN_LENGTH));

RegistDialog::RegistDialog(Settings *settings, const QString &host, QWidget *parent)
	: ModernDialog(tr("Register Console"), parent, Icon::None, 900),
	settings(settings)
{
	SetText(tr("On the console, open Settings › System › Remote Play › Link Device to see the PIN."));

	auto content = GetContentLayout();
	const int control_width = 340;

	target_combo_box = new QComboBox(this);
	target_combo_box->addItem(tr("PS5"), (int)CHIAKI_TARGET_PS5_1);
	target_combo_box->addItem(tr("PS4 Firmware >= 8.0"), (int)CHIAKI_TARGET_PS4_10);
	target_combo_box->addItem(tr("PS4 Firmware >= 7.0, < 8.0"), (int)CHIAKI_TARGET_PS4_9);
	target_combo_box->addItem(tr("PS4 Firmware < 7.0"), (int)CHIAKI_TARGET_PS4_8);
	content->addWidget(ModernWidgets::CreateRow(this, tr("Console"), QString(), target_combo_box, control_width));

	host_edit = new QLineEdit(this);
	host_edit->setText(host.isEmpty() ? "255.255.255.255" : host);
	content->addWidget(ModernWidgets::CreateRow(this, tr("Host"), QString(), host_edit, control_width));

	broadcast_toggle = new ToggleSwitch(this);
	broadcast_toggle->setChecked(host.isEmpty());
	content->addWidget(ModernWidgets::CreateRow(this, tr("Broadcast"),
				tr("Search for the console on the whole network."), broadcast_toggle));

	psn_online_id_edit = new QLineEdit(this);
	content->addWidget(ModernWidgets::CreateRow(this, tr("PSN Online-ID"),
				tr("Only for PS4 firmware < 7.0. Case-sensitive."), psn_online_id_edit, control_width));

	psn_account_id_edit = new QLineEdit(this);
	content->addWidget(ModernWidgets::CreateRow(this, tr("PSN Account-ID"),
				tr("In base64, see the psn-account-id tool."), psn_account_id_edit, control_width));

	pin_edit = new QLineEdit(this);
	pin_edit->setValidator(new QRegularExpressionValidator(pin_re, pin_edit));
	pin_edit->setPlaceholderText("12345678");
	content->addWidget(ModernWidgets::CreateRow(this, tr("PIN"), tr("8 digits shown on the console."), pin_edit, control_width));

	auto UpdatePSNIDEdits = [this]() {
		bool need_account_id = NeedAccountId();
		psn_online_id_edit->setEnabled(!need_account_id);
		psn_account_id_edit->setEnabled(need_account_id);
		ValidateInput();
	};
	connect(target_combo_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, UpdatePSNIDEdits);

	register_button = AddButton(tr("Register"), ButtonRole::Accept);
	AddButton(tr("Cancel"), ButtonRole::Reject);
	SetInitialFocus(target_combo_box);

	connect(host_edit, &QLineEdit::textChanged, this, &RegistDialog::ValidateInput);
	connect(psn_online_id_edit, &QLineEdit::textChanged, this, &RegistDialog::ValidateInput);
	connect(psn_account_id_edit, &QLineEdit::textChanged, this, &RegistDialog::ValidateInput);
	connect(pin_edit, &QLineEdit::textChanged, this, &RegistDialog::ValidateInput);
	UpdatePSNIDEdits();
}

RegistDialog::~RegistDialog()
{
}

bool RegistDialog::NeedAccountId()
{
	return target_combo_box->currentData().toInt() != (int)CHIAKI_TARGET_PS4_8;
}

void RegistDialog::ValidateInput()
{
	bool need_account_id = NeedAccountId();
	bool valid = !host_edit->text().trimmed().isEmpty()
				 && !(!need_account_id && psn_online_id_edit->text().trimmed().isEmpty())
				 && !(need_account_id && psn_account_id_edit->text().trimmed().isEmpty())
				 && pin_edit->text().length() == PIN_LENGTH;
	register_button->setEnabled(valid);
}

void RegistDialog::accept()
{
	ChiakiRegistInfo info = {};
	QByteArray host = host_edit->text().trimmed().toUtf8();
	info.host = host.constData();

	info.target = (ChiakiTarget)target_combo_box->currentData().toInt();

	bool need_account_id = NeedAccountId();
	QByteArray psn_id; // keep this out of the if scope
	if(!need_account_id)
	{
		psn_id = psn_online_id_edit->text().trimmed().toUtf8();
		info.psn_online_id = psn_id.constData();
	}
	else
	{
		QString account_id_b64 = psn_account_id_edit->text().trimmed();
		QByteArray account_id = QByteArray::fromBase64(account_id_b64.toUtf8());
		if(account_id.size() != CHIAKI_PSN_ACCOUNT_ID_SIZE)
		{
			ModernDialog::Message(this, Icon::Error, tr("Invalid Account-ID"), tr("The PSN Account-ID must be exactly %1 bytes encoded as base64.").arg(CHIAKI_PSN_ACCOUNT_ID_SIZE));
			return;
		}
		info.psn_online_id = nullptr;
		memcpy(info.psn_account_id, account_id.constData(), CHIAKI_PSN_ACCOUNT_ID_SIZE);
	}

	info.broadcast = broadcast_toggle->isChecked();
	info.pin = (uint32_t)pin_edit->text().toULong();

	RegistExecuteDialog execute_dialog(settings, info, this);
	int r = execute_dialog.exec();
	if(r == QDialog::Accepted)
	{
		this->registered_host = execute_dialog.GetRegisteredHost();
		QDialog::accept();
	}
}

static void RegistExecuteDialogLogCb(ChiakiLogLevel level, const char *msg, void *user);
static void RegistExecuteDialogRegistCb(ChiakiRegistEvent *event, void *user);

RegistExecuteDialog::RegistExecuteDialog(Settings *settings, const ChiakiRegistInfo &regist_info, QWidget *parent)
	: ModernDialog(tr("Register Console"), parent, Icon::None, 760),
	finished(false)
{
	this->settings = settings;

	SetText(tr("Registering, this can take a few seconds…"));

	log_edit = new QPlainTextEdit(this);
	log_edit->setReadOnly(true);
	log_edit->setFocusPolicy(Qt::NoFocus);
	log_edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	log_edit->setMinimumHeight(260);
	GetContentLayout()->addWidget(log_edit);

	cancel_button = AddButton(tr("Cancel"), ButtonRole::Reject);

	chiaki_log_init(&log, settings->GetLogLevelMask(), RegistExecuteDialogLogCb, this);
	chiaki_regist_start(&regist, &log, &regist_info, RegistExecuteDialogRegistCb, this);
}

RegistExecuteDialog::~RegistExecuteDialog()
{
	chiaki_regist_stop(&regist);
	chiaki_regist_fini(&regist);
}

void RegistExecuteDialog::Log(ChiakiLogLevel level, QString msg)
{
	log_edit->appendPlainText(QString("[%1] %2").arg(chiaki_log_level_char(level)).arg(msg));
	log_edit->verticalScrollBar()->setValue(log_edit->verticalScrollBar()->maximum());
}

void RegistExecuteDialog::Finished()
{
	if(finished)
		return;
	finished = true;
	SetText(tr("Registration failed. Check the log below, the PIN and the console's address."));
	cancel_button->setText(tr("Close"));
}

void RegistExecuteDialog::Success(RegisteredHost host)
{
	CHIAKI_LOGI(&log, "Successfully registered %s", host.GetServerNickname().toLocal8Bit().constData());
	this->registered_host = host;

	if(settings->GetRegisteredHostRegistered(host.GetServerMAC()))
	{
		bool overwrite = ModernDialog::Confirm(this,
				tr("Console already registered"),
				tr("The console with ID %1 has already been registered. Should the previous record be overwritten?").arg(host.GetServerMAC().ToString()),
				tr("Overwrite"), tr("Keep Previous"));
		if(!overwrite)
		{
			accept();
			return;
		}
	}

	settings->AddRegisteredHost(host);

	ModernDialog::Message(this, Icon::Success, tr("Console registered"), tr("The Console %1 with ID %2 has been successfully registered!").arg(host.GetServerNickname(), host.GetServerMAC().ToString()));

	accept();
}

void RegistExecuteDialog::Failed()
{
	Finished();
}


class RegistExecuteDialogPrivate
{
	public:
		static void Log(RegistExecuteDialog *dialog, ChiakiLogLevel level, QString msg)
			{ QMetaObject::invokeMethod(dialog, "Log", Qt::ConnectionType::QueuedConnection, Q_ARG(ChiakiLogLevel, level), Q_ARG(QString, msg)); }

		static void RegistEvent(RegistExecuteDialog *dialog, ChiakiRegistEvent *event)
		{
			switch(event->type)
			{
				case CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS:
					QMetaObject::invokeMethod(dialog, "Success", Qt::ConnectionType::QueuedConnection, Q_ARG(RegisteredHost, *event->registered_host));
					break;
				case CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED:
					QMetaObject::invokeMethod(dialog, "Failed", Qt::ConnectionType::QueuedConnection);
					break;
				default:
					break;
			}
		}
};

static void RegistExecuteDialogLogCb(ChiakiLogLevel level, const char *msg, void *user)
{
	chiaki_log_cb_print(level, msg, nullptr);
	auto dialog = reinterpret_cast<RegistExecuteDialog *>(user);
	RegistExecuteDialogPrivate::Log(dialog, level, msg);
}

static void RegistExecuteDialogRegistCb(ChiakiRegistEvent *event, void *user)
{
	auto dialog = reinterpret_cast<RegistExecuteDialog *>(user);
	RegistExecuteDialogPrivate::RegistEvent(dialog, event);
}
