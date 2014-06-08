#include "MapScene.h"


MapScene::MapScene() : Scene()
{
	active_map = 0;
	previous_map = 13;
	memset(flags, 0, sizeof(bool) * 16 * 256);
	active_script = 0;
	last_healed_map = 0;
	teleport_stage = 0;
	teleport_steps = 0;
	teleport_timer = 0;

	//Initialize the player
	entities.push_back(new OverworldEntity(active_map, 1, STARTING_X, STARTING_Y, ENTITY_DOWN, false, nullptr, [this]() {Walk(); }));
	focus_entity = entities[0];

	current_fade.Reset();
	Focus(STARTING_X, STARTING_Y);
	SwitchMap(STARTING_MAP);
}

MapScene::~MapScene()
{
	if (active_map)
		delete active_map;
	if (active_script)
	{
		delete active_script;
		active_script = 0;
	}
	ClearEntities(true);
}

/// <summary>
/// Updates this instance.
/// </summary>
void MapScene::Update()
{
	current_fade.Update();
	ProcessTeleport();
	Engine::GetMusicPlayer().Update();
	if (active_script && (focus_entity ? focus_entity->Snapped() : true))
		active_script->Update();
	if (!teleport_stage)
	{
		CheckWarp();

		if (InputController::KeyDownOnce(sf::Keyboard::F1))
			memset(flags, 0, sizeof(bool) * 4096);
		if (!UpdateTextboxes() && current_fade.Done() && input_enabled && !focus_entity->Frozen())
		{
			if (!Interact())
			{
				if (sf::Keyboard::isKeyPressed(INPUT_DOWN))
				{
					focus_entity->StartMoving(ENTITY_DOWN);
					TryResetWarp();
				}
				else if (sf::Keyboard::isKeyPressed(INPUT_UP))
				{
					focus_entity->StartMoving(ENTITY_UP);
					TryResetWarp();
				}
				else if (sf::Keyboard::isKeyPressed(INPUT_LEFT))
				{
					focus_entity->StartMoving(ENTITY_LEFT);
					TryResetWarp();
				}
				else if (sf::Keyboard::isKeyPressed(INPUT_RIGHT))
				{
					focus_entity->StartMoving(ENTITY_RIGHT);
					TryResetWarp();
				}
				else if (sf::Keyboard::isKeyPressed(sf::Keyboard::F1))
				{
					Players::GetPlayer1()->RandomParty();
				}
				else
					focus_entity->StopMoving();

				if (focus_entity->Snapped() && InputController::KeyDownOnce(INPUT_START))
				{
					ShowTextbox(MenuCache::StartMenu());
					MenuCache::StartMenu()->SetArrowState(ArrowStates::ACTIVE);
				}
			}
		}
		else
			focus_entity->StopMoving();

		for (unsigned int i = 0; i < entities.size(); i++)
		{
			if (entities[i])
				entities[i]->Update();
		}

		int x = (int)(focus_entity ? focus_entity->x : 0);
		int y = (int)(focus_entity ? focus_entity->y : 0);

		if (x < 0 && active_map->HasConnection(CONNECTION_WEST))
		{
			MapConnection connection = active_map->connections[CONNECTION_WEST];
			SwitchMap(active_map->connections[CONNECTION_WEST].map);
			focus_entity->x = active_map->width * 32 - (focus_entity->GetIndex() > 0 ? 1 : 2);
			focus_entity->y = y + (connection.y_alignment + (connection.y_alignment < 0 ? 0 : 0)) * 16;
			//FocusFree(active_map->width * 32 + 16, y + (connection.y_alignment + (connection.y_alignment < 0 ? -1 : -1)) * 16);
		}
		else if (x >= active_map->width * 32 && active_map->HasConnection(CONNECTION_EAST))
		{
			MapConnection connection = active_map->connections[CONNECTION_EAST];
			SwitchMap(active_map->connections[CONNECTION_EAST].map);
			focus_entity->x = 0;
			focus_entity->y = y + (connection.y_alignment + (connection.y_alignment < 0 ? 0 : 0)) * 16;
			//FocusFree(16, y + (connection.y_alignment + (connection.y_alignment < 0 ? -1 : -1)) * 16);
		}

		x = (int)(focus_entity ? focus_entity->x : 0);
		y = (int)(focus_entity ? focus_entity->y : 0);
		if (y < 0 && active_map->HasConnection(CONNECTION_NORTH))
		{
			MapConnection connection = active_map->connections[CONNECTION_NORTH];
			SwitchMap(active_map->connections[CONNECTION_NORTH].map);
			focus_entity->x = x + (connection.x_alignment + (connection.x_alignment < 0 ? 0 : 0)) * 16;
			focus_entity->y = active_map->height * 32 - (focus_entity->GetIndex() > 0 ? 1 : 2);
			//FocusFree(x + (connection.x_alignment + (connection.x_alignment < 0 ? 1 : 1)) * 16, active_map->height * 32 - 20);
		}
		else if (y >= active_map->height * 32 && active_map->HasConnection(CONNECTION_SOUTH))
		{
			MapConnection connection = active_map->connections[CONNECTION_SOUTH];
			SwitchMap(active_map->connections[CONNECTION_SOUTH].map);
			focus_entity->x = x + (connection.x_alignment + (connection.x_alignment < 0 ? 0 : 0)) * 16;
			focus_entity->y = 0;
			//FocusFree(x + (connection.x_alignment + (connection.x_alignment < 0 ? 1 : 1)) * 16, -16);
		}
	}

	Tileset* tex = ResourceCache::GetTileset(active_map->tileset);
	if (tex)
		tex->AnimateTiles();

	if (!current_fade.Done() && current_fade.CurrentFade() != current_fade.LastFade())
	{
		SetPalette(current_fade.GetCurrentPalette());
	}

}

