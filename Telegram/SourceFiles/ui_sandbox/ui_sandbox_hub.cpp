#include "ui_sandbox/ui_sandbox_hub.h"

#include "base/timer.h"
#include "core/file_utilities.h"
#include "core/version.h"
#include "data/data_changes.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/layers/show.h"
#include "ui/painter.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "ui_sandbox/ui_sandbox_config.h"
#include "ui_sandbox/ui_sandbox_controls.h"

#include <QtCore/QFile>
#include <QtCore/QBuffer>
#include <QtGui/QImageReader>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>

#include <algorithm>
#include <limits>

#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace UiSandbox {
namespace {

enum class Page {
	Home,
	Profile,
	Gifts,
	Appearance,
	Messages,
	Privacy,
	Info,
	Media,
	Settings,
	All,
};

constexpr auto kMaxImport = qint64(4 * 1024 * 1024);

struct Category {
	Page page;
	const char16_t *title;
	const char16_t *description;
	const style::icon *icon;
};

const auto kCategories = std::array{
	Category{ Page::Profile, u"Профиль и статус", u"Личные данные, бейджи и заметки", &st::menuIconProfile },
	Category{ Page::Gifts, u"Подарки и локальные звёзды", u"Коллекция, баланс и тестовые покупки", &st::menuIconGiftPremium },
	Category{ Page::Appearance, u"Внешний вид и Liquid Glass", u"Оформление меню UI Sandbox", &st::menuIconEdit },
	Category{ Page::Messages, u"Сообщения", u"Ввод, время и отображение сообщений", &st::menuIconChatBubble },
	Category{ Page::Privacy, u"Приватность", u"Локальная видимость номера и активность", &st::menuIconLock },
	Category{ Page::Info, u"Информация в профиле", u"Идентификаторы, даты и просмотры", &st::menuIconInfo },
	Category{ Page::Media, u"Дополнительно и медиа", u"Поведение аудиоплеера", &st::menuIconManage },
	Category{ Page::Settings, u"Разделы меню и настройки", u"Скрытие пунктов, импорт и экспорт", &st::menuIconManage },
	Category{ Page::All, u"Все настройки", u"Сводный список настроек мода", &st::menuIconInfo },
};

struct Item {
	Page page;
	QString search;
	not_null<Ui::SlideWrap<Ui::VerticalLayout>*> wrap;
};

struct HubState {
	Page page = Page::Home;
	bool refreshFields = false;
	std::vector<Item> items;
	base::Timer toast;
};

not_null<Ui::InputField*> AddInput(
		not_null<Ui::VerticalLayout*> parent,
		const QString &label,
		const QString &text = QString()) {
	return parent->add(object_ptr<Ui::InputField>(
		parent,
		st::defaultInputField,
		Ui::InputField::Mode::NoNewlines,
		rpl::single(label),
		text));
}

not_null<Ui::SettingsButton*> AddButton(
		not_null<Ui::VerticalLayout*> parent,
		const QString &text,
		Fn<void()> callback) {
	const auto button = parent->add(object_ptr<Ui::SettingsButton>(
		parent,
		rpl::single(text),
		st::settingsButtonNoIcon));
	button->setClickedCallback(std::move(callback));
	return button;
}

void NotifyProfile(not_null<Main::Session*> session) {
	using Flag = Data::PeerUpdate::Flag;
	session->changes().peerUpdated(
		session->user(),
		Flag::PhoneNumber | Flag::Username | Flag::Usernames
			| Flag::EmojiStatus | Flag::PeerGifts);
}

bool MatchQuery(const QString &haystack, const QStringList &words) {
	return std::ranges::all_of(words, [&](const QString &word) {
		return haystack.contains(word, Qt::CaseInsensitive);
	});
}

} // namespace

void EditConfigBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	box->setTitle(rpl::single(u"UI Sandbox"_q));
	box->setWidth(st::boxWideWidth);

	auto &config = Config::Instance();
	const auto state = box->lifetime().make_state<HubState>();
	state->toast.setCallback([=] {
		box->showToast(u"UI Sandbox: сохранено"_q);
	});
	const auto save = [=](Values values) {
		if (!Config::Instance().save(std::move(values))) {
			box->showToast(u"Не удалось записать ui_sandbox.json."_q);
			return false;
		}
		NotifyProfile(session);
		state->toast.callOnce(450);
		return true;
	};

	const auto header = box->setPinnedToTopContent(
		object_ptr<Ui::VerticalLayout>(box));
	auto emblem = object_ptr<Ui::RpWidget>(header);
	emblem->resize(66, 58);
	const auto logo = header->add(std::move(emblem), style::al_top);
	logo->paintRequest() | rpl::on_next([=] {
		auto painter = QPainter(logo);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(Qt::NoPen);
		painter.setBrush(st::windowActiveTextFg->c);
		painter.drawEllipse(QRect(10, 4, 46, 46));
		painter.setPen(st::windowBg->c);
		painter.drawText(QRect(10, 4, 46, 46), Qt::AlignCenter, u"UI"_q);
	}, logo->lifetime());
	header->add(object_ptr<Ui::FlatLabel>(header, u"UI Sandbox"_q), style::al_top);
	header->add(object_ptr<Ui::FlatLabel>(
		header,
		u"Telegram Desktop %1"_q.arg(QString::fromLatin1(AppVersionStr))),
		style::al_top);
	const auto search = AddInput(header, u"Поиск настроек"_q);
	box->setFocusCallback([=] { search->setFocusFast(); });

	const auto content = box->verticalLayout();
	const auto nav = content->add(object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		content,
		object_ptr<Ui::VerticalLayout>(content)));
	const auto back = content->add(object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		content,
		object_ptr<Ui::VerticalLayout>(content)));
	const auto noMatches = content->add(
		object_ptr<Ui::SlideWrap<Ui::FlatLabel>>(
			content,
			object_ptr<Ui::FlatLabel>(content, u"Ничего не найдено."_q)));

	const auto refresh = [=] {
		const auto words = search->getLastText().simplified().split(
			u' ',
			Qt::SkipEmptyParts);
		const auto searching = !words.isEmpty();
		const auto home = !searching && state->page == Page::Home;
		nav->toggle(home, anim::type::instant);
		back->toggle(!home, anim::type::instant);
		auto found = false;
		for (const auto &item : state->items) {
			const auto shown = searching
				? MatchQuery(item.search, words)
				: (state->page == item.page || state->page == Page::All);
			item.wrap->toggle(shown, anim::type::instant);
			found = found || shown;
		}
		noMatches->toggle(searching && !found, anim::type::instant);
	};

	AddButton(back->entity(), u"‹ Категории"_q, [=] {
		state->page = Page::Home;
		search->setText(QString());
		refresh();
	});
	for (const auto &category : kCategories) {
		const auto title = QString::fromUtf16(category.title);
		const auto button = AddButton(nav->entity(), title, [=, page = category.page] {
			state->page = page;
			refresh();
		});
		button->setPaddingOverride(st::settingsButton.padding);
		const auto icon = Ui::CreateChild<Ui::RpWidget>(button.get());
		icon->setAttribute(Qt::WA_TransparentForMouseEvents);
		button->sizeValue() | rpl::on_next([=](QSize size) {
			icon->setGeometry(12, (size.height() - 24) / 2, 24, 24);
		}, icon->lifetime());
		icon->paintRequest() | rpl::on_next([=, image = category.icon] {
			auto painter = QPainter(icon);
			image->paint(painter, 0, 0, icon->width());
		}, icon->lifetime());
		icon->show();
		nav->entity()->add(object_ptr<Ui::FlatLabel>(
			nav->entity(),
			QString::fromUtf16(category.description),
			st::boxDividerLabel),
			QMargins(12, 0, 12, 10));
	}

	const auto row = [=](Page page, const QString &title, const QString &details) {
		const auto wrap = content->add(object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			content,
			object_ptr<Ui::VerticalLayout>(content)),
			QMargins(12, 4, 12, 4));
		const auto inner = wrap->entity();
		inner->paintRequest() | rpl::on_next([=] {
			if (!Enabled(Option::GlassHub)) {
				return;
			}
			auto painter = QPainter(inner);
			painter.setRenderHint(QPainter::Antialiasing);
			auto tint = st::windowActiveTextFg->c;
			tint.setAlpha(24);
			painter.setPen(Qt::NoPen);
			painter.setBrush(tint);
			painter.drawRoundedRect(inner->rect(), 12, 12);
		}, inner->lifetime());
		OptionValue(Option::GlassHub) | rpl::on_next([=] {
			inner->update();
		}, inner->lifetime());
		inner->add(object_ptr<Ui::FlatLabel>(inner, details, st::boxDividerLabel),
			QMargins(12, 5, 12, 0));
		const auto category = std::ranges::find(kCategories, page, &Category::page);
		state->items.push_back({
			page,
			title + u' ' + details + u' ' + QString::fromUtf16(category->title),
			wrap,
		});
		return not_null(inner);
	};

	const auto toggle = [&](Page page, QString title, QString description, auto read, auto write) {
		const auto inner = row(page, title, description);
		const auto check = inner->add(object_ptr<Ui::Checkbox>(
			inner,
			std::move(title),
			read(config.current()),
			st::settingsCheckbox),
			QMargins(12, 4, 12, 8));
		const auto sync = [=] {
			check->setChecked(
				read(Config::Instance().current()),
				Ui::Checkbox::NotifyAboutChange::DontNotify);
		};
		check->checkedChanges() | rpl::on_next([=](bool checked) {
			auto values = Config::Instance().current();
			if (read(values) == checked) {
				return;
			}
			write(values, checked);
			if (!save(std::move(values))) {
				sync();
			}
		}, check->lifetime());
		config.changes() | rpl::on_next(sync, check->lifetime());
	};
	const auto option = [&](Page page, QString title, QString details, Option key) {
		const auto index = static_cast<size_t>(key);
		toggle(page, std::move(title), std::move(details), [=](const Values &v) {
			return v.options[index];
		}, [=](Values &v, bool checked) { v.options[index] = checked; });
	};
	const auto boolean = [&](Page page, QString title, QString details, bool Values::*member) {
		toggle(page, std::move(title), std::move(details), [=](const Values &v) {
			return v.*member;
		}, [=](Values &v, bool checked) { v.*member = checked; });
	};
	const auto field = [&](Page page, QString title, QString details, auto read, auto write, bool defer = false) {
		const auto inner = row(page, title, details);
		const auto input = AddInput(inner, title, read(config.current()));
		const auto commit = [=] {
			if (state->refreshFields) {
				return;
			}
			auto values = Config::Instance().current();
			const auto text = input->getLastText();
			if (read(values) != text && write(values, text)) {
				if (!save(std::move(values))) {
					input->showError();
				}
			}
		};
		const auto timer = input->lifetime().make_state<base::Timer>(commit);
		input->changes() | rpl::on_next([=] {
			if (!state->refreshFields) {
				if (defer) {
					timer->callOnce(400);
				} else {
					commit();
				}
			}
		}, input->lifetime());
		if (defer) {
			box->boxClosing() | rpl::on_next([=] {
				if (timer->isActive()) {
					timer->cancel();
					commit();
				}
			}, input->lifetime());
		}
		config.changes() | rpl::on_next([=] {
			if (state->refreshFields) {
				timer->cancel();
				const auto text = read(Config::Instance().current());
				if (input->getLastText() != text) {
					input->setText(text);
				}
			}
		}, input->lifetime());
	};
	const auto textField = [&](Page page, QString title, QString details, QString Values::*member) {
		field(page, std::move(title), std::move(details), [=](const Values &v) {
			return v.*member;
		}, [=](Values &v, const QString &text) {
			v.*member = text;
			return true;
		});
	};
	const auto idField = [&](QString title, QString details, uint64 Values::*member) {
		field(Page::Profile, std::move(title), std::move(details), [=](const Values &v) {
			return (v.*member) ? QString::number(v.*member) : QString();
		}, [=](Values &v, const QString &text) {
			static const auto digits = QRegularExpression(u"^[0-9]+\\z"_q);
			auto valid = false;
			const auto id = text.toULongLong(&valid);
			if (!text.isEmpty() && (!valid || !digits.match(text).hasMatch())) {
				return false;
			}
			v.*member = id;
			return true;
		}, true);
	};
	const auto number = [&](Page page, QString title, QString details, auto member, auto fallback) {
		field(page, std::move(title), std::move(details), [=](const Values &v) {
			return QString::number(v.*member);
		}, [=](Values &v, const QString &text) {
			using Number = decltype(fallback);
			auto valid = false;
			const auto value = text.toLongLong(&valid);
			if (!text.isEmpty() && (!valid || value < 0
				|| value > std::numeric_limits<Number>::max())) {
				return false;
			}
			v.*member = text.isEmpty() ? fallback : static_cast<Number>(value);
			return true;
		});
	};
	const auto action = [&](Page page, QString title, QString details, Fn<void()> callback) {
		const auto inner = row(page, title, details);
		AddButton(inner, title, std::move(callback));
	};

	boolean(Page::Profile, u"Premium"_q, u"Локальный бейдж собственного профиля"_q, &Values::premium);
	boolean(Page::Profile, u"Verified"_q, u"Локальная галочка собственного профиля"_q, &Values::verified);
	idField(u"Бейдж верификации"_q, u"ID эмодзи. Пустое поле оставляет обычную галочку."_q, &Values::verifiedBadgeId);
	idField(u"Эмодзи-статус"_q, u"ID документа эмодзи или стикера"_q, &Values::emojiStatusId);
	textField(Page::Profile, u"Номер телефона"_q, u"Строка отображается как введено"_q, &Values::phone);
	textField(Page::Profile, u"Username"_q, u"Локальная подпись имени пользователя"_q, &Values::username);
	field(Page::Profile, u"Коллекционные юзернеймы"_q, u"Список через запятую"_q, [](const Values &v) {
		return v.collectibleUsernames.join(u", "_q);
	}, [](Values &v, const QString &text) {
		v.collectibleUsernames.clear();
		for (const auto &part : text.split(u',')) {
			const auto name = part.trimmed();
			if (!name.isEmpty()) {
				v.collectibleUsernames.push_back(name);
			}
		}
		return true;
	});
	number(Page::Profile, u"Рейтинг профиля"_q, u"Ноль скрывает локальный рейтинг"_q, &Values::rating, 0);
	textField(Page::Profile, u"Дата регистрации"_q, u"Локальная подпись с датой"_q, &Values::registrationDate);
	textField(Page::Profile, u"Активность или трек"_q, u"Локальная строка в собственном профиле"_q, &Values::activity);
	textField(Page::Profile, u"Стена заметок"_q, u"Личная заметка в собственном профиле"_q, &Values::notes);

	const auto show = box->uiShow();
	boolean(Page::Gifts, u"Локальные покупки"_q, u"Покупка себе за виртуальные Stars"_q, &Values::virtualPurchases);
	option(Page::Gifts, u"Изменить баланс звёзд"_q, u"Показывать виртуальное значение в разделе Stars"_q, Option::OverrideStarsBalance);
	number(Page::Gifts, u"Виртуальный баланс Stars"_q, u"Целое неотрицательное число"_q, &Values::virtualStars, kDefaultVirtualStars);
	option(Page::Gifts, u"Скрыть вкладку «Подарки»"_q, u"Скрывает кнопку подарков в профиле"_q, Option::HideGifts);
	action(Page::Gifts, u"Управление подарками"_q, u"Добавление пресетов, просмотр списка и удаление"_q, [=] {
		show->showBox(Box(EditGiftsBox));
	});

	action(Page::Profile, u"Выбрать баннер профиля"_q, u"Локальное изображение для шапки собственного профиля"_q, [=] {
		FileDialog::GetOpenPath(box.get(), u"Баннер профиля"_q,
			u"Изображения (*.png *.jpg *.jpeg *.webp)"_q,
			crl::guard(box, [=](FileDialog::OpenResult &&result) {
				QBuffer buffer(&result.remoteContent);
				QImageReader reader;
				if (!result.remoteContent.isEmpty() && buffer.open(QIODevice::ReadOnly)) {
					reader.setDevice(&buffer);
				} else if (!result.paths.isEmpty()) {
					reader.setFileName(result.paths.front());
				}
				const auto size = reader.size();
				if (!size.isValid() || size.width() > 8192 || size.height() > 8192) {
					box->showToast(u"Не удалось прочитать изображение."_q);
					return;
				}
				reader.setAutoTransform(true);
				reader.setScaledSize(size.scaled(QSize(1200, 600), Qt::KeepAspectRatio));
				const auto image = reader.read();
				auto bytes = QByteArray();
				auto output = QBuffer(&bytes);
				if (image.isNull() || !output.open(QIODevice::WriteOnly)
					|| !image.save(&output, "JPG", 85)) {
					box->showToast(u"Не удалось подготовить изображение."_q);
					return;
				}
				auto values = Config::Instance().current();
				values.profileBanner = QString::fromLatin1(bytes.toBase64());
				if (!save(std::move(values))) {
					return;
				}
			}));
	});
	action(Page::Profile, u"Убрать баннер"_q, u"Восстанавливает обычный фон профиля"_q, [=] {
		auto values = Config::Instance().current();
		values.profileBanner.clear();
		if (!save(std::move(values))) {
			return;
		}
	});
	option(Page::Profile, u"Размыть аватар на фоне"_q, u"Использует аватар как мягкий фон профиля"_q, Option::BlurProfileAvatar);
	option(Page::Profile, u"Снег"_q, u"Анимация в шапке собственного профиля"_q, Option::SnowProfile);

	option(Page::Appearance, u"Квадратные аватарки"_q, u"Фотографии в списке чатов и в шапке профиля"_q, Option::SquareAvatars);
	option(Page::Appearance, u"Тонировать фон профиля"_q, u"Полупрозрачная заливка поверх фона"_q, Option::GlassProfile);
	const auto scaleRow = row(Page::Appearance, u"Размер стикеров"_q,
		u"Размер стикеров сообщений от 50% до 150%"_q);
	const auto scaleLabel = scaleRow->add(object_ptr<Ui::FlatLabel>(
		scaleRow,
		QString::number(config.current().stickerScale) + u'%',
		st::boxDividerLabel));
	const auto scale = scaleRow->add(object_ptr<Ui::MediaSlider>(
		scaleRow,
		st::settingsScale));
	scale->setFixedHeight(24);
	scale->setAccessibleName(u"Размер стикеров"_q);
	const auto refreshScale = [=] {
		const auto percent = Config::Instance().current().stickerScale;
		scale->setValue((percent - 50) / 100.);
		scaleLabel->setText(QString::number(percent) + u'%');
	};
	scale->setChangeProgressCallback([=](float64 ratio) {
		const auto percent = 50 + qRound(ratio * 100.);
		scaleLabel->setText(QString::number(percent) + u'%');
	});
	scale->setChangeFinishedCallback([=](float64 ratio) {
		auto values = Config::Instance().current();
		values.stickerScale = 50 + qRound(ratio * 100.);
		if (!save(std::move(values))) {
			refreshScale();
		}
	});
	refreshScale();
	config.changes() | rpl::on_next(refreshScale, scale->lifetime());
	option(Page::Appearance, u"Свечение реакций"_q, u"Мягкий ореол в цвет плашки реакции"_q, Option::ReactionGlow);
	option(Page::Appearance, u"Полупрозрачная сетка подарков"_q, u"Ослабляет заливку карточек подарков"_q, Option::GlassGifts);
	option(Page::Appearance, u"Тонированные карточки"_q, u"Полупрозрачная заливка настроек UI Sandbox"_q, Option::GlassHub);
	option(Page::Messages, u"Отправка по Enter"_q, u"Enter отправляет сообщение, Shift+Enter добавляет строку"_q, Option::EnterToSend);
	option(Page::Messages, u"Панель форматирования"_q, u"Жирный, курсив и зачёркивание над полем ввода"_q, Option::QuickFormatting);
	option(Page::Messages, u"Счётчик символов"_q, u"Длина текста в поле ввода обычного диалога"_q, Option::CharacterCount);
	option(Page::Messages, u"Секунды во времени"_q, u"Время сообщений в формате ЧЧ:ММ:СС"_q, Option::Seconds);
	option(Page::Messages, u"Фильтр Zalgo"_q, u"Ограничивает повторяющиеся диакритические символы при отрисовке"_q, Option::FilterZalgo);
	option(Page::Messages, u"Раскрывать спойлеры"_q, u"Показывает скрытый текст сообщений"_q, Option::RevealSpoilers);
	option(Page::Messages, u"Скрыть рекламу в каналах"_q, u"Скрывает рекламную плашку и сообщения"_q, Option::HideSponsored);
	option(Page::Privacy, u"Предупреждать перед звонком"_q, u"Запрашивает подтверждение исходящего личного звонка"_q, Option::ConfirmCall);
	option(Page::Privacy, u"Скрыть мой номер"_q, u"Скрывает номер в профиле и настройках"_q, Option::HidePhone);
	option(Page::Privacy, u"Скрыть индикатор набора"_q, u"Не отправляет статусы набора и записи"_q, Option::HideTyping);
	option(Page::Privacy, u"Оставлять сообщения непрочитанными"_q, u"Открытие диалога не отправляет отметку о прочтении"_q, Option::KeepUnread);
	option(Page::Privacy, u"Отмечать прочитанным при наборе"_q, u"Исключение для режима непрочитанных сообщений"_q, Option::ReadOnTyping);
	option(Page::Privacy, u"Скрыть онлайн"_q, u"Подавляет активный статус клиента"_q, Option::HideOnline);
	option(Page::Info, u"Отображать Peer ID и DC"_q, u"ID профиля и дата-центр загруженной аватарки"_q, Option::PeerId);
	option(Page::Info, u"Дата создания чата"_q, u"Дата из данных группы или канала"_q, Option::CreationDate);
	option(Page::Info, u"Полные просмотры"_q, u"Точное число просмотров поста"_q, Option::ExactViews);
	option(Page::Media, u"Не воспроизводить следующее голосовое"_q, u"Останавливает очередь после текущего голосового"_q, Option::NoVoiceAutoplay);
	option(Page::Settings, u"Скрыть «Устройства»"_q, u"Главный экран настроек"_q, Option::HideDevices);
	option(Page::Settings, u"Скрыть «Папки»"_q, u"Главный экран настроек"_q, Option::HideFolders);
	option(Page::Settings, u"Скрыть «Конфиденциальность»"_q, u"Главный экран настроек"_q, Option::HidePrivacy);
	option(Page::Settings, u"Скрыть «Язык»"_q, u"Главный экран настроек"_q, Option::HideLanguage);
	option(Page::Settings, u"Скрыть FAQ"_q, u"Главный экран настроек"_q, Option::HideFaq);

	const auto filter = u"UI Sandbox (*.json)"_q;
	action(Page::Settings, u"Экспортировать JSON"_q, u"Выгрузка всех настроек и локальных подарков"_q, [=] {
		FileDialog::GetWritePath(box.get(), u"Экспорт UI Sandbox"_q, filter, u"ui_sandbox.json"_q, crl::guard(box, [=](QString &&path) {
			auto file = QSaveFile(path);
			const auto data = Config::Instance().exportJson();
			const auto ok = file.open(QIODevice::WriteOnly)
				&& file.write(data) == static_cast<qint64>(data.size())
				&& file.commit();
			box->showToast(ok ? u"Настройки экспортированы."_q : u"Ошибка экспорта."_q);
		}));
	});
	action(Page::Settings, u"Импортировать JSON"_q, u"Заменяет текущие локальные настройки"_q, [=] {
		FileDialog::GetOpenPath(box.get(), u"Импорт UI Sandbox"_q, filter, crl::guard(box, [=](FileDialog::OpenResult &&result) {
			auto data = std::move(result.remoteContent);
			if (data.isEmpty() && !result.paths.isEmpty()) {
				auto file = QFile(result.paths.front());
				if (file.open(QIODevice::ReadOnly)) {
					data = file.read(kMaxImport + 1);
				}
			}
			state->refreshFields = true;
			const auto ok = Config::Instance().importJson(data);
			state->refreshFields = false;
			if (ok) {
				NotifyProfile(session);
			}
			box->showToast(ok ? u"Настройки импортированы."_q : u"Не удалось импортировать JSON."_q);
		}));
	});
	action(Page::Settings, u"Сбросить настройки"_q, u"Удаляет и настройки, и локальные подарки"_q, [=] {
		show->showBox(Ui::MakeConfirmBox({
			.text = u"Сбросить все настройки и удалить локальные подарки?"_q,
			.confirmed = crl::guard(box, [=] {
				state->refreshFields = true;
				const auto ok = Config::Instance().reset();
				state->refreshFields = false;
				if (ok) {
					NotifyProfile(session);
				}
				box->showToast(ok ? u"UI Sandbox: сброшено"_q : u"Не удалось сбросить настройки."_q);
			}),
			.confirmText = rpl::single(u"Сбросить"_q),
		}));
	});

	search->changes() | rpl::on_next(refresh, search->lifetime());
	refresh();
}

} // namespace UiSandbox
