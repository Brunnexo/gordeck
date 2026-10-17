// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <theme.h>

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QPainter>
#include <QSvgRenderer>

static const char * const stylesheet = R"(
QWidget {
	font-size: 11pt;
}

QDialog, QMessageBox {
	background-color: #101727;
}

QToolTip {
	color: #ecf1fa;
	background-color: #1f2a44;
	border: 1px solid #2f8cff;
	padding: 4px;
}

QGroupBox {
	background-color: #151d30;
	border: 1px solid #24304c;
	border-radius: 10px;
	margin-top: 1.6em;
	padding: 10px 8px 8px 8px;
	font-weight: bold;
}

QGroupBox::title {
	subcontrol-origin: margin;
	subcontrol-position: top left;
	left: 12px;
	padding: 0 4px;
	color: #6cb4ff;
}

QPushButton {
	background-color: #1f2a44;
	color: #ecf1fa;
	border: 1px solid #2b3858;
	border-radius: 8px;
	padding: 7px 16px;
	min-height: 1.4em;
}

QPushButton:hover {
	background-color: #283657;
	border-color: #3a4c78;
}

QPushButton:focus, QPushButton:default {
	border: 2px solid #2f8cff;
}

QPushButton:pressed {
	background-color: #2f8cff;
	color: #ffffff;
}

QPushButton:disabled {
	color: #5d6a82;
	background-color: #161e30;
	border-color: #1f2a40;
}

QLineEdit, QComboBox, QPlainTextEdit, QListWidget, QSpinBox {
	background-color: #0c1322;
	color: #ecf1fa;
	border: 1px solid #2b3858;
	border-radius: 7px;
	padding: 6px 8px;
	selection-background-color: #2f8cff;
	selection-color: #ffffff;
}

QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus, QListWidget:focus, QSpinBox:focus {
	border: 2px solid #2f8cff;
}

QLineEdit:disabled, QComboBox:disabled {
	color: #5d6a82;
	background-color: #111827;
}

QComboBox::drop-down {
	border: none;
	width: 30px;
}

QComboBox::down-arrow {
	image: url(:/icons/ui-chevron-down.svg);
	width: 18px;
	height: 18px;
}

QPushButton {
	outline: none;
}

QComboBox QAbstractItemView {
	background-color: #151d30;
	color: #ecf1fa;
	border: 1px solid #2f8cff;
	selection-background-color: #2f8cff;
	outline: none;
	padding: 4px;
}

QListWidget::item {
	padding: 6px;
	border-radius: 5px;
}

QListWidget::item:selected {
	background-color: #2f8cff;
	color: #ffffff;
}

QCheckBox, QRadioButton {
	spacing: 8px;
	padding: 3px;
}

QCheckBox:focus, QRadioButton:focus {
	color: #6cb4ff;
}

QCheckBox::indicator, QRadioButton::indicator {
	width: 18px;
	height: 18px;
}

QMenu {
	background-color: #151d30;
	color: #ecf1fa;
	border: 1px solid #2f8cff;
	border-radius: 8px;
	padding: 6px;
	font-size: 13pt;
}

QMenu::item {
	padding: 10px 28px 10px 16px;
	border-radius: 6px;
}

QMenu::item:selected {
	background-color: #2f8cff;
	color: #ffffff;
}

QMenu::item:disabled {
	color: #5d6a82;
}

QMenu::separator {
	height: 1px;
	background: #2b3858;
	margin: 6px 10px;
}

QScrollBar:vertical {
	background: transparent;
	width: 10px;
	margin: 2px;
}

QScrollBar::handle:vertical {
	background: #2b3858;
	border-radius: 4px;
	min-height: 30px;
}

QScrollBar::add-line, QScrollBar::sub-line {
	height: 0px;
	width: 0px;
}
)";

QPixmap Theme::RenderIcon(const QString &filename, int size, const QColor &color, qreal dpr)
{
	QSvgRenderer renderer(filename);
	QPixmap pixmap(QSize(size, size) * dpr);
	pixmap.setDevicePixelRatio(dpr);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	renderer.render(&painter, QRectF(0, 0, size, size));
	if(color.isValid())
	{
		painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
		painter.fillRect(QRectF(0, 0, size, size), color);
	}
	return pixmap;
}

void Theme::Apply(QApplication &app)
{
	app.setStyle(QStyleFactory::create("Fusion"));

	QPalette palette;
	palette.setColor(QPalette::Window, surface);
	palette.setColor(QPalette::WindowText, text);
	palette.setColor(QPalette::Base, QColor(0x0c, 0x13, 0x22));
	palette.setColor(QPalette::AlternateBase, surface_light);
	palette.setColor(QPalette::ToolTipBase, surface_light);
	palette.setColor(QPalette::ToolTipText, text);
	palette.setColor(QPalette::Text, text);
	palette.setColor(QPalette::Button, surface_light);
	palette.setColor(QPalette::ButtonText, text);
	palette.setColor(QPalette::BrightText, Qt::white);
	palette.setColor(QPalette::Link, accent_light);
	palette.setColor(QPalette::Highlight, accent);
	palette.setColor(QPalette::HighlightedText, Qt::white);
	palette.setColor(QPalette::PlaceholderText, text_muted);
	palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x5d, 0x6a, 0x82));
	palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x5d, 0x6a, 0x82));
	palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x5d, 0x6a, 0x82));
	app.setPalette(palette);

	app.setStyleSheet(stylesheet);
}
