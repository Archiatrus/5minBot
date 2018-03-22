#include "sc2api/sc2_api.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"
#include "rapidjson/document.h"
#include "JSONTools.h"
#include "Util.h"

#include <iostream>
#include <string>
#include <random>
#include <cmath>
#include <chrono>
#include <thread>

#include "CCBot.h"

#ifdef LADDEREXE
bool useDebug = false;
bool useAutoObserver = true;
#elif VSHUMAN
bool useDebug = false;
bool useAutoObserver = false;
#else
bool useDebug = true;
bool useAutoObserver = false;
#endif


#ifdef LADDEREXE
#include "LadderInterface.h"

int main(int argc, char* argv[])
{
	RunBot(argc, argv, new CCBot(), sc2::Race::Terran);

	return 0;
}
#elif VSHUMAN

class Human : public sc2::Agent {
public:
	void OnGameStart() final {
	}
};

struct ConnectionOptions
{
	sc2::Race HumanRace;
	std::string map;
};

static void ParseArguments(int argc, char *argv[], ConnectionOptions &connect_options)
{
	sc2::ArgParser arg_parser(argv[0]);
	arg_parser.AddOptions({
		{ "-h", "--HumanRace", "Race of the human player"},
		{ "-m", "--Map", "MapName.SC2Map"}
	});
	arg_parser.Parse(argc, argv);

	std::string race;
	arg_parser.Get("HumanRace", race);
	if (race == "Zerg")
	{
		connect_options.HumanRace = sc2::Zerg;
	}
	else if (race == "Protoss")
	{
		connect_options.HumanRace = sc2::Protoss;
	}
	else if (race == "Terran")
	{
		connect_options.HumanRace = sc2::Terran;
	}
	else
	{
		std::cout << std::endl << "Could not read race option. Please provide it via --HumanRace \"Zerg/Protoss/Terran\". Using default: Terran" << std::endl << std::endl;
		connect_options.HumanRace = sc2::Terran;
	}

	if (!arg_parser.Get("Map", connect_options.map))
	{
		std::cout << "Could not read map option. Please provide it via --Map \"InterloperLE.SC2Map\". Using default: InterloperLE.SC2Map" << std::endl << std::endl;
		connect_options.map = "InterloperLE.SC2Map";
	}
}


int main(int argc, char* argv[])
{
	ConnectionOptions Options;
	ParseArguments(argc, argv, Options);

	int stepSize = 1;
	// Add the custom bot, it will control the players.
	CCBot bot;
	Human human;
	sc2::Coordinator coordinator;
	if (!coordinator.LoadSettings(argc, argv))
	{
		std::cout << "Unable to find or parse settings." << std::endl;
		return 1;
	}
	// WARNING: Bot logic has not been thorougly tested on step sizes > 1
	//		  Setting this = N means the bot's onFrame gets called once every N frames
	//		  The bot may crash or do unexpected things if its logic is not called every frame
	coordinator.SetStepSize(stepSize);
	coordinator.SetRealtime(false);
	coordinator.SetMultithreaded(true);
	coordinator.SetParticipants({
		CreateParticipant(Options.HumanRace, &human),
		CreateParticipant(sc2::Race::Terran, &bot)
	});
	// Start the game.
	coordinator.LaunchStarcraft();
	coordinator.StartGame(Options.map);

	// Step forward the game simulation.
	while (coordinator.Update())
	{
	}
	coordinator.LeaveGame();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	return 0;
}
#else //DEBUG
int main(int argc, char* argv[])
{
	const int stepSize = 1;
	while (true)
	{
		// Add the custom bot, it will control the players.
		CCBot bot;
		//CCBot bot2;
		sc2::Coordinator coordinator;
		if (!coordinator.LoadSettings(argc, argv))
		{
			std::cout << "Unable to find or parse settings." << std::endl;
			return 1;
		}
		// WARNING: Bot logic has not been thorougly tested on step sizes > 1
		//		  Setting this = N means the bot's onFrame gets called once every N frames
		//		  The bot may crash or do unexpected things if its logic is not called every frame
		coordinator.SetStepSize(stepSize);
		coordinator.SetRealtime(false);
		coordinator.SetMultithreaded(true);
		coordinator.SetParticipants({
			CreateParticipant(sc2::Race::Terran, &bot),
			//sc2::PlayerSetup(sc2::PlayerType::Observer,Util::GetRaceFromString(enemyRaceString)),
			CreateComputer(sc2::Race::Random, sc2::Difficulty::CheatInsane)
			//CreateParticipant(sc2::Race::Terran, &bot2),
		});
		// Start the game.
		coordinator.LaunchStarcraft();
		//coordinator.StartGame("C:/Program Files (x86)/StarCraft II/Maps/AcolyteLE.SC2Map");
		coordinator.StartGame("BelShirVestigeLE.SC2Map");
		//coordinator.StartGame("Proxima Station LE");
		//coordinator.StartGame("SequencerLE.SC2Map");

		// Step forward the game simulation.
		while (coordinator.Update())
		{
		}
		if (bot.Control()->SaveReplay("C:/Users/D/Documents/StarCraft II/Accounts/115842237/2-S2-1-1338490/Replays/Multiplayer/asdf.Sc2Replay"))
		{
			std::cout << "REPLAY SUCESS" << "replay/asdf.Sc2Replay" << std::endl;
		}
		else
		{
			std::cout << "REPLAY FAIL" << "replay/asdf.Sc2Replay" << std::endl;
		}
		coordinator.LeaveGame();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
	return 0;
}
#endif