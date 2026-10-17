// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <settingsdialog.h>
#include <settings.h>
#include <settingskeycapturedialog.h>
#include <registdialog.h>
#include <sessionlog.h>
#include <confirmdialog.h>
#include <i18n.h>
#include <theme.h>

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <chiaki/config.h>
#include <chiaki/ffmpegdecoder.h>

#define CONTROL_HEIGHT 44

static const char * const stylesheet = R"(
QFrame#settingRow {
	background-color: rgba(31, 42, 68, 150);
	border: 2px solid transparent;
	border-radius: 14px;
}

QFrame#settingRow[focused="true"] {
	background-color: rgba(47, 140, 255, 38);
	border-color: #2f8cff;
}

QPushButton#categoryButton {
	text-align: left;
	padding: 12px 18px;
	border: none;
	border-radius: 12px;
	background: transparent;
	font-size: 13pt;
	color: #8e9bb4;
}

QPushButton#categoryButton:checked {
	background-color: #2f8cff;
	color: #ffffff;
	font-weight: bold;
}

QPushButton#categoryButton:hover:!checked {
	background-color: rgba(255, 255, 255, 18);
}

QComboBox, QLineEdit, QPushButton {
	min-height: 28px;
	font-size: 12pt;
}

QComboBox:focus, QLineEdit:focus, QPushButton:focus {
	border: 2px solid #2f8cff;
}
)";

/**
 * On/off switch, toggled with A/Space/click or set with left/right
 */
class ToggleSwitch : public QAbstractButton
{
	public:
		explicit ToggleSwitch(QWidget *parent = nullptr) : QAbstractButton(parent)
		{
			setCheckable(true);
			setFocusPolicy(Qt::StrongFocus);
			setCursor(Qt::PointingHandCursor);
		}

		QSize sizeHint() const override { return QSize(64, 34); }

	protected:
		void paintEvent(QPaintEvent *) override
		{
			QPainter painter(this);
			painter.setRenderHint(QPainter::Antialiasing);
			QRectF track = QRectF(rect()).adjusted(2, 2, -2, -2);
			qreal radius = track.height() / 2.0;
			painter.setPen(hasFocus() ? QPen(Theme::accent_light, 2.0) : QPen(QColor(0x2b, 0x38, 0x58), 1.5));
			painter.setBrush(isChecked() ? Theme::accent : QColor(0x0c, 0x13, 0x22));
			if(!isEnabled())
				painter.setOpacity(0.4);
			painter.drawRoundedRect(track, radius, radius);
			qreal knob = track.height() - 8.0;
			qreal x = isChecked() ? track.right() - 4.0 - knob : track.left() + 4.0;
			painter.setPen(Qt::NoPen);
			painter.setBrush(isChecked() ? QColor(Qt::white) : Theme::text_muted);
			painter.drawEllipse(QRectF(x, track.top() + 4.0, knob, knob));
		}

		void keyPressEvent(QKeyEvent *event) override
		{
			if(event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)
			{
				bool checked = event->key() == Qt::Key_Right;
				if(checked != isChecked())
					setChecked(checked);
				return;
			}
			QAbstractButton::keyPressEvent(event);
		}
};

static QFrame *FindRow(QWidget *widget)
{
	for(QWidget *w = widget; w; w = w->parentWidget())
	{
		if(w->objectName() == "settingRow")
			return qobject_cast<QFrame *>(w);
	}
	return nullptr;
}

static void SetRowFocused(QWidget *widget, bool focused)
{
	QFrame *row = FindRow(widget);
	if(!row || row->property("focused").toBool() == focused)
		return;
	row->setProperty("focused", focused);
	row->style()->unpolish(row);
	row->style()->polish(row);
}

static QIcon CategoryIcon(const QString &filename)
{
	QIcon icon;
	icon.addPixmap(Theme::RenderIcon(filename, 48, Theme::text_muted, 1.0), QIcon::Normal, QIcon::Off);
	icon.addPixmap(Theme::RenderIcon(filename, 48, Qt::white, 1.0), QIcon::Normal, QIcon::On);
	return icon;
}

