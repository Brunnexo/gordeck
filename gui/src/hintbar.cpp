// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <hintbar.h>
#include <theme.h>

#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

#define GLYPH_SIZE 30.0
#define GLYPH_TEXT_SPACING 9.0
#define HINT_PADDING 10.0

HintButton::HintButton(HintGlyph glyph, const QString &key_text, QWidget *parent)
	: QAbstractButton(parent),
	glyph(glyph),
	key_text(key_text),
	style(HintStyle::Keyboard)
{
	setFocusPolicy(Qt::NoFocus);
	setCursor(Qt::PointingHandCursor);
	// the application stylesheet takes precedence over setFont()
	setStyleSheet("font-size: 12pt;");
}

void HintButton::SetStyle(HintStyle style)
{
	if(this->style == style)
		return;
	this->style = style;
	updateGeometry();
	update();
}

void HintButton::SetKeyText(const QString &key_text)
{
	this->key_text = key_text;
	updateGeometry();
	update();
}

qreal HintButton::GlyphWidth() const
{
	if(style != HintStyle::Keyboard)
	{
		if(glyph == HintGlyph::Shoulders)
			return GLYPH_SIZE * 2.4;
		// Steam Deck and PlayStation system buttons are pill shaped
		if((glyph == HintGlyph::Start || glyph == HintGlyph::Select) && style != HintStyle::Xbox)
			return GLYPH_SIZE * 1.4;
		return GLYPH_SIZE;
	}
	QFont f = font();
	f.setPointSizeF(10.0);
	f.setBold(true);
	return std::max(GLYPH_SIZE, QFontMetricsF(f).horizontalAdvance(key_text) + 16.0);
}

QSize HintButton::sizeHint() const
{
	qreal w = HINT_PADDING * 2.0 + GlyphWidth() + GLYPH_TEXT_SPACING + QFontMetricsF(font()).horizontalAdvance(text());
	return QSize((int)std::ceil(w), (int)GLYPH_SIZE + 12);
}

void HintButton::enterEvent(QEvent *event)
{
	update();
	QAbstractButton::enterEvent(event);
}

void HintButton::leaveEvent(QEvent *event)
{
	update();
	QAbstractButton::leaveEvent(event);
}

static QColor GlyphColor(HintGlyph glyph, HintStyle style)
{
	if(style == HintStyle::PlayStation)
	{
		switch(glyph)
		{
			case HintGlyph::A: return QColor(0x7c, 0xa6, 0xff); // cross
			case HintGlyph::B: return QColor(0xff, 0x6b, 0x7a); // circle
			case HintGlyph::X: return QColor(0xe5, 0x8a, 0xd6); // square
			case HintGlyph::Y: return QColor(0x43, 0xd6, 0xa6); // triangle
			default: return Theme::text;
		}
	}
	switch(glyph)
	{
		case HintGlyph::A: return Theme::button_a;
		case HintGlyph::B: return Theme::button_b;
		case HintGlyph::X: return Theme::button_x;
		case HintGlyph::Y: return Theme::button_y;
		default: return Theme::text;
	}
}

static QString GlyphLetter(HintGlyph glyph)
{
	switch(glyph)
	{
		case HintGlyph::A: return "A";
		case HintGlyph::B: return "B";
		case HintGlyph::X: return "X";
		case HintGlyph::Y: return "Y";
		default: return QString();
	}
}

static const QColor dark_button(0x1b, 0x1f, 0x27);
static const QColor dark_button_rim(0x5a, 0x62, 0x72);

