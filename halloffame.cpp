#include <string>
#include <iostream>
#include <fstream>
#include <experimental/filesystem>
#include <algorithm>
#include <sstream>

#include <cstdint>
#include <cstring>

#include <sys/time.h>
#include <unistd.h>

namespace fs = std::experimental::filesystem;

#ifdef LOCAL
#define PLAY_SOUND
#define FONT_FILE "font.txt"
#else
#define FONT_FILE "/font.txt"
#endif

struct tty_header
{
	uint32_t sec;
	uint32_t usec;
	uint32_t len;
};

struct tty_block
{
	uint64_t at_ms() const
	{
		return (header.sec * 1000) + (header.usec / 1000);
	}

	uint64_t get_us() const
	{
		uint64_t us = header.sec;
		us *= 1000000;
		us += header.usec;
		return us;
	}

	tty_header header;
	std::vector<char> buf;
};

class death_replay
{
public:
	death_replay(const fs::path &path)
		: m_in{path}
		, m_first{0}
		, m_last{0}
	{
	}

	bool play()
	{
		if(!find_death_frames())
		{
			return false;
		}

		tty_block block;
		uint64_t last_time = 0;

		for(int i = 0; i <= m_last; ++i)
		{
			if(!read_block(block))
			{
				return false;
			}

			if(i >= m_first)
			{
				if(0 != last_time)
				{
					usleep(block.get_us() - last_time);
				}
				last_time = block.get_us();
			}

			fwrite(block.buf.data(), 1, block.buf.size(), stdout);
		}

		sleep(2);
		return true;
	}
private:
	bool read_block(tty_block &block)
	{
		auto n = m_in.read(
			reinterpret_cast<char *>(&block.header),
			sizeof(block.header)).gcount();

		if(n != sizeof(block.header))
		{
			return false;
		}

		auto &buf = block.buf;
		buf.resize(block.header.len);

		return (buf.size() == m_in.read(buf.data(), buf.size()).gcount());
	}

	bool find_death_frames()
	{
		tty_block block;
		std::vector<uint64_t> frame_timing;

		for(m_last = 0;; ++m_last)
		{
			if(!read_block(block))
			{
				return false;
			}

			frame_timing.push_back(block.at_ms());

			auto &buf = block.buf;

			static const char *deaths[] =
			{
				"\x1b[HYou die...",
				"\x1b[HYou drown.",
				"\x1b[HDo you want your possessions identified?",
			};

			for(int death = 0; death < (sizeof(deaths) / sizeof(deaths[0])); ++death)
			{
				if(nullptr != strstr(buf.data(), deaths[death]))
				{
					uint64_t find_time = frame_timing[m_last] - 30000;

					for(m_first = (m_last - 1); m_first > 0; --m_first)
					{
						if(frame_timing[m_first] <= find_time)
						{
							++m_first;
							break;
						}
					}

					m_in.clear();
					m_in.seekg(0);
					return true;
				}
			}
		}

		return false;
	}
private:
	std::ifstream m_in;
	int m_first;
	int m_last;
};

#define VT_BLACK 30
#define VT_RED 31
#define VT_GREEN 32
#define VT_YELLOW 33
#define VT_BLUE 34
#define VT_MAGENTA 35
#define VT_CYAN 36
#define VT_WHITE 37

struct console
{
	using glyph = std::vector<std::string>;
	using font_map = std::vector<glyph>;
public:
	static font_map load_big_font(const std::string &filename)
	{
		std::ifstream in(filename);
		std::string line;

		font_map m;
		glyph nothing{6};
		for(int i = 0; i < '!'; ++i)
		{
			m.push_back(nothing);
		}

		glyph next;

		while(std::getline(in, line))
		{
			if(nullptr == strstr(line.c_str(), "@@"))
			{
				next.emplace_back(line.substr(0, line.find_first_of('@')));
				std::cout << next.back() << std::endl;
			}
			else
			{
				m.emplace_back(std::move(next));
				next.clear();
			}
		}

		return m;
	}

	console()
		: m_map{load_big_font(FONT_FILE)}
	{
	}

	void hide_cursor() const
	{
		puts("\x1b[?25l");
	}

	void show_cursor() const
	{
		puts("\x1b[?25h");
	}

	void go_to(int x, int y) const
	{
		printf("\x1b[%d;%dH", y, x);
	}

	void clear() const
	{
		puts("\x1b[2J");
	}

	void set_color(int color) const
	{
		printf("\x1b[1;%dm", color);
	}

