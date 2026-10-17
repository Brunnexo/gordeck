// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <mainwindow.h>
#include <consolecarousel.h>
#include <moderndialog.h>
#include <hintbar.h>
#include <i18n.h>
#include <settings.h>
#include <registdialog.h>
#include <settingsdialog.h>
#include <streamsession.h>
#include <streamwindow.h>
#include <manualhostdialog.h>
#include <theme.h>

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <functional>
#include <QPainter>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>

#define TOAST_DURATION_MS 2800
#define CLOCK_INTERVAL_MS 10000
#define PENDING_CONNECT_TIMEOUT_MS 90000


/**
 * Full window background with a subtle gradient and glow
 */
class BackgroundWidget : public QWidget
{
	public:
		explicit BackgroundWidget(QWidget *parent = nullptr) : QWidget(parent) {}

	protected:
		void paintEvent(QPaintEvent *) override
		{
			QPainter painter(this);
			painter.setRenderHint(QPainter::Antialiasing);
			QRectF r = rect();

			QLinearGradient base(r.topLeft(), r.bottomLeft());
			base.setColorAt(0.0, Theme::background_light);
			base.setColorAt(0.55, Theme::background);
			base.setColorAt(1.0, QColor(0x04, 0x07, 0x0f));
			painter.fillRect(r, base);

			QRadialGradient glow(QPointF(r.center().x(), r.height() * 0.42), r.width() * 0.55);
			QColor c = Theme::accent;
			c.setAlpha(46);
			glow.setColorAt(0.0, c);
			c.setAlpha(0);
			glow.setColorAt(1.0, c);
			painter.fillRect(r, glow);
		}
};

