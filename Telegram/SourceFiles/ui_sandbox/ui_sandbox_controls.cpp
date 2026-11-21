#include "ui_sandbox/ui_sandbox_controls.h"

#include "ui_sandbox/ui_sandbox_config.h"
#include "base/invoke_queued.h"
#include "data/data_document.h"
#include "data/data_star_gift.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"

#include <QtCore/QRegularExpression>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace UiSandbox {
namespace {

bool ParseId(const QString &text, uint64 &id) {
	static const auto digits = QRegularExpression(u"^[0-9]+\\z"_q);
	auto parsed = false;
	id = text.toULongLong(&parsed);
	return parsed && digits.match(text).hasMatch();
}

void AddGiftControls(not_null<Ui::GenericBox*> box) {
	const auto list = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto rebuild = [=] {
		list->clear();
		const auto gifts = std::make_shared<const std::vector<GiftPreset>>(
			Config::Instance().current().gifts);
		for (auto i = size_t(0); i != gifts->size(); ++i) {
			const auto &gift = (*gifts)[i];
			const auto title = gift.num > 0
				? gift.title + u" #"_q + QString::number(gift.num)
				: gift.title;
			const auto button = list->add(object_ptr<Ui::SettingsButton>(
				list,
				rpl::single(u"Удалить: %1"_q.arg(title)),
				st::settingsButtonNoIcon));
			button->setClickedCallback([=] {
				auto next = Config::Instance().current();
				if (next.gifts != *gifts) {
					box->showToast(u"Список изменился. Повторите удаление."_q);
					return;
				}
				next.gifts.erase(next.gifts.begin()
					+ static_cast<std::vector<GiftPreset>::difference_type>(i));
				if (!Config::Instance().save(std::move(next))) {
					box->showToast(u"Не удалось удалить подарок."_q);
				}
			});
		}
	};
	rebuild();
	Config::Instance().changes() | rpl::on_next([=] {
		InvokeQueued(list, rebuild);
	}, list->lifetime());

	const auto addField = [&](QString label) {
		return box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			Ui::InputField::Mode::NoNewlines,
			rpl::single(std::move(label))));
	};
	const auto document = addField(u"ID документа подарка"_q);
	const auto title = addField(u"Название подарка"_q);
	const auto number = addField(u"Коллекционный номер (необязательно)"_q);
	const auto center = addField(u"Цвет центра (#RRGGBB)"_q);
	const auto edge = addField(u"Цвет края (#RRGGBB)"_q);
	const auto pattern = addField(u"ID документа узора (необязательно)"_q);
	const auto add = box->addRow(object_ptr<Ui::SettingsButton>(
		box,
		rpl::single(u"Добавить подарок"_q),
		st::settingsButtonNoIcon));
	add->setClickedCallback([=] {
		auto preset = GiftPreset{
			.title = title->getLastText().trimmed(),
			.centerColor = center->getLastText().trimmed(),
			.edgeColor = edge->getLastText().trimmed(),
		};
		if (!ParseId(document->getLastText().trimmed(), preset.documentId)
			|| !preset.documentId) {
			document->showError();
			return;
		}
		if (preset.title.isEmpty()) {
			title->showError();
			return;
		}
		const auto numberText = number->getLastText().trimmed();
		auto numberOk = false;
		preset.num = numberText.toInt(&numberOk);
		if (!numberText.isEmpty() && (!numberOk || preset.num < 0)) {
			number->showError();
			return;
		}
		if (!IsGiftColor(preset.centerColor)) {
			center->showError();
			return;
		}
		if (!IsGiftColor(preset.edgeColor)) {
			edge->showError();
			return;
		}
		const auto patternText = pattern->getLastText().trimmed();
		if (!patternText.isEmpty() && !ParseId(patternText, preset.patternDocumentId)) {
			pattern->showError();
			return;
		}
		auto next = Config::Instance().current();
		next.gifts.push_back(std::move(preset));
		if (!Config::Instance().save(std::move(next))) {
			box->showToast(u"Не удалось сохранить подарок."_q);
			return;
		}
		for (const auto field : { document, title, number, center, edge, pattern }) {
			field->setText(QString());
		}
		box->showToast(u"Локальный подарок добавлен."_q);
	});
}

} // namespace

