// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <streamsession.h>
#include <settings.h>
#include <controllermanager.h>

#include <chiaki/base64.h>

#include <QKeyEvent>

#include <SDL.h>

#include <algorithm>

#include <cstring>
#include <cstdlib>

#include <chiaki/session.h>

#define SETSU_UPDATE_INTERVAL_MS 4
// haptics-derived rumble: one haptics packet is 10 ms, rumble is refreshed every 3 packets
#define RUMBLE_HAPTICS_PACKETS_PER_RUMBLE 3
#define RUMBLE_HAPTICS_INTERVAL_MS (RUMBLE_HAPTICS_PACKETS_PER_RUMBLE * 10)
// average amplitude (0..65535 scale) below which a haptics packet counts as silence
#define HAPTIC_RUMBLE_MIN_STRENGTH 100
// DualSense is 1919 x 1079, DualShock4 is 1920 x 942 (take highest of both here to map completely to both)
#define PS_TOUCHPAD_MAX_X 1920
#define PS_TOUCHPAD_MAX_Y 1079

StreamSessionConnectInfo::StreamSessionConnectInfo(Settings *settings, ChiakiTarget target, QString host, QByteArray regist_key, QByteArray morning, QString initial_login_pin, bool fullscreen, bool zoom, bool stretch)
	: settings(settings)
{
	key_map = settings->GetControllerMappingForDecoding();
	decoder = settings->GetDecoder();
	hw_decoder = settings->GetHardwareDecoder();
	audio_out_device = settings->GetAudioOutDevice();
	audio_in_device = settings->GetAudioInDevice();
	mic_auto_start = settings->GetMicAutoStart();
	mic_toggle_button = settings->GetMicToggleButton();
	log_level_mask = settings->GetLogLevelMask();
	log_file = CreateLogFilename();
	video_profile = settings->GetVideoProfile();
	this->target = target;
	this->host = host;
	this->regist_key = regist_key;
	this->morning = morning;
	this->initial_login_pin = initial_login_pin;
	audio_buffer_size = settings->GetAudioBufferSize();
	rumble_multiplier = settings->GetRumbleMultiplier();
	this->fullscreen = fullscreen;
	this->zoom = zoom;
	this->stretch = stretch;
	this->enable_keyboard = false; // TODO: from settings
}

static void AudioSettingsCb(uint32_t channels, uint32_t rate, void *user);
static void AudioFrameCb(int16_t *buf, size_t samples_count, void *user);
static void HapticsFrameCb(uint8_t *buf, size_t buf_size, void *user);
static void EventCb(ChiakiEvent *event, void *user);
#if CHIAKI_GUI_ENABLE_SETSU
static void SessionSetsuCb(SetsuEvent *event, void *user);
#endif
static void FfmpegFrameCb(ChiakiFfmpegDecoder *decoder, void *user);

StreamSession::StreamSession(const StreamSessionConnectInfo &connect_info, QObject *parent)
	: QObject(parent),
	log(this, connect_info.log_level_mask, connect_info.log_file),
	ffmpeg_decoder(nullptr),
#if CHIAKI_LIB_ENABLE_PI_DECODER
	pi_decoder(nullptr),
