// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_STREAMSESSION_H
#define CHIAKI_STREAMSESSION_H

#include <chiaki/session.h>
#include <chiaki/opusdecoder.h>
#include <chiaki/opusencoder.h>
#include <chiaki/ffmpegdecoder.h>

#if CHIAKI_LIB_ENABLE_PI_DECODER
#include <chiaki/pidecoder.h>
#endif

#if CHIAKI_GUI_ENABLE_SETSU
#include <setsu.h>
#include <chiaki/orientation.h>
#endif

#include "exception.h"
#include "sessionlog.h"
#include "controllermanager.h"
#include "settings.h"

#include <QObject>
#include <QImage>
#include <QMouseEvent>
#include <QTimer>
#include <QMutex>
#include <QQueue>

class QIODevice;
class QKeyEvent;
class Settings;

class ChiakiException: public Exception
{
	public:
		explicit ChiakiException(const QString &msg) : Exception(msg) {};
};

struct StreamSessionConnectInfo
{
	Settings *settings;
	QMap<Qt::Key, int> key_map;
	Decoder decoder;
	QString hw_decoder;
	QString audio_out_device;
	QString audio_in_device;
	bool mic_auto_start;
	MicToggleButton mic_toggle_button;
	uint32_t log_level_mask;
	QString log_file;
	ChiakiTarget target;
	QString host;
	QByteArray regist_key;
	QByteArray morning;
	QString initial_login_pin;
	ChiakiConnectVideoProfile video_profile;
	unsigned int audio_buffer_size;
	float rumble_multiplier;
	bool fullscreen;
	bool zoom;
	bool stretch;
	bool enable_keyboard;

	StreamSessionConnectInfo(Settings *settings, ChiakiTarget target, QString host, QByteArray regist_key, QByteArray morning, QString initial_login_pin, bool fullscreen, bool zoom, bool stretch);
};

// holding the PS/home button this long force-quits the stream
// (some controllers, like the DualSense, turn off when the button is held for about 10 s)
#define HOME_HOLD_QUIT_MS 7000

class StreamSession : public QObject
{
	friend class StreamSessionPrivate;

	Q_OBJECT

	private:
		SessionLog log;
		ChiakiSession session;
		ChiakiOpusDecoder opus_decoder;
		bool connected;

		// microphone: captured with SDL, encoded to Opus and sent to the console
		static const int MICROPHONE_SAMPLES = 480; // 10 ms @ 48 kHz, one Opus frame
		ChiakiOpusEncoder opus_encoder;
		uint32_t audio_in; // SDL_AudioDeviceID, 0 when closed
		QString audio_in_device_name;
		bool mic_connected; // announced to the console
		bool mic_muted;
		bool allow_unmute; // only once the stream is up
		bool mic_auto_start;
		MicToggleButton mic_toggle_button;
		int16_t mic_buf[MICROPHONE_SAMPLES * 2];
		size_t mic_buf_fill; // bytes
		void InitMic();
		void CloseMic();
		void QueueMicData(const uint8_t *data, size_t size);

		QHash<int, Controller *> controllers;

		// holding the PS/home button for a while force-quits the stream
		QTimer *home_hold_timer;
		bool home_held;
#if CHIAKI_GUI_ENABLE_SETSU
		Setsu *setsu;
		QMap<QPair<QString, SetsuTrackingId>, uint8_t> setsu_ids;
		ChiakiControllerState setsu_state;
		SetsuDevice *setsu_motion_device;
		ChiakiOrientationTracker orient_tracker;
		bool orient_dirty;
#endif

		ChiakiControllerState keyboard_state;
		ChiakiControllerState touch_state;
		QMap<int, uint8_t> touch_tracker;
		int8_t mouse_touch_id;

		ChiakiFfmpegDecoder *ffmpeg_decoder;
		void TriggerFfmpegFrameAvailable();
#if CHIAKI_LIB_ENABLE_PI_DECODER
		ChiakiPiDecoder *pi_decoder;
#endif

		QString audio_out_device_name;
		unsigned int audio_buffer_size; // bytes, see Settings::GetAudioBufferSize()
		uint32_t audio_out; // SDL_AudioDeviceID, 0 when no device is open
		unsigned int audio_out_sample_size; // bytes per sample frame (channels * 16 bit)
		unsigned int audio_queue_max; // queued bytes beyond which stale audio is dropped
		void CloseAudio();

		QMap<Qt::Key, int> key_map;

		void PushAudioFrame(int16_t *buf, size_t samples_count);

		// PS5 haptics (DualSense PCM stream) turned into plain rumble for controllers without
		// haptics: each 10 ms packet becomes a strength, a timer averages them and rumbles.
		void PushHapticsFrame(uint8_t *buf, size_t buf_size);
		QMutex rumble_haptics_mutex;
		QQueue<uint16_t> rumble_haptics;
		bool rumble_haptics_on;
		float rumble_multiplier; // from Settings::GetRumbleMultiplier()
		uint16_t ScaleRumble(uint32_t strength);
		void ApplyRumbleHaptics();
#if CHIAKI_GUI_ENABLE_SETSU
		void HandleSetsuEvent(SetsuEvent *event);
#endif

	private slots:
		void InitAudio(unsigned int channels, unsigned int rate);
		void Event(ChiakiEvent *event);

	public:
		explicit StreamSession(const StreamSessionConnectInfo &connect_info, QObject *parent = nullptr);
		~StreamSession();

		bool IsConnected()	{ return connected; }

		void Start();
		void Stop();
		void GoToBed();

		void SetLoginPIN(const QString &pin);
		bool IsMicMuted()	{ return mic_muted; }

		ChiakiLog *GetChiakiLog()				{ return log.GetChiakiLog(); }
		QList<Controller *> GetControllers()	{ return controllers.values(); }
		ChiakiFfmpegDecoder *GetFfmpegDecoder()	{ return ffmpeg_decoder; }
#if CHIAKI_LIB_ENABLE_PI_DECODER
		ChiakiPiDecoder *GetPiDecoder()	{ return pi_decoder; }
#endif

		void HandleKeyboardEvent(QKeyEvent *event);
		void HandleTouchEvent(QTouchEvent *event);
		void HandleMouseReleaseEvent(QMouseEvent *event);
		void HandleMousePressEvent(QMouseEvent *event);
		void HandleMouseMoveEvent(QMouseEvent *event, float width, float height);

	signals:
		void FfmpegFrameAvailable();
		void SessionQuit(ChiakiQuitReason reason, const QString &reason_str);
		void LoginPINRequested(bool incorrect);

		/**
		 * The PS/home button was pressed (true) or released (false)
		 */
		void HomeButtonHeld(bool held);
		void ForceQuitRequested();
		void MicMutedChanged(bool muted);

	public slots:
		void ToggleMute();

	private slots:
		void UpdateGamepads();
		void SendFeedbackState();
		void HandleControllerButtonForMic(int sdl_button);
};

Q_DECLARE_METATYPE(ChiakiQuitReason)

#endif // CHIAKI_STREAMSESSION_H
