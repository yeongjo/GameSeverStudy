#define SFML_STATIC 1
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <chrono>
#include <list>
#include <sstream>
using namespace std;

#ifdef _DEBUG
#pragma comment (lib, "lib/sfml-graphics-s-d.lib")
#pragma comment (lib, "lib/sfml-window-s-d.lib")
#pragma comment (lib, "lib/sfml-system-s-d.lib")
#pragma comment (lib, "lib/sfml-network-s-d.lib")
#else
#pragma comment (lib, "lib/sfml-graphics-s.lib")
#pragma comment (lib, "lib/sfml-window-s.lib")
#pragma comment (lib, "lib/sfml-system-s.lib")
#pragma comment (lib, "lib/sfml-network-s.lib")
#endif
#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "ws2_32.lib")


sf::TcpSocket socket;

constexpr auto SCREEN_WIDTH = 16;
constexpr auto SCREEN_HEIGHT = 16;

constexpr auto TILE_WIDTH = 65;
constexpr auto WINDOW_WIDTH = TILE_WIDTH * SCREEN_WIDTH + 10;   // size of window
constexpr auto WINDOW_HEIGHT = TILE_WIDTH * SCREEN_WIDTH + 10;
constexpr auto BUF_SIZE = 256;
#include "..\..\_SimplestMmoRpg\_SimplestMmoRpgServer\protocol.h"

int g_left_x;
int g_top_y;
int g_myid;

sf::RenderWindow* g_window;
sf::Font g_font;
sf::Text messageText;

namespace sf {
	class TextField : public sf::Transformable, public sf::Drawable {
	private:
		unsigned int m_size;
		sf::Font m_font;
		sf::Text text;
		std::string m_text;
		sf::RectangleShape m_rect;
		bool m_hasfocus;
	public:
		TextField(unsigned int maxChars) :
			m_size(maxChars),
			m_rect(sf::Vector2f(15 * m_size, 35)), // 15 pixels per char, 20 pixels height, you can tweak
			m_hasfocus(false) {
			m_font.loadFromFile("cour.ttf"); // I'm working on Windows, you can put your own font instead
			m_rect.setOutlineThickness(1.3f);
			m_rect.setFillColor(sf::Color::White);
			m_rect.setOutlineColor(sf::Color(127, 127, 127));
			m_rect.setPosition(this->getPosition());
			text.setFont(m_font);
			text.setPosition(this->getPosition());
			text.setFillColor(sf::Color::Black);
			text.setOutlineColor(sf::Color::White);
			text.setOutlineThickness(1);
		}
		void setPosition(int x, int y) {
			Transformable::setPosition(x, y);
			m_rect.setPosition(this->getPosition());
			text.setPosition(this->getPosition());
		}
		const std::string getText() const;
		bool contains(sf::Vector2f point) const;
		void setFocus(bool focus);
		void handleInput(sf::Event e);
		void draw(RenderTarget& target, RenderStates states) const;
		void clear() {
			m_text = "";
		}
	};

	const std::string sf::TextField::getText() const {
		return m_text;
	}

	bool sf::TextField::contains(sf::Vector2f point) const {
		return m_rect.getGlobalBounds().contains(point);
	}

	void TextField::setFocus(bool focus) {
		m_hasfocus = focus;
		if (focus) {
			m_rect.setOutlineColor(Color::Blue);
			m_rect.setFillColor(Color(0, 0, 0, 100));
		} else {
			m_rect.setOutlineColor(Color(127, 127, 127)); // Gray color
			m_rect.setFillColor(Color(0, 0, 0, 30));
		}
	}

	void TextField::handleInput(Event e) {
		if (!m_hasfocus || e.type != Event::TextEntered)
			return;

		if (e.text.unicode == 8) {   // Delete key
			m_text = m_text.substr(0, m_text.size() - 1);
		} else if (m_text.size() < m_size) {
			m_text += e.text.unicode;
		}
		text.setString(m_text);
	}

	void TextField::draw(RenderTarget& target, RenderStates states) const {
		target.draw(m_rect);
		target.draw(text);
	}
}

struct MessageLine {
	string message;
	long long print_time;
	MessageLine(string message, long long print_time) : message(message), print_time(print_time) {

	}
};
list<MessageLine> message_box;
sf::TextField* textField;

long long get_this_time() {
	return static_cast<long long>(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().
		time_since_epoch()).count());
}

void add_message(string message) {
	message_box.emplace_back(message, get_this_time());
	if (message_box.size() > 10) {
		message_box.pop_front();
	}
}