#endif
	audio_in(0),
	mic_connected(false),
	mic_muted(true),
	allow_unmute(false),
	mic_buf_fill(0),
	audio_out(0),
	audio_out_sample_size(0),
	audio_queue_max(0)
{
	connected = false;
	ChiakiErrorCode err;

	home_held = false;
	home_hold_timer = new QTimer(this);
	home_hold_timer->setSingleShot(true);
	home_hold_timer->setInterval(HOME_HOLD_QUIT_MS);
	connect(home_hold_timer, &QTimer::timeout, this, &StreamSession::ForceQuitRequested);

#if CHIAKI_LIB_ENABLE_PI_DECODER
	if(connect_info.decoder == Decoder::Pi)
	{
		pi_decoder = CHIAKI_NEW(ChiakiPiDecoder);
		if(chiaki_pi_decoder_init(pi_decoder, log.GetChiakiLog()) != CHIAKI_ERR_SUCCESS)
			throw ChiakiException(tr("Failed to initialize Raspberry Pi Decoder"));
	}
	else
	{
#endif
		ffmpeg_decoder = new ChiakiFfmpegDecoder;
		ChiakiLogSniffer sniffer;
		chiaki_log_sniffer_init(&sniffer, CHIAKI_LOG_ALL, GetChiakiLog());
		err = chiaki_ffmpeg_decoder_init(ffmpeg_decoder,
				chiaki_log_sniffer_get_log(&sniffer),
				chiaki_target_is_ps5(connect_info.target) ? connect_info.video_profile.codec : CHIAKI_CODEC_H264,
				connect_info.hw_decoder.isEmpty() ? NULL : connect_info.hw_decoder.toUtf8().constData(),
				FfmpegFrameCb, this);
		if(err != CHIAKI_ERR_SUCCESS)
		{
			QString log = QString::fromUtf8(chiaki_log_sniffer_get_buffer(&sniffer));
			chiaki_log_sniffer_fini(&sniffer);
			throw ChiakiException(tr("Failed to initialize FFMPEG Decoder:") + "\n" + log);
		}
		chiaki_log_sniffer_fini(&sniffer);
		ffmpeg_decoder->log = GetChiakiLog();
#if CHIAKI_LIB_ENABLE_PI_DECODER
	}
#endif

	audio_out_device_name = connect_info.audio_out_device;
	audio_in_device_name = connect_info.audio_in_device;
	mic_auto_start = connect_info.mic_auto_start;
	mic_toggle_button = connect_info.mic_toggle_button;
	chiaki_opus_encoder_init(&opus_encoder, log.GetChiakiLog());

	chiaki_opus_decoder_init(&opus_decoder, log.GetChiakiLog());
	audio_buffer_size = connect_info.audio_buffer_size;
	rumble_multiplier = connect_info.rumble_multiplier;

	QByteArray host_str = connect_info.host.toUtf8();

	ChiakiConnectInfo chiaki_connect_info = {};
	chiaki_connect_info.ps5 = chiaki_target_is_ps5(connect_info.target);
	chiaki_connect_info.host = host_str.constData();
	chiaki_connect_info.video_profile = connect_info.video_profile;
	chiaki_connect_info.video_profile_auto_downgrade = true;
	chiaki_connect_info.enable_keyboard = false;
	// Always talk to the console as a DualSense: on the PS5 that is the only way to get any
	// vibration at all (as a haptics stream, converted to rumble below). The PS4 ignores it.
	chiaki_connect_info.enable_dualsense = true;

#if CHIAKI_LIB_ENABLE_PI_DECODER
	if(connect_info.decoder == Decoder::Pi && chiaki_connect_info.video_profile.codec != CHIAKI_CODEC_H264)
	{
		CHIAKI_LOGW(GetChiakiLog(), "A codec other than H264 was requested for Pi Decoder. Falling back to it.");
		chiaki_connect_info.video_profile.codec = CHIAKI_CODEC_H264;
	}
#endif

	if(connect_info.regist_key.size() != sizeof(chiaki_connect_info.regist_key))
		throw ChiakiException(tr("RegistKey invalid"));
	memcpy(chiaki_connect_info.regist_key, connect_info.regist_key.constData(), sizeof(chiaki_connect_info.regist_key));

	if(connect_info.morning.size() != sizeof(chiaki_connect_info.morning))
		throw ChiakiException(tr("Morning invalid"));
	memcpy(chiaki_connect_info.morning, connect_info.morning.constData(), sizeof(chiaki_connect_info.morning));

	chiaki_controller_state_set_idle(&keyboard_state);
	chiaki_controller_state_set_idle(&touch_state);
	touch_tracker=QMap<int, uint8_t>();
	mouse_touch_id=-1;

	err = chiaki_session_init(&session, &chiaki_connect_info, GetChiakiLog());
	if(err != CHIAKI_ERR_SUCCESS)
		throw ChiakiException(tr("Session Init failed: %1").arg(QString::fromLocal8Bit(chiaki_error_string(err))));

	chiaki_opus_decoder_set_cb(&opus_decoder, AudioSettingsCb, AudioFrameCb, this);
	ChiakiAudioSink audio_sink;
	chiaki_opus_decoder_get_sink(&opus_decoder, &audio_sink);
	chiaki_session_set_audio_sink(&session, &audio_sink);

	// microphone format the console expects: stereo 16 bit, 48 kHz, 10 ms frames
	ChiakiAudioHeader mic_header;
	chiaki_audio_header_set(&mic_header, 2, 16, 48000, MICROPHONE_SAMPLES);
	chiaki_opus_encoder_header(&mic_header, &opus_encoder, &session);

	ChiakiAudioSink haptics_sink = {};
	haptics_sink.user = this;
	haptics_sink.frame_cb = HapticsFrameCb;
	chiaki_session_set_haptics_sink(&session, &haptics_sink);
	rumble_haptics_on = false;
	auto rumble_haptics_timer = new QTimer(this);
	connect(rumble_haptics_timer, &QTimer::timeout, this, &StreamSession::ApplyRumbleHaptics);
	rumble_haptics_timer->start(RUMBLE_HAPTICS_INTERVAL_MS);

#if CHIAKI_LIB_ENABLE_PI_DECODER
	if(pi_decoder)
		chiaki_session_set_video_sample_cb(&session, chiaki_pi_decoder_video_sample_cb, pi_decoder);
	else
	{
#endif
		chiaki_session_set_video_sample_cb(&session, chiaki_ffmpeg_decoder_video_sample_cb, ffmpeg_decoder);
#if CHIAKI_LIB_ENABLE_PI_DECODER
	}
#endif

	chiaki_session_set_event_cb(&session, EventCb, this);

#if CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	connect(ControllerManager::GetInstance(), &ControllerManager::AvailableControllersUpdated, this, &StreamSession::UpdateGamepads);
#endif

#if CHIAKI_GUI_ENABLE_SETSU
	setsu_motion_device = nullptr;
	chiaki_controller_state_set_idle(&setsu_state);
	setsu_ids=QMap<QPair<QString, SetsuTrackingId>, uint8_t>();
	orient_dirty = true;
	chiaki_orientation_tracker_init(&orient_tracker);
	setsu = setsu_new();
	auto timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this]{
		setsu_poll(setsu, SessionSetsuCb, this);
		if(orient_dirty)
		{
			chiaki_orientation_tracker_apply_to_controller_state(&orient_tracker, &setsu_state);
			SendFeedbackState();
			orient_dirty = false;
		}
	});
	timer->start(SETSU_UPDATE_INTERVAL_MS);