SettingsDialog::SettingsDialog(Settings *settings, HintStyle hint_style, QWidget *parent)
	: QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint),
	settings(settings),
	language_changed(false),
	hint_style(hint_style)
{
	setWindowTitle(tr("Settings"));
	setStyleSheet(stylesheet);
	resize(1280, 800);

	auto root_layout = new QVBoxLayout(this);
	root_layout->setContentsMargins(36, 22, 36, 20);
	root_layout->setSpacing(14);

	// Header

	auto header_layout = new QHBoxLayout();
	header_layout->setSpacing(12);
	root_layout->addLayout(header_layout);
	auto header_icon = new QLabel(this);
	header_icon->setPixmap(Theme::RenderIcon(":/icons/ui-settings.svg", 34, Theme::accent_light, devicePixelRatioF()));
	header_layout->addWidget(header_icon);
	auto header_label = new QLabel(tr("Settings"), this);
	header_label->setStyleSheet("font-size: 21pt; font-weight: bold; color: #ecf1fa;");
	header_layout->addWidget(header_label);
	header_layout->addStretch(1);
	auto version_label = new QLabel(QString("Gordeck %1").arg(CHIAKI_VERSION), this);
	version_label->setStyleSheet("font-size: 11pt; color: #8e9bb4;");
	header_layout->addWidget(version_label);

	// Categories and pages

	auto body_layout = new QHBoxLayout();
	body_layout->setSpacing(24);
	root_layout->addLayout(body_layout, 1);

	auto sidebar_layout = new QVBoxLayout();
	sidebar_layout->setSpacing(6);
	body_layout->addLayout(sidebar_layout);

	page_stack = new QStackedWidget(this);
	page_stack->setFocusPolicy(Qt::NoFocus);
	body_layout->addWidget(page_stack, 1);

	auto AddCategory = [this, sidebar_layout](const QString &title, const QString &icon) {
		auto button = new QPushButton(CategoryIcon(icon), "  " + title, this);
		button->setObjectName("categoryButton");
		button->setIconSize(QSize(24, 24));
		button->setCheckable(true);
		button->setFocusPolicy(Qt::NoFocus);
		button->setAutoDefault(false);
		button->setMinimumWidth(310);
		int index = category_buttons.count();
		connect(button, &QPushButton::clicked, this, [this, index]() { SetCurrentPage(index); });
		category_buttons.append(button);
		sidebar_layout->addWidget(button);
		return AddPage(title, icon);
	};

	// Interface

	auto interface_page = AddCategory(tr("Interface"), ":/icons/ui-display.svg");

	language_combo_box = new QComboBox(this);
	for(const auto &lang : I18n::AvailableLanguages())
	{
		language_combo_box->addItem(lang.second, lang.first);
		if(lang.first == I18n::CurrentLanguage())
			language_combo_box->setCurrentIndex(language_combo_box->count() - 1);
	}
	connect(language_combo_box, SIGNAL(currentIndexChanged(int)), this, SLOT(LanguageSelected()));
	interface_page->addWidget(CreateRow(tr("Language"), tr("Language of the interface."), language_combo_box));

	fullscreen_ui_toggle = new ToggleSwitch(this);
	fullscreen_ui_toggle->setChecked(settings->GetFullscreenUI());
	connect(fullscreen_ui_toggle, &QAbstractButton::toggled, this, [this](bool checked) {
		this->settings->SetFullscreenUI(checked);
	});
	interface_page->addWidget(CreateRow(tr("Start in Fullscreen"), tr("Gordeck opens using the whole screen."), fullscreen_ui_toggle));

	// Stream

	auto stream_page = AddCategory(tr("Stream"), ":/icons/ui-play.svg");

	resolution_combo_box = new QComboBox(this);
	static const QList<QPair<ChiakiVideoResolutionPreset, const char *>> resolution_strings = {
		{ CHIAKI_VIDEO_RESOLUTION_PRESET_360p, "360p" },
		{ CHIAKI_VIDEO_RESOLUTION_PRESET_540p, "540p" },
		{ CHIAKI_VIDEO_RESOLUTION_PRESET_720p, "720p" },
		{ CHIAKI_VIDEO_RESOLUTION_PRESET_1080p, QT_TR_NOOP("1080p (PS5 and PS4 Pro only)") }
	};
	auto current_res = settings->GetResolution();
	for(const auto &p : resolution_strings)
	{
		resolution_combo_box->addItem(tr(p.second), (int)p.first);
		if(current_res == p.first)
			resolution_combo_box->setCurrentIndex(resolution_combo_box->count() - 1);
	}
	connect(resolution_combo_box, SIGNAL(currentIndexChanged(int)), this, SLOT(ResolutionSelected()));
	stream_page->addWidget(CreateRow(tr("Resolution"), tr("Resolution of the streamed video."), resolution_combo_box));

	fps_combo_box = new QComboBox(this);
	static const QList<QPair<ChiakiVideoFPSPreset, QString>> fps_strings = {
		{ CHIAKI_VIDEO_FPS_PRESET_30, "30" },
		{ CHIAKI_VIDEO_FPS_PRESET_60, "60" }
	};
	auto current_fps = settings->GetFPS();
	for(const auto &p : fps_strings)
	{
		fps_combo_box->addItem(p.second, (int)p.first);
		if(current_fps == p.first)
			fps_combo_box->setCurrentIndex(fps_combo_box->count() - 1);
	}
	connect(fps_combo_box, SIGNAL(currentIndexChanged(int)), this, SLOT(FPSSelected()));
	stream_page->addWidget(CreateRow(tr("Frame Rate"), tr("Frames per second of the stream."), fps_combo_box));

	codec_combo_box = new QComboBox(this);
	static const QList<QPair<ChiakiCodec, const char *>> codec_strings = {
		{ CHIAKI_CODEC_H264, "H264" },
		{ CHIAKI_CODEC_H265, QT_TR_NOOP("H265 (PS5 only)") }
	};
	auto current_codec = settings->GetCodec();
	for(const auto &p : codec_strings)
	{
		codec_combo_box->addItem(tr(p.second), (int)p.first);
		if(current_codec == p.first)
			codec_combo_box->setCurrentIndex(codec_combo_box->count() - 1);
	}
	connect(codec_combo_box, SIGNAL(currentIndexChanged(int)), this, SLOT(CodecSelected()));
	stream_page->addWidget(CreateRow(tr("Codec"), tr("H265 gives better quality at the same bitrate, but only works with the PS5."), codec_combo_box));

	bitrate_edit = new QLineEdit(this);
	bitrate_edit->setValidator(new QIntValidator(2000, 50000, bitrate_edit));
	unsigned int bitrate = settings->GetBitrate();
	bitrate_edit->setText(bitrate ? QString::number(bitrate) : "");
	connect(bitrate_edit, &QLineEdit::textEdited, this, &SettingsDialog::BitrateEdited);
	UpdateBitratePlaceholder();
	stream_page->addWidget(CreateRow(tr("Bitrate"), tr("In kbit/s. Leave empty to use the automatic value."), bitrate_edit));

	audio_device_combo_box = new QComboBox(this);
	audio_device_combo_box->addItem(tr("Auto"));
	auto current_audio_device = settings->GetAudioOutDevice();
	if(!current_audio_device.isEmpty())
	{
		// temporarily add the selected device before async fetching is done
		audio_device_combo_box->addItem(current_audio_device, current_audio_device);
		audio_device_combo_box->setCurrentIndex(1);
	}
	connect(audio_device_combo_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
		this->settings->SetAudioOutDevice(audio_device_combo_box->currentData().toString());
	});

	// do this async because it's slow, assuming availableDevices() is thread-safe
	auto audio_devices_future = QtConcurrent::run([]() {
		return QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
	});
	auto audio_devices_future_watcher = new QFutureWatcher<QList<QAudioDeviceInfo>>(this);
	connect(audio_devices_future_watcher, &QFutureWatcher<QList<QAudioDeviceInfo>>::finished, this, [this, audio_devices_future_watcher, settings]() {
		auto available_devices = audio_devices_future_watcher->result();
		QSignalBlocker blocker(audio_device_combo_box);
		while(audio_device_combo_box->count() > 1) // remove all but "Auto"
			audio_device_combo_box->removeItem(1);
		for(QAudioDeviceInfo di : available_devices)
			audio_device_combo_box->addItem(di.deviceName(), di.deviceName());
		int audio_out_device_index = audio_device_combo_box->findData(settings->GetAudioOutDevice());
		audio_device_combo_box->setCurrentIndex(audio_out_device_index < 0 ? 0 : audio_out_device_index);
	});
	audio_devices_future_watcher->setFuture(audio_devices_future);
	stream_page->addWidget(CreateRow(tr("Audio Output Device"), tr("Where the sound of the console is played."), audio_device_combo_box));

	audio_buffer_size_edit = new QLineEdit(this);
	audio_buffer_size_edit->setValidator(new QIntValidator(1024, 0x20000, audio_buffer_size_edit));
	unsigned int audio_buffer_size = settings->GetAudioBufferSizeRaw();
	audio_buffer_size_edit->setText(audio_buffer_size ? QString::number(audio_buffer_size) : "");
	audio_buffer_size_edit->setPlaceholderText(tr("Default (%1)").arg(settings->GetAudioBufferSizeDefault()));
	connect(audio_buffer_size_edit, &QLineEdit::textEdited, this, &SettingsDialog::AudioBufferSizeEdited);
	stream_page->addWidget(CreateRow(tr("Audio Buffer Size"), tr("Increase it if the sound stutters."), audio_buffer_size_edit));

	disconnect_action_combo_box = new QComboBox(this);
	QList<QPair<DisconnectAction, const char *>> disconnect_action_strings = {
		{ DisconnectAction::AlwaysNothing, QT_TR_NOOP("Do Nothing")},
		{ DisconnectAction::AlwaysSleep, QT_TR_NOOP("Enter Sleep Mode")},
		{ DisconnectAction::Ask, QT_TR_NOOP("Ask")}
	};
	auto current_disconnect_action = settings->GetDisconnectAction();
	for(const auto &p : disconnect_action_strings)
	{
		disconnect_action_combo_box->addItem(tr(p.second), (int)p.first);
		if(current_disconnect_action == p.first)
			disconnect_action_combo_box->setCurrentIndex(disconnect_action_combo_box->count() - 1);
	}
	connect(disconnect_action_combo_box, SIGNAL(currentIndexChanged(int)), this, SLOT(DisconnectActionSelected()));
	stream_page->addWidget(CreateRow(tr("Action on Disconnect"), tr("What happens to the console when the stream is closed."), disconnect_action_combo_box));

	// Decoding

	auto decode_page = AddCategory(tr("Decoding"), ":/icons/ui-chip.svg");

