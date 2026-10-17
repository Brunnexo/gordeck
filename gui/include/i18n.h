// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_I18N_H
#define CHIAKI_I18N_H

#include <chiaki/discoveryservice.h>
#include <chiaki/session.h>

#include <QList>
#include <QPair>
#include <QString>

class Settings;

namespace I18n
{
	/**
	 * Install the translators for the language stored in settings.
	 * Must be called after the QApplication has been created.
	 */
	void Init(Settings *settings);

	/**
	 * Switch the language at runtime. Widgets receive QEvent::LanguageChange.
	 * @param lang language code, e.g. "pt_BR" or "en"
	 */
	void SetLanguage(const QString &lang);

	QString CurrentLanguage();

	/**
	 * @return pairs of (language code, native language name)
	 */
	QList<QPair<QString, QString>> AvailableLanguages();

	QString DiscoveryHostStateString(ChiakiDiscoveryHostState state);
	QString QuitReasonString(ChiakiQuitReason reason);
}

#endif // CHIAKI_I18N_H