void MapScene::Render(sf::RenderWindow* window)
{
	FocusFree(focus_entity->x, focus_entity->y);
	window->setView(viewport);

	if (active_map)
	{
		DrawMap(window, *active_map, -1, 0);
		for (int i = 0; i < 4; i++)
		{
			if (active_map->HasConnection(i))
			{
				DrawMap(window, *active_map->connected_maps[i], i, &active_map->connections[i]);
			}
		}
	}

	for (int i = entities.size() - 1; i > -1; i--)
		entities[i]->Render(window);
	window->setView(window->getDefaultView());

	for (unsigned int i = 0; i < textboxes.size(); i++)
		textboxes[i]->Render(window);
}

void MapScene::NotifySwitchedTo()
{
	poison_steps = 4;
}

void MapScene::SwitchMap(unsigned char index)
{
	ClearEntities();
	unsigned char previous_palette = 0;
	if (index <= OUTSIDE_MAP)
		previous_map = index;

	if (active_script)
	{
		delete active_script;
		active_script = 0;
	}
	active_script = Script::TryLoad(this, index, 255);

	if (!active_map)
	{
		active_map = new Map(index, &entities);
	}
	else
	{
		previous_palette = ResourceCache::GetMapPaletteIndex(active_map->index);
		active_map->index = index;
	}

	if (!active_map->Load())
	{
#ifdef _DEBUG
		cout << "FATAL: Failed to load map " << index << "!";
#endif
		return;
	}

	//temporary until we get healing working
	if (active_map->tileset == 2 || active_map->tileset == 6) //pc
		last_healed_map = previous_map;

	for (unsigned int i = 0; i < active_map->entities.size(); i++)
	{
		Entity e = active_map->entities[i];
		NPC* o = new NPC(active_map, e, Script::TryLoad(this, active_map->index, active_map->entities[i].text));

		entities.push_back(o);
	}

	if (focus_entity)
		focus_entity->SetMap(active_map);

	if (focus_entity->GetIndex() == 0 && !ResourceCache::CanUseBicycle(active_map->tileset) && active_map->index != 34 && active_map->index != 9)
		focus_entity->SetSprite(1);

	SetPalette(active_map->GetPalette());

	if (teleport_stage == 0 && focus_entity->GetIndex() != 0)
		Engine::GetMusicPlayer().Play(ResourceCache::GetMusicIndex(active_map->index), true);
}

void MapScene::Focus(signed char x, signed char y)
{
	viewport.reset(sf::FloatRect((float)(int)(x - VIEWPORT_WIDTH / 2 + 1) * 16, (float)(int)(y - VIEWPORT_HEIGHT / 2) * 16, VIEWPORT_WIDTH * 16, VIEWPORT_HEIGHT * 16));
}