#if CHIAKI_LIB_ENABLE_PI_DECODER
	pi_decoder_toggle = new ToggleSwitch(this);
	pi_decoder_toggle->setChecked(settings->GetDecoder() == Decoder::Pi);
	connect(pi_decoder_toggle, &QAbstractButton::toggled, this, [this](bool checked) {
		this->settings->SetDecoder(checked ? Decoder::Pi : Decoder::Ffmpeg);
		UpdateHardwareDecodeEngineComboBox();
	});
	decode_page->addWidget(CreateRow(tr("Use Raspberry Pi Decoder"), QString(), pi_decoder_toggle));
#else
	pi_decoder_toggle = nullptr;
#endif

	hw_decoder_combo_box = new QComboBox(this);
	hw_decoder_combo_box->addItem(tr("none"), QString());
	auto current_hw_decoder = settings->GetHardwareDecoder();
	enum AVHWDeviceType hw_dev = AV_HWDEVICE_TYPE_NONE;
	while(true)
	{
		hw_dev = av_hwdevice_iterate_types(hw_dev);
		if(hw_dev == AV_HWDEVICE_TYPE_NONE)
			break;
		QString name = QString::fromUtf8(av_hwdevice_get_type_name(hw_dev));
		hw_decoder_combo_box->addItem(name, name);
		if(current_hw_decoder == name)
			hw_decoder_combo_box->setCurrentIndex(hw_decoder_combo_box->count() - 1);
	}
	connect(hw_decoder_combo_box, SIGNAL(currentIndexChanged(int)), this, SLOT(HardwareDecodeEngineSelected()));
	decode_page->addWidget(CreateRow(tr("Hardware Decoding"), tr("Decodes the video on the GPU. On the Steam Deck, use vaapi."), hw_decoder_combo_box));
	UpdateHardwareDecodeEngineComboBox();

	// Registered consoles

	auto consoles_page = AddCategory(tr("Registered Consoles"), ":/icons/ui-gamepad.svg");
	registered_hosts_layout = new QVBoxLayout();
	registered_hosts_layout->setSpacing(10);
	consoles_page->addLayout(registered_hosts_layout);

	auto register_button = new QPushButton(tr("Register"), this);
	register_button->setAutoDefault(false);
	connect(register_button, &QPushButton::clicked, this, &SettingsDialog::RegisterNewHost);
	consoles_page->addWidget(CreateRow(tr("Register New Console"),
				tr("Pair Gordeck with a PS4 or PS5 using the PIN shown on the console."), register_button, 220));

	// Keyboard mapping

	auto keys_page = AddCategory(tr("Keyboard"), ":/icons/ui-keyboard.svg");
	auto keys_description = new QLabel(tr("Keys that control the console during the stream when using a keyboard. "
				"Select a key and press the new key to change it."), this);
	keys_description->setWordWrap(true);
	keys_description->setStyleSheet("font-size: 11pt; color: #8e9bb4;");
	keys_page->addWidget(keys_description);

	auto keys_grid = new QGridLayout();
	keys_grid->setHorizontalSpacing(12);
	keys_grid->setVerticalSpacing(10);
	keys_page->addLayout(keys_grid);

	QMap<int, Qt::Key> key_map = this->settings->GetControllerMapping();
	int i = 0;
	for(auto it = key_map.begin(); it != key_map.end(); ++it, ++i)
	{
		int chiaki_button = it.key();
		auto button = new QPushButton(QKeySequence(it.value()).toString(), this);
		button->setAutoDefault(false);
		keys_grid->addWidget(CreateRow(Settings::GetChiakiControllerButtonName(chiaki_button), QString(), button, 130), i / 2, i % 2);
		// Launch key capture dialog on clicked event
		connect(button, &QPushButton::clicked, this, [this, chiaki_button, button]() {
			auto dialog = new SettingsKeyCaptureDialog(this);
			// Store captured key to settings and change button label on KeyCaptured event
			connect(dialog, &SettingsKeyCaptureDialog::KeyCaptured, button, [this, button, chiaki_button](Qt::Key key) {
				button->setText(QKeySequence(key).toString());
				this->settings->SetControllerButtonMapping(chiaki_button, key);
			});
			dialog->exec();
			dialog->deleteLater();
		});
	}

	// About

	auto about_page = AddCategory(tr("About"), ":/icons/ui-info.svg");

	auto about_layout = new QHBoxLayout();
	about_layout->setSpacing(20);
	auto about_icon = new QLabel(this);
	about_icon->setPixmap(Theme::RenderIcon(":/icons/gordeck.svg", 96, QColor(), devicePixelRatioF()));
	about_icon->setAlignment(Qt::AlignTop);
	about_layout->addWidget(about_icon);
	auto about_label = new QLabel(this);
	about_label->setWordWrap(true);
	about_label->setTextFormat(Qt::RichText);
	about_label->setStyleSheet("font-size: 11pt; color: #8e9bb4;");
	about_label->setText(
			"<span style=\"font-size: 20pt; font-weight: bold; color: #ecf1fa;\">Gordeck</span><br>"
			+ tr("Version %1").arg(CHIAKI_VERSION)
			+ "<p>" + tr("Based on chiaki4deck by Street Pea and Chiaki by thestr4ng3r.") + "</p>"
			+ "<p>" + tr("This program is free software: you can redistribute it and/or modify "
				"it under the terms of the GNU Affero General Public License version 3 "
				"as published by the Free Software Foundation.") + "</p>"
			+ "<p>" + tr("This program is distributed in the hope that it will be useful, "
				"but WITHOUT ANY WARRANTY; without even the implied warranty of "
				"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
				"GNU General Public License for more details.") + "</p>");
	about_layout->addWidget(about_label, 1);
	about_page->addLayout(about_layout);

	log_verbose_toggle = new ToggleSwitch(this);
	log_verbose_toggle->setChecked(settings->GetLogVerbose());
	connect(log_verbose_toggle, &QAbstractButton::toggled, this, &SettingsDialog::LogVerboseChanged);
	about_page->addWidget(CreateRow(tr("Verbose Logging"),
				tr("Warning: this logs A LOT! Don't enable it for regular use."), log_verbose_toggle));

	auto log_directory_label = new QLabel(GetLogBaseDir(), this);
	log_directory_label->setWordWrap(true);
	log_directory_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
	log_directory_label->setStyleSheet("font-size: 10pt; color: #8e9bb4;");
	about_page->addWidget(CreateRow(tr("Log Directory"), QString(), log_directory_label, 420));

	sidebar_layout->addStretch(1);

	// Hints

	auto hints = new HintBar(this);
	auto hint_navigate = hints->AddHint(HintGlyph::DPad, QString::fromUtf8("↑↓"));
	hint_navigate->setText(tr("Navigate"));
	auto hint_category = hints->AddHint(HintGlyph::Shoulders, "Ctrl+Tab");
	hint_category->setText(tr("Category"));
	hints->AddStretch();
	auto hint_select = hints->AddHint(HintGlyph::A, "Space");
	hint_select->setText(tr("Select"));
	auto hint_back = hints->AddHint(HintGlyph::B, "Esc");
	hint_back->setText(tr("Back"));
	hints->SetStyle(hint_style);
	root_layout->addWidget(hints);
	connect(hint_category, &QAbstractButton::clicked, this, [this]() {
		SetCurrentPage((GetCurrentPage() + 1) % page_stack->count());
	});
	connect(hint_select, &QAbstractButton::clicked, this, [this]() {
		if(auto button = qobject_cast<QAbstractButton *>(focusWidget()))
			button->animateClick();
	});
	connect(hint_back, &QAbstractButton::clicked, this, &QDialog::reject);

	// highlight the row of the focused control
	connect(qApp, &QApplication::focusChanged, this, [](QWidget *old, QWidget *now) {
		if(old)
			SetRowFocused(old, false);
		if(now)
		{
			SetRowFocused(now, true);
			for(QWidget *w = now; w; w = w->parentWidget())
			{
				if(auto scroll = qobject_cast<QScrollArea *>(w))
				{
					scroll->ensureWidgetVisible(now, 20, 60);
					break;
				}
			}
		}
	});

	UpdateRegisteredHosts();
	connect(settings, &Settings::RegisteredHostsUpdated, this, &SettingsDialog::UpdateRegisteredHosts);

	SetCurrentPage(0);
}