MainWindow::MainWindow(Settings *settings, QWidget *parent)
	: QMainWindow(parent),
	settings(settings),
	gamepad(nullptr),
	pending_connect(false),
	title_label(nullptr),
	toast_label(nullptr),
	hint_style(HintStyle::Keyboard)
{
	setWindowTitle(QGuiApplication::applicationDisplayName());

	auto background = new BackgroundWidget(this);
	setCentralWidget(background);

	auto root_layout = new QVBoxLayout(background);
	root_layout->setContentsMargins(36, 22, 36, 20);
	root_layout->setSpacing(10);

	// Header

	auto header_layout = new QHBoxLayout();
	header_layout->setSpacing(10);
	root_layout->addLayout(header_layout);

	auto logo_label = new QLabel(this);
	logo_label->setPixmap(Theme::RenderIcon(":/icons/gordeck.svg", 44, QColor(), devicePixelRatioF()));
	header_layout->addWidget(logo_label);

	title_label = new QLabel(this);
	title_label->setStyleSheet("font-size: 21pt; font-weight: bold; color: #ecf1fa;");
	header_layout->addWidget(title_label);

	header_layout->addStretch(1);

	auto AddStatusIcon = [this, header_layout]() {
		auto label = new QLabel(this);
		header_layout->addWidget(label);
		return label;
	};
	auto AddStatusText = [this, header_layout]() {
		auto label = new QLabel(this);
		label->setStyleSheet("font-size: 11pt; color: #8e9bb4;");
		header_layout->addWidget(label);
		header_layout->addSpacing(18);
		return label;
	};

	controller_icon_label = AddStatusIcon();
	controller_label = AddStatusText();
	discovery_icon_label = AddStatusIcon();
	discovery_label = AddStatusText();

	clock_label = new QLabel(this);
	clock_label->setStyleSheet("font-size: 17pt; font-weight: bold; color: #ecf1fa;");
	header_layout->addWidget(clock_label);

	// Carousel

	root_layout->addSpacing(8);

	section_label = new QLabel(this);
	section_label->setAlignment(Qt::AlignCenter);
	section_label->setStyleSheet("font-size: 12pt; font-weight: bold; letter-spacing: 3px; color: #6cb4ff;");
	root_layout->addWidget(section_label);

	carousel = new ConsoleCarousel(this);
	root_layout->addWidget(carousel, 1);
	connect(carousel, &ConsoleCarousel::CurrentIndexChanged, this, [this]() {
		UpdateDetails();
		UpdateHints();
	});
	connect(carousel, &ConsoleCarousel::Activated, this, &MainWindow::ActivateCurrent);
	connect(carousel, &ConsoleCarousel::ContextMenuRequested, this, &MainWindow::ShowContextMenu);

	details_label = new QLabel(this);
	details_label->setAlignment(Qt::AlignCenter);
	details_label->setWordWrap(true);
	details_label->setTextFormat(Qt::RichText);
	details_label->setStyleSheet("font-size: 12pt; color: #8e9bb4;");
	details_label->setMinimumHeight(52);
	root_layout->addWidget(details_label);

	root_layout->addSpacing(6);

	// Hints

	hint_bar = new HintBar(this);
	root_layout->addWidget(hint_bar);

	hint_discovery = hint_bar->AddHint(HintGlyph::Select, "F3");
	hint_settings = hint_bar->AddHint(HintGlyph::Start, "F1");
	hint_bar->AddStretch();
	hint_options = hint_bar->AddHint(HintGlyph::Y, "O");
	hint_wake = hint_bar->AddHint(HintGlyph::X, "W");
	hint_quit = hint_bar->AddHint(HintGlyph::B, "Esc");
	hint_activate = hint_bar->AddHint(HintGlyph::A, "Enter");

	connect(hint_discovery, &QAbstractButton::clicked, this, &MainWindow::ToggleDiscovery);
	connect(hint_settings, &QAbstractButton::clicked, this, &MainWindow::ShowSettings);
	connect(hint_options, &QAbstractButton::clicked, this, &MainWindow::ShowOptionsMenu);
	connect(hint_wake, &QAbstractButton::clicked, this, &MainWindow::WakeCurrent);
	connect(hint_quit, &QAbstractButton::clicked, this, &MainWindow::RequestQuit);
	connect(hint_activate, &QAbstractButton::clicked, this, &MainWindow::ActivateCurrent);

	// Toast for short notifications

	toast_label = new QLabel(background);
	toast_label->setAlignment(Qt::AlignCenter);
	toast_label->setStyleSheet("background-color: rgba(21, 29, 48, 235); color: #ecf1fa; font-size: 13pt;"
			"border: 1px solid #2f8cff; border-radius: 14px; padding: 12px 26px;");
	toast_label->hide();
	auto toast_effect = new QGraphicsOpacityEffect(toast_label);
	toast_label->setGraphicsEffect(toast_effect);
	toast_timer = new QTimer(this);
	toast_timer->setSingleShot(true);
	connect(toast_timer, &QTimer::timeout, this, [this, toast_effect]() {
		auto anim = new QPropertyAnimation(toast_effect, "opacity", this);
		anim->setDuration(350);
		anim->setStartValue(1.0);
		anim->setEndValue(0.0);
		connect(anim, &QPropertyAnimation::finished, toast_label, &QLabel::hide);
		anim->start(QAbstractAnimation::DeleteWhenStopped);
	});

	// Shortcuts without hints

	auto quit_action = new QAction(this);
	quit_action->setShortcut(Qt::CTRL + Qt::Key_Q);
	addAction(quit_action);
	connect(quit_action, &QAction::triggered, this, &MainWindow::Quit);

	auto fullscreen_action = new QAction(this);
	fullscreen_action->setShortcut(Qt::Key_F11);
	addAction(fullscreen_action);
	connect(fullscreen_action, &QAction::triggered, this, &MainWindow::ToggleFullscreen);

	pending_connect_timer = new QTimer(this);
	pending_connect_timer->setSingleShot(true);
	connect(pending_connect_timer, &QTimer::timeout, this, [this]() {
		if(!pending_connect)
			return;
		pending_connect = false;
		ShowToast(tr("The console did not turn on in time. Please try again."));
	});

	auto clock_timer = new QTimer(this);
	connect(clock_timer, &QTimer::timeout, this, &MainWindow::UpdateClock);
	clock_timer->start(CLOCK_INTERVAL_MS);

	gamepad = new GamepadNavigator(this);
	connect(gamepad, &GamepadNavigator::ButtonPressed, this, &MainWindow::GamepadButtonPressed);
	connect(gamepad, &GamepadNavigator::ControllersChanged, this, &MainWindow::UpdateStatus);

	resize(1280, 800);
	setMinimumSize(800, 500);

	connect(&discovery_manager, &DiscoveryManager::HostsUpdated, this, &MainWindow::UpdateDisplayServers);
	connect(settings, &Settings::RegisteredHostsUpdated, this, &MainWindow::UpdateDisplayServers);
	connect(settings, &Settings::ManualHostsUpdated, this, &MainWindow::UpdateDisplayServers);

	Retranslate();
	UpdateDisplayServers();
	discovery_manager.SetActive(settings->GetDiscoveryEnabled());
	UpdateStatus();
	UpdateClock();
	carousel->setFocus();
}

