// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <virtualkeyboard.h>
#include <modernwidgets.h>

#include <QApplication>
#include <QDoubleValidator>
#include <QFile>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QValidator>

#define KEY_STYLE "QPushButton { font-size: 15pt; font-weight: bold; border-radius: 10px; outline: none; padding: 0px; }" \
	"QPushButton:focus { background-color: #2f8cff; border-color: #2f8cff; color: #ffffff; }"

static const char *const rows_lower[] = { "qwertyuiop", "asdfghjkl:", "zxcvbnm.-_@" };
static const char *const rows_upper[] = { "QWERTYUIOP", "ASDFGHJKL;", "ZXCVBNM,/\\?" };
static const char *const rows_symbols[] = { "!@#$%^&*()", "~`[]{}<>/\\", "+=;'\",?:|_#" };

static bool IsNumeric(QLineEdit *edit)
{
	if(edit->inputMethodHints() & Qt::ImhDigitsOnly)
		return true;
	const QValidator *v = edit->validator();
	return qobject_cast<const QIntValidator *>(v) || qobject_cast<const QDoubleValidator *>(v);
}

VirtualKeyboard::VirtualKeyboard(QLineEdit *target, QWidget *parent)
	: ModernDialog(tr("Keyboard"), parent, Icon::None, IsNumeric(target) ? 540 : 900),
	target(target),
	text(target->text()),
	password(target->echoMode() != QLineEdit::Normal),
	mode(Mode::Lower),
	shift_key(nullptr),
	symbols_key(nullptr)
{
	text_label = new QLabel(this);
	text_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	text_label->setMinimumHeight(52);
	text_label->setStyleSheet("font-size: 18pt; color: #ecf1fa; background-color: #0c1322;"
			"border: 1px solid #2b3858; border-radius: 12px; padding: 4px 12px;");
	GetContentLayout()->addWidget(text_label);

	auto pad = new QWidget(this);
	ModernWidgets::SetGridNavigation(pad);
	if(IsNumeric(target))
		BuildNumericLayout(pad);
	else
		BuildTextLayout(pad);
	GetContentLayout()->addWidget(pad);

	AddButton(tr("OK"), ButtonRole::Accept);
	AddButton(tr("Cancel"), ButtonRole::Reject);
	UpdateText();
	UpdateKeys();
}

QPushButton *VirtualKeyboard::AddKey(QWidget *parent, QGridLayout *grid, const QString &label, int row, int col, int col_span)
{
	auto button = new QPushButton(label, parent);
	button->setAutoDefault(false);
	button->setMinimumHeight(52);
	button->setStyleSheet(KEY_STYLE);
	grid->addWidget(button, row, col, 1, col_span);
	return button;
}

void VirtualKeyboard::BuildNumericLayout(QWidget *pad)
{
	auto grid = new QGridLayout(pad);
	grid->setContentsMargins(40, 6, 40, 0);
	grid->setSpacing(8);
	QPushButton *first = nullptr;
	for(int i=1; i<=9; i++)
	{
		int n = i;
		int row = (9 - i) / 3; // 7 8 9 on top
		int col = (i - 1) % 3;
		auto b = AddKey(pad, grid, QString::number(n), row, col);
		connect(b, &QPushButton::clicked, this, [this, n]() { Append(QString::number(n)); });
		if(i == 5)
			first = b;
	}
	auto dot = AddKey(pad, grid, ".", 3, 0);
	connect(dot, &QPushButton::clicked, this, [this]() { Append("."); });
	auto zero = AddKey(pad, grid, "0", 3, 1);
	connect(zero, &QPushButton::clicked, this, [this]() { Append("0"); });
	auto back = AddKey(pad, grid, QString::fromUtf8("⌫"), 3, 2);
	connect(back, &QPushButton::clicked, this, [this]() { Backspace(); });
	SetInitialFocus(first);
}

void VirtualKeyboard::BuildTextLayout(QWidget *pad)
{
	auto grid = new QGridLayout(pad);
	grid->setContentsMargins(0, 6, 0, 0);
	grid->setSpacing(6);
	QPushButton *first = nullptr;

	// digits
	for(int i=0; i<10; i++)
	{
		QString d = QString::number((i + 1) % 10);
		auto b = AddKey(pad, grid, d, 0, i);
		connect(b, &QPushButton::clicked, this, [this, d]() { Append(d); });
		if(i == 0)
			first = b;
	}
	auto back = AddKey(pad, grid, QString::fromUtf8("⌫"), 0, 10);
	connect(back, &QPushButton::clicked, this, [this]() { Backspace(); });

	// characters, the labels depend on the mode
	for(int r=0; r<3; r++)
	{
		int n = r == 2 ? 11 : 10;
		for(int c=0; c<n; c++)
		{
			auto b = AddKey(pad, grid, QString(), r + 1, c);
			b->setProperty("kbRow", r);
			b->setProperty("kbCol", c);
			connect(b, &QPushButton::clicked, this, [this, b]() { Append(b->text()); });
			char_keys.append(b);
		}
	}
	shift_key = AddKey(pad, grid, QString::fromUtf8("⇧"), 1, 10);
	connect(shift_key, &QPushButton::clicked, this, [this]() {
		mode = mode == Mode::Lower ? Mode::Upper : Mode::Lower;
		UpdateKeys();
	});
	symbols_key = AddKey(pad, grid, "?123", 2, 10);
	connect(symbols_key, &QPushButton::clicked, this, [this]() {
		mode = mode == Mode::Symbols ? Mode::Lower : Mode::Symbols;
		UpdateKeys();
	});

	auto space = AddKey(pad, grid, tr("Space"), 4, 0, 8);
	connect(space, &QPushButton::clicked, this, [this]() { Append(" "); });
	auto clear = AddKey(pad, grid, tr("Clear"), 4, 8, 3);
	connect(clear, &QPushButton::clicked, this, [this]() { text.clear(); UpdateText(); });
	SetInitialFocus(first);
}