QVBoxLayout *SettingsDialog::AddPage(const QString &title, const QString &icon)
{
	Q_UNUSED(icon);

	auto scroll_area = new QScrollArea(page_stack);
	scroll_area->setWidgetResizable(true);
	scroll_area->setFrameShape(QFrame::NoFrame);
	scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	// only the controls should be reachable with up/down
	scroll_area->setFocusPolicy(Qt::NoFocus);
	scroll_area->setStyleSheet("QScrollArea { background: transparent; border: none; }"
			"QScrollArea > QWidget > QWidget { background: transparent; }");
	scroll_area->viewport()->setAutoFillBackground(false);

	auto content = new QWidget(scroll_area);
	content->setAutoFillBackground(false);
	scroll_area->setWidget(content);

	auto layout = new QVBoxLayout(content);
	layout->setContentsMargins(4, 0, 14, 8);
	layout->setSpacing(10);

	auto title_label = new QLabel(title, content);
	title_label->setStyleSheet("font-size: 17pt; font-weight: bold; color: #6cb4ff; padding-bottom: 4px;");
	layout->addWidget(title_label);

	// keep the rows at the top
	auto outer = new QVBoxLayout();
	layout->addLayout(outer);
	layout->addStretch(1);

	page_stack->addWidget(scroll_area);
	return outer;
}

