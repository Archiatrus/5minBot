# 5minBot
The Terran bot plays macro bio. A notable feature is the drop play.

At this stage the bot has a fair chance to beat the inbuilt insane AI (sc2::Difficulty::CheatInsane). It won season 3 of the Starcraft 2 AI Ladder.

The bot is always work in progress. I can not guarantee that it is always working as intended. The list of known bugs is still growing with every game...

---

If you use my bot as sparing partner for your bot (this is my intention with uploading it to github) feel free to drop me a message in the discord channel https://discord.gg/qTZ65sh Archiatrus#3053

---
The starting point of this bot was the [CommandCenter bot](https://github.com/davechurchill/commandcenter) by David Churchill cloned mid November 2017.

## Building
For more explicit instructions that hopefully still work, see the [Commandcenter Readme.](https://github.com/davechurchill/commandcenter/blob/master/README.md) General steps are listed below.
### Windows
1. Install SC2 via the [Blizzard app.](https://www.blizzard.com/en-us/apps/battle.net/desktop) Update it.
2. Install [VS2017 Community.](https://www.visualstudio.com/downloads/)
3. Build [s2client-api](https://github.com/Blizzard/s2client-api) and know where the includes and libs are.
4. Use VS2017 and open `vs/5minBot.sln`. To get it building:
  - You might have to retarget the Windows SDK by right clicking on the project and clicking `Retarget...`
  - You will have to make sure the include and lib paths are right by right clicking on the project, choosing `Properties` and changing the VC++ include and libs to point to the right path. An easy way to get it right is to have `5minBot` in the same level directory as `s2client-api`, since it uses relative paths.
5. Release vs. Debug
  - For use with [Sc2LadderServer](https://github.com/Cryptyc/Sc2LadderServer), use Release. The binary will be in `bin/5minBot.exe`.
  - For debugging locally, use Debug and SC2 should automatically launch when you click the run button. You will probably want to skip the intro, exit fullscreen, and enable HP bars by pressing `'` if you haven't already.
### Linux
1. Install `cmake` and have some `g++` (tested with g++ 7.3.0) and `make`.
2. Run `cmake .` and then `make` in the root directory.
3. The binary for use with [Sc2LadderServer](https://github.com/Cryptyc/Sc2LadderServer) will be in `bin/5minBot.exe`. If you want to debug with gdb you can add a target in the CMakeLists.txt (and please send a PR).
