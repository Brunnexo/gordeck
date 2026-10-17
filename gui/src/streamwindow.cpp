// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <streamwindow.h>
#include <streamsession.h>
#include <avopenglwidget.h>
#include <loginpindialog.h>
#include <settings.h>
#include <i18n.h>
#include <screensaverinhibitor.h>

#include <moderndialog.h>

#include <QLabel>
#include <QCoreApplication>
#include <QAction>
#include <QTimer>

StreamWindow::StreamWindow(const StreamSessionConnectInfo &connect_info, QWidget *parent)
	: QMainWindow(parent),
	connect_info(connect_info)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setAttribute(Qt::WA_AcceptTouchEvents);
	setWindowTitle(tr("Stream"));
		
	session = nullptr;
	av_widget = nullptr;
	screensaver_inhibitor = nullptr;
	home_hold_label = nullptr;
	home_hold_update_timer = nullptr;
	force_quit = false;

	try
	{
		if(connect_info.fullscreen || connect_info.zoom || connect_info.stretch)
			showFullScreen();
		Init();
	}
	catch(const Exception &e)
	{
		ModernDialog::Message(this, ModernDialog::Icon::Error, tr("Stream failed"), tr("Failed to initialize Stream Session: %1").arg(e.what()));
		close();
	}
}

StreamWindow::~StreamWindow()
{
	// uses the session's log, so delete it before the session
	delete screensaver_inhibitor;
	// make sure av_widget is always deleted before the session
	delete av_widget;
}

void StreamWindow::Init()
{
	session = new StreamSession(connect_info, this);

	connect(session, &StreamSession::SessionQuit, this, &StreamWindow::SessionQuit);
	connect(session, &StreamSession::LoginPINRequested, this, &StreamWindow::LoginPINRequested);
	connect(session, &StreamSession::HomeButtonHeld, this, &StreamWindow::HomeButtonHeld);
	connect(session, &StreamSession::ForceQuitRequested, this, &StreamWindow::ForceQuit);

	home_hold_label = new QLabel(this);
	home_hold_label->setAlignment(Qt::AlignCenter);
	home_hold_label->setStyleSheet("background-color: rgba(21, 29, 48, 235); color: #ecf1fa; font-size: 15pt;"
			"border: 2px solid #ff5c6c; border-radius: 16px; padding: 14px 28px;");
	home_hold_label->hide();
	home_hold_update_timer = new QTimer(this);
	home_hold_update_timer->setInterval(200);
	connect(home_hold_update_timer, &QTimer::timeout, this, &StreamWindow::UpdateHomeHoldLabel);

	AVOpenGLWidget::ResolutionMode resolution_mode;
	if(connect_info.zoom)
		resolution_mode = AVOpenGLWidget::Zoom;
	else if(connect_info.stretch)
		resolution_mode = AVOpenGLWidget::Stretch;
	else
		resolution_mode = AVOpenGLWidget::Normal;

	if(session->GetFfmpegDecoder())
	{
		av_widget = new AVOpenGLWidget(session, this, resolution_mode);
		setCentralWidget(av_widget);
		av_widget->HideMouse();
	}
	else
	{
		QWidget *bg_widget = new QWidget(this);
		bg_widget->setStyleSheet("background-color: black;");
		setCentralWidget(bg_widget);
	}

	grabKeyboard();

	session->Start();

	// gamepad input does not count as activity for the desktop, keep the screen on while streaming
	screensaver_inhibitor = new ScreenSaverInhibitor(session->GetChiakiLog(), this);
	screensaver_inhibitor->Inhibit(tr("Streaming from the PlayStation"));

	auto fullscreen_action = new QAction(tr("Fullscreen"), this);
	fullscreen_action->setShortcut(Qt::Key_F11);
	addAction(fullscreen_action);
	connect(fullscreen_action, &QAction::triggered, this, &StreamWindow::ToggleFullscreen);

	auto stretch_action = new QAction(tr("Stretch"), this);
	stretch_action->setShortcut(Qt::CTRL + Qt::Key_S);
	addAction(stretch_action);
	connect(stretch_action, &QAction::triggered, this, &StreamWindow::ToggleStretch);

	auto zoom_action = new QAction(tr("Zoom"), this);
	zoom_action->setShortcut(Qt::CTRL + Qt::Key_Z);
	addAction(zoom_action);
	connect(zoom_action, &QAction::triggered, this, &StreamWindow::ToggleZoom);


	auto quit_action = new QAction(tr("Quit"), this);
	quit_action->setShortcut(Qt::CTRL + Qt::Key_Q);
	addAction(quit_action);
	connect(quit_action, &QAction::triggered, this, &StreamWindow::Quit);

	resize(connect_info.video_profile.width, connect_info.video_profile.height);
	show();
}

void StreamWindow::keyPressEvent(QKeyEvent *event)
{
	if(session)
		session->HandleKeyboardEvent(event);
}

void StreamWindow::keyReleaseEvent(QKeyEvent *event)
{
	if(session)
		session->HandleKeyboardEvent(event);
}

bool StreamWindow::event(QEvent *event)
{
	if(session)
	{
		if ((event->type() == QEvent::TouchBegin) || (event->type() == QEvent::TouchUpdate) || (event->type() == QEvent::TouchEnd))
		{
			session->HandleTouchEvent(static_cast<QTouchEvent *>(event));
			return true;
		}
	}
	// hand non-touch events + cancelled touches back to regular handler
	return QMainWindow::event(event);
}