QWidget *SettingsDialog::CreateRow(const QString &title, const QString &description, QWidget *control, int control_width)
{
	auto row = new QFrame(this);
	row->setObjectName("settingRow");
	row->setProperty("focused", false);

	auto layout = new QHBoxLayout(row);
	layout->setContentsMargins(20, 12, 16, 12);
	layout->setSpacing(18);

	auto text_layout = new QVBoxLayout();
	text_layout->setSpacing(2);
	auto title_label = new QLabel(title, row);
	title_label->setStyleSheet("font-size: 13pt; color: #ecf1fa; background: transparent;");
	text_layout->addWidget(title_label);
	if(!description.isEmpty())
	{
		auto description_label = new QLabel(description, row);
		description_label->setWordWrap(true);
		description_label->setStyleSheet("font-size: 10.5pt; color: #8e9bb4; background: transparent;");
		text_layout->addWidget(description_label);
	}
	layout->addLayout(text_layout, 1);

	control->setParent(row);
	if(dynamic_cast<ToggleSwitch *>(control))
		control->setFixedSize(control->sizeHint());
	else
	{
		control->setFixedWidth(control_width);
		if(!qobject_cast<QLabel *>(control))
			control->setMinimumHeight(CONTROL_HEIGHT);
	}
	layout->addWidget(control, 0, Qt::AlignVCenter | Qt::AlignRight);
	return row;
}