#endif

	key_map = connect_info.key_map;
	UpdateGamepads();
}

StreamSession::~StreamSession()
{
	CloseMic(); // before join: the capture callback sends into the session
	chiaki_session_join(&session);
	chiaki_session_fini(&session);
	CloseAudio(); // only after join: the session thread is what pushes audio
	chiaki_opus_decoder_fini(&opus_decoder);
	chiaki_opus_encoder_fini(&opus_encoder);
#if CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	for(auto controller : controllers)
		delete controller;
#endif
#if CHIAKI_GUI_ENABLE_SETSU
	setsu_free(setsu);
#endif
#if CHIAKI_LIB_ENABLE_PI_DECODER
	if(pi_decoder)
	{
		chiaki_pi_decoder_fini(pi_decoder);
		free(pi_decoder);
	}
#endif
	if(ffmpeg_decoder)
	{
		chiaki_ffmpeg_decoder_fini(ffmpeg_decoder);
		delete ffmpeg_decoder;
	}
}

void StreamSession::Start()
{
	ChiakiErrorCode err = chiaki_session_start(&session);
	if(err != CHIAKI_ERR_SUCCESS)
	{
		chiaki_session_fini(&session);
		throw ChiakiException(tr("Session Start failed"));
	}
}

void StreamSession::Stop()
{
	chiaki_session_stop(&session);
}

void StreamSession::GoToBed()
{
	chiaki_session_goto_bed(&session);
}

void StreamSession::SetLoginPIN(const QString &pin)
{
	QByteArray data = pin.toUtf8();
	chiaki_session_set_login_pin(&session, (const uint8_t *)data.constData(), data.size());
}

void StreamSession::HandleMousePressEvent(QMouseEvent *event)
{
	// left button for touchpad gestures, others => touchpad click
	if (event->button() != Qt::MouseButton::LeftButton)
		keyboard_state.buttons |= CHIAKI_CONTROLLER_BUTTON_TOUCHPAD;
	SendFeedbackState();
}

void StreamSession::HandleMouseReleaseEvent(QMouseEvent *event)
{
	// left button => end of touchpad gesture
	if (event->button() == Qt::LeftButton)
	{
		if(mouse_touch_id >= 0)
		{
			chiaki_controller_state_stop_touch(&keyboard_state, (uint8_t)mouse_touch_id);
			mouse_touch_id = -1;
		}
	}
	keyboard_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_TOUCHPAD;
	SendFeedbackState();
}

void StreamSession::HandleMouseMoveEvent(QMouseEvent *event, float width, float height)
{
	// left button with move => touchpad gesture, otherwise ignore
	if (event->buttons() == Qt::LeftButton)
	{
		float x = event->screenPos().x();
		float y = event->screenPos().y();
		float psx = x * ((float)PS_TOUCHPAD_MAX_X / width);
		float psy = y * ((float)PS_TOUCHPAD_MAX_Y / height);
		// if touch id is set, move, otherwise start
		if(mouse_touch_id >= 0)
			chiaki_controller_state_set_touch_pos(&keyboard_state, (uint8_t)mouse_touch_id, (uint16_t)psx, (uint16_t)psy);
		else
			mouse_touch_id=chiaki_controller_state_start_touch(&keyboard_state, (uint16_t)psx, (uint16_t)psy);	
	}
	SendFeedbackState();
}

void StreamSession::HandleKeyboardEvent(QKeyEvent *event)
{
	if(event->key() == Qt::Key_M && event->type() == QEvent::KeyPress && !event->isAutoRepeat())
	{
		ToggleMute();
		return;
	}
	if(key_map.contains(Qt::Key(event->key())) == false)
		return;

	if(event->isAutoRepeat())
		return;

	int button = key_map[Qt::Key(event->key())];
	bool press_event = event->type() == QEvent::Type::KeyPress;

	switch(button)
	{
		case CHIAKI_CONTROLLER_ANALOG_BUTTON_L2:
			keyboard_state.l2_state = press_event ? 0xff : 0;
			break;
		case CHIAKI_CONTROLLER_ANALOG_BUTTON_R2:
			keyboard_state.r2_state = press_event ? 0xff : 0;
			break;
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_Y_UP):
			keyboard_state.right_y = press_event ? -0x7fff : 0;
			break;
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_Y_DOWN):
			keyboard_state.right_y = press_event ? 0x7fff : 0;
			break;
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_X_UP):
			keyboard_state.right_x = press_event ? 0x7fff : 0;
			break;
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_RIGHT_X_DOWN):
			keyboard_state.right_x = press_event ? -0x7fff : 0;
			break;
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_Y_UP):
			keyboard_state.left_y = press_event ? -0x7fff : 0;
			break;
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_Y_DOWN):
			keyboard_state.left_y = press_event ? 0x7fff : 0;
			break;
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_X_UP):
			keyboard_state.left_x = press_event ? 0x7fff : 0;
			break;
		case static_cast<int>(ControllerButtonExt::ANALOG_STICK_LEFT_X_DOWN):
			keyboard_state.left_x = press_event ? -0x7fff : 0;
			break;
		default:
			if(press_event)
				keyboard_state.buttons |= button;
			else
				keyboard_state.buttons &= ~button;
			break;
	}

	SendFeedbackState();
}

