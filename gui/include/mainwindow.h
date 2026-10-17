// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_MAINWINDOW_H
#define CHIAKI_MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>

#include "discoverymanager.h"
#include "host.h"
#include "gamepadnavigator.h"

class ConsoleCarousel;
class HintBar;
class HintButton;
class QLabel;
class QTimer;
class Settings;
class StreamWindow;

struct DisplayServer
{
	DiscoveryHost discovery_host;
	ManualHost manual_host;
	bool discovered;

	RegisteredHost registered_host;
	bool registered;

	QString GetHostAddr() const { return discovered ? discovery_host.host_addr : manual_host.GetHost(); }
	bool IsPS5() const { return discovered ? discovery_host.ps5 :
		(registered ? chiaki_target_is_ps5(registered_host.GetTarget()) : false); }
};

class MainWindow : public QMainWindow
{
	Q_OBJECT

	private:
		Settings *settings;

		DiscoveryManager discovery_manager;
		QList<DisplayServer> display_servers;

		GamepadNavigator *gamepad;
		QPointer<StreamWindow> stream_window;

		// console that was woken up and should be connected to as soon as it is ready
		bool pending_connect;
		HostMAC pending_connect_mac;
		QTimer *pending_connect_timer;

		QLabel *title_label;
		QLabel *discovery_icon_label;
		QLabel *discovery_label;
		QLabel *controller_icon_label;
		QLabel *controller_label;
		QLabel *clock_label;
		QLabel *section_label;
		ConsoleCarousel *carousel;
		QLabel *details_label;
		QLabel *toast_label;
		QTimer *toast_timer;

		HintBar *hint_bar;
		HintButton *hint_activate;
		HintButton *hint_wake;
		HintButton *hint_options;
		HintButton *hint_discovery;
		HintButton *hint_settings;
		HintButton *hint_quit;

		DisplayServer *CurrentServer();
		bool CurrentIsAddEntry();

		void Retranslate();
		void UpdateEntries();
		void UpdateDetails();
		void UpdateHints();
		void UpdateStatus();
		void UpdateClock();
		void ShowToast(const QString &text);

		bool SendWakeup(const DisplayServer &server, bool show_toast = true);
		void CheckPendingConnect();
		void ConnectToServer(const DisplayServer &server);
		void StartStream(const DisplayServer &server);
		void AddManualHost();
		void EditManualHost(int id);
		void DeleteManualHost(const DisplayServer &server);

	private slots:
		void ActivateCurrent();
		void WakeCurrent();
		void ShowOptionsMenu();
		void ShowContextMenu(int index, const QPoint &global_pos);
		void ToggleDiscovery();
		void ShowSettings();
		void ToggleFullscreen();
		void RequestQuit();
		void Quit();

		void GamepadButtonPressed(GamepadButton button);
		void UpdateDisplayServers();

	protected:
		void keyPressEvent(QKeyEvent *event) override;
		void changeEvent(QEvent *event) override;
		void resizeEvent(QResizeEvent *event) override;

	public:
		explicit MainWindow(Settings *settings, QWidget *parent = nullptr);
		~MainWindow() override;
};

#endif //CHIAKI_MAINWINDOW_H