int SettingsDialog::GetCurrentPage() const
{
	return page_stack->currentIndex();
}

void SettingsDialog::SetCurrentPage(int index)
{
	if(index < 0 || index >= page_stack->count())
		return;
	page_stack->setCurrentIndex(index);
	for(int i=0; i<category_buttons.count(); i++)
		category_buttons[i]->setChecked(i == index);

	// focus the first control of the page
	QWidget *page = page_stack->currentWidget();
	for(auto w : page->findChildren<QWidget *>())
	{
		if((w->focusPolicy() & Qt::TabFocus) && w->isEnabled() && !qobject_cast<QScrollArea *>(w))
		{
			w->setFocus(Qt::TabFocusReason);
			return;
		}
	}
}

void SettingsDialog::keyPressEvent(QKeyEvent *event)
{
	bool ctrl = event->modifiers() & Qt::ControlModifier;
	if((ctrl && event->key() == Qt::Key_Tab) || event->key() == Qt::Key_PageDown)
	{
		SetCurrentPage((GetCurrentPage() + 1) % page_stack->count());
		return;
	}
	if((ctrl && event->key() == Qt::Key_Backtab) || event->key() == Qt::Key_PageUp)
	{
		SetCurrentPage((GetCurrentPage() + page_stack->count() - 1) % page_stack->count());
		return;
	}
	if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
	{
		// buttons are not auto default, so Enter activates the focused one instead
		if(auto button = qobject_cast<QAbstractButton *>(focusWidget()))
			button->animateClick();
		else if(auto combo = qobject_cast<QComboBox *>(focusWidget()))
			combo->showPopup();
		return;
	}
	QDialog::keyPressEvent(event);
}

void SettingsDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	// cover the main window
	if(parentWidget())
	{
		QWidget *window = parentWidget()->window();
		if(window->isFullScreen())
			setWindowState(windowState() | Qt::WindowFullScreen);
		else
			setGeometry(QRect(window->mapToGlobal(QPoint(0, 0)), window->size()));
	}
	activateWindow();
}

void SettingsDialog::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	QRectF r = rect();
	QLinearGradient base(r.topLeft(), r.bottomLeft());
	base.setColorAt(0.0, Theme::background_light);
	base.setColorAt(0.55, Theme::background);
	base.setColorAt(1.0, QColor(0x04, 0x07, 0x0f));
	painter.fillRect(r, base);
}

void SettingsDialog::LanguageSelected()
{
	QString lang = language_combo_box->currentData().toString();
	if(lang == I18n::CurrentLanguage())
		return;
	settings->SetLanguage(lang);
	I18n::SetLanguage(lang);
	language_changed = true;
	accept();
}

void SettingsDialog::ResolutionSelected()
{
	settings->SetResolution((ChiakiVideoResolutionPreset)resolution_combo_box->currentData().toInt());
	UpdateBitratePlaceholder();
}

void SettingsDialog::DisconnectActionSelected()
{
	settings->SetDisconnectAction(static_cast<DisconnectAction>(disconnect_action_combo_box->currentData().toInt()));
}