void StreamWindow::HomeButtonHeld(bool held)
{
	if(held)
	{
		home_hold_elapsed.start();
		home_hold_update_timer->start();
	}
	else
	{
		home_hold_update_timer->stop();
		home_hold_label->hide();
	}
}

void StreamWindow::UpdateHomeHoldLabel()
{
	// only show the countdown when the button is held on purpose
	const qint64 show_after_ms = 2000;
	const qint64 quit_after_ms = HOME_HOLD_QUIT_MS;
	qint64 elapsed = home_hold_elapsed.elapsed();
	if(elapsed < show_after_ms)
		return;
	qint64 remaining = (quit_after_ms - elapsed + 999) / 1000;
	if(remaining < 1)
		remaining = 1;
	home_hold_label->setText(tr("Keep holding the home button to end the stream: %1 s").arg(remaining));
	home_hold_label->adjustSize();
	home_hold_label->move((width() - home_hold_label->width()) / 2, height() / 8);
	home_hold_label->show();
	home_hold_label->raise();
}

void StreamWindow::ForceQuit()
{
	home_hold_update_timer->stop();
	home_hold_label->hide();
	force_quit = true;
	close();
}

void StreamWindow::Quit()
{
	close();
}

void StreamWindow::mousePressEvent(QMouseEvent *event)
{
	if(session)
		session->HandleMousePressEvent(event);
}
void StreamWindow::mouseReleaseEvent(QMouseEvent *event)
{
	if(session)
		session->HandleMouseReleaseEvent(event);
}

void StreamWindow::mouseMoveEvent(QMouseEvent *event)
{
	if(session)
		session->HandleMouseMoveEvent(event, frameSize().width(), frameSize().height());
}

void StreamWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
	ToggleFullscreen();

	QMainWindow::mouseDoubleClickEvent(event);
}

void StreamWindow::closeEvent(QCloseEvent *event)
{
	if(session)
	{
		if(session->IsConnected() && !force_quit)
		{
			bool sleep = false;
			switch(connect_info.settings->GetDisconnectAction())
			{
				case DisconnectAction::Ask: {
					int res = ModernDialog::Choice(this, ModernDialog::Icon::Question, tr("Disconnect Session"),
							tr("Do you want the Console to go into sleep mode?"),
							{ tr("Sleep Mode"), tr("Keep On"), tr("Cancel") }, 0);
					switch(res)
					{
						case 0:
							sleep = true;
							break;
						case 1:
							break;
						default: // cancel or back
							event->ignore();
							return;
					}
					break;
				}
				case DisconnectAction::AlwaysSleep:
					sleep = true;
					break;
				default:
					break;
			}
			if(sleep)
				session->GoToBed();
		}
		session->Stop();
	}
}

void StreamWindow::SessionQuit(ChiakiQuitReason reason, const QString &reason_str)
{
	if(screensaver_inhibitor)
		screensaver_inhibitor->Release();
	if(reason != CHIAKI_QUIT_REASON_STOPPED)
	{
		QString m = I18n::QuitReasonString(reason);
		if(!reason_str.isEmpty())
			m += "\n" + tr("Reason") + ": \"" + reason_str + "\"";
		ModernDialog::Message(this, ModernDialog::Icon::Error, tr("The stream session has quit"), m);
	}
	close();
}

void StreamWindow::LoginPINRequested(bool incorrect)
{

	if(!connect_info.initial_login_pin.isEmpty() && incorrect == false)
	{
		if(!session)
			return;
		session->SetLoginPIN(connect_info.initial_login_pin);
	}
	else
	{
		auto dialog = new LoginPINDialog(incorrect, this);
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->setModal(true);
		connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
			grabKeyboard();

			if(!session)
				return;

			if(result == QDialog::Accepted)
				session->SetLoginPIN(dialog->GetPIN());
			else
				session->Stop();
		});
		releaseKeyboard();
		dialog->show();
	}
}

void StreamWindow::ToggleFullscreen()
{
	if(isFullScreen())
		showNormal();
	else
	{
		showFullScreen();
	}
}

void StreamWindow::ToggleStretch()
{
	if(av_widget)
		av_widget->ToggleStretch();
}

void StreamWindow::ToggleZoom()
{
	if(av_widget)
		av_widget->ToggleZoom();
}

void StreamWindow::resizeEvent(QResizeEvent *event)
{
	UpdateVideoTransform();
	QMainWindow::resizeEvent(event);
}

void StreamWindow::moveEvent(QMoveEvent *event)
{
	UpdateVideoTransform();
	QMainWindow::moveEvent(event);
}

void StreamWindow::changeEvent(QEvent *event)
{
	if(event->type() == QEvent::ActivationChange)
		UpdateVideoTransform();
	QMainWindow::changeEvent(event);
}

void StreamWindow::UpdateVideoTransform()
{
#if CHIAKI_LIB_ENABLE_PI_DECODER
	ChiakiPiDecoder *pi_decoder = session->GetPiDecoder();
	if(pi_decoder)
	{
		QRect r = geometry();
		chiaki_pi_decoder_set_params(pi_decoder, r.x(), r.y(), r.width(), r.height(), isActiveWindow());
	}
#endif
}
