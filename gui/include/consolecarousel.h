// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_CONSOLECAROUSEL_H
#define CHIAKI_CONSOLECAROUSEL_H

#include <chiaki/discoveryservice.h>

#include <QHash>
#include <QImage>
#include <QList>
#include <QRectF>
#include <QSvgRenderer>
#include <QVector>
#include <QWidget>

class QVariantAnimation;

struct CarouselEntry
{
	enum class Type
	{
		Console,
		AddConsole
	};

	Type type = Type::Console;

	/**
	 * Identifies the entry across updates to keep the selection stable
	 */
	QString key;

	QString title;
	QString subtitle;
	bool ps5 = false;
	ChiakiDiscoveryHostState state = CHIAKI_DISCOVERY_HOST_STATE_UNKNOWN;
	bool discovered = false;
};

/**
 * Horizontal, animated carousel of big console cards in the style of EmulationStation.
 */
class ConsoleCarousel : public QWidget
{
	Q_OBJECT

	private:
		QList<CarouselEntry> entries;
		int current_index;
		qreal scroll_pos;
		QVariantAnimation *scroll_animation;

		QSvgRenderer ps4_renderer;
		QSvgRenderer ps5_renderer;
		QSvgRenderer add_renderer;
		QHash<QString, QImage> image_cache;

		// card rects of the last paint, in paint order (topmost last)
		QVector<QPair<int, QRectF>> card_rects;

		QPoint press_pos;
		bool pressed;

		QSizeF CardSize() const;
		QImage ConsoleImage(bool ps5, ChiakiDiscoveryHostState state, int width);
		QImage AddImage(int size);
		void PaintCard(QPainter &painter, const CarouselEntry &entry, const QRectF &rect, qreal focus);
		void PaintPageIndicator(QPainter &painter);
		int CardAt(const QPoint &pos) const;

	protected:
		void paintEvent(QPaintEvent *event) override;
		void resizeEvent(QResizeEvent *event) override;
		void mousePressEvent(QMouseEvent *event) override;
		void mouseReleaseEvent(QMouseEvent *event) override;
		void wheelEvent(QWheelEvent *event) override;
		void contextMenuEvent(QContextMenuEvent *event) override;

	public:
		explicit ConsoleCarousel(QWidget *parent = nullptr);

		/**
		 * Replace all entries, keeping the entry with the same key selected if possible.
		 */
		void SetEntries(const QList<CarouselEntry> &entries);
		const QList<CarouselEntry> &GetEntries() const	{ return entries; }

		int GetCurrentIndex() const						{ return current_index; }
		void SetCurrentIndex(int index, bool animate = true);
		void Next();
		void Previous();

		/**
		 * @return center of the current card in global coordinates
		 */
		QPoint CurrentCardGlobalCenter() const;

	signals:
		void CurrentIndexChanged(int index);
		void Activated(int index);
		void ContextMenuRequested(int index, const QPoint &global_pos);
};

#endif // CHIAKI_CONSOLECAROUSEL_H
