// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <screensaverinhibitor.h>

#ifdef CHIAKI_GUI_ENABLE_DBUS
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>

#define SCREENSAVER_SERVICE "org.freedesktop.ScreenSaver"
#define SCREENSAVER_PATH "/org/freedesktop/ScreenSaver"
#define SCREENSAVER_INTERFACE "org.freedesktop.ScreenSaver"

#define PORTAL_SERVICE "org.freedesktop.portal.Desktop"
#define PORTAL_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_INHIBIT_INTERFACE "org.freedesktop.portal.Inhibit"
#define PORTAL_REQUEST_INTERFACE "org.freedesktop.portal.Request"

// org.freedesktop.portal.Inhibit flags
#define PORTAL_INHIBIT_SUSPEND 4
#define PORTAL_INHIBIT_IDLE 8
#endif

#define ACTIVITY_INTERVAL_MS 30000

ScreenSaverInhibitor::ScreenSaverInhibitor(ChiakiLog *log, QObject *parent)
	: QObject(parent),
	log(log),
	active(false),
	screensaver_cookie(0),
	screensaver_inhibited(false)
{
	activity_timer.setInterval(ACTIVITY_INTERVAL_MS);
	connect(&activity_timer, &QTimer::timeout, this, &ScreenSaverInhibitor::SimulateUserActivity);
}

ScreenSaverInhibitor::~ScreenSaverInhibitor()
{
	Release();
}

void ScreenSaverInhibitor::Inhibit(const QString &reason)
{
	if(active)
		return;
	active = true;

#ifdef CHIAKI_GUI_ENABLE_DBUS
	QDBusConnection bus = QDBusConnection::sessionBus();
	if(!bus.isConnected())
	{
		CHIAKI_LOGW(log, "No D-Bus session bus, can't keep the screen on");
		return;
	}

	// org.freedesktop.ScreenSaver, implemented by KDE, GNOME and Steam
	QDBusMessage screensaver_msg = QDBusMessage::createMethodCall(SCREENSAVER_SERVICE, SCREENSAVER_PATH,
			SCREENSAVER_INTERFACE, "Inhibit");
	screensaver_msg << QString("Gordeck") << reason;
	auto screensaver_watcher = new QDBusPendingCallWatcher(bus.asyncCall(screensaver_msg), this);
	connect(screensaver_watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
		QDBusPendingReply<uint> reply = *watcher;
		watcher->deleteLater();
		if(reply.isError())
		{
			CHIAKI_LOGW(log, "ScreenSaver inhibit failed: %s", reply.error().message().toLocal8Bit().constData());
			return;
		}
		screensaver_cookie = reply.value();
		screensaver_inhibited = true;
		CHIAKI_LOGI(log, "Screen saver inhibited");
		// the stream might have ended in the meantime
		if(!active)
			Release();
	});

	// xdg-desktop-portal, the official way for sandboxed apps
	QDBusMessage portal_msg = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH,
			PORTAL_INHIBIT_INTERFACE, "Inhibit");
	QVariantMap options;
	options.insert("reason", reason);
	portal_msg << QString() << (uint)(PORTAL_INHIBIT_IDLE | PORTAL_INHIBIT_SUSPEND) << options;
	auto portal_watcher = new QDBusPendingCallWatcher(bus.asyncCall(portal_msg), this);
	connect(portal_watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
		QDBusPendingReply<QDBusObjectPath> reply = *watcher;
		watcher->deleteLater();
		if(reply.isError())
		{
			CHIAKI_LOGW(log, "Portal inhibit failed: %s", reply.error().message().toLocal8Bit().constData());
			return;
		}
		portal_request = reply.value().path();
		CHIAKI_LOGI(log, "Idle and suspend inhibited through the portal");
		if(!active)
			Release();
	});

	activity_timer.start();
#else
	Q_UNUSED(reason);
#endif
}

void ScreenSaverInhibitor::Release()
{
	active = false;
	activity_timer.stop();

#ifdef CHIAKI_GUI_ENABLE_DBUS
	QDBusConnection bus = QDBusConnection::sessionBus();
	if(screensaver_inhibited)
	{
		QDBusMessage msg = QDBusMessage::createMethodCall(SCREENSAVER_SERVICE, SCREENSAVER_PATH,
				SCREENSAVER_INTERFACE, "UnInhibit");
		msg << screensaver_cookie;
		bus.call(msg, QDBus::NoBlock);
		screensaver_inhibited = false;
		CHIAKI_LOGI(log, "Screen saver no longer inhibited");
	}
	if(!portal_request.isEmpty())
	{
		QDBusMessage msg = QDBusMessage::createMethodCall(PORTAL_SERVICE, portal_request,
				PORTAL_REQUEST_INTERFACE, "Close");
		bus.call(msg, QDBus::NoBlock);
		portal_request.clear();
	}
#endif
}

void ScreenSaverInhibitor::SimulateUserActivity()
{
#ifdef CHIAKI_GUI_ENABLE_DBUS
	// in case the desktop ignores the inhibition, e.g. for dimming
	QDBusMessage msg = QDBusMessage::createMethodCall(SCREENSAVER_SERVICE, SCREENSAVER_PATH,
			SCREENSAVER_INTERFACE, "SimulateUserActivity");
	QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
#endif
}
