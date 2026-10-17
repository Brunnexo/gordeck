// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_SCREENSAVERINHIBITOR_H
#define CHIAKI_SCREENSAVERINHIBITOR_H

#include <chiaki/log.h>

#include <QObject>
#include <QString>
#include <QTimer>

/**
 * Keeps the screen on and the system awake while it exists, e.g. during a stream.
 *
 * Gamepad input is read directly by SDL and does not count as user activity
 * for the desktop, so without this the screen would turn off while playing.
 */
class ScreenSaverInhibitor : public QObject
{
	Q_OBJECT

	private:
		ChiakiLog *log;
		bool active;
		uint screensaver_cookie;
		bool screensaver_inhibited;
		QString portal_request;
		QTimer activity_timer;

		void SimulateUserActivity();

	public:
		explicit ScreenSaverInhibitor(ChiakiLog *log, QObject *parent = nullptr);
		~ScreenSaverInhibitor() override;

		void Inhibit(const QString &reason);
		void Release();
};

#endif // CHIAKI_SCREENSAVERINHIBITOR_H