void MapScene::FocusFree(int x, int y)
{
	viewport.reset(sf::FloatRect((float)(x - (int)(VIEWPORT_WIDTH / 2 - 1) * 16), (float)(y - ((int)(VIEWPORT_HEIGHT / 2)) * 16), VIEWPORT_WIDTH * 16, VIEWPORT_HEIGHT * 16));
}

void MapScene::DrawMap(sf::RenderWindow* window, Map& map, int connection_index, MapConnection* connection)
{
	int startX = (int)(viewport.getCenter().x - viewport.getSize().x / 2) / 32;
	int startY = (int)(viewport.getCenter().y - viewport.getSize().y / 2) / 32;
	int endX = (int)(viewport.getCenter().x + viewport.getSize().x / 2) / 32;
	int endY = (int)(viewport.getCenter().y + viewport.getSize().y / 2) / 32;

	switch (connection_index)
	{
	case CONNECTION_NORTH:
		startX += (connection->x_alignment + 1) / 2;
		endX += (connection->x_alignment + 1) / 2;
		startY += ((unsigned char)connection->y_alignment + 1) / 2;
		endY += ((unsigned char)connection->y_alignment + 1) / 2;

		endY = min(endY, map.height - 1);
		break;
	case CONNECTION_SOUTH:
		startX += (connection->x_alignment + 1) / 2;
		endX += (connection->x_alignment + 1) / 2;
		startY += ((unsigned char)connection->y_alignment + 1) / 2;
		endY += ((unsigned char)connection->y_alignment + 1) / 2;

		startY -= active_map->height;
		endY -= active_map->height;
		break;
	case CONNECTION_WEST:
		startX += ((unsigned char)connection->x_alignment + 1) / 2;
		endX += ((unsigned char)connection->x_alignment + 1) / 2;
		startY += (connection->y_alignment + 1) / 2;
		endY += (connection->y_alignment + 1) / 2;

		endX = min(endX, map.width - 1);
		break;
	case CONNECTION_EAST:
		startX += ((unsigned char)connection->x_alignment + 1) / 2;
		endX += ((unsigned char)connection->x_alignment + 1) / 2;
		startY += (connection->y_alignment + 1) / 2;
		endY += (connection->y_alignment + 1) / 2;

		startX -= active_map->width;
		endX -= active_map->width;
		break;
	}

	Tileset* tileset = ResourceCache::GetTileset(map.tileset); //isn't it great having the textures be small and cached so we don't have to worry about loading once and passing them around? :D
	if (!tileset)
		return;
	for (int x = startX - 1; x <= endX; x++)
	{
		for (int y = startY - 1; y <= endY; y++)
		{
			unsigned char tile = map.border_tile;
			if (x >= 0 && x < map.width && y >= 0 && y < map.height) //are we drawing a piece of the map or a border tile
				tile = map.tiles[y * map.width + x];
			else if (connection_index != -1) //don't draw border tiles for connected maps
				continue;
			int drawX = x;
			int drawY = y;
			if (connection)
			{
				switch (connection_index)
				{
				case CONNECTION_NORTH: //north
				case CONNECTION_SOUTH: //south
					drawX -= (connection->x_alignment + 1) / 2;
					drawY -= ((unsigned char)connection->y_alignment + 1) / 2;
					if (connection->x_alignment < 0)
						drawX++;
					break;

				case CONNECTION_WEST: //west
				case CONNECTION_EAST:
					drawX -= ((unsigned char)connection->x_alignment + 1) / 2;
					drawY -= (connection->y_alignment + 1) / 2;
					if (connection->y_alignment < 0)
						drawY++;
					break;
				}

				switch (connection_index)
				{
				case CONNECTION_SOUTH: //south
					drawY += active_map->height;
					break;
				case CONNECTION_EAST: //west
					drawX += active_map->width;
					break;
				}
			}

			tileset->Render(window, drawX, drawY, tile, 4, 4);
		}
	}
}

void MapScene::ClearEntities(bool focused)
{
	for (unsigned int i = 0; i < entities.size(); i++)
	{
		if (entities[i] != focus_entity || focused)
		{
			delete entities[i];
			entities.erase(entities.begin() + i--);
		}
	}
}