void StreamSession::HandleTouchEvent(QTouchEvent *event)
{
	//unset touchpad (we will set it if user touches edge of screen)
	touch_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_TOUCHPAD;

	const QList<QTouchEvent::TouchPoint> touchPoints = event->touchPoints();

	for (const QTouchEvent::TouchPoint &touchPoint : touchPoints)
	{
		int id = touchPoint.id();
		switch (touchPoint.state())
		{
			//skip unchanged touchpoints
			case Qt::TouchPointStationary:
				continue;
			case Qt::TouchPointPressed:
			case Qt::TouchPointMoved:
			{
				float norm_x = touchPoint.normalizedPos().x();
				float norm_y = touchPoint.normalizedPos().y();
				
				// Touching edges of screen is a touchpad click
				if(norm_x <= 0.05 || norm_x >= 0.95 || norm_y <= 0.05 || norm_y >= 0.95)
					touch_state.buttons |= CHIAKI_CONTROLLER_BUTTON_TOUCHPAD;
				// Scale to PS TouchPad since that's what PS Console expects
				float psx = norm_x * (float)PS_TOUCHPAD_MAX_X;
				float psy = norm_y * (float)PS_TOUCHPAD_MAX_Y;
				auto it = touch_tracker.find(id);
				if(it == touch_tracker.end())
				{
					int8_t cid = chiaki_controller_state_start_touch(&touch_state, (uint16_t)psx, (uint16_t)psy);
					// if cid < 0 => already too many multi-touches
					if(cid >= 0)
						touch_tracker[id] = (uint8_t)cid;
					else
						break;
				}
				else
					chiaki_controller_state_set_touch_pos(&touch_state, it.value(), (uint16_t)psx, (uint16_t)psy);
				break;
			}
			case Qt::TouchPointReleased:
			{
				for(auto it=touch_tracker.begin(); it!=touch_tracker.end(); it++)
				{
					if(it.key() == id)
					{
						chiaki_controller_state_stop_touch(&touch_state, it.value());
						touch_tracker.erase(it);
						break;
					}
				}
				break;
			}
		}
	}
	SendFeedbackState();
}

void StreamSession::UpdateGamepads()
{
#if CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER
	for(auto controller_id : controllers.keys())
	{
		auto controller = controllers[controller_id];
		if(!controller->IsConnected())
		{
			CHIAKI_LOGI(log.GetChiakiLog(), "Controller %d disconnected", controller->GetDeviceID());
			controllers.remove(controller_id);
			delete controller;
		}
	}

	const auto available_controllers = ControllerManager::GetInstance()->GetAvailableControllers();
	for(auto controller_id : available_controllers)
	{
		if(!controllers.contains(controller_id))
		{
			auto controller = ControllerManager::GetInstance()->OpenController(controller_id);
			if(!controller)
			{
				CHIAKI_LOGE(log.GetChiakiLog(), "Failed to open controller %d", controller_id);
				continue;
			}
			CHIAKI_LOGI(log.GetChiakiLog(), "Controller %d opened: \"%s\"", controller_id, controller->GetName().toLocal8Bit().constData());
			connect(controller, &Controller::StateChanged, this, &StreamSession::SendFeedbackState);
			connect(controller, &Controller::SdlButtonPressed, this, &StreamSession::HandleControllerButtonForMic);
			controllers[controller_id] = controller;
		}
	}
	
	SendFeedbackState();
#endif
}

static void CopyMotion(ChiakiControllerState *dst, const ChiakiControllerState *src)
{
	dst->gyro_x = src->gyro_x; dst->gyro_y = src->gyro_y; dst->gyro_z = src->gyro_z;
	dst->accel_x = src->accel_x; dst->accel_y = src->accel_y; dst->accel_z = src->accel_z;
	dst->orient_x = src->orient_x; dst->orient_y = src->orient_y; dst->orient_z = src->orient_z; dst->orient_w = src->orient_w;
}

void StreamSession::SendFeedbackState()
{
	ChiakiControllerState state;
	chiaki_controller_state_set_idle(&state);

	// Motion (gyro, accel, orient) is not merged by chiaki_controller_state_or, so exactly one
	// source provides it: setsu first (the Steam Deck's own sensors or a DualShock/DualSense,
	// both seen as evdev motion devices), overridden by a controller exposing sensors via SDL.
#if CHIAKI_GUI_ENABLE_SETSU
	state = setsu_state;
#endif

	for(auto controller : controllers)
	{
		auto controller_state = controller->GetState();
		chiaki_controller_state_or(&state, &state, &controller_state);
		if(controller->HasMotion())
			CopyMotion(&state, &controller_state);
	}

	chiaki_controller_state_or(&state, &state, &keyboard_state);
	chiaki_controller_state_or(&state, &state, &touch_state);
	chiaki_session_set_controller_state(&session, &state);

	bool home = state.buttons & CHIAKI_CONTROLLER_BUTTON_PS;
	if(home != home_held)
	{
		home_held = home;
		if(home)
			home_hold_timer->start();
		else
			home_hold_timer->stop();
		emit HomeButtonHeld(home);
	}
}

