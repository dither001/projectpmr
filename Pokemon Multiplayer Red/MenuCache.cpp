#include "MenuCache.h"

Textbox* MenuCache::start_menu = 0;
Textbox* MenuCache::debug_menu = 0;

MenuCache::MenuCache()
{
}

MenuCache::~MenuCache()
{
}

void MenuCache::ReleaseResources()
{
	if (start_menu)
		delete start_menu;
	if (debug_menu)
		delete debug_menu;
}

//Return the start menu or create it if it doesn't exist
//Since the game starts without the pokedex, we shouldn't add it here but in MapScene
Textbox* MenuCache::StartMenu(std::vector<Textbox*>* owner)
{
	if (start_menu)
		return start_menu;

	start_menu = new Textbox(10, 0, 10, 16, false);
	auto cancel = []()->void
	{
		Textbox* message = new Textbox();
		message->SetText(new TextItem(start_menu, []()->void { start_menu->SetArrowState(ArrowStates::ACTIVE); }, string("You cannot\ncancel!")));
		start_menu->SetArrowState(ArrowStates::INACTIVE);
		start_menu->ShowTextbox(message);
		start_menu->CancelClose();
	};
	start_menu->SetMenu(true, 7, sf::Vector2i(1, 1), sf::Vector2u(0, 2), nullptr, MenuFlags::FOCUSABLE | MenuFlags::WRAPS);

	auto doe = []()->void
	{

		Textbox* message = new Textbox();
		message->SetText(new TextItem(start_menu, []()->void { start_menu->SetArrowState(ArrowStates::ACTIVE); }, string("Sorry! That\nfeature has not\rbeen implemented\nyet.")));
		start_menu->SetArrowState(ArrowStates::INACTIVE);
		start_menu->ShowTextbox(message);
	};
	start_menu->GetItems().push_back(new TextItem(start_menu, doe, "POK�DEX", 0));
	start_menu->GetItems().push_back(new TextItem(start_menu, doe, "POK�MON", 1));
	start_menu->GetItems().push_back(new TextItem(start_menu, []()->void
	{
		Textbox* t = new Textbox();
		t->SetText(new TextItem(start_menu, 0, "You selected the\nitems option.\rDramatic...\rtrigger...\revent!\f"));
		start_menu->ShowTextbox(t);
	}
	, "ITEMS", 2));
	start_menu->GetItems().push_back(new TextItem(start_menu, 0, "Lin", 3));
	start_menu->GetItems().push_back(new TextItem(start_menu, 0, "SAVE", 4));
	start_menu->GetItems().push_back(new TextItem(start_menu, 0, "OPTIONS", 5));
	start_menu->GetItems().push_back(new TextItem(start_menu, []()->void { start_menu->Close(); }, "EXIT", 6));

	start_menu->UpdateMenu();
	return start_menu;
}

Textbox* MenuCache::DebugMenu(std::vector<Textbox*>* owner)
{
	if (debug_menu)
		return debug_menu;

	debug_menu = new Textbox(0, 8, 14, 7, false);
	debug_menu->SetMenu(true, 3, sf::Vector2i(1, 1), sf::Vector2u(0, 1), 0, MenuFlags::FOCUSABLE);
	debug_menu->GetItems().push_back(new TextItem(debug_menu, 0, "Testing"));
	debug_menu->GetItems().push_back(new TextItem(debug_menu, 0, "multiple"));
	debug_menu->GetItems().push_back(new TextItem(debug_menu, 0, "menus."));

	debug_menu->UpdateMenu();
	return debug_menu;
}