class OBJECT {
private:
	bool m_showing;
	sf::Sprite m_sprite;
	sf::Text m_name;
	sf::Text m_chat;
	chrono::system_clock::time_point m_mess_end_time;
public:
	int m_x, m_y;

	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
		set_name("NONAME");
		m_mess_end_time = chrono::system_clock::now();
	}
	OBJECT() {
		m_showing = false;
	}
	void show() {
		m_showing = true;
	}
	void hide() {
		m_showing = false;
	}

	void a_move(int x, int y) {
		m_sprite.setPosition((float)x, (float)y);
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void move(int x, int y) {
		m_x = x;
		m_y = y;
	}
	void draw() {
		if (false == m_showing) return;
		float rx = (m_x - g_left_x) * 65.0f + 8;
		float ry = (m_y - g_top_y) * 65.0f + 8;
		m_sprite.setPosition(rx, ry);
		g_window->draw(m_sprite);
		if (m_mess_end_time < chrono::system_clock::now()) {
			m_name.setPosition(rx - 10, ry - 20);
			g_window->draw(m_name);
		} else {
			m_chat.setPosition(rx - 10, ry - 20);
			g_window->draw(m_chat);
		}
	}
	void set_name(const char str[]) {
		m_name.setFont(g_font);
		m_name.setString(str);
		m_name.setFillColor(sf::Color(255, 255, 0));
		m_name.setStyle(sf::Text::Bold);
	}
	string get_name() {
		return m_name.getString();
	}
	void set_chat(const char str[]) {
		m_chat.setFont(g_font);
		m_chat.setString(str);
		m_chat.setFillColor(sf::Color(255, 255, 255));
		m_chat.setStyle(sf::Text::Bold);
		m_mess_end_time = chrono::system_clock::now() + chrono::seconds(3);
	}
	void print_stats(const char str[]) {
		m_chat.setFont(g_font);
		m_chat.setString(str);
		m_chat.setFillColor(sf::Color(255, 0, 0));
		m_chat.setStyle(sf::Text::Bold);
		m_mess_end_time = chrono::system_clock::now() + chrono::seconds(1);
	}
};
class Player : public OBJECT {
	int hp, level, exp;
	long long last_attack_time = 0;

public:
	Player(sf::Texture& t, int x, int y, int x2, int y2) : OBJECT(t, x, y, x2, y2) {}
	Player() :OBJECT() {}
	bool attack();
	int GetHp() { return hp; }
	int GetLevel() { return level; }
	int GetExp() { return exp; }
	void SetHp(int hp) { this->hp = hp; }
	void SetLevel(int level) { this->level = level; }
	void SetExp(int exp) { this->exp = exp; }
};

Player avatar;
OBJECT players[MAX_USER];

OBJECT white_tile;
OBJECT black_tile;

sf::Texture* board;
sf::Texture* pieces;
sf::Texture* mapTexture;
sf::Image mapImage;
sf::Vector2u mapSize;


ETile GetMapTile(int x, int y) {
	_ASSERT(0 <= x && x < mapSize.x);
	_ASSERT(0 <= y && y < mapSize.y);
	auto color = mapImage.getPixel(x, y);
	if(color.r > 0.5f){
		return Wall;
	}
	return Empty;
}

OBJECT* get_player(int id) {
	return &players[id];
}

void client_initialize() {
	board = new sf::Texture;
	pieces = new sf::Texture;
	mapTexture = new sf::Texture;
	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		while (true);
	}
	board->loadFromFile("chessmap.bmp");
	pieces->loadFromFile("chess2.png");
	mapTexture->loadFromFile(MAP_PATH);
	mapImage = mapTexture->copyToImage();
	mapSize = mapImage.getSize();
	textField = new sf::TextField(MAX_STR_LEN-1);
	textField->setPosition(0, 40);
	white_tile = OBJECT{ *board, 5, 5, TILE_WIDTH, TILE_WIDTH };
	black_tile = OBJECT{ *board, 69, 5, TILE_WIDTH, TILE_WIDTH };
	avatar = Player{ *pieces, 128, 0, 64, 64 };
	//avatar.move(4, 4);
	for (auto& pl : players) {
		pl = OBJECT{ *pieces, 64, 0, 64, 64 };
	}
}

void client_finish() {
	delete board;
	delete pieces;
}