void StreamSession::InitAudio(unsigned int channels, unsigned int rate)
{
	bool first_time = !allow_unmute;
	allow_unmute = true; // the audio header means the stream is up, the console can take a microphone now
	if(first_time && mic_auto_start && mic_muted)
		QTimer::singleShot(500, this, &StreamSession::ToggleMute); // let the control channel settle first
	CloseAudio();

	if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		CHIAKI_LOGE(log.GetChiakiLog(), "Failed to initialize SDL audio: %s", SDL_GetError());
		return;
	}

	audio_out_sample_size = channels * sizeof(int16_t);

	// The device buffer is the main source of output latency, so it is kept small.
	// The setting is in bytes for compatibility with the value the old Qt backend used.
	SDL_AudioSpec want = {};
	want.freq = (int)rate;
	want.channels = (Uint8)channels;
	want.format = AUDIO_S16SYS;
	want.samples = (Uint16)std::max(64u, audio_buffer_size / audio_out_sample_size);

	SDL_AudioSpec have = {};
	QByteArray device_name_utf8 = audio_out_device_name.toUtf8();
	const char *device_name = audio_out_device_name.isEmpty() ? nullptr : device_name_utf8.constData();
	audio_out = SDL_OpenAudioDevice(device_name, 0, &want, &have, 0);
	if(!audio_out && device_name)
	{
		CHIAKI_LOGW(log.GetChiakiLog(), "Failed to open Audio Device \"%s\" (%s), falling back to the default device",
				device_name, SDL_GetError());
		device_name = nullptr;
		audio_out = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
	}
	if(!audio_out)
	{
		CHIAKI_LOGE(log.GetChiakiLog(), "Failed to open default Audio Device: %s", SDL_GetError());
		return;
	}

	// Audio is queued as it arrives from the console. A Wi-Fi stall or clock drift between the
	// console and the Steam Deck would otherwise accumulate into permanent extra delay, so when
	// the queue grows past this limit the stale audio is dropped and playback catches up.
	unsigned int bytes_per_ms = rate * audio_out_sample_size / 1000;
	audio_queue_max = std::max(4 * audio_buffer_size, 100 * bytes_per_ms);

	SDL_PauseAudioDevice(audio_out, 0);

	CHIAKI_LOGI(log.GetChiakiLog(), "Audio Device \"%s\" opened with SDL (%s) with %u channels @ %u Hz, device buffer %u samples (%u ms), max queue %u ms",
			device_name ? device_name : "default", SDL_GetCurrentAudioDriver(), channels, rate,
			(unsigned int)have.samples, (unsigned int)have.samples * 1000 / rate, audio_queue_max / bytes_per_ms);
}

void StreamSession::PushHapticsFrame(uint8_t *buf, size_t buf_size)
{
	// 16-bit stereo PCM at 3 kHz, 120 bytes per 10 ms packet
	const size_t sample_size = 2 * sizeof(int16_t);
	if(buf_size < sample_size || (buf_size % sample_size) != 0)
	{
		CHIAKI_LOGW(log.GetChiakiLog(), "Haptics packet with invalid size %zu", buf_size);
		return;
	}

	// mean absolute amplitude per channel, doubled to use the full 16-bit range
	uint32_t suml = 0, sumr = 0;
	size_t count = buf_size / sample_size;
	for(size_t i=0; i<count; i++)
	{
		int16_t l, r;
		memcpy(&l, buf + i * sample_size, sizeof(int16_t));
		memcpy(&r, buf + i * sample_size + sizeof(int16_t), sizeof(int16_t));
		suml += (uint32_t)abs(l) * 2;
		sumr += (uint32_t)abs(r) * 2;
	}
	uint32_t left = suml / count;
	uint32_t right = sumr / count;
	if(left <= HAPTIC_RUMBLE_MIN_STRENGTH)
		left = 0;
	if(right <= HAPTIC_RUMBLE_MIN_STRENGTH)
		right = 0;
	uint16_t strength = ScaleRumble(left > right ? left : right);

	QMutexLocker locker(&rumble_haptics_mutex);
	rumble_haptics.enqueue(strength);
	while(rumble_haptics.size() > 2 * RUMBLE_HAPTICS_PACKETS_PER_RUMBLE) // never let a backlog add latency
		rumble_haptics.dequeue();
}

// Applies the user's intensity multiplier and clamps to 16 bit. Values just above zero are
// raised to a floor, because some controllers shift the strength down to 7 bits and would
// otherwise ignore them.
uint16_t StreamSession::ScaleRumble(uint32_t strength)
{
	if(strength == 0)
		return 0;
	float scaled = (float)strength * rumble_multiplier;
	if(scaled > (float)UINT16_MAX)
		scaled = (float)UINT16_MAX;
	uint32_t result = (uint32_t)scaled;
	if(result < (1 << 9))
		result = 1 << 9;
	return (uint16_t)result;
}

