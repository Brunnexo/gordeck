// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_SETTINGSDIALOG_H
#define CHIAKI_SETTINGSDIALOG_H

#include "hintbar.h"

#include <QDialog>

class Settings;
class QComboBox;
class QAbstractButton;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

/**
 * Settings screen in the style of the main interface: categories on the left,
 * one page of settings rows on the right, button hints at the bottom.
 * Covers the parent window and can be used completely with a gamepad.
 */
class SettingsDialog : public QDialog
{
	Q_OBJECT

	private:
		Settings *settings;
		bool language_changed;

		QStackedWidget *page_stack;
		QList<QPushButton *> category_buttons;

		QComboBox *language_combo_box;
		QAbstractButton *fullscreen_ui_toggle;

		QAbstractButton *log_verbose_toggle;
		QComboBox *disconnect_action_combo_box;
		QComboBox *rumble_intensity_combo_box;
		QComboBox *audio_in_device_combo_box;
		QAbstractButton *mic_auto_start_toggle;
		QComboBox *mic_toggle_button_combo_box;

		QComboBox *resolution_combo_box;
		QComboBox *fps_combo_box;
		QLineEdit *bitrate_edit;
		QComboBox *codec_combo_box;
		QLineEdit *audio_buffer_size_edit;
		QComboBox *audio_device_combo_box;
		QAbstractButton *pi_decoder_toggle;
		QComboBox *hw_decoder_combo_box;

		HintStyle hint_style;
		QVBoxLayout *registered_hosts_layout;

		QVBoxLayout *AddPage(const QString &title, const QString &icon);
		QWidget *CreateRow(const QString &title, const QString &description, QWidget *control, int control_width = 340);
		void UpdateBitratePlaceholder();

	private slots:
		void LanguageSelected();
		void LogVerboseChanged();
		void DisconnectActionSelected();
		void RumbleIntensitySelected();
		void MicToggleButtonSelected();

		void ResolutionSelected();
		void FPSSelected();
		void BitrateEdited();
		void CodecSelected();
		void AudioBufferSizeEdited();
		void HardwareDecodeEngineSelected();
		void UpdateHardwareDecodeEngineComboBox();

		void UpdateRegisteredHosts();
		void RegisterNewHost();

	protected:
		void paintEvent(QPaintEvent *event) override;
		void showEvent(QShowEvent *event) override;
		void keyPressEvent(QKeyEvent *event) override;

	public:
		SettingsDialog(Settings *settings, HintStyle hint_style, QWidget *parent = nullptr);

		int GetCurrentPage() const;
		void SetCurrentPage(int index);

		/**
		 * @return true if the dialog was closed because the language was changed
		 */
		bool GetLanguageChanged() const	{ return language_changed; }
};

#endif // CHIAKI_SETTINGSDIALOG_H