	void print_big(int at_x, int at_y, int color, const std::string &text) const
	{
		set_color(color);
		for(int y = 0; y < 6; ++y)
		{
			go_to(at_x, at_y + y);
			for(int x = 0; x < text.size(); ++x)
			{
				size_t idx = static_cast<size_t>(text[x]);
				if(idx >= m_map.size())
				{
					continue;
				}

				auto p = (0x20 == idx)
					? "     "
					: m_map[idx][y].c_str();
				printf("%s ", p);
			}
		}
	}

private:
	font_map m_map;
};

struct game
{
public:
	game(std::string && name, int points, int maxlvl, int maxhp, int turns,
		std::string && role, std::string && race, std::string && gender, std::string && align,
		std::string && death)
		: m_name{std::move(name)}
		, m_points{points}
		, m_maxlvl{maxlvl}
		, m_maxhp{maxhp}
		, m_turns{turns}
		, m_role{std::move(role)}
		, m_race{std::move(race)}
		, m_gender{std::move(gender)}
		, m_align{std::move(align)}
		, m_death{std::move(death)}
	{
	}

	const char *name() const { return m_name.c_str(); }
	const char *role() const { return m_role.c_str(); }
	const char *race() const { return m_race.c_str(); }
	const char *gender() const { return m_gender.c_str(); }
	const char *align() const { return m_align.c_str(); }
	const char *death() const { return m_death.c_str(); }


	std::string m_name;
	uint32_t m_endtime;
	int m_points;
	int m_maxlvl;
	int m_maxhp;
	int m_turns;
	std::string m_role;
	std::string m_race;
	std::string m_gender;
	std::string m_align;
	std::string m_death;
};

struct highscore
{
	highscore()
		: index{0}
		, value{0}
	{
	}

	void set(size_t i, int v)
	{
		index = i;
		value = v;
	}

	size_t index;
	int value;
};

class hall_of_fame
{
private:
	static std::string get_value(const std::string &name, const std::string &line)
	{
		std::string needle{"\t" + name + "="};
		auto pos = line.find(needle);

		if(std::string::npos == pos)
		{
			throw std::runtime_error("Missing value in xlogfile");
		}

		pos += needle.length();

		return line.substr(pos, line.find_first_of('\t', pos  + 1) - pos);
	}

	static int get_value_int(const std::string &name, const std::string &line)
	{
		return std::stoi(get_value(name, line));
	}

public:
	hall_of_fame(const fs::path &root)
		: m_root{std::move(root)}
		, m_in{root / "nh361/var/xlogfile"}
		, m_first_time{true}
	{
		update_dead(false);
	}

	void run()
	{
		static const int hs_y = 17;

		m_console.go_to(0, 0);
		m_console.hide_cursor();

		if(!m_first_time && !update_dead(true))
		{
			//
			// Nothing changed.
			//
			return;
		}

		m_first_time = false;

		m_console.clear();
		m_console.print_big(28,	2, VT_WHITE, "NH2018");
		m_console.print_big(3, 10, VT_YELLOW, "HALL OF FAME");

		print_highscore("MOST TURNS SURVIVED", hs_y + 0, m_high_turns);
		print_highscore("DEEPEST DUNGEON LEVEL", hs_y + 2, m_high_level);
		print_highscore("MOST POINTS SCORED", hs_y + 4, m_high_points);

		m_console.go_to(40, hs_y + 7);
		m_console.set_color(VT_RED);

		printf("FALLEN HEROES (%d TOTAL)", static_cast<uint32_t>(m_games.size()));
		int count = 0;

		for(int i = static_cast<int>(m_games.size()) - 1; i >= 0; --i)
		{
			m_console.set_color(VT_RED);
			print_game(hs_y + 8 + count, i);
			if(8 == ++count)
			{
				break;
			}
		}

		m_console.hide_cursor();
	}

private:
	void play_death(std::string name)
	{
		fs::path most_recent;
		fs::path user_dir(m_root / "dgldir/userdata" / name / "ttyrec");

		for(auto &p : fs::directory_iterator(user_dir))
		{
			if(most_recent.empty())
			{
				most_recent = p;
			}
			else if(fs::last_write_time(most_recent) < fs::last_write_time(p))
			{
				most_recent = p;
			}
		}

		if(!most_recent.empty())
		{
			show_player_dead(name);
			death_replay dr{most_recent};

			m_console.set_color(VT_WHITE);
			m_console.clear();
			m_console.show_cursor();
			dr.play();
			m_console.hide_cursor();
		}
	}

	bool game_is_record(int index) const
	{
		if(index >= m_games.size())
		{
			return false;
		}

		if(index == m_high_points.index
		&& 0 != m_high_points.value)
		{
			return true;
		}
		if(index == m_high_turns.index
		&& 0 != m_high_turns.value)
		{
			return true;
		}
		if(index == m_high_level.index
		&& 0 != m_high_level.value)
		{
			return true;
		}
		return false;
	}