void StreamSession::ApplyRumbleHaptics()
{
	uint32_t strength = 0;
	bool got_any = false;
	{
		QMutexLocker locker(&rumble_haptics_mutex);
		for(size_t i=0; i<RUMBLE_HAPTICS_PACKETS_PER_RUMBLE && !rumble_haptics.isEmpty(); i++)
		{
			strength += rumble_haptics.dequeue();
			got_any = true;
		}
	}
	strength /= RUMBLE_HAPTICS_PACKETS_PER_RUMBLE;
	if(!got_any && !rumble_haptics_on)
		return;
	// keep refreshing while rumbling, and send one final zero to stop
	for(auto controller : controllers)
		controller->SetRumble16((uint16_t)strength, (uint16_t)strength);
	rumble_haptics_on = strength > 0;
}

void StreamSession::InitMic()
{
	CloseMic();

	if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		CHIAKI_LOGE(log.GetChiakiLog(), "Failed to initialize SDL audio for the microphone: %s", SDL_GetError());
		return;
	}

	mic_buf_fill = 0;

	SDL_AudioSpec want = {};
	want.freq = 48000;
	want.channels = 2;
	want.format = AUDIO_S16SYS;
	want.samples = MICROPHONE_SAMPLES;
	want.userdata = this;
	want.callback = [](void *userdata, Uint8 *stream, int len) {
		static_cast<StreamSession *>(userdata)->QueueMicData(stream, (size_t)len);
	};

	SDL_AudioSpec have = {};
	QByteArray device_name_utf8 = audio_in_device_name.toUtf8();
	const char *device_name = audio_in_device_name.isEmpty() ? nullptr : device_name_utf8.constData();
	audio_in = SDL_OpenAudioDevice(device_name, 1, &want, &have, 0);
	if(!audio_in && device_name)
	{
		CHIAKI_LOGW(log.GetChiakiLog(), "Failed to open Microphone \"%s\" (%s), falling back to the default one",
				device_name, SDL_GetError());
		device_name = nullptr;
		audio_in = SDL_OpenAudioDevice(nullptr, 1, &want, &have, 0);
	}
	if(!audio_in)
	{
		CHIAKI_LOGE(log.GetChiakiLog(), "Failed to open default Microphone: %s", SDL_GetError());
		return;
	}

	CHIAKI_LOGI(log.GetChiakiLog(), "Microphone \"%s\" opened with SDL (%s), %u channels @ %u Hz, buffer %u samples",
			device_name ? device_name : "default", SDL_GetCurrentAudioDriver(),
			(unsigned int)have.channels, (unsigned int)have.freq, (unsigned int)have.samples);
}

void StreamSession::CloseMic()
{
	if(!audio_in)
		return;
	SDL_CloseAudioDevice(audio_in);
	audio_in = 0;
}

// SDL capture thread: collect exactly one Opus frame of stereo samples, then encode and send
void StreamSession::QueueMicData(const uint8_t *data, size_t size)
{
	const size_t frame_bytes = sizeof(mic_buf);
	while(size > 0)
	{
		size_t n = std::min(size, frame_bytes - mic_buf_fill);
		memcpy(reinterpret_cast<uint8_t *>(mic_buf) + mic_buf_fill, data, n);
		mic_buf_fill += n;
		data += n;
		size -= n;
		if(mic_buf_fill == frame_bytes)
		{
			chiaki_opus_encoder_frame(mic_buf, &opus_encoder);
			mic_buf_fill = 0;
		}
	}
}

void StreamSession::HandleControllerButtonForMic(int sdl_button)
{
#if defined(CHIAKI_GUI_ENABLE_SDL_GAMECONTROLLER) && SDL_VERSION_ATLEAST(2, 0, 14)
	// SDL names for the Steam Deck: misc1 = "...", paddle1 = R4, paddle2 = L4, paddle3 = R5, paddle4 = L5
	int wanted = -1;
	switch(mic_toggle_button)
	{
		case MicToggleButton::QuickAccess: wanted = SDL_CONTROLLER_BUTTON_MISC1; break;
		case MicToggleButton::L4: wanted = SDL_CONTROLLER_BUTTON_PADDLE2; break;
		case MicToggleButton::L5: wanted = SDL_CONTROLLER_BUTTON_PADDLE4; break;
		case MicToggleButton::R4: wanted = SDL_CONTROLLER_BUTTON_PADDLE1; break;
		case MicToggleButton::R5: wanted = SDL_CONTROLLER_BUTTON_PADDLE3; break;
		case MicToggleButton::L4AndL5: {
			// pressing one left back button while the other is held
			if(sdl_button != SDL_CONTROLLER_BUTTON_PADDLE2 && sdl_button != SDL_CONTROLLER_BUTTON_PADDLE4)
				return;
			auto controller = qobject_cast<Controller *>(sender());
			int other = sdl_button == SDL_CONTROLLER_BUTTON_PADDLE2 ? SDL_CONTROLLER_BUTTON_PADDLE4 : SDL_CONTROLLER_BUTTON_PADDLE2;
			if(controller && controller->IsSdlButtonPressed(other))
				ToggleMute();
			return;
		}
		case MicToggleButton::None:
			return;
	}
	if(sdl_button == wanted)
		ToggleMute();
#else
	Q_UNUSED(sdl_button);
#endif
}