void ProcessPacket(char* ptr) {
	static bool first_time = true;
	switch (ptr[1]) {
	case S2C_LOGIN_OK: {
		s2c_login_ok* packet = reinterpret_cast<s2c_login_ok*>(ptr);
		g_myid = packet->id;
		avatar.m_x = packet->x;
		avatar.m_y = packet->y;
		avatar.SetHp(packet->HP);
		avatar.SetLevel(packet->LEVEL);
		avatar.SetExp(packet->EXP);
		//avatar.set_name(packet->name);
		g_left_x = packet->x - SCREEN_WIDTH / 2;
		g_top_y = packet->y - SCREEN_HEIGHT / 2;
		avatar.move(packet->x, packet->y);
		avatar.show();
	}
	break;
	case S2C_ADD_PLAYER: {
		s2c_add_player* my_packet = reinterpret_cast<s2c_add_player*>(ptr);
		int id = my_packet->id;

		//players[id].set_name(my_packet->name);

		if (id < MAX_USER) {
			stringstream ss;
			ss << my_packet->name;
			ss << "/";
			ss << my_packet->id;
			players[id].set_name(ss.str().c_str());
			players[id].move(my_packet->x, my_packet->y);
			players[id].show();
		} else {
			//npc[id - NPC_START].x = my_packet->x;
			//npc[id - NPC_START].y = my_packet->y;
			//npc[id - NPC_START].attr |= BOB_ATTR_VISIBLE;
		}
		break;
	}
	case S2C_MOVE_PLAYER: {
		s2c_move_player* my_packet = reinterpret_cast<s2c_move_player*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
		} else if (other_id < MAX_USER) {
			players[other_id].move(my_packet->x, my_packet->y);
		} else {
			//npc[other_id - NPC_START].x = my_packet->x;
			//npc[other_id - NPC_START].y = my_packet->y;
		}
		break;
	}

	case S2C_REMOVE_PLAYER: {
		s2c_remove_player* my_packet = reinterpret_cast<s2c_remove_player*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hide();
		} else if (other_id < MAX_USER) {
			players[other_id].hide();
		} else {
			//		npc[other_id - NPC_START].attr &= ~BOB_ATTR_VISIBLE;
		}
		break;
	}
	case S2C_CHAT: {
		s2c_chat* my_packet = reinterpret_cast<s2c_chat*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.set_chat(my_packet->message);
		} else if (other_id < MAX_USER) {
			players[other_id].set_chat(my_packet->message);
		} else {
			//		npc[other_id - NPC_START].attr &= ~BOB_ATTR_VISIBLE;
		}
		stringstream ss;
		ss << get_player(other_id)->get_name();
		ss << ": ";
		ss << my_packet->message;
		add_message(ss.str());
		break;
	}
	case SC_STAT_CHANGE: {
		auto my_packet = reinterpret_cast<sc_packet_stat_change*>(ptr);
		stringstream ss;
		ss << my_packet->HP;
		ss << " ";
		if (my_packet->id == g_myid) {
			avatar.SetHp(my_packet->HP);
			avatar.SetLevel(my_packet->LEVEL);
			avatar.SetExp(my_packet->EXP);
			ss << my_packet->LEVEL;
			ss << " ";
			ss << my_packet->EXP;
		}
		players[my_packet->id].print_stats(ss.str().c_str());
		ss.str("");
		ss.clear();
		ss << "id: ";
		ss << my_packet->id;
		ss << " hp:";
		ss << my_packet->HP;
		ss << " lvl:";
		ss << my_packet->LEVEL;
		ss << " exp:";
		ss << my_packet->EXP;
		add_message(ss.str());
		break;
	}
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}
}

void process_data(char* net_buf, size_t io_byte) {
	char* ptr = net_buf;
	static size_t in_packet_size = 0;
	static size_t saved_packet_size = 0;
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte) {
		if (0 == in_packet_size) in_packet_size = ptr[0];
		if (io_byte + saved_packet_size >= in_packet_size) {
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			io_byte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		} else {
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
			saved_packet_size += io_byte;
			io_byte = 0;
		}
	}
}

