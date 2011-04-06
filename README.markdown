README
======

This is an implementation of the game to The Gatherings Hardcore Programming Compo.
  http://www.gathering.org/tg11/en/creative/competitions/hardcore-programming/

"In the game, every player controls a thunderstorm, and the goal is to become
the biggest thunderstorm by absorbing vapor from clouds and other players."

Description of the original game can be found here:
  http://www.gathering.org/tg11/en/creative/competitions/hardcore-programming/cloudwars/

The network protocol is described here:
  http://www.gathering.org/tg11/en/creative/competitions/hardcore-programming/cloudwars/protocol/

This game has a couple of new features the original game don't have:
* Platform independent code!!
* Music and sound effect
* Retro mode (no gfx)
* Debug mode
* Fullscreen
* Can define width and height
* Can define tcp port
* Commandline arguments(?)
* Two player - p1: arrow keys p2: wsad (not the same wind function)
* ?

The compo assignment is to create an Artificial Intelligence (AI) that plays the
game.

Client framework code:
* C# Framework:
    https://github.com/downloads/bondehagen/CloudWars/ClientAI_CSharp.zip
* Java Framework:
    https://github.com/downloads/bondehagen/CloudWars/ClientAI_Java.zip

The support thread for the original game and AI-bots is:
  http://forums.gathering.org/showthread.php?9420-Hardcore-Programming-Compo-Support-Thread

INSTALL
-------
The game has the following dependencies:
* SDL
* SDL_gfx
* SDL_image
* SDL_mixer
* SDL_net
* SDL_ttf

You can also install the sprites from the original game, if not you have to run
the game in retro (-r) mode!

Tested on Ubuntu 10.10:

    ./install_dependencies
    ./install_sprites
    make

RUN
---
Usage: ./cloudwarsx -m [deathmatch, timelimit] -1 [ai, human] -2 [ai, human]
  -m gamemode   - deathmatch / timelimit
  -s seconds    - time limit in seconds
  -1 ai / human - player 1
  -2 ai / human - player 2
  -3            - player 3 - only ai
  -4            - player 4 - only ai
  -l filename   - level filename
  -r            - enable retromode (no gfx)
  -x width      - set width
  -y height     - set height
  -f            - enable fullscreen
  -p port       - tcp port for server
  -n            - no sound
  -d            - debug mode
  -v            - show the version