	void print_game(int y, int index) const
	{
		if(index < 0 || index >= m_games.size())
		{
			m_console.set_color(VT_WHITE);
			m_console.go_to(8, y + 1);
			printf("---");
			return;
		}

		auto &g = m_games[index];

		m_console.go_to(8, y);
		m_console.set_color(game_is_record(index) ? VT_GREEN : VT_RED);
		printf("%s", g.name());
		m_console.set_color(VT_CYAN);
		printf(" %s", g.death());
		m_console.set_color(VT_WHITE);
		printf(" - (%s %s %s %s) P: %d, T: %d, L: %d, HP: %d",
			g.role(), g.race(), g.gender(), g.align(),
			g.m_points, g.m_turns, g.m_maxlvl, g.m_maxhp);
	}

	void print_highscore(const char *title, int y, highscore &hs)
	{
		m_console.set_color(VT_YELLOW);
		m_console.go_to(35, y);
		printf("%s", title);

		if(0 == hs.value || hs.index >= m_games.size())
		{
			print_game(y + 1, -1);
		}
		else
		{
			m_console.set_color(VT_WHITE);
			printf(" - ");
			m_console.set_color(VT_GREEN);
			printf("%d by %s", hs.value, m_games[hs.index].name());
			print_game(y + 1, hs.index);
		}
	}

	bool update_dead(bool play)
	{
		std::string line;
		auto old_size = m_games.size();

		while(std::getline(m_in, line))
		{
			m_games.emplace_back(game{
				get_value("name", line),
				get_value_int("points", line),
				get_value_int("maxlvl", line),
				get_value_int("maxhp", line),
				get_value_int("turns", line),
				get_value("role", line),
				get_value("race", line),
				get_value("gender", line),
				get_value("align", line),
				get_value("death", line)
			});

			const auto &g = m_games.back();

			if(play)
			{
				if(g.m_death != "quit")
				{
					play_death(g.m_name);
				}
			}

			bool hs = false;
			if(m_high_points.value < g.m_points)
			{
				m_high_points.set(m_games.size() - 1, g.m_points);
				hs = true;
			}
			if(m_high_turns.value < g.m_turns)
			{
				m_high_turns.set(m_games.size() - 1, g.m_turns);
				hs = true;
			}
			if(m_high_level.value < g.m_maxlvl)
			{
				m_high_level.set(m_games.size() - 1, g.m_maxlvl);
				hs = true;
			}

			if(play && hs)
			{
				show_new_highscore(g.m_name);
			}
		}
		m_in.clear();

		return old_size != m_games.size();
	}

	static void play_sound(const std::string &name)
	{
#ifdef PLAY_SOUND
		std::stringstream ss;
		ss << "aplay " << name << " 2>/dev/null &";
		system(ss.str().c_str());
#endif
	}

	void typeout(int x, int y, int color, const std::string &msg)
	{
		for(int i = 0; i < msg.length(); ++i)
		{
			m_console.print_big(4, 6, color, msg.substr(0, i + 1));
			play_sound(rand() & 1 ? "type0.wav" : "type1.wav");
			usleep(150 * 1000);
		}
	}

	void show_new_highscore(const std::string &name)
	{
		m_console.clear();
		typeout(4, 6, VT_MAGENTA, "NEW RECORD!");

		sleep(1);
		play_sound("record.wav");
		m_console.print_big(14, 16, VT_GREEN, name);
		sleep(3);
	}

	void show_player_dead(const std::string &name)
	{
		m_console.clear();
		typeout(4, 6, VT_GREEN, name);

		sleep(1);
		play_sound("dead.wav");
		m_console.print_big(14, 16, VT_RED, "DED.");
		sleep(3);
	}

private:
	fs::path m_root;
	std::ifstream m_in;
	bool m_first_time;
	console m_console;
	std::vector<game> m_games;
	highscore m_high_points;
	highscore m_high_turns;
	highscore m_high_level;
};

static bool wait_key(int secs)
{
    int rc;
    struct timeval tv;
    fd_set rdfs;

    tv.tv_sec = secs;
    tv.tv_usec = 0;

    FD_ZERO(&rdfs);
    FD_SET(STDIN_FILENO, &rdfs);

    rc = select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
    return (1 == rc) && FD_ISSET(STDIN_FILENO, &rdfs);
 }

int main(int argc, const char *argv[])
{
	srand(time(0));

	if(argc < 2)
	{
		std::cout << "Usage: " << argv[0] << " <dglroot>" << std::endl;
		return -1;
	}

	hall_of_fame hof{fs::path{argv[1]}};
	setbuf(stdout, NULL);

	for(;;)
	{
		hof.run();
		if(wait_key(1))
		{
			break;
		}
	}
}

