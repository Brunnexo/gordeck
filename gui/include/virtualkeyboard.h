// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_VIRTUALKEYBOARD_H
#define CHIAKI_VIRTUALKEYBOARD_H

#include "moderndialog.h"

#include <QList>

class QLabel;
class QLineEdit;
class QPushButton;

/**
 * On-screen keyboard to fill in text fields with a gamepad or touch screen.
 * Shows a number pad for numeric fields. Typing on a real keyboard works as well.
 */
class VirtualKeyboard : public ModernDialog
{
	Q_OBJECT

	private:
		enum class Mode { Lower, Upper, Symbols };

		QLineEdit *target;
		QString text;
		bool password;
		Mode mode;
		QLabel *text_label;
		QList<QPushButton *> char_keys;
		QPushButton *shift_key;
		QPushButton *symbols_key;

		QPushButton *AddKey(QWidget *parent, class QGridLayout *grid, const QString &label, int row, int col, int col_span = 1);
		void BuildTextLayout(QWidget *pad);
		void BuildNumericLayout(QWidget *pad);
		void Append(const QString &s);
		void Backspace();
		void UpdateText();
		void UpdateKeys();

	protected:
		void keyPressEvent(QKeyEvent *event) override;

	public:
		VirtualKeyboard(QLineEdit *target, QWidget *parent);

		/**
		 * Let the user edit the text of the field. The field is only changed if confirmed.
		 * @return whether the text was changed
		 */
		static bool Edit(QLineEdit *edit);

		/**
		 * Open the keyboard when text fields are touched (and on Steam Deck, clicked)
		 */
		static void InstallTouchTrigger(QObject *app);
};

#endif // CHIAKI_VIRTUALKEYBOARD_H
