// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <consolecarousel.h>
#include <theme.h>
#include <i18n.h>

#include <QContextMenuEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QVariantAnimation>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#define SCROLL_ANIMATION_MS 260
#define SWIPE_THRESHOLD 60

ConsoleCarousel::ConsoleCarousel(QWidget *parent)
	: QWidget(parent),
	current_index(0),
	scroll_pos(0.0),
	ps4_renderer(QString(":/icons/console-ps4.svg")),
	ps5_renderer(QString(":/icons/console-ps5.svg")),
	add_renderer(QString(":/icons/ui-add-circle.svg")),
	pressed(false)
{
	setFocusPolicy(Qt::StrongFocus);
	setMinimumHeight(220);
	setAttribute(Qt::WA_AcceptTouchEvents, false);

	scroll_animation = new QVariantAnimation(this);
	scroll_animation->setDuration(SCROLL_ANIMATION_MS);
	scroll_animation->setEasingCurve(QEasingCurve::OutCubic);
	connect(scroll_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
		scroll_pos = value.toReal();
		update();
	});
}

void ConsoleCarousel::SetEntries(const QList<CarouselEntry> &entries)
{
	QString current_key;
	// if only the "add" entry existed, select the first console as soon as one appears
	if(current_index >= 0 && current_index < this->entries.count()
			&& !(this->entries.count() == 1 && this->entries[0].type == CarouselEntry::Type::AddConsole))
		current_key = this->entries[current_index].key;

	this->entries = entries;

	int index = -1;
	for(int i=0; i<entries.count(); i++)
	{
		if(entries[i].key == current_key)
		{
			index = i;
			break;
		}
	}
	if(index < 0)
		index = std::min(current_index, (int)entries.count() - 1);
	if(index < 0)
		index = 0;

	bool changed = index != current_index || current_key.isEmpty();
	current_index = index;
	if(scroll_animation->state() != QAbstractAnimation::Running)
		scroll_pos = current_index;
	update();
	if(changed)
		emit CurrentIndexChanged(current_index);
}

void ConsoleCarousel::SetCurrentIndex(int index, bool animate)
{
	if(entries.isEmpty())
		return;
	index = std::max(0, std::min(index, (int)entries.count() - 1));
	if(index == current_index)
		return;
	current_index = index;
	scroll_animation->stop();
	if(animate)
	{
		scroll_animation->setStartValue(scroll_pos);
		scroll_animation->setEndValue((qreal)current_index);
		scroll_animation->start();
	}
	else
	{
		scroll_pos = current_index;
		update();
	}
	emit CurrentIndexChanged(current_index);
}

void ConsoleCarousel::Next()
{
	SetCurrentIndex(current_index + 1);
}

void ConsoleCarousel::Previous()
{
	SetCurrentIndex(current_index - 1);
}

QPoint ConsoleCarousel::CurrentCardGlobalCenter() const
{
	for(const auto &p : card_rects)
	{
		if(p.first == current_index)
			return mapToGlobal(p.second.center().toPoint());
	}
	return mapToGlobal(rect().center());
}

QSizeF ConsoleCarousel::CardSize() const
{
	// leave room at the bottom for the page indicator
	qreal avail_h = height() - 36.0;
	qreal w = std::min(width() * 0.34, avail_h / 0.92);
	return QSizeF(w, w * 0.92);
}

static void RenderSvgElement(QSvgRenderer &renderer, QPainter &painter, const QString &id, const QRectF &target)
{
	QRectF view_box = renderer.viewBoxF();
	QRectF src = renderer.boundsOnElement(id);
	QRectF dst = src.translated(-view_box.left(), -view_box.top());
	dst = QRectF(
			target.width() * dst.left() / view_box.width(),
			target.height() * dst.top() / view_box.height(),
			target.width() * dst.width() / view_box.width(),
			target.height() * dst.height() / view_box.height());
	renderer.render(&painter, id, dst.translated(target.topLeft()));
}

