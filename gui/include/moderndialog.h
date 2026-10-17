// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_MODERNDIALOG_H
#define CHIAKI_MODERNDIALOG_H

#include <QDialog>
#include <QList>
#include <QPointer>

class QBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;
class HintButton;

/**
 * Frameless modal in the style of the main interface: dims the window behind it,
 * shows an optional icon, a title, a text, custom content, big buttons and
 * the button hints of the controller in use. Usable with gamepad, keyboard and touch.
 */
class ModernDialog : public QDialog
{
	Q_OBJECT

	public:
		enum class Icon
		{
			None,
			Info,
			Question,
			Warning,
			Error,
			Success
		};

		enum class ButtonRole
		{
			Accept,			// calls accept()
			Reject,			// calls reject()
			Destructive,	// like Accept, highlighted in red
			Other			// closes the dialog with the button's index as result of GetClickedIndex()
		};

		struct MenuEntry
		{
			QString icon;
			QString text;
			bool enabled;
		};

	private:
		QLabel *icon_label;
		QLabel *title_label;
		QLabel *text_label;
		QVBoxLayout *content_layout;
		QBoxLayout *button_layout;
		QWidget *button_container;
		bool vertical_buttons;
		QList<QPushButton *> buttons;
		int clicked_index;
		QPointer<QWidget> initial_focus;
		HintButton *hint_back;

		QWidget *dim_overlay;
		QPointer<QWidget> keyboard_grabber;

	protected:
		void paintEvent(QPaintEvent *event) override;
		void showEvent(QShowEvent *event) override;
		void hideEvent(QHideEvent *event) override;
		void keyPressEvent(QKeyEvent *event) override;

	public:
		explicit ModernDialog(const QString &title, QWidget *parent = nullptr, Icon icon = Icon::None, int width = 580);
		~ModernDialog() override;

		void SetText(const QString &text);
		void SetTitle(const QString &title);
		QVBoxLayout *GetContentLayout()		{ return content_layout; }

		/**
		 * Stack buttons vertically over the full width, like a menu. Call before adding buttons.
		 */
		void SetVerticalButtons(bool vertical);
		QPushButton *AddButton(const QString &text, ButtonRole role = ButtonRole::Accept, const QString &icon = QString());
		void SetInitialFocus(QWidget *widget)	{ initial_focus = widget; }
		void SetBackHintText(const QString &text);

		/**
		 * @return index of the clicked button, or -1 if the dialog was closed with back/Esc
		 */
		int GetClickedIndex() const			{ return clicked_index; }

		static void Message(QWidget *parent, Icon icon, const QString &title, const QString &text);

		/**
		 * @return index of the chosen button, or -1 if closed with back/Esc
		 */
		static int Choice(QWidget *parent, Icon icon, const QString &title, const QString &text,
				const QStringList &button_texts, int default_index = 0, int destructive_index = -1);

		static bool Confirm(QWidget *parent, const QString &title, const QString &text,
				const QString &confirm_text, const QString &cancel_text, bool destructive = false);

		/**
		 * Vertical list of actions
		 * @return index of the chosen entry, or -1 if closed with back/Esc
		 */
		static int Menu(QWidget *parent, const QString &title, const QString &subtitle, const QList<MenuEntry> &entries);
};

#endif // CHIAKI_MODERNDIALOG_H
