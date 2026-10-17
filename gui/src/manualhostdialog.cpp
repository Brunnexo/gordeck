// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <manualhostdialog.h>
#include <modernwidgets.h>
#include <host.h>
#include <settings.h>

#include <QComboBox>
#include <QFrame>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ManualHostDialog::ManualHostDialog(Settings *settings, int id, QWidget *parent)
	: ModernDialog(id >= 0 ? tr("Edit Manual Console") : tr("Add Manual Console"), parent, Icon::None, 760)
{
	this->settings = settings;
	host_id = id;

	SetText(tr("Enter the IP address or host name of the console."));

	ManualHost host;
	if(id >= 0 && settings->GetManualHostExists(id))
		host = settings->GetManualHost(id);

	host_edit = new QLineEdit(this);
	host_edit->setText(host.GetHost());
	host_edit->setPlaceholderText("192.168.0.10");
	connect(host_edit, &QLineEdit::textChanged, this, &ManualHostDialog::ValidateInput);
	GetContentLayout()->addWidget(ModernWidgets::CreateRow(this, tr("Host"), QString(), host_edit, 320));

	registered_host_combo_box = new QComboBox(this);
	registered_host_combo_box->addItem(tr("Register on first Connection"));
	auto registered_hosts = settings->GetRegisteredHosts();
	for(const auto &registered_host : registered_hosts)
		registered_host_combo_box->addItem(QString("%1 (%2)").arg(registered_host.GetServerNickname(), registered_host.GetServerMAC().ToString()), QVariant::fromValue(registered_host.GetServerMAC()));
	if(host.GetRegistered())
	{
		for(int i=1; i<registered_host_combo_box->count(); i++)
		{
			if(registered_host_combo_box->itemData(i).value<HostMAC>() == host.GetMAC())
			{
				registered_host_combo_box->setCurrentIndex(i);
				break;
			}
		}
	}
	GetContentLayout()->addWidget(ModernWidgets::CreateRow(this, tr("Registered Console"),
				tr("Use the registration of a console that was already registered."), registered_host_combo_box, 320));

	save_button = AddButton(tr("Save"), ButtonRole::Accept);
	AddButton(tr("Cancel"), ButtonRole::Reject);
	SetInitialFocus(host_edit);

	ValidateInput();
}

void ManualHostDialog::ValidateInput()
{
	save_button->setEnabled(!host_edit->text().trimmed().isEmpty());
}

void ManualHostDialog::accept()
{
	bool registered = false;
	HostMAC registered_mac;
	QVariant registered_host_data = registered_host_combo_box->currentData();
	if(registered_host_data.isValid())
	{
		registered = true;
		registered_mac = registered_host_data.value<HostMAC>();
	}

	ManualHost host(host_id, host_edit->text().trimmed(), registered, registered_mac);
	settings->SetManualHost(host);
	QDialog::accept();
}