void MapScene::SetPalette(sf::Color* pal)
{
	Tileset* tex = ResourceCache::GetTileset(active_map->tileset);

	if (tex)
	{
		tex->SetPalette(pal);
		for (int i = 0; i < 4; i++)
		{
			if (active_map->connected_maps[i])
				ResourceCache::GetTileset(active_map->connected_maps[i]->tileset)->SetPalette(pal);
		}
	}

	if (current_fade.Done())
	{
		ResourceCache::GetMenuTexture()->SetPalette(pal);
		ResourceCache::GetFontTexture()->SetPalette(pal);
		for (int i = 0; i < 3; i++)
		{
			sf::Color hp[4] = { pal[0], ResourceCache::GetPalette(31 + i)[0], ResourceCache::GetPalette(31 + i)[2], ResourceCache::GetPalette(31 + i)[3] };
			ResourceCache::GetStatusesTexture(i)->SetPalette(hp);
		}

		sf::Color palette[4] = { pal[0], ResourceCache::GetPalette(POKE_DEFAULT_PAL)[0], ResourceCache::GetPalette(POKE_DEFAULT_PAL)[1], ResourceCache::GetPalette(POKE_DEFAULT_PAL)[3] };
		ResourceCache::GetPokemonIcons()->SetPalette(palette);
	}

	for (unsigned int i = 0; i < entities.size(); i++)
	{
		if (entities[i])
			entities[i]->SetPalette(pal);
	}
}

bool MapScene::Interact()
{
	if (!focus_entity || !focus_entity->Snapped() || !InputController::KeyDownOnce(INPUT_A))
		return false;
	int x = focus_entity->x / 16 + DELTAX(focus_entity->GetDirection());
	int y = focus_entity->y / 16 + DELTAY(focus_entity->GetDirection());

	//check for signs
	for (unsigned int i = 0; i < active_map->signs.size(); i++)
	{
		if (active_map->signs[i].x == x && active_map->signs[i].y == y)
		{
			Textbox* t = new Textbox();
			t->SetText(new TextItem(t, nullptr, pokestring(string("This is a sign\nwith index ").append(itos((int)i).append(".")).c_str())));
			textboxes.push_back(t);
			return true;
		}
	}

	//check for entities
	for (unsigned int i = 1; i < entities.size(); i++)
	{
		if (entities[i]->Snapped() && entities[i]->GetMovementDirection() == MOVEMENT_NONE && entities[i]->x / 16 == x && entities[i]->y / 16 == y)
		{
			Textbox* t = new Textbox();
			string s;
			if ((active_map->entities[i - 1].text & 0x40) != 0)
				s = pokestring(string("This is a trainer\nwith index ").append(itos((int)(i - 1)).append(".")).append("\rTrainer ID: ").append(itos(active_map->entities[i - 1].trainer)).append("\n#MON set: ").append(itos(active_map->entities[i - 1].pokemon_set)));
			else if ((active_map->entities[i - 1].text & 0x80) != 0)
			{
				if (!Players::GetPlayer1()->GetInventory()->AddItem(active_map->entities[i - 1].item, 1))
					s = pokestring("No more room for\nitems!");
				else
				{
					s = string(Players::GetPlayer1()->GetName()).append(pokestring(" found\n")).append(ResourceCache::GetItemName(active_map->entities[i - 1].item)).append(pokestring("!"));
					entities.erase(entities.begin() + i--);
					t->SetText(new TextItem(t, nullptr, s, i));
					textboxes.push_back(t);
					continue;
				}
			}
			else
			{
				s = pokestring(string("This is a person\nwith index ").append(itos((int)(i - 1)).append(".")));
				if (entities[i]->GetScript() != 0)
				{
					entities[i]->Face((1 - (focus_entity->GetDirection() % 2) + (focus_entity->GetDirection() / 2 * 2)));
					entities[i]->SetScriptState(true);
					return true;
				}
			}

			t->SetText(new TextItem(t, nullptr, s, i));
			textboxes.push_back(t);
			entities[i]->Face((1 - (focus_entity->GetDirection() % 2) + (focus_entity->GetDirection() / 2 * 2)));
			((NPC*)entities[i])->occupation = t;

			return true;
		}
	}

	return false;
}

void MapScene::WarpTo(Warp& w)
{
	poison_steps = 4;
	current_fade.SetFadeToBlack(active_map->GetPalette());
	current_fade.Start(w);
	if (w.dest_map != ELEVATOR_MAP)
	{
		elevator_map = active_map->index;
	}
	can_warp = false;
	input_enabled = false;
	focus_entity->ForceStop();
}

