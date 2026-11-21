#include "ui_sandbox/ui_sandbox_documents.h"

#include "base/flat_map.h"
#include "base/flat_set.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_session.h"
#include "data/stickers/data_custom_emoji.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "ui_sandbox/ui_sandbox_config.h"

namespace UiSandbox {
namespace {

struct PreloadState {
	bool active = false;
	base::flat_set<DocumentId> wanted;
	base::flat_set<DocumentId> requested;
	base::flat_map<DocumentId, std::shared_ptr<Data::DocumentMedia>> media;
};

bool KeepMedia(
		not_null<PreloadState*> state,
		not_null<DocumentData*> document) {
	if (state->media.contains(document->id)) {
		return false;
	}
	auto media = document->createMediaView();
	media->checkStickerLarge();
	state->media.emplace(document->id, std::move(media));
	return true;
}

void RefreshDocuments(
		not_null<Main::Session*> session,
		std::shared_ptr<PreloadState> state) {
	if (!state->active) {
		return;
	}
	auto &config = Config::Instance();
	const auto &values = config.current();
	auto wanted = base::flat_set<DocumentId>();
	const auto add = [&](DocumentId id) {
		if (id) {
			wanted.emplace(id);
		}
	};
	add(values.emojiStatusId);
	for (const auto &gift : values.gifts) {
		add(gift.documentId);
		add(gift.patternDocumentId);
	}
	state->wanted = wanted;
	for (auto i = state->media.begin(); i != state->media.end();) {
		if (!wanted.contains(i->first)) {
			i = state->media.erase(i);
		} else {
			++i;
		}
	}
	auto &owner = session->data();
	for (const auto id : wanted) {
		const auto document = owner.document(id);
		if (document->sticker() && document->hasRemoteLocation()) {
			if (KeepMedia(state.get(), document)) {
				config.notifyDocumentChanged({ session.get(), id });
			}
			continue;
		}
		if (!state->requested.emplace(id).second) {
			continue;
		}
		const auto notify = [=] {
			if (state->active && state->wanted.contains(id)) {
				Config::Instance().notifyDocumentChanged({ session.get(), id });
			}
		};
		owner.customEmojiManager().resolve(
			id,
			true
		) | rpl::on_next_error([=](not_null<DocumentData*> result) {
			if (state->active && state->wanted.contains(id)) {
				KeepMedia(state.get(), result);
			}
			notify();
		}, notify, session->lifetime());
	}
}

} // namespace

void PreloadDocuments(not_null<Main::Session*> session) {
	const auto state = std::make_shared<PreloadState>();
	Config::Instance().changes() | rpl::on_next([=] {
		RefreshDocuments(session, state);
	}, session->lifetime());
	session->domain().activeSessionValue(
	) | rpl::on_next([=](Main::Session *active) {
		state->active = (active == session.get());
		RefreshDocuments(session, state);
	}, session->lifetime());
}

} // namespace UiSandbox