void StreamSession::ToggleMute()
{
	if(!allow_unmute)
	{
		CHIAKI_LOGI(log.GetChiakiLog(), "Microphone can only be switched once the stream is running");
		return;
	}
	if(!mic_connected)
	{
		InitMic();
		if(!audio_in)
		{
			CHIAKI_LOGE(log.GetChiakiLog(), "Microphone initialization failed, it stays muted");
			return;
		}
		chiaki_session_connect_microphone(&session);
		mic_connected = true;
	}
	mic_muted = !mic_muted;
	chiaki_session_set_microphone_muted(&session, mic_muted);
	if(audio_in)
		SDL_PauseAudioDevice(audio_in, mic_muted ? 1 : 0);
	CHIAKI_LOGI(log.GetChiakiLog(), "Microphone %s", mic_muted ? "muted" : "unmuted");
	emit MicMutedChanged(mic_muted);
}

void StreamSession::CloseAudio()
{
	if(!audio_out)
		return;
	SDL_CloseAudioDevice(audio_out);
	audio_out = 0;
}

void StreamSession::PushAudioFrame(int16_t *buf, size_t samples_count)
{
	if(!audio_out)
		return;

	Uint32 queued = SDL_GetQueuedAudioSize(audio_out);
	if(queued > audio_queue_max)
	{
		CHIAKI_LOGW(log.GetChiakiLog(), "Audio queue grew to %u bytes, dropping it to catch up with the stream", (unsigned int)queued);
		SDL_ClearQueuedAudio(audio_out);
	}

	if(SDL_QueueAudio(audio_out, buf, (Uint32)(samples_count * audio_out_sample_size)) < 0)
		CHIAKI_LOGE(log.GetChiakiLog(), "Failed to queue audio: %s", SDL_GetError());
}

void StreamSession::Event(ChiakiEvent *event)
{
	switch(event->type)
	{
		case CHIAKI_EVENT_CONNECTED:
			connected = true;
			break;
		case CHIAKI_EVENT_QUIT:
			connected = false;
			emit SessionQuit(event->quit.reason, event->quit.reason_str ? QString::fromUtf8(event->quit.reason_str) : QString());
			break;
		case CHIAKI_EVENT_LOGIN_PIN_REQUEST:
			emit LoginPINRequested(event->login_pin_request.pin_incorrect);
			break;
		case CHIAKI_EVENT_RUMBLE: {
			uint8_t left = event->rumble.left;
			uint8_t right = event->rumble.right;
			CHIAKI_LOGV(GetChiakiLog(), "Rumble from console: left %u, right %u, forwarding to %d controller(s)",
					(unsigned int)left, (unsigned int)right, controllers.size());
			uint16_t left16 = ScaleRumble((uint32_t)left << 8);
			uint16_t right16 = ScaleRumble((uint32_t)right << 8);
			QMetaObject::invokeMethod(this, [this, left16, right16]() {
				for(auto controller : controllers)
					controller->SetRumble16(left16, right16);
			});
			break;
		}
		default:
			break;
	}
}