void MapScene::CheckWarp()
{
	if (focus_entity->Snapped())
	{
		Warp* w = active_map->GetWarpAt(focus_entity->x / 16, focus_entity->y / 16);
		if (can_warp && active_map->CanWarp(focus_entity->x / 16, focus_entity->y / 16, focus_entity->GetMovementDirection(), w))
		{
			WarpTo(*w);
		}
		else if (current_fade.Done())
		{
			if (current_fade.GetWarpTo().dest_map != 254)
			{
				Warp to = current_fade.GetWarpTo();
				unsigned char walk_direction = 0xFF;
				//Determine whether or not the player walks after exiting a map
				if (to.dest_map == 255)
					to.dest_map = previous_map;
				else if (to.dest_map == ELEVATOR_MAP)
					to.dest_map = elevator_map;

				if (to.type == WARP_TYPE_NORMAL)
				{
					if (focus_entity->GetDirection() == ENTITY_DOWN && to.type == WARP_TO_OUTSIDE && to.dest_map <= OUTSIDE_MAP)
						walk_direction = ENTITY_DOWN;

					SwitchMap(to.dest_map);
					focus_entity->x = active_map->GetWarp(to.dest_point).x * 16;
					focus_entity->y = active_map->GetWarp(to.dest_point).y * 16;

					if (active_map->IsPassable(focus_entity->x / 16, focus_entity->y / 16 + 1) && active_map->CanWarp(focus_entity->x / 16, focus_entity->y / 16, 0xFF, &to) && !active_map->IsPassable(focus_entity->x / 16, focus_entity->y / 16 - 1) && !active_map->IsPassable(focus_entity->x / 16 - 1, focus_entity->y / 16) && !active_map->IsPassable(focus_entity->x / 16 + 1, focus_entity->y / 16))
						walk_direction = ENTITY_DOWN;
					if (walk_direction == ENTITY_DOWN)
					{
						focus_entity->Move(walk_direction, 1);
						poison_steps = 5;
					}
					else
						poison_steps = 4;
				}
				else
				{
					SwitchMap(to.dest_map);
					focus_entity->x = to.x * 16;
					focus_entity->y = to.y * 16;
					focus_entity->Face(ENTITY_DOWN);
				}
				current_fade.SetWarpTo(Warp(254));

				input_enabled = true;
			}
		}
	}
	else if (current_fade.Done())
		can_warp = true;
}

void MapScene::TryResetWarp()
{
	Warp* w = active_map->GetWarpAt(focus_entity->x / 16, focus_entity->y / 16);
	if (!active_map->CanWarp(focus_entity->x / 16, focus_entity->y / 16, focus_entity->GetMovementDirection(), w))
	{
		if (w)
		{
			can_warp = true;
		}
	}
}

void MapScene::Walk()
{
	Warp* w = active_map->GetWarpAt(focus_entity->x / 16, focus_entity->y / 16);
	if (can_warp && active_map->CanWarp(focus_entity->x / 16, focus_entity->y / 16, focus_entity->GetMovementDirection(), w))
		return;
	if (repel_steps > 0)
	{
		repel_steps--;
		if (!repel_steps)
		{
			Textbox* t = new Textbox();
			t->SetText(new TextItem(t, nullptr, pokestring("REPEL's effect\nwore off.")));
			ShowTextbox(t);
		}
	}

	poison_steps--;
	if (poison_steps == 0)
	{
		poison_steps = 4;
		unsigned char fainted = 0;
		string s = "";
		for (unsigned int i = 0; i < Players::GetPlayer1()->GetPartyCount(); i++)
		{
			Pokemon* p = Players::GetPlayer1()->GetParty()[i];
			if (p->status == Statuses::POISONED && p->hp > 0)
			{
				ResourceCache::GetTileset(active_map->tileset)->SetPoisonTimer();
				p->hp--;
				if (!p->hp)
				{
					p->status = Statuses::FAINTED;
					if (s != "")
						s.append(pokestring("\r"));
					s.append(string(p->nickname).append(pokestring("\nfainted!")));
				}
			}
			if (!p->hp)
				fainted++;
		}

		if (s != "")
		{
			focus_entity->ForceStop();
			Textbox* t = new Textbox();
			t->SetText(new TextItem(t, nullptr, s));
			ShowTextbox(t);

			if (fainted == Players::GetPlayer1()->GetPartyCount())
			{
				PokemonUtils::Faint(this, t);
			}
		}
	}
}

