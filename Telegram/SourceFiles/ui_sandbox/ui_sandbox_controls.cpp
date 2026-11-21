#include "ui_sandbox/ui_sandbox_controls.h"

#include "ui_sandbox/ui_sandbox_config.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/popup_menu.h"

#include <QtCore/QRegularExpression>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace UiSandbox {

void EditConfigBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(rpl::single(u"UI Sandbox"_q));

	const auto &values = Config::Instance().current();
	const auto premium = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		u"Premium"_q,
		values.premium,
		st::settingsCheckbox));
	const auto verified = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		u"Verified"_q,
		values.verified,
		st::settingsCheckbox));
	const auto addField = [&](QString label, const QString &value) {
		return box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			Ui::InputField::Mode::NoNewlines,
			rpl::single(std::move(label)),
			value));
	};
	const auto emoji = addField(
		u"Emoji status document ID"_q,
		values.emojiStatusId ? QString::number(values.emojiStatusId) : QString());
	const auto phone = addField(u"Phone"_q, values.phone);
	const auto username = addField(u"Username"_q, values.username);
	const auto collectibles = addField(
		u"Collectible usernames (comma separated)"_q,
		values.collectibleUsernames.join(u", "_q));
	const auto rating = addField(
		u"Profile rating"_q,
		values.rating ? QString::number(values.rating) : QString());

	const auto save = box->addRow(object_ptr<Ui::SettingsButton>(
		box,
		rpl::single(u"Сохранить"_q),
		st::settingsButtonNoIcon));
	const auto reset = box->addRow(object_ptr<Ui::SettingsButton>(
		box,
		rpl::single(u"Сбросить"_q),
		st::settingsButtonNoIcon));

	save->setClickedCallback([=] {
		const auto emojiText = emoji->getLastText().trimmed();
		auto emojiOk = false;
		const auto emojiId = emojiText.toULongLong(&emojiOk);
		static const auto digits = QRegularExpression(u"^[0-9]+$"_q);
		if (!emojiText.isEmpty()
			&& (!emojiOk || !digits.match(emojiText).hasMatch())) {
			emoji->showError();
			box->showToast(u"Введите числовой ID документа."_q);
			return;
		}
		const auto ratingText = rating->getLastText().trimmed();
		auto ratingOk = false;
		const auto ratingValue = ratingText.toInt(&ratingOk);
		if (!ratingText.isEmpty() && (!ratingOk || ratingValue < 0)) {
			rating->showError();
			box->showToast(u"Введите целое неотрицательное число."_q);
			return;
		}
		auto names = QStringList();
		for (const auto &part : collectibles->getLastText().split(',')) {
			const auto name = part.trimmed();
			if (!name.isEmpty()) {
				names.push_back(name);
			}
		}
		if (!Config::Instance().save({
			.premium = premium->checked(),
			.verified = verified->checked(),
			.emojiStatusId = emojiId,
			.phone = phone->getLastText().trimmed(),
			.username = username->getLastText().trimmed(),
			.collectibleUsernames = std::move(names),
			.rating = ratingValue,
		})) {
			box->showToast(u"Не удалось сохранить ui_sandbox.json."_q);
			return;
		}
		box->showToast(u"Настройки UI Sandbox сохранены."_q);
	});
	reset->setClickedCallback([=] {
		if (!Config::Instance().save({})) {
			box->showToast(u"Не удалось сбросить ui_sandbox.json."_q);
			return;
		}
		premium->setChecked(false);
		verified->setChecked(false);
		emoji->setText(QString());
		phone->setText(QString());
		username->setText(QString());
		collectibles->setText(QString());
		rating->setText(QString());
		box->showToast(u"Настройки UI Sandbox сброшены."_q);
	});
}

void AddCopyDocumentIdAction(
		not_null<Ui::PopupMenu*> menu,
		DocumentId id,
		bool showId) {
	if (!id) {
		return;
	}
	const auto value = QString::number(id);
	menu->addAction(
		showId ? u"Copy Document ID: "_q + value : u"Copy Document ID"_q,
		[value] { QGuiApplication::clipboard()->setText(value); },
		&st::menuIconCopy);
}

} // namespace UiSandbox
