// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <i18n.h>
#include <settings.h>

#include <QAbstractButton>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

static QTranslator *app_translator = nullptr;
static QTranslator *qt_translator = nullptr;
static QString current_language = "en";

static QString StandardButtonText(QDialogButtonBox::StandardButton button)
{
	switch(button)
	{
		case QDialogButtonBox::Ok: return QCoreApplication::translate("I18n", "OK");
		case QDialogButtonBox::Save: return QCoreApplication::translate("I18n", "Save");
		case QDialogButtonBox::SaveAll: return QCoreApplication::translate("I18n", "Save All");
		case QDialogButtonBox::Open: return QCoreApplication::translate("I18n", "Open");
		case QDialogButtonBox::Yes: return QCoreApplication::translate("I18n", "&Yes");
		case QDialogButtonBox::YesToAll: return QCoreApplication::translate("I18n", "Yes to &All");
		case QDialogButtonBox::No: return QCoreApplication::translate("I18n", "&No");
		case QDialogButtonBox::NoToAll: return QCoreApplication::translate("I18n", "N&o to All");
		case QDialogButtonBox::Abort: return QCoreApplication::translate("I18n", "Abort");
		case QDialogButtonBox::Retry: return QCoreApplication::translate("I18n", "Retry");
		case QDialogButtonBox::Ignore: return QCoreApplication::translate("I18n", "Ignore");
		case QDialogButtonBox::Close: return QCoreApplication::translate("I18n", "Close");
		case QDialogButtonBox::Cancel: return QCoreApplication::translate("I18n", "Cancel");
		case QDialogButtonBox::Discard: return QCoreApplication::translate("I18n", "Discard");
		case QDialogButtonBox::Help: return QCoreApplication::translate("I18n", "Help");
		case QDialogButtonBox::Apply: return QCoreApplication::translate("I18n", "Apply");
		case QDialogButtonBox::Reset: return QCoreApplication::translate("I18n", "Reset");
		case QDialogButtonBox::RestoreDefaults: return QCoreApplication::translate("I18n", "Restore Defaults");
		default: return QString();
	}
}

/**
 * Standard button texts usually come from the platform theme. The KDE theme uses its own
 * translations, which are missing in Flatpak, so the texts are replaced right before
 * any button box (including the ones in message boxes) is shown.
 */
class StandardButtonTranslator : public QObject
{
	public:
		using QObject::QObject;

	protected:
		bool eventFilter(QObject *watched, QEvent *event) override
		{
			if(event->type() == QEvent::Show)
			{
				if(auto box = qobject_cast<QDialogButtonBox *>(watched))
				{
					for(auto button : box->buttons())
					{
						QString text = StandardButtonText(box->standardButton(button));
						if(!text.isEmpty())
							button->setText(text);
					}
				}
			}
			return QObject::eventFilter(watched, event);
		}
};

void I18n::Init(Settings *settings)
{
	qApp->installEventFilter(new StandardButtonTranslator(qApp));
	SetLanguage(settings->GetLanguage());
}

void I18n::SetLanguage(const QString &lang)
{
	if(app_translator)
	{
		QCoreApplication::removeTranslator(app_translator);
		delete app_translator;
		app_translator = nullptr;
	}
	if(qt_translator)
	{
		QCoreApplication::removeTranslator(qt_translator);
		delete qt_translator;
		qt_translator = nullptr;
	}

	current_language = "en";
	QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));

	// English is the source language, no translator needed
	if(lang.isEmpty() || lang == "en")
		return;

	auto translator = new QTranslator(qApp);
	if(!translator->load(":/i18n/chiaki_" + lang + ".qm"))
	{
		delete translator;
		return;
	}
	app_translator = translator;
	QCoreApplication::installTranslator(app_translator);
	current_language = lang;
	QLocale::setDefault(QLocale(lang));

	// translations for Qt's own strings (standard dialog buttons, context menus, ...)
	translator = new QTranslator(qApp);
	if(translator->load("qtbase_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
	{
		qt_translator = translator;
		QCoreApplication::installTranslator(qt_translator);
	}
	else
		delete translator;
}

QString I18n::CurrentLanguage()
{
	return current_language;
}

QList<QPair<QString, QString>> I18n::AvailableLanguages()
{
	// names are intentionally not translated, each language is shown in its own name
	return {
		{ "pt_BR", QString::fromUtf8("Português (Brasil)") },
		{ "en", "English" }
	};
}

QString I18n::DiscoveryHostStateString(ChiakiDiscoveryHostState state)
{
	switch(state)
	{
		case CHIAKI_DISCOVERY_HOST_STATE_READY:
			return QCoreApplication::translate("I18n", "Ready");
		case CHIAKI_DISCOVERY_HOST_STATE_STANDBY:
			return QCoreApplication::translate("I18n", "Standby");
		default:
			return QCoreApplication::translate("I18n", "Unknown");
	}
}

QString I18n::QuitReasonString(ChiakiQuitReason reason)
{
	switch(reason)
	{
		case CHIAKI_QUIT_REASON_STOPPED:
			return QCoreApplication::translate("I18n", "Stopped");
		case CHIAKI_QUIT_REASON_SESSION_REQUEST_UNKNOWN:
			return QCoreApplication::translate("I18n", "Unknown Session Request Error");
		case CHIAKI_QUIT_REASON_SESSION_REQUEST_CONNECTION_REFUSED:
			return QCoreApplication::translate("I18n", "Connection Refused in Session Request");
		case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_IN_USE:
			return QCoreApplication::translate("I18n", "Remote Play on Console is already in use");
		case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_CRASH:
			return QCoreApplication::translate("I18n", "Remote Play on Console has crashed");
		case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_VERSION_MISMATCH:
			return QCoreApplication::translate("I18n", "RP-Version mismatch");
		case CHIAKI_QUIT_REASON_CTRL_UNKNOWN:
			return QCoreApplication::translate("I18n", "Unknown Ctrl Error");
		case CHIAKI_QUIT_REASON_CTRL_CONNECTION_REFUSED:
			return QCoreApplication::translate("I18n", "Connection Refused in Ctrl");
		case CHIAKI_QUIT_REASON_CTRL_CONNECT_FAILED:
			return QCoreApplication::translate("I18n", "Ctrl failed to connect");
		case CHIAKI_QUIT_REASON_STREAM_CONNECTION_UNKNOWN:
			return QCoreApplication::translate("I18n", "Unknown Error in Stream Connection");
		case CHIAKI_QUIT_REASON_STREAM_CONNECTION_REMOTE_DISCONNECTED:
			return QCoreApplication::translate("I18n", "Remote has disconnected from Stream Connection");
		case CHIAKI_QUIT_REASON_NONE:
		default:
			return QCoreApplication::translate("I18n", "Unknown");
	}
}