QImage ConsoleCarousel::ConsoleImage(bool ps5, ChiakiDiscoveryHostState state, int width)
{
	QString key = QString("console_%1_%2_%3").arg(ps5).arg((int)state).arg(width);
	auto it = image_cache.find(key);
	if(it != image_cache.end())
		return it.value();

	QSvgRenderer &renderer = ps5 ? ps5_renderer : ps4_renderer;
	QRectF view_box = renderer.viewBoxF();
	if(view_box.width() < 0.00001 || view_box.height() < 0.00001)
		return QImage();

	qreal dpr = devicePixelRatioF();
	QSizeF size(width, width * view_box.height() / view_box.width());
	QImage image((size * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
	image.setDevicePixelRatio(dpr);
	image.fill(Qt::transparent);

	QImage body = image;
	{
		QPainter painter(&body);
		painter.setRenderHint(QPainter::Antialiasing);
		RenderSvgElement(renderer, painter, "console", QRectF(QPointF(0, 0), size));
		painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
		QLinearGradient gradient(0, 0, 0, size.height());
		gradient.setColorAt(0.0, QColor(0xff, 0xff, 0xff));
		gradient.setColorAt(1.0, QColor(0xb9, 0xc6, 0xdd));
		painter.fillRect(QRectF(QPointF(0, 0), size), gradient);
	}

	{
		QPainter painter(&image);
		painter.setRenderHint(QPainter::Antialiasing);
		switch(state)
		{
			case CHIAKI_DISCOVERY_HOST_STATE_READY:
				RenderSvgElement(renderer, painter, "light_on", QRectF(QPointF(0, 0), size));
				break;
			case CHIAKI_DISCOVERY_HOST_STATE_STANDBY:
				RenderSvgElement(renderer, painter, "light_standby", QRectF(QPointF(0, 0), size));
				break;
			default:
				break;
		}
		painter.drawImage(QPointF(0, 0), body);
	}

	image_cache.insert(key, image);
	return image;
}

QImage ConsoleCarousel::AddImage(int size)
{
	QString key = QString("add_%1").arg(size);
	auto it = image_cache.find(key);
	if(it != image_cache.end())
		return it.value();

	qreal dpr = devicePixelRatioF();
	QImage image(QSize(size, size) * dpr, QImage::Format_ARGB32_Premultiplied);
	image.setDevicePixelRatio(dpr);
	image.fill(Qt::transparent);
	{
		QPainter painter(&image);
		painter.setRenderHint(QPainter::Antialiasing);
		add_renderer.render(&painter, QRectF(0, 0, size, size));
		painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
		painter.fillRect(QRectF(0, 0, size, size), Theme::accent_light);
	}
	image_cache.insert(key, image);
	return image;
}

static void PaintReflection(QPainter &painter, const QImage &image, const QPointF &top_left, qreal opacity)
{
	if(image.isNull())
		return;
	qreal dpr = image.devicePixelRatio();
	QSizeF size = QSizeF(image.size()) / dpr;

	QImage reflection = image.mirrored(false, true);
	reflection.setDevicePixelRatio(dpr);
	{
		QPainter p(&reflection);
		p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
		QLinearGradient fade(0, 0, 0, size.height());
		fade.setColorAt(0.0, QColor(0, 0, 0, 110));
		fade.setColorAt(0.6, QColor(0, 0, 0, 0));
		p.fillRect(QRectF(QPointF(0, 0), size), fade);
	}
	painter.save();
	painter.setOpacity(painter.opacity() * opacity);
	painter.drawImage(QPointF(top_left.x(), top_left.y() + size.height() + 2.0), reflection);
	painter.restore();
}

static QColor StateColor(const CarouselEntry &entry)
{
	if(!entry.discovered)
		return Theme::accent_light;
	switch(entry.state)
	{
		case CHIAKI_DISCOVERY_HOST_STATE_READY:
			return Theme::state_ready;
		case CHIAKI_DISCOVERY_HOST_STATE_STANDBY:
			return Theme::state_standby;
		default:
			return Theme::state_unknown;
	}
}

static void PaintPill(QPainter &painter, const QRectF &anchor, bool align_right, const QString &text,
		const QColor &color, qreal font_px, bool dot)
{
	QFont font = painter.font();
	font.setPixelSize(std::max(1, (int)font_px));
	font.setBold(true);
	QFontMetricsF fm(font);
	qreal pad = font_px * 0.7;
	qreal dot_size = dot ? font_px * 0.6 : 0.0;
	qreal dot_space = dot ? dot_size + font_px * 0.45 : 0.0;
	qreal w = fm.horizontalAdvance(text) + pad * 2.0 + dot_space;
	qreal h = fm.height() + font_px * 0.5;
	QRectF r(align_right ? anchor.right() - w : anchor.left(), anchor.top(), w, h);

	QColor bg = color;
	bg.setAlpha(46);
	painter.setPen(Qt::NoPen);
	painter.setBrush(bg);
	painter.drawRoundedRect(r, h / 2.0, h / 2.0);

	if(dot)
	{
		painter.setBrush(color);
		painter.drawEllipse(QRectF(r.left() + pad, r.center().y() - dot_size / 2.0, dot_size, dot_size));
	}

	painter.setFont(font);
	painter.setPen(color);
	painter.drawText(r.adjusted(pad + dot_space, 0, -pad, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
}

void ConsoleCarousel::PaintCard(QPainter &painter, const CarouselEntry &entry, const QRectF &rect, qreal focus)
{
	qreal radius = rect.width() * 0.07;

	// glow around the selected card
	if(focus > 0.01)
	{
		painter.setBrush(Qt::NoBrush);
		for(int i=1; i<=14; i++)
		{
			QColor c = Theme::accent;
			c.setAlphaF(0.055 * focus * (1.0 - i / 15.0));
			painter.setPen(QPen(c, 2.0));
			painter.drawRoundedRect(rect.adjusted(-i * 1.5, -i * 1.5, i * 1.5, i * 1.5), radius + i * 1.5, radius + i * 1.5);
		}
	}

	QLinearGradient bg(rect.topLeft(), rect.bottomLeft());
	QColor top = Theme::surface_light;
	QColor bottom = Theme::surface;
	if(focus > 0.01)
	{
		top = QColor::fromRgbF(
				top.redF() + (0.13 - top.redF()) * focus * 0.6,
				top.greenF() + (0.25 - top.greenF()) * focus * 0.6,
				top.blueF() + (0.47 - top.blueF()) * focus * 0.6);
	}
	bg.setColorAt(0.0, top);
	bg.setColorAt(1.0, bottom);
	painter.setBrush(bg);
	QColor border = Theme::surface_light.lighter(130);
	if(focus > 0.01)
	{
		border = QColor::fromRgbF(
				border.redF() + (Theme::accent.redF() - border.redF()) * focus,
				border.greenF() + (Theme::accent.greenF() - border.greenF()) * focus,
				border.blueF() + (Theme::accent.blueF() - border.blueF()) * focus);
	}
	painter.setPen(QPen(border, 1.0 + 2.0 * focus));
	painter.drawRoundedRect(rect, radius, radius);

	qreal pad = rect.width() * 0.07;
	QRectF inner = rect.adjusted(pad, pad, -pad, -pad);
	qreal small_font = rect.height() * 0.05;

	// visual area: upper part of the card
	QRectF visual(inner.left(), inner.top() + small_font * 2.0, inner.width(), inner.height() * 0.56);

	painter.save();
	QPainterPath clip;
	clip.addRoundedRect(rect, radius, radius);
	painter.setClipPath(clip);

	QColor glow_color = entry.type == CarouselEntry::Type::AddConsole ? Theme::accent : StateColor(entry);
	QRadialGradient glow(visual.center(), visual.width() * 0.6);
	QColor gc = glow_color;
	gc.setAlphaF(0.10 + 0.14 * focus);
	glow.setColorAt(0.0, gc);
	gc.setAlphaF(0.0);
	glow.setColorAt(1.0, gc);
	painter.setPen(Qt::NoPen);
	painter.setBrush(glow);
	painter.drawRect(rect);

	if(entry.type == CarouselEntry::Type::AddConsole)
	{
		int s = (int)(std::min(visual.width(), visual.height()) * 0.62);
		QImage img = AddImage(s);
		painter.drawImage(QPointF(visual.center().x() - s / 2.0, visual.center().y() - s / 2.0), img);
	}
	else
	{
		int w = (int)(visual.width() * 0.92);
		QImage img = ConsoleImage(entry.ps5, entry.discovered ? entry.state : CHIAKI_DISCOVERY_HOST_STATE_UNKNOWN, w);
		QSizeF img_size = QSizeF(img.size()) / img.devicePixelRatio();
		QPointF pos(visual.center().x() - img_size.width() / 2.0, visual.center().y() - img_size.height() * 0.8);
		painter.drawImage(pos, img);
		PaintReflection(painter, img, pos, 0.9);
	}
	painter.restore();

	// badges
	if(entry.type == CarouselEntry::Type::Console)
	{
		PaintPill(painter, inner, false, entry.ps5 ? "PS5" : "PS4", Theme::text, small_font, false);
		QString state_text = entry.discovered
			? I18n::DiscoveryHostStateString(entry.state)
			: tr("Manual");
		PaintPill(painter, inner, true, state_text, StateColor(entry), small_font, true);
	}

	// title and subtitle
	QFont title_font = painter.font();
	title_font.setPixelSize(std::max(1, (int)(rect.height() * 0.088)));
	title_font.setBold(true);
	QFontMetricsF title_fm(title_font);
	QRectF title_rect(inner.left(), visual.bottom() + inner.height() * 0.06, inner.width(), title_fm.height());
	painter.setFont(title_font);
	painter.setPen(Theme::text);
	painter.drawText(title_rect, Qt::AlignHCenter | Qt::AlignVCenter,
			title_fm.elidedText(entry.title, Qt::ElideRight, title_rect.width()));

	QFont sub_font = painter.font();
	sub_font.setPixelSize(std::max(1, (int)(rect.height() * 0.058)));
	sub_font.setBold(false);
	QFontMetricsF sub_fm(sub_font);
	QRectF sub_rect(inner.left(), title_rect.bottom() + 2.0, inner.width(), sub_fm.height());
	painter.setFont(sub_font);
	painter.setPen(Theme::text_muted);
	painter.drawText(sub_rect, Qt::AlignHCenter | Qt::AlignVCenter,
			sub_fm.elidedText(entry.subtitle, Qt::ElideRight, sub_rect.width()));
}

void ConsoleCarousel::PaintPageIndicator(QPainter &painter)
{
	int count = entries.count();
	if(count < 2)
		return;
	qreal dot = 8.0;
	qreal active_w = 26.0;
	qreal gap = 8.0;
	qreal total = (count - 1) * (dot + gap) + active_w;
	qreal x = (width() - total) / 2.0;
	qreal y = height() - 18.0;
	painter.setPen(Qt::NoPen);
	for(int i=0; i<count; i++)
	{
		bool active = i == current_index;
		qreal w = active ? active_w : dot;
		painter.setBrush(active ? Theme::accent : Theme::surface_light.lighter(150));
		painter.drawRoundedRect(QRectF(x, y - dot / 2.0, w, dot), dot / 2.0, dot / 2.0);
		x += w + gap;
	}
}

void ConsoleCarousel::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.setRenderHint(QPainter::TextAntialiasing);

	card_rects.clear();
	if(entries.isEmpty())
		return;

	QSizeF card = CardSize();
	qreal center_x = width() / 2.0;
	qreal center_y = (height() - 36.0) / 2.0;

	// paint farthest cards first so the selected one ends up on top
	QVector<int> order;
	for(int i=0; i<entries.count(); i++)
		order.append(i);
	std::sort(order.begin(), order.end(), [this](int a, int b) {
		return std::abs(a - scroll_pos) > std::abs(b - scroll_pos);
	});

	for(int i : order)
	{
		qreal d = i - scroll_pos;
		qreal ad = std::abs(d);
		if(ad > 3.5)
			continue;

		qreal near = std::min(ad, 1.0);
		qreal far = std::max(0.0, std::min(ad - 1.0, 2.0));
		qreal offset = (near * 0.98 + far * 0.62) * card.width();
		qreal scale = 1.0 - 0.24 * near - 0.12 * far;
		qreal opacity = 1.0 - 0.42 * near - 0.22 * far;
		qreal focus = 1.0 - near;

		QSizeF size = card * scale;
		QPointF c(center_x + (d < 0 ? -offset : offset), center_y);
		QRectF r(c.x() - size.width() / 2.0, c.y() - size.height() / 2.0, size.width(), size.height());

		painter.save();
		painter.setOpacity(std::max(0.0, opacity));
		PaintCard(painter, entries[i], r, focus);
		painter.restore();

		card_rects.append(qMakePair(i, r));
	}

	PaintPageIndicator(painter);
}

void ConsoleCarousel::resizeEvent(QResizeEvent *event)
{
	image_cache.clear();
	QWidget::resizeEvent(event);
}

int ConsoleCarousel::CardAt(const QPoint &pos) const
{
	for(int i=card_rects.count()-1; i>=0; i--)
	{
		if(card_rects[i].second.contains(pos))
			return card_rects[i].first;
	}
	return -1;
}

void ConsoleCarousel::mousePressEvent(QMouseEvent *event)
{
	if(event->button() != Qt::LeftButton)
	{
		QWidget::mousePressEvent(event);
		return;
	}
	pressed = true;
	press_pos = event->pos();
}

void ConsoleCarousel::mouseReleaseEvent(QMouseEvent *event)
{
	if(event->button() != Qt::LeftButton || !pressed)
	{
		QWidget::mouseReleaseEvent(event);
		return;
	}
	pressed = false;

	int dx = event->pos().x() - press_pos.x();
	if(std::abs(dx) > SWIPE_THRESHOLD)
	{
		// swipe on touchscreens
		if(dx < 0)
			Next();
		else
			Previous();
		return;
	}

	int index = CardAt(event->pos());
	if(index < 0)
		return;
	if(index == current_index)
		emit Activated(index);
	else
		SetCurrentIndex(index);
}

void ConsoleCarousel::wheelEvent(QWheelEvent *event)
{
	int delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->angleDelta().x();
	if(delta > 0)
		Previous();
	else if(delta < 0)
		Next();
	event->accept();
}

void ConsoleCarousel::contextMenuEvent(QContextMenuEvent *event)
{
	int index = CardAt(event->pos());
	if(index < 0)
		return;
	SetCurrentIndex(index);
	emit ContextMenuRequested(index, event->globalPos());
}