#if CHIAKI_GUI_ENABLE_SETSU
void StreamSession::HandleSetsuEvent(SetsuEvent *event)
{
	if(!setsu)
		return;
	switch(event->type)
	{
		case SETSU_EVENT_DEVICE_ADDED:
			switch(event->dev_type)
			{
				case SETSU_DEVICE_TYPE_TOUCHPAD:
					// connect all the touchpads!
					if(setsu_connect(setsu, event->path, event->dev_type))
						CHIAKI_LOGI(GetChiakiLog(), "Connected Setsu Touchpad Device %s", event->path);
					else
						CHIAKI_LOGE(GetChiakiLog(), "Failed to connect to Setsu Touchpad Device %s", event->path);
					break;
				case SETSU_DEVICE_TYPE_MOTION:
					// connect only one motion since multiple make no sense
					if(setsu_motion_device)
					{
						CHIAKI_LOGI(GetChiakiLog(), "Setsu Motion Device %s detected there is already one connected",
								event->path);
						break;
					}
					setsu_motion_device = setsu_connect(setsu, event->path, event->dev_type);
					if(setsu_motion_device)
						CHIAKI_LOGI(GetChiakiLog(), "Connected Setsu Motion Device %s", event->path);
					else
						CHIAKI_LOGE(GetChiakiLog(), "Failed to connect to Setsu Motion Device %s", event->path);
					break;
			}
			break;
		case SETSU_EVENT_DEVICE_REMOVED:
			switch(event->dev_type)
			{
				case SETSU_DEVICE_TYPE_TOUCHPAD:
					CHIAKI_LOGI(GetChiakiLog(), "Setsu Touchpad Device %s disconnected", event->path);
					for(auto it=setsu_ids.begin(); it!=setsu_ids.end();)
					{
						if(it.key().first == event->path)
						{
							chiaki_controller_state_stop_touch(&setsu_state, it.value());
							setsu_ids.erase(it++);
						}
						else
							it++;
					}
					SendFeedbackState();
					break;
				case SETSU_DEVICE_TYPE_MOTION:
					if(!setsu_motion_device || strcmp(setsu_device_get_path(setsu_motion_device), event->path))
						break;
					CHIAKI_LOGI(GetChiakiLog(), "Setsu Motion Device %s disconnected", event->path);
					setsu_motion_device = nullptr;
					chiaki_orientation_tracker_init(&orient_tracker);
					orient_dirty = true;
					break;
			}
			break;
		case SETSU_EVENT_TOUCH_DOWN:
			break;
		case SETSU_EVENT_TOUCH_UP:
			for(auto it=setsu_ids.begin(); it!=setsu_ids.end(); it++)
			{
				if(it.key().first == setsu_device_get_path(event->dev) && it.key().second == event->touch.tracking_id)
				{
					chiaki_controller_state_stop_touch(&setsu_state, it.value());
					setsu_ids.erase(it);
					break;
				}
			}
			SendFeedbackState();
			break;
		case SETSU_EVENT_TOUCH_POSITION: {
			QPair<QString, SetsuTrackingId> k =  { setsu_device_get_path(event->dev), event->touch.tracking_id };
			auto it = setsu_ids.find(k);
			if(it == setsu_ids.end())
			{
				int8_t cid = chiaki_controller_state_start_touch(&setsu_state, event->touch.x, event->touch.y);
				if(cid >= 0)
					setsu_ids[k] = (uint8_t)cid;
				else
					break;
			}
			else
				chiaki_controller_state_set_touch_pos(&setsu_state, it.value(), event->touch.x, event->touch.y);
			SendFeedbackState();
			break;
		}
		case SETSU_EVENT_BUTTON_DOWN:
			setsu_state.buttons |= CHIAKI_CONTROLLER_BUTTON_TOUCHPAD;
			break;
		case SETSU_EVENT_BUTTON_UP:
			setsu_state.buttons &= ~CHIAKI_CONTROLLER_BUTTON_TOUCHPAD;
			break;
		case SETSU_EVENT_MOTION:
			chiaki_orientation_tracker_update(&orient_tracker,
					event->motion.gyro_x, event->motion.gyro_y, event->motion.gyro_z,
					event->motion.accel_x, event->motion.accel_y, event->motion.accel_z,
					event->motion.timestamp);
			orient_dirty = true;
			break;
	}
}
#endif

void StreamSession::TriggerFfmpegFrameAvailable()
{
	emit FfmpegFrameAvailable();
}

class StreamSessionPrivate
{
	public:
		static void InitAudio(StreamSession *session, uint32_t channels, uint32_t rate)
		{
			QMetaObject::invokeMethod(session, "InitAudio", Qt::ConnectionType::BlockingQueuedConnection, Q_ARG(unsigned int, channels), Q_ARG(unsigned int, rate));
		}

		static void PushAudioFrame(StreamSession *session, int16_t *buf, size_t samples_count)	{ session->PushAudioFrame(buf, samples_count); }
		static void PushHapticsFrame(StreamSession *session, uint8_t *buf, size_t buf_size)		{ session->PushHapticsFrame(buf, buf_size); }
		static void Event(StreamSession *session, ChiakiEvent *event)							{ session->Event(event); }
#if CHIAKI_GUI_ENABLE_SETSU
		static void HandleSetsuEvent(StreamSession *session, SetsuEvent *event)					{ session->HandleSetsuEvent(event); }
#endif
		static void TriggerFfmpegFrameAvailable(StreamSession *session)							{ session->TriggerFfmpegFrameAvailable(); }
};

static void AudioSettingsCb(uint32_t channels, uint32_t rate, void *user)
{
	auto session = reinterpret_cast<StreamSession *>(user);
	StreamSessionPrivate::InitAudio(session, channels, rate);
}

static void AudioFrameCb(int16_t *buf, size_t samples_count, void *user)
{
	auto session = reinterpret_cast<StreamSession *>(user);
	StreamSessionPrivate::PushAudioFrame(session, buf, samples_count);
}

static void HapticsFrameCb(uint8_t *buf, size_t buf_size, void *user)
{
	auto session = reinterpret_cast<StreamSession *>(user);
	StreamSessionPrivate::PushHapticsFrame(session, buf, buf_size);
}

static void EventCb(ChiakiEvent *event, void *user)
{
	auto session = reinterpret_cast<StreamSession *>(user);
	StreamSessionPrivate::Event(session, event);
}

#if CHIAKI_GUI_ENABLE_SETSU
static void SessionSetsuCb(SetsuEvent *event, void *user)
{
	auto session = reinterpret_cast<StreamSession *>(user);
	StreamSessionPrivate::HandleSetsuEvent(session, event);
}
#endif

static void FfmpegFrameCb(ChiakiFfmpegDecoder *decoder, void *user)
{
	auto session = reinterpret_cast<StreamSession *>(user);
	StreamSessionPrivate::TriggerFfmpegFrameAvailable(session);
}