void VirtualKeyboard::UpdateKeys()
{
	if(!shift_key)
		return;
	const char *const *rows = mode == Mode::Upper ? rows_upper : mode == Mode::Symbols ? rows_symbols : rows_lower;
	for(auto b : char_keys)
		b->setText(QString(QChar(rows[b->property("kbRow").toInt()][b->property("kbCol").toInt()])));
	shift_key->setEnabled(mode != Mode::Symbols);
	symbols_key->setText(mode == Mode::Symbols ? "abc" : "?123");
}

void VirtualKeyboard::Append(const QString &s)
{
	if(target->maxLength() > 0 && text.length() + s.length() > target->maxLength())
		return;
	text += s;
	if(mode == Mode::Upper)
	{
		mode = Mode::Lower; // shift only applies to one character
		UpdateKeys();
	}
	UpdateText();
}

void VirtualKeyboard::Backspace()
{
	text.chop(1);
	UpdateText();
}

void VirtualKeyboard::UpdateText()
{
	QString shown = password ? QString(text.length(), QChar(0x25CF)) : text;
	if(shown.isEmpty() && !target->placeholderText().isEmpty())
		text_label->setText("<span style=\"color: #6b7a99;\">" + target->placeholderText().toHtmlEscaped() + "</span>");
	else
		text_label->setText(shown.toHtmlEscaped().replace(" ", "&nbsp;") + "<span style=\"color: #2f8cff;\">|</span>");
}

void VirtualKeyboard::keyPressEvent(QKeyEvent *event)
{
	// a physical keyboard types directly
	QString t = event->text();
	if(!(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) && t.length() == 1 && t[0].isPrint())
	{
		Append(t);
		return;
	}
	if(event->key() == Qt::Key_Backspace)
	{
		Backspace();
		return;
	}
	ModernDialog::keyPressEvent(event);
}

bool VirtualKeyboard::Edit(QLineEdit *edit)
{
	if(!edit || edit->isReadOnly() || !edit->isEnabled())
		return false;
	VirtualKeyboard keyboard(edit, edit->window());
	if(keyboard.exec() != QDialog::Accepted || keyboard.text == edit->text())
		return false;
	// goes through the validator and emits textEdited like typing would
	edit->selectAll();
	if(keyboard.text.isEmpty())
		edit->del();
	else
		edit->insert(keyboard.text);
	edit->setFocus();
	return true;
}

static bool RunningOnSteamDeck()
{
	auto ReadFile = [](const char *path) {
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
	};
	QString product = ReadFile("/sys/devices/virtual/dmi/id/product_name");
	return ReadFile("/sys/devices/virtual/dmi/id/board_vendor") == "Valve" && (product == "Jupiter" || product == "Galileo");
}

namespace
{
class TouchTrigger : public QObject
{
	public:
		bool deck = RunningOnSteamDeck();
		bool busy = false;

		using QObject::QObject;

		bool eventFilter(QObject *watched, QEvent *event) override
		{
			if(event->type() != QEvent::MouseButtonRelease || busy)
				return false;
			auto edit = qobject_cast<QLineEdit *>(watched);
			auto me = static_cast<QMouseEvent *>(event);
			if(!edit || me->button() != Qt::LeftButton || !edit->isEnabled() || edit->isReadOnly())
				return false;
			if(!qobject_cast<QDialog *>(edit->window()) || !edit->rect().contains(me->pos()))
				return false;
			if(!deck && me->source() == Qt::MouseEventNotSynthesized)
				return false;
			busy = true;
			QMetaObject::invokeMethod(this, [this, edit]() {
				if(edit)
					VirtualKeyboard::Edit(edit);
				busy = false;
			}, Qt::QueuedConnection);
			return false;
		}
};
}

void VirtualKeyboard::InstallTouchTrigger(QObject *app)
{
	app->installEventFilter(new TouchTrigger(app));
}