GiftPreset MakeGiftPreset(
		DocumentId documentId,
		QString title,
		QString text,
		const Data::UniqueGift *unique,
		const Data::StarGiftBackground *background) {
	auto preset = GiftPreset{
		.documentId = documentId,
		.title = title.isEmpty() ? u"Подарок"_q : std::move(title),
		.centerColor = u"#3A7BD5"_q,
		.edgeColor = u"#244D87"_q,
		.text = std::move(text),
		.attributes = {},
		.patternColor = {},
		.textColor = {},
		.sourceSlug = {},
	};
	if (!unique) {
		if (background) {
			preset.centerColor = background->center.name(QColor::HexRgb);
			preset.edgeColor = background->edge.name(QColor::HexRgb);
			preset.textColor = background->text.name(QColor::HexRgb);
		}
		return preset;
	}
	const auto &backdrop = unique->backdrop;
	preset.documentId = unique->model.document->id;
	if (!unique->title.isEmpty()) {
		preset.title = unique->title;
	}
	if (preset.text.isEmpty()) {
		preset.text = unique->originalDetails.message.text;
	}
	preset.num = unique->number;
	preset.centerColor = backdrop.centerColor.name(QColor::HexRgb);
	preset.edgeColor = backdrop.edgeColor.name(QColor::HexRgb);
	preset.patternColor = backdrop.patternColor.name(QColor::HexRgb);
	preset.textColor = backdrop.textColor.name(QColor::HexRgb);
	preset.patternDocumentId = unique->pattern.document->id;
	preset.sourceSlug = unique->slug;
	for (const auto attribute : {
		static_cast<const Data::UniqueGiftAttribute*>(&unique->model),
		static_cast<const Data::UniqueGiftAttribute*>(&unique->pattern),
		static_cast<const Data::UniqueGiftAttribute*>(&backdrop),
	}) {
		if (!attribute->name.isEmpty()) {
			preset.attributes.push_back(Data::UniqueGiftAttributeText(*attribute));
		}
	}
	return preset;
}

GiftStoreResult StoreGift(GiftPreset preset, int64 cost) {
	auto next = Config::Instance().current();
	if (!preset.documentId || cost < 0 || (cost && !next.virtualPurchases)) {
		return GiftStoreResult::Failed;
	}
	if (ranges::any_of(next.gifts, [&](const GiftPreset &gift) {
		return preset.sourceSlug.isEmpty()
			? gift == preset
			: gift.sourceSlug == preset.sourceSlug;
	})) {
		return GiftStoreResult::AlreadyAdded;
	}
	if (cost > next.virtualStars) {
		return GiftStoreResult::InsufficientBalance;
	}
	next.virtualStars -= cost;
	next.gifts.push_back(std::move(preset));
	return Config::Instance().save(std::move(next))
		? GiftStoreResult::Saved
		: GiftStoreResult::Failed;
}

QString GiftStoreMessage(GiftStoreResult result) {
	switch (result) {
	case GiftStoreResult::Saved: return u"UI Sandbox: подарок добавлен"_q;
	case GiftStoreResult::AlreadyAdded: return u"Подарок уже есть в UI Sandbox."_q;
	case GiftStoreResult::InsufficientBalance: return u"Недостаточно виртуальных Stars."_q;
	case GiftStoreResult::Failed: return u"Не удалось сохранить подарок в UI Sandbox."_q;
	}
	Unexpected("UI Sandbox gift store result.");
}

void EditGiftsBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(rpl::single(u"Локальные подарки"_q));
	AddGiftControls(box);
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