void SettingsDialog::LogVerboseChanged()
{
	settings->SetLogVerbose(log_verbose_toggle->isChecked());
}

void SettingsDialog::FPSSelected()
{
	settings->SetFPS((ChiakiVideoFPSPreset)fps_combo_box->currentData().toInt());
}

void SettingsDialog::BitrateEdited()
{
	settings->SetBitrate(bitrate_edit->text().toUInt());
}

void SettingsDialog::CodecSelected()
{
	settings->SetCodec((ChiakiCodec)codec_combo_box->currentData().toInt());
}

void SettingsDialog::AudioBufferSizeEdited()
{
	settings->SetAudioBufferSize(audio_buffer_size_edit->text().toUInt());
}

void SettingsDialog::HardwareDecodeEngineSelected()
{
	settings->SetHardwareDecoder(hw_decoder_combo_box->currentData().toString());
}

void SettingsDialog::UpdateHardwareDecodeEngineComboBox()
{
	hw_decoder_combo_box->setEnabled(settings->GetDecoder() == Decoder::Ffmpeg);
}

void SettingsDialog::UpdateBitratePlaceholder()
{
	bitrate_edit->setPlaceholderText(tr("Automatic (%1)").arg(settings->GetVideoProfile().bitrate));
}

void SettingsDialog::UpdateRegisteredHosts()
{
	while(auto item = registered_hosts_layout->takeAt(0))
	{
		if(item->widget())
			item->widget()->deleteLater();
		delete item;
	}

	auto hosts = settings->GetRegisteredHosts();
	if(hosts.isEmpty())
	{
		auto label = new QLabel(tr("No console registered yet."), this);
		label->setStyleSheet("font-size: 12pt; color: #8e9bb4; padding: 8px 4px;");
		registered_hosts_layout->addWidget(label);
		return;
	}

	for(const auto &host : hosts)
	{
		auto delete_button = new QPushButton(tr("Delete"), this);
		delete_button->setAutoDefault(false);
		HostMAC mac = host.GetServerMAC();
		QString nickname = host.GetServerNickname();
		connect(delete_button, &QPushButton::clicked, this, [this, mac, nickname]() {
			if(!ConfirmDialog::Ask(this, hint_style, tr("Delete registered Console"),
					tr("Are you sure you want to delete the registered console %1 (ID %2)?").arg(nickname, mac.ToString()),
					tr("Delete"), tr("Cancel"), true))
				return;
			settings->RemoveRegisteredHost(mac);
		});
		QString description = QString("%1  •  ID %2")
			.arg(chiaki_target_is_ps5(host.GetTarget()) ? "PS5" : "PS4", mac.ToString());
		registered_hosts_layout->addWidget(CreateRow(nickname, description, delete_button, 160));
	}
}

void SettingsDialog::RegisterNewHost()
{
	RegistDialog dialog(settings, QString(), this);
	dialog.exec();
}
