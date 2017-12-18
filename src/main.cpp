#include "sc2api/sc2_api.h"
#include "sc2utils/sc2_manage_process.h"
#include "rapidjson/document.h"
#include "JSONTools.h"
#include "Util.h"

#include <iostream>
#include <string>
#include <random>
#include <cmath>

#include "CCBot.h"


int main(int argc, char* argv[]) 
{
    
    
    rapidjson::Document doc;
    std::string config = JSONTools::ReadFile("BotConfig.txt");
    if (config.length() == 0)
    {
        std::cerr << "Config file could not be found, and is required for starting the bot\n";
        std::cerr << "Please read the instructions and try again\n";
        exit(-1);
    }

    bool parsingFailed = doc.Parse(config.c_str()).HasParseError();
    if (parsingFailed)
    {
        std::cerr << "Config file could not be parsed, and is required for starting the bot\n";
        std::cerr << "Please read the instructions and try again\n";
        exit(-1);
    }

    std::string botRaceString;
    std::string enemyRaceString;
    std::string mapString;
    int stepSize = 1;
    sc2::Difficulty enemyDifficulty = sc2::Difficulty::VeryHard;

    if (doc.HasMember("Game Info") && doc["Game Info"].IsObject())
    {
        const rapidjson::Value & info = doc["Game Info"];
        JSONTools::ReadString("BotRace", info, botRaceString);
        JSONTools::ReadString("EnemyRace", info, enemyRaceString);
        JSONTools::ReadString("MapFile", info, mapString);
        JSONTools::ReadInt("StepSize", info, stepSize);
        JSONTools::ReadInt("EnemyDifficulty", info, enemyDifficulty);
    }
    else
    {
        std::cerr << "Config file has no 'Game Info' object, required for starting the bot\n";
        std::cerr << "Please read the instructions and try again\n";
        exit(-1);
    }


	while (true)
	{
		// Add the custom bot, it will control the players.
		CCBot bot;
		
		sc2::Coordinator coordinator;
		if (!coordinator.LoadSettings(argc, argv))
		{
			std::cout << "Unable to find or parse settings." << std::endl;
			return 1;
		}
		// WARNING: Bot logic has not been thorougly tested on step sizes > 1
		//          Setting this = N means the bot's onFrame gets called once every N frames
		//          The bot may crash or do unexpected things if its logic is not called every frame
		coordinator.SetStepSize(stepSize);
		coordinator.SetRealtime(false);
		coordinator.SetMultithreaded(true);

		coordinator.SetParticipants({
			CreateParticipant(Util::GetRaceFromString(botRaceString), &bot),
			//sc2::PlayerSetup(sc2::PlayerType::Observer,Util::GetRaceFromString(enemyRaceString)),
			CreateComputer(sc2::Race::Zerg, sc2::Difficulty::CheatInsane)
		});
		// Start the game.
		coordinator.LaunchStarcraft();
		coordinator.StartGame("Odyssey LE");


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
		Sleep(1000);
	}
    return 0;
}