void client_main() {
	char net_buf[BUF_SIZE];
	size_t	received;

	auto recv_result = socket.receive(net_buf, BUF_SIZE, received);
	if (recv_result == sf::Socket::Error) {
		wcout << L"Recv 에러!";
		while (true);
	}
	if (recv_result != sf::Socket::NotReady)
		if (received > 0) process_data(net_buf, received);

	for (int i = 0; i < SCREEN_WIDTH; ++i)
		for (int j = 0; j < SCREEN_HEIGHT; ++j) {
			int tile_x = i + g_left_x;
			int tile_y = j + g_top_y;
			if ((tile_x < 0) || (tile_y < 0)) continue;
			auto tileType = GetMapTile(tile_x, tile_y);
			switch (tileType){
			case Wall: {
				black_tile.a_move(TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
				black_tile.a_draw();
				break;
			}
			default: {
				white_tile.a_move(TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
				white_tile.a_draw();
				break;
			}
			}
			//if ((((tile_x / 3) + (tile_y / 3)) % 2) == 1) {
			//	white_tile.a_move(TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
			//	white_tile.a_draw();
			//} else {
			//	black_tile.a_move(TILE_WIDTH * i + 7, TILE_WIDTH * j + 7);
			//	black_tile.a_draw();
			//}
		}
	avatar.draw();
	for (auto& pl : players) pl.draw();
	sf::Text text;
	text.setFont(g_font);
	text.setPosition(0, 0);
	text.setFillColor(sf::Color::Black);
	text.setOutlineColor(sf::Color::White);
	text.setOutlineThickness(1);
	char buf[100];
	sprintf_s(buf, "(%d, %d) hp %d  lvl %d  exp %d", avatar.m_x, avatar.m_y, avatar.GetHp(), avatar.GetLevel(), avatar.GetExp());
	text.setString(buf);
	g_window->draw(text);
	auto pos = text.getPosition();
	pos.y += 30;
	for (auto message_line : message_box) {
		pos.y += 30;
		text.setPosition(pos.x, pos.y);
		text.setString(message_line.message.c_str());
		g_window->draw(text);
	}

	g_window->draw(*textField);
}

void send_move_packet(DIRECTION dr) {
	c2s_move packet;
	packet.size = sizeof(packet);
	packet.type = C2S_MOVE;
	packet.direction = dr;
	packet.move_time = 0;
	size_t sent = 0;
	socket.send(&packet, sizeof(packet), sent);
}

void send_login_packet(string& name) {
	c2s_login packet;
	packet.size = sizeof(packet);
	packet.type = C2S_LOGIN;
	strcpy_s(packet.player_id, name.c_str());
	size_t sent = 0;
	socket.send(&packet, sizeof(packet), sent);
}

void send_attack_packet() {
	cs_packet_attack packet;
	packet.size = sizeof(packet);
	packet.type = CS_ATTACK;
	size_t sent = 0;
	socket.send(&packet, sizeof(packet), sent);
}

void send_chat_packet(const string& mess) {
	cs_packet_chat packet;
	packet.size = sizeof(packet);
	packet.type = CS_CHAT;
	strcpy_s(packet.message, mess.c_str());
	size_t sent = 0;
	socket.send(&packet, sizeof(packet), sent);
	stringstream ss;
	ss << avatar.get_name();
	ss << ": ";
	ss << mess;
	add_message(ss.str());
}

bool Player::attack() {
	auto now = get_this_time();
	if (now - last_attack_time >= 1000) {
		send_attack_packet();
		last_attack_time = now;
		return true;
	}
	return false;
}

bool isLogin = false;

int main() {
	wcout.imbue(locale("korean"));
	sf::Socket::Status status = socket.connect("127.0.0.1", SERVER_PORT);


	socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		wcout << L"서버와 연결할 수 없습니다.\n";
		while (true);
	}

	client_initialize();
	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				window.close();
			else if (event.type == sf::Event::KeyPressed) {
				DIRECTION p_type = D_NO;
				switch (event.key.code) {
				case sf::Keyboard::Left:
					p_type = D_W;
					break;
				case sf::Keyboard::Right:
					p_type = D_E;
					break;
				case sf::Keyboard::Up:
					p_type = D_N;
					break;
				case sf::Keyboard::Down:
					p_type = D_S;
					break;
				case sf::Keyboard::A: {
					avatar.attack();
					break;
				}
				case sf::Keyboard::Enter: {
					if(!isLogin){
						auto name = textField->getText();
						name = name.substr(0, min(static_cast<int>(name.size()), MAX_ID_LEN-1));
						send_login_packet(name);
						avatar.set_name(name.c_str());
						isLogin = true;
					}else{
						send_chat_packet(textField->getText());
					}
					
					textField->clear();
					break;
				}
				case sf::Keyboard::Escape:
					window.close();
					break;
				}
				if (D_NO != p_type) send_move_packet(p_type);
			} else if (event.type == sf::Event::MouseButtonReleased) {
				auto pos = sf::Mouse::getPosition(window);
				textField->setFocus(false);
				if (textField->contains(sf::Vector2f(pos))) {
					textField->setFocus(true);
				}
			} else {
				textField->handleInput(event);
			}
		}

		window.clear();
		client_main();
		window.display();
	}
	client_finish();

	return 0;
}