MainWindow::~MainWindow()
{
}

void MainWindow::Retranslate()
{
	title_label->setText("Gordeck");
	section_label->setText(tr("YOUR CONSOLES"));
	hint_discovery->setText(tr("Auto Search"));
	hint_settings->setText(tr("Settings"));
	hint_options->setText(tr("Options"));
	hint_wake->setText(tr("Wake Up"));
	hint_quit->setText(tr("Quit"));
	UpdateEntries();
	UpdateStatus();
	UpdateClock();
}

void MainWindow::changeEvent(QEvent *event)
{
	if(event->type() == QEvent::LanguageChange && title_label)
		Retranslate();
	QMainWindow::changeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
	QMainWindow::resizeEvent(event);
	if(toast_label && toast_label->isVisible())
	{
		toast_label->adjustSize();
		toast_label->move((centralWidget()->width() - toast_label->width()) / 2, 90);
	}
}

DisplayServer *MainWindow::CurrentServer()
{
	int index = carousel->GetCurrentIndex();
	if(index < 0 || index >= display_servers.count())
		return nullptr;
	return &display_servers[index];
}

bool MainWindow::CurrentIsAddEntry()
{
	return carousel->GetCurrentIndex() == display_servers.count();
}

void MainWindow::UpdateDisplayServers()
{
	display_servers.clear();

	for(const auto &host : discovery_manager.GetHosts())
	{
		DisplayServer server;
		server.discovered = true;
		server.discovery_host = host;

		server.registered = settings->GetRegisteredHostRegistered(host.GetHostMAC());
		if(server.registered)
			server.registered_host = settings->GetRegisteredHost(host.GetHostMAC());

		display_servers.append(server);
	}

	for(const auto &host : settings->GetManualHosts())
	{
		DisplayServer server;
		server.discovered = false;
		server.manual_host = host;

		server.registered = false;
		if(host.GetRegistered() && settings->GetRegisteredHostRegistered(host.GetMAC()))
		{
			server.registered = true;
			server.registered_host = settings->GetRegisteredHost(host.GetMAC());
		}

		display_servers.append(server);
	}

	UpdateEntries();
	CheckPendingConnect();
}

void MainWindow::CheckPendingConnect()
{
	if(!pending_connect || stream_window)
		return;
	for(const auto &server : display_servers)
	{
		if(!server.discovered || !server.registered
				|| !(server.discovery_host.GetHostMAC() == pending_connect_mac))
			continue;
		if(server.discovery_host.state != CHIAKI_DISCOVERY_HOST_STATE_READY)
			return;
		pending_connect = false;
		pending_connect_timer->stop();
		// start from the event loop, not from within the discovery update
		QTimer::singleShot(0, this, [this, server]() {
			StartStream(server);
		});
		return;
	}
}

void MainWindow::UpdateEntries()
{
	QList<CarouselEntry> entries;
	for(const auto &server : display_servers)
	{
		CarouselEntry entry;
		entry.type = CarouselEntry::Type::Console;
		entry.key = server.discovered
			? "discovered:" + server.discovery_host.GetHostMAC().ToString()
			: "manual:" + QString::number(server.manual_host.GetID());
		if(server.discovered)
			entry.title = server.discovery_host.host_name;
		else if(server.registered)
			entry.title = server.registered_host.GetServerNickname();
		if(entry.title.isEmpty())
			entry.title = server.GetHostAddr();
		entry.subtitle = server.registered ? tr("Registered") : tr("Not registered");
		entry.ps5 = server.IsPS5();
		entry.discovered = server.discovered;
		entry.state = server.discovered ? server.discovery_host.state : CHIAKI_DISCOVERY_HOST_STATE_UNKNOWN;
		entries.append(entry);
	}

	CarouselEntry add_entry;
	add_entry.type = CarouselEntry::Type::AddConsole;
	add_entry.key = "add";
	add_entry.title = tr("Add Console");
	add_entry.subtitle = tr("Connect using IP address");
	entries.append(add_entry);

	carousel->SetEntries(entries);
	UpdateDetails();
	UpdateHints();
}

void MainWindow::UpdateDetails()
{
	auto server = CurrentServer();
	if(!server)
	{
		QString text = tr("Add a console that is not found automatically by entering its IP address or host name.");
		if(display_servers.isEmpty())
		{
			text = (settings->GetDiscoveryEnabled()
				? tr("Searching for consoles on your network…")
				: tr("Automatic search is disabled."))
				+ "<br>" + text;
		}
		details_label->setText(text);
		return;
	}

	auto Item = [](const QString &label, const QString &value) {
		return QString("<span style=\"color:#8e9bb4;\">%1</span> <span style=\"color:#ecf1fa;\">%2</span>")
			.arg(label.toHtmlEscaped(), value.toHtmlEscaped());
	};
	const QString sep = "&nbsp;&nbsp;&nbsp;•&nbsp;&nbsp;&nbsp;";

	QStringList first_line;
	first_line.append(Item(tr("Address:"), server->GetHostAddr()));
	if(server->discovered || server->registered)
	{
		QString mac = server->discovered
			? server->discovery_host.GetHostMAC().ToString()
			: server->registered_host.GetServerMAC().ToString();
		first_line.append(Item(tr("ID:"), mac));
	}
	first_line.append(Item(tr("Source:"), server->discovered ? tr("found on network") : tr("added manually")));

	QString text = first_line.join(sep);

	if(server->discovered)
	{
		QStringList second_line;
		second_line.append(Item(tr("State:"), I18n::DiscoveryHostStateString(server->discovery_host.state)));
		if(!server->discovery_host.running_app_name.isEmpty())
			second_line.append(Item(tr("Running:"), server->discovery_host.running_app_name));
		if(!server->discovery_host.running_app_titleid.isEmpty())
			second_line.append(Item(tr("Title ID:"), server->discovery_host.running_app_titleid));
		text += "<br>" + second_line.join(sep);
	}

	details_label->setText(text);
}

void MainWindow::UpdateHints()
{
	auto server = CurrentServer();
	if(!server)
		hint_activate->setText(tr("Add"));
	else
	{
		if(!server->registered)
			hint_activate->setText(tr("Register"));
		else if(server->discovered && server->discovery_host.state == CHIAKI_DISCOVERY_HOST_STATE_STANDBY)
			hint_activate->setText(tr("Turn On & Connect"));
		else
			hint_activate->setText(tr("Connect"));
	}

	hint_wake->setVisible(server && server->registered);
	hint_options->setVisible(server != nullptr);
}

void MainWindow::UpdateStatus()
{
	qreal dpr = devicePixelRatioF();
	bool gamepad_connected = gamepad && gamepad->HasControllers();
	controller_icon_label->setVisible(gamepad_connected);
	controller_label->setVisible(gamepad_connected);
	HintStyle style = HintStyle::Keyboard;
	if(gamepad_connected)
	{
		controller_icon_label->setPixmap(Theme::RenderIcon(":/icons/ui-gamepad.svg", 22, Theme::state_ready, dpr));
		QString name = gamepad->GetActiveControllerName();
		switch(gamepad->GetActiveLayout())
		{
			case ControllerLayout::SteamDeck:
				style = HintStyle::SteamDeck;
				controller_label->setText(tr("Steam Deck controls"));
				break;
			case ControllerLayout::PlayStation:
				style = HintStyle::PlayStation;
				controller_label->setText(name.isEmpty() ? tr("Controller connected") : name);
				break;
			default:
				style = HintStyle::Xbox;
				controller_label->setText(name.isEmpty() ? tr("Controller connected") : name);
				break;
		}
	}
	hint_bar->SetStyle(style);
	hint_style = style;
	HintBar::SetGlobalStyle(style);

	bool discovery = settings->GetDiscoveryEnabled();
	discovery_icon_label->setPixmap(Theme::RenderIcon(discovery ? ":/icons/ui-search.svg" : ":/icons/ui-search-off.svg",
			22, discovery ? Theme::accent_light : Theme::text_muted, dpr));
	discovery_label->setText(discovery ? tr("Auto search on") : tr("Auto search off"));
}

void MainWindow::UpdateClock()
{
	clock_label->setText(QLocale().toString(QTime::currentTime(), QLocale::ShortFormat));
}

void MainWindow::ShowToast(const QString &text)
{
	toast_label->setText(text);
	toast_label->adjustSize();
	toast_label->move((centralWidget()->width() - toast_label->width()) / 2, 90);
	if(auto effect = qobject_cast<QGraphicsOpacityEffect *>(toast_label->graphicsEffect()))
		effect->setOpacity(1.0);
	toast_label->show();
	toast_label->raise();
	toast_timer->start(TOAST_DURATION_MS);
}

bool MainWindow::SendWakeup(const DisplayServer &server, bool show_toast)
{
	if(!server.registered)
		return false;

	try
	{
		discovery_manager.SendWakeup(server.GetHostAddr(), server.registered_host.GetRPRegistKey(),
				chiaki_target_is_ps5(server.registered_host.GetTarget()));
	}
	catch(const Exception &e)
	{
		ModernDialog::Message(this, ModernDialog::Icon::Error, tr("Wakeup failed"), tr("Failed to send Wakeup packet:\n%1").arg(e.what()));
		return false;
	}

	if(show_toast)
		ShowToast(tr("Wakeup packet sent. The console should turn on in a few seconds."));
	return true;
}

void MainWindow::ActivateCurrent()
{
	if(stream_window)
		return;
	if(CurrentIsAddEntry())
	{
		AddManualHost();
		return;
	}
	auto server = CurrentServer();
	if(server)
		ConnectToServer(*server);
}

void MainWindow::WakeCurrent()
{
	auto server = CurrentServer();
	if(server && server->registered)
		SendWakeup(*server);
}

void MainWindow::ConnectToServer(const DisplayServer &server)
{
	if(server.registered)
	{
		if(server.discovered && server.discovery_host.state == CHIAKI_DISCOVERY_HOST_STATE_STANDBY)
		{
			// wake the console up and connect automatically once it reports being ready
			if(SendWakeup(server, false))
			{
				pending_connect = true;
				pending_connect_mac = server.discovery_host.GetHostMAC();
				pending_connect_timer->start(PENDING_CONNECT_TIMEOUT_MS);
				ShowToast(tr("Turning on %1… The stream starts automatically when the console is ready.")
						.arg(server.discovery_host.host_name));
			}
			return;
		}

		// start from the event loop, gamepad events may still be in flight for the current controllers
		QTimer::singleShot(0, this, [this, server]() {
			StartStream(server);
		});
	}
	else
	{
		RegistDialog regist_dialog(settings, server.GetHostAddr(), this);
		int r = regist_dialog.exec();
		if(r == QDialog::Accepted && !server.discovered) // success
		{
			ManualHost manual_host = server.manual_host;
			manual_host.Register(regist_dialog.GetRegisteredHost());
			settings->SetManualHost(manual_host);
		}
	}
}

void MainWindow::StartStream(const DisplayServer &server)
{
	if(stream_window)
		return;

	pending_connect = false;
	pending_connect_timer->stop();

	ShowToast(tr("Connecting to %1…").arg(server.discovered
				? server.discovery_host.host_name
				: server.registered_host.GetServerNickname()));

	// the stream session needs to open the controllers itself
	gamepad->Suspend();

	QString host = server.GetHostAddr();
	StreamSessionConnectInfo info(settings, server.registered_host.GetTarget(), host,
			server.registered_host.GetRPRegistKey(), server.registered_host.GetRPKey(), QString(""),
			isFullScreen(), false, false);
	stream_window = new StreamWindow(info);
	connect(stream_window.data(), &QObject::destroyed, this, [this]() {
		// resume after the stream session and its controllers are fully gone
		QTimer::singleShot(0, this, [this]() {
			gamepad->Resume();
			activateWindow();
			raise();
			carousel->setFocus();
		});
	});
}

void MainWindow::AddManualHost()
{
	ManualHostDialog dialog(settings, -1, this);
	dialog.exec();
}

void MainWindow::EditManualHost(int id)
{
	ManualHostDialog dialog(settings, id, this);
	dialog.exec();
}

void MainWindow::DeleteManualHost(const DisplayServer &server)
{
	if(server.discovered)
		return;
	if(!ModernDialog::Confirm(this, tr("Delete Console"),
			tr("Do you really want to remove the console %1 from the list?").arg(server.GetHostAddr()),
			tr("Delete"), tr("Cancel"), true))
		return;
	settings->RemoveManualHost(server.manual_host.GetID());
}

void MainWindow::ShowOptionsMenu()
{
	ShowContextMenu(carousel->GetCurrentIndex(), carousel->CurrentCardGlobalCenter());
}