void MapScene::UseEscapeRope()
{
	if (!focus_entity)
		return;
	teleport_steps = 15;
	teleport_timer = 60;
	teleport_stage = 1;
	focus_entity->Face(ENTITY_DOWN);
	Engine::GetMusicPlayer().Play(0, true);
}

void MapScene::ProcessTeleport()
{
	switch (teleport_stage)
	{
	case 1:
		if (teleport_timer > 0)
			teleport_timer--;
		else
		{
			teleport_timer = 15;
			teleport_stage = 2;
		}
		break;

	case 2:
		if (teleport_timer > 0)
		{
			teleport_timer--;
			if (!teleport_timer)
			{
				if (focus_entity->GetDirection() == ENTITY_DOWN)
					focus_entity->Face(ENTITY_LEFT);
				else if (focus_entity->GetDirection() == ENTITY_LEFT)
					focus_entity->Face(ENTITY_UP);
				else if (focus_entity->GetDirection() == ENTITY_UP)
					focus_entity->Face(ENTITY_RIGHT);
				else if (focus_entity->GetDirection() == ENTITY_RIGHT)
					focus_entity->Face(ENTITY_DOWN);
				if (teleport_steps > 0)
				{
					teleport_steps--;
					teleport_timer = teleport_steps;
				}
				else
				{
					focus_entity->Face(ENTITY_DOWN);
					teleport_stage = 3;
				}
			}
		}
		else
			teleport_stage = 3;
		break;

	case 3:
		focus_entity->Face(ENTITY_DOWN);
		if (teleport_timer > 0)
			teleport_timer--;
		else
		{
			if (focus_entity->offset_y > -80)
			{
				teleport_timer = 0;
				focus_entity->offset_y -= 8;
			}
			else
			{
				focus_entity->offset_y = -80;
				teleport_stage = 4;
				current_fade.SetFadeToWhite(active_map->GetPalette());
				current_fade.Start(Warp());
			}
		}
		break;

	case 4:
		if (current_fade.Done())
		{
			teleport_stage = 5;
			teleport_timer = 16;
			Warp w;
			w.dest_map = this->last_healed_map;
			for (int i = 0; i < 13; i++)
			{
				if (ResourceCache::GetFlyPoint(i).map == w.dest_map)
				{
					w.x = ResourceCache::GetFlyPoint(i).x;
					w.y = ResourceCache::GetFlyPoint(i).y;
					w.type = WARP_TYPE_SET;
					break;
				}
			}
			SwitchMap(w.dest_map);
			focus_entity->x = w.x * 16;
			focus_entity->y = w.y * 16;
			current_fade.SetFadeFromWhite(active_map->GetPalette());
			current_fade.Start(w);
		}
		break;

	case 5:
		if (current_fade.Done())
		{
			if (teleport_timer > 0)
				teleport_timer--;
			else
			{
				if (focus_entity->offset_y < 0)
				{
					teleport_timer = 0;
					focus_entity->offset_y += 8;
				}
				else
				{
					teleport_stage = 6;
					teleport_steps = 1;
					teleport_timer = 1;
				}
			}
		}

	case 6:
		if (teleport_timer > 0)
		{
			teleport_timer--;
			if (!teleport_timer)
			{
				if (focus_entity->GetDirection() == ENTITY_DOWN)
					focus_entity->Face(ENTITY_LEFT);
				else if (focus_entity->GetDirection() == ENTITY_LEFT)
					focus_entity->Face(ENTITY_UP);
				else if (focus_entity->GetDirection() == ENTITY_UP)
					focus_entity->Face(ENTITY_RIGHT);
				else if (focus_entity->GetDirection() == ENTITY_RIGHT)
					focus_entity->Face(ENTITY_DOWN);
				if (teleport_steps < 9)
				{
					teleport_steps++;
					teleport_timer = teleport_steps;
				}
				else
				{
					focus_entity->Face(ENTITY_DOWN);
					teleport_timer = 30;
					teleport_stage = 7;
				}
			}
		}
		break;

	case 7:
		if (teleport_timer > 0)
			teleport_timer--;
		else
		{
			teleport_stage = 0;
			Engine::GetMusicPlayer().Play(ResourceCache::GetMusicIndex(active_map->index), false);
		}
		break;
	}
}
