// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#ifndef CHIAKI_HINTBAR_H
#define CHIAKI_HINTBAR_H

#include <QAbstractButton>
#include <QWidget>

class QHBoxLayout;
class QPainter;

enum class HintGlyph
{
	A,
	B,
	X,
	Y,
	Start,
	Select,
	DPad,
	Shoulders
};

enum class HintStyle
{
	Keyboard,
	Xbox,
	SteamDeck,
	PlayStation
};

/**
 * One clickable button hint, like "(A) Connect", showing either
 * a gamepad glyph or the keyboard key.
 */
class HintButton : public QAbstractButton
{
	Q_OBJECT

	private:
		HintGlyph glyph;
		QString key_text;
		HintStyle style;

		qreal GlyphWidth() const;
		void PaintFaceButton(QPainter &painter, const QRectF &rect);
		void PaintSystemButton(QPainter &painter, const QRectF &rect);
		void PaintDPad(QPainter &painter, const QRectF &rect);
		void PaintShoulders(QPainter &painter, const QRectF &rect);

	protected:
		void paintEvent(QPaintEvent *event) override;
		void enterEvent(QEvent *event) override;
		void leaveEvent(QEvent *event) override;

	public:
		HintButton(HintGlyph glyph, const QString &key_text, QWidget *parent = nullptr);

		void SetStyle(HintStyle style);
		void SetKeyText(const QString &key_text);

		QSize sizeHint() const override;
};

class HintBar : public QWidget
{
	Q_OBJECT

	private:
		QHBoxLayout *layout;
		QList<HintButton *> buttons;

	protected:
		void paintEvent(QPaintEvent *event) override;

	public:
		explicit HintBar(QWidget *parent = nullptr);

		HintButton *AddHint(HintGlyph glyph, const QString &key_text);
		void AddStretch();
		void SetStyle(HintStyle style);
};

#endif // CHIAKI_HINTBAR_H