void HintButton::PaintFaceButton(QPainter &painter, const QRectF &rect)
{
	QRectF r = rect.adjusted(1, 1, -1, -1);
	QColor color = GlyphColor(glyph, style);

	if(style == HintStyle::Xbox)
	{
		QColor bg = color;
		bg.setAlpha(50);
		painter.setPen(QPen(color, 2.0));
		painter.setBrush(bg);
		painter.drawEllipse(r);
		QFont f = font();
		f.setBold(true);
		f.setPointSizeF(11.5);
		painter.setFont(f);
		painter.setPen(color.lighter(115));
		painter.drawText(rect, Qt::AlignCenter, GlyphLetter(glyph));
		return;
	}

	// Steam Deck and PlayStation: dark buttons with a light rim
	painter.setPen(QPen(dark_button_rim, 1.5));
	painter.setBrush(dark_button);
	painter.drawEllipse(r);

	if(style == HintStyle::SteamDeck)
	{
		QFont f = font();
		f.setBold(true);
		f.setPointSizeF(11.0);
		painter.setFont(f);
		painter.setPen(QColor(0xf2, 0xf4, 0xf8));
		painter.drawText(rect, Qt::AlignCenter, GlyphLetter(glyph));
		return;
	}

	// PlayStation symbols
	QPointF c = rect.center();
	qreal s = rect.width() * 0.2;
	painter.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
	painter.setBrush(Qt::NoBrush);
	switch(glyph)
	{
		case HintGlyph::A:
			painter.drawLine(QPointF(c.x() - s, c.y() - s), QPointF(c.x() + s, c.y() + s));
			painter.drawLine(QPointF(c.x() - s, c.y() + s), QPointF(c.x() + s, c.y() - s));
			break;
		case HintGlyph::B:
			painter.drawEllipse(c, s * 1.1, s * 1.1);
			break;
		case HintGlyph::X:
			painter.drawRect(QRectF(c.x() - s, c.y() - s, s * 2.0, s * 2.0));
			break;
		case HintGlyph::Y:
		{
			QPolygonF triangle;
			triangle << QPointF(c.x(), c.y() - s * 1.15)
				<< QPointF(c.x() + s * 1.15, c.y() + s * 0.85)
				<< QPointF(c.x() - s * 1.15, c.y() + s * 0.85);
			painter.drawPolygon(triangle);
			break;
		}
		default:
			break;
	}
}

static void PaintMenuLines(QPainter &painter, const QPointF &c, qreal s, const QColor &color)
{
	painter.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap));
	for(int i=-1; i<=1; i++)
		painter.drawLine(QPointF(c.x() - s * 1.3, c.y() + i * s), QPointF(c.x() + s * 1.3, c.y() + i * s));
}

static void PaintViewWindows(QPainter &painter, const QPointF &c, qreal s, const QColor &color, const QColor &bg)
{
	painter.setPen(QPen(color, 1.6));
	painter.setBrush(Qt::NoBrush);
	painter.drawRoundedRect(QRectF(c.x() - s * 1.4, c.y() - s * 1.2, s * 1.9, s * 1.6), 2, 2);
	painter.setBrush(bg);
	painter.drawRoundedRect(QRectF(c.x() - s * 0.5, c.y() - s * 0.4, s * 1.9, s * 1.6), 2, 2);
}

void HintButton::PaintSystemButton(QPainter &painter, const QRectF &rect)
{
	QPointF c = rect.center();
	qreal s = GLYPH_SIZE * 0.18;

	if(style == HintStyle::Xbox)
	{
		QColor bg = Theme::surface_light.lighter(140);
		painter.setPen(Qt::NoPen);
		painter.setBrush(bg);
		painter.drawEllipse(rect);
		if(glyph == HintGlyph::Start)
			PaintMenuLines(painter, c, s, Theme::text);
		else
			PaintViewWindows(painter, c, s, Theme::text, bg);
		return;
	}

	// pill shaped like the small buttons of the Steam Deck and the DualSense
	QRectF pill = rect.adjusted(1, rect.height() * 0.14, -1, -rect.height() * 0.14);
	painter.setPen(QPen(dark_button_rim, 1.5));
	painter.setBrush(dark_button);
	painter.drawRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);

	QColor fg(0xf2, 0xf4, 0xf8);
	s *= 0.85;
	if(glyph == HintGlyph::Start)
		PaintMenuLines(painter, c, s, fg); // Steam Deck "menu", DualSense "options"
	else if(style == HintStyle::SteamDeck)
		PaintViewWindows(painter, c, s, fg, dark_button);
	else
	{
		// DualSense "create": three short rays
		painter.setPen(QPen(fg, 2.0, Qt::SolidLine, Qt::RoundCap));
		painter.drawLine(QPointF(c.x(), c.y() - s * 1.2), QPointF(c.x(), c.y() + s * 0.2));
		painter.drawLine(QPointF(c.x() - s * 1.3, c.y() - s * 0.6), QPointF(c.x() - s * 0.6, c.y() + s * 0.5));
		painter.drawLine(QPointF(c.x() + s * 1.3, c.y() - s * 0.6), QPointF(c.x() + s * 0.6, c.y() + s * 0.5));
	}
}

void HintButton::PaintDPad(QPainter &painter, const QRectF &rect)
{
	QPointF c = rect.center();
	qreal arm = rect.width() * 0.17;
	qreal len = rect.width() * 0.46;
	QPainterPath cross;
	cross.addRoundedRect(QRectF(c.x() - arm, c.y() - len, arm * 2.0, len * 2.0), 2.5, 2.5);
	cross.addRoundedRect(QRectF(c.x() - len, c.y() - arm, len * 2.0, arm * 2.0), 2.5, 2.5);
	cross.setFillRule(Qt::WindingFill);
	painter.setPen(QPen(dark_button_rim, 1.2));
	painter.setBrush(style == HintStyle::Xbox ? Theme::surface_light.lighter(160) : dark_button);
	painter.drawPath(cross.simplified());
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(0xf2, 0xf4, 0xf8, 200));
	painter.drawEllipse(c, arm * 0.45, arm * 0.45);
}

void HintButton::PaintShoulders(QPainter &painter, const QRectF &rect)
{
	QString left = style == HintStyle::Xbox ? "LB" : "L1";
	QString right = style == HintStyle::Xbox ? "RB" : "R1";
	qreal gap = 4.0;
	qreal w = (rect.width() - gap) / 2.0;
	QFont f = font();
	f.setBold(true);
	f.setPointSizeF(9.0);
	painter.setFont(f);
	int i = 0;
	for(const QString &label : { left, right })
	{
		QRectF r(rect.left() + i * (w + gap), rect.top() + rect.height() * 0.16, w, rect.height() * 0.68);
		painter.setPen(QPen(dark_button_rim, 1.5));
		painter.setBrush(style == HintStyle::Xbox ? Theme::surface_light.lighter(140) : dark_button);
		painter.drawRoundedRect(r, 6.0, 6.0);
		painter.setPen(QColor(0xf2, 0xf4, 0xf8));
		painter.drawText(r, Qt::AlignCenter, label);
		i++;
	}
}

void HintButton::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);

	QRectF r = rect();
	if(underMouse() || isDown())
	{
		painter.setPen(Qt::NoPen);
		painter.setBrush(isDown() ? Theme::accent : QColor(255, 255, 255, 18));
		painter.drawRoundedRect(r, r.height() / 2.0, r.height() / 2.0);
	}

	qreal gw = GlyphWidth();
	QRectF glyph_rect(HINT_PADDING, (r.height() - GLYPH_SIZE) / 2.0, gw, GLYPH_SIZE);
	if(style == HintStyle::Keyboard)
	{
		// keyboard key cap
		QFont glyph_font = font();
		glyph_font.setBold(true);
		glyph_font.setPointSizeF(10.0);
		painter.setPen(QPen(Theme::text_muted, 1.5));
		painter.setBrush(Theme::surface_light);
		painter.drawRoundedRect(glyph_rect.adjusted(0.75, 0.75, -0.75, -0.75), 6.0, 6.0);
		painter.setFont(glyph_font);
		painter.setPen(Theme::text);
		painter.drawText(glyph_rect, Qt::AlignCenter, key_text);
	}
	else if(glyph == HintGlyph::Start || glyph == HintGlyph::Select)
		PaintSystemButton(painter, glyph_rect);
	else if(glyph == HintGlyph::DPad)
		PaintDPad(painter, glyph_rect);
	else if(glyph == HintGlyph::Shoulders)
		PaintShoulders(painter, glyph_rect);
	else
		PaintFaceButton(painter, glyph_rect);

	painter.setFont(font());
	painter.setPen(isEnabled() ? Theme::text : Theme::text_muted);
	QRectF text_rect(glyph_rect.right() + GLYPH_TEXT_SPACING, 0, r.width() - glyph_rect.right() - GLYPH_TEXT_SPACING, r.height());
	painter.drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, text());
}

HintBar::HintBar(QWidget *parent) : QWidget(parent)
{
	layout = new QHBoxLayout(this);
	layout->setContentsMargins(16, 8, 16, 8);
	layout->setSpacing(6);
	setLayout(layout);
}

HintButton *HintBar::AddHint(HintGlyph glyph, const QString &key_text)
{
	auto button = new HintButton(glyph, key_text, this);
	layout->addWidget(button);
	buttons.append(button);
	return button;
}

void HintBar::AddStretch()
{
	layout->addStretch(1);
}

void HintBar::SetStyle(HintStyle style)
{
	for(auto button : buttons)
		button->SetStyle(style);
}

void HintBar::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	QColor bg = Theme::background;
	bg.setAlpha(170);
	painter.setBrush(bg);
	QRectF r = rect();
	painter.drawRoundedRect(r, r.height() / 2.0, r.height() / 2.0);
}