void MainWindow::ShowContextMenu(int index, const QPoint &global_pos)
{
	Q_UNUSED(global_pos);
	if(index < 0 || index >= display_servers.count())
		return;
	DisplayServer server = display_servers[index];

	QList<ModernDialog::MenuEntry> entries;
	QList<std::function<void()>> actions;
	entries.append({ server.registered ? ":/icons/ui-play.svg" : ":/icons/ui-key.svg",
			server.registered ? tr("Connect") : tr("Register Console"), true });
	actions.append([this, server]() { ConnectToServer(server); });
	entries.append({ ":/icons/ui-power.svg", tr("Send Wakeup Packet"), server.registered });
	actions.append([this, server]() { SendWakeup(server); });
	if(!server.discovered)
	{
		int id = server.manual_host.GetID();
		entries.append({ ":/icons/ui-edit.svg", tr("Edit Console"), true });
		actions.append([this, id]() { EditManualHost(id); });
		entries.append({ ":/icons/ui-delete.svg", tr("Delete"), true });
		actions.append([this, server]() { DeleteManualHost(server); });
	}

	QString title = server.discovered ? server.discovery_host.host_name
		: (server.registered ? server.registered_host.GetServerNickname() : server.GetHostAddr());
	int chosen = ModernDialog::Menu(this, title, tr("Options"), entries);
	if(chosen >= 0 && chosen < actions.count())
		actions[chosen]();
}

void MainWindow::ToggleDiscovery()
{
	bool enabled = !settings->GetDiscoveryEnabled();
	settings->SetDiscoveryEnabled(enabled);
	discovery_manager.SetActive(enabled);
	UpdateStatus();
	UpdateDetails();
	ShowToast(enabled ? tr("Automatic search enabled") : tr("Automatic search disabled"));
}

void MainWindow::ShowSettings()
{
	// the dialog is reopened if the language was changed, so it gets fully retranslated
	int page = 0;
	while(true)
	{
		SettingsDialog dialog(settings, hint_style, this);
		dialog.SetCurrentPage(page);
		dialog.exec();
		if(!dialog.GetLanguageChanged())
			break;
		page = dialog.GetCurrentPage();
	}

	bool fullscreen = settings->GetFullscreenUI();
	if(fullscreen != isFullScreen())
	{
		if(fullscreen)
			showFullScreen();
		else
			showNormal();
	}
}

void MainWindow::ToggleFullscreen()
{
	if(isFullScreen())
		showNormal();
	else
		showFullScreen();
	settings->SetFullscreenUI(isFullScreen());
}

void MainWindow::RequestQuit()
{
	if(ModernDialog::Confirm(this, tr("Quit"), tr("Do you want to quit Gordeck?"),
			tr("Quit"), tr("Cancel")))
		Quit();
}

void MainWindow::Quit()
{
	qApp->exit();
}

void MainWindow::GamepadButtonPressed(GamepadButton button)
{
	switch(button)
	{
		case GamepadButton::Left:
			carousel->Previous();
			break;
		case GamepadButton::Right:
			carousel->Next();
			break;
		case GamepadButton::L1:
			carousel->SetCurrentIndex(0);
			break;
		case GamepadButton::R1:
			carousel->SetCurrentIndex(carousel->GetEntries().count() - 1);
			break;
		case GamepadButton::A:
			ActivateCurrent();
			break;
		case GamepadButton::B:
			RequestQuit();
			break;
		case GamepadButton::X:
			WakeCurrent();
			break;
		case GamepadButton::Y:
			ShowOptionsMenu();
			break;
		case GamepadButton::Start:
			ShowSettings();
			break;
		case GamepadButton::Select:
			ToggleDiscovery();
			break;
		default:
			break;
	}
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	switch(event->key())
	{
		case Qt::Key_Left:
		case Qt::Key_A:
			carousel->Previous();
			break;
		case Qt::Key_Right:
		case Qt::Key_D:
			carousel->Next();
			break;
		case Qt::Key_Home:
			carousel->SetCurrentIndex(0);
			break;
		case Qt::Key_End:
			carousel->SetCurrentIndex(carousel->GetEntries().count() - 1);
			break;
		case Qt::Key_Return:
		case Qt::Key_Enter:
		case Qt::Key_Space:
			ActivateCurrent();
			break;
		case Qt::Key_Escape:
		case Qt::Key_Backspace:
			RequestQuit();
			break;
		case Qt::Key_W:
			WakeCurrent();
			break;
		case Qt::Key_O:
		case Qt::Key_Menu:
			ShowOptionsMenu();
			break;
		case Qt::Key_F1:
			ShowSettings();
			break;
		case Qt::Key_F3:
			ToggleDiscovery();
			break;
		case Qt::Key_Insert:
		case Qt::Key_N:
			AddManualHost();
			break;
		case Qt::Key_Delete:
			if(auto server = CurrentServer())
				DeleteManualHost(*server);
			break;
		default:
			QMainWindow::keyPressEvent(event);
			return;
	}
	event->accept();
}
