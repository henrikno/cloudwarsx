/*
	cloudwarsx
	Copyright (C) 2011 Ole Kristian Lien

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_rotozoom.h> // SDL_gfx
#include <SDL/SDL_net.h>
#include <SDL/SDL_mixer.h>

#include <iostream>
#include <string>
#include <sstream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <fstream>

////////////////////////////////////////////////////////////////////////////////
// Config
////////////////////////////////////////////////////////////////////////////////
int width = 1280;
int height = 720;
const int bpp = 32;
std::string title = "CloudWarsX";
const float version = 0.7;

bool fullscreen = false;
bool retro = false;
bool debug = false;
bool nosound = false;
bool level = false;

int Winner = 0;
int timeLimit;
int limit;
int defaultTimeLimit = 5;

float absorb = 1.0;
const int MAX_CLOUDS = 50;
int startClouds = 20;
int thunderCloud = 0;
int rainCloud = 2;
int vaporStart = 1000;

int iteration = 0;
bool done = false;

int channel;

// Net
int port = 1986;
const unsigned short BUFFER_SIZE = 1024;
const unsigned short MAX_SOCKETS = 4; // 1 server + 3 klienter
const unsigned short MAX_CLIENTS = MAX_SOCKETS - 1;

int clientCount = 0;
int playerCount = 0;
int aiPlayers = 0;

////////////////////////////////////////////////////////////////////////////////
// Objects
////////////////////////////////////////////////////////////////////////////////
SDL_Surface *background = NULL;
SDL_Surface *blue = NULL;
SDL_Surface *gray = NULL;
SDL_Surface *orange = NULL;
SDL_Surface *purple = NULL;
SDL_Surface *red = NULL;
SDL_Surface *screen = NULL;
SDL_Event event;

Mix_Music *waitingMusic = NULL;
Mix_Music *bounceSound = NULL;
Mix_Music *absorbSound = NULL;
Mix_Chunk *music = NULL;
Mix_Chunk *winnerSound = NULL;

TTF_Font *font = NULL;
TTF_Font *fontWinner = NULL;
TTF_Font *fontWaiting = NULL;
SDL_Color textColor = {255, 255, 255};
SDL_Surface *winner = NULL;

SDL_Thread *thread = NULL;

enum gamemodes { 
	deathmatch,
	timelimit
};

////////////////////////////////////////////////////////////////////////////////
// Split string function
////////////////////////////////////////////////////////////////////////////////
void split(const std::string& s, char c, std::vector<std::string>& v) {
	std::string::size_type i = 0;
	std::string::size_type j = s.find(c);

	while(j != std::string::npos) {
		v.push_back(s.substr(i, j-i));
		i = ++j;
		j = s.find(c, j);

		if(j == std::string::npos)
			v.push_back(s.substr(i, s.length()));
	}
}

////////////////////////////////////////////////////////////////////////////////
// Draw functions
////////////////////////////////////////////////////////////////////////////////

// only 32-bit pixels
void setPixel(SDL_Surface *surface, int x, int y, Uint32 pixel) {
	if((x > width) || (x < 0) || (y < 0) || (y > height)) {
		//std::cout << "hit: " << x << " " << y << std::endl;
	} else {
		Uint8 *target_pixel = (Uint8 *)surface->pixels + y * surface->pitch + x * 4;
		*(Uint32 *)target_pixel = pixel;
	}
}

// Draw line
void drawLine(SDL_Surface *surface, int x1, int y1, int x2, int y2, Uint32 color) {
	double x = x2 - x1;
	double y = y2 - y1;
	double length = sqrt(x*x + y*y);

	double addX = x / length;
	double addY = y / length;

	x = x1;
	y = y1;

	for(double i = 0; i < length; i += 1) {
		setPixel(surface, (int)x, (int)y, color);
		x += addX;
		y += addY;
	}
}

// Midpoint Circle Algorithm 
// http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
void drawCircle(SDL_Surface *surface, int cx, int cy, int radius, Uint32 pixel) {
	int error = -radius;
	int x = radius;
	int y = 0;

	while(x >= y) {
		setPixel(surface, cx + x, cy + y, pixel);
		setPixel(surface, cx + y, cy + x, pixel);

		if(x != 0) {
			setPixel(surface, cx - x, cy + y, pixel);
			setPixel(surface, cx + y, cy - x, pixel);
		}

		if(y != 0) {
			setPixel(surface, cx + x, cy - y, pixel);
			setPixel(surface, cx - y, cy + x, pixel);
		}

		if(x != 0 && y != 0) {
			setPixel(surface, cx - x, cy - y, pixel);
			setPixel(surface, cx - y, cy - x, pixel);
		}

		error += y;
		++y;
		error += y;

		if(error >= 0) {
			--x;
			error -= x;
			error -= x;
		}
	}
}

// Blit surface
void drawSurface(int x, int y, SDL_Surface* source, SDL_Surface* destination) {
	SDL_Rect offset;

	offset.x = x;
	offset.y = y;

	SDL_BlitSurface(source, NULL, destination, &offset);
}
void drawText(std::string text, int x, int y, TTF_Font *font, SDL_Color color) {
	SDL_Surface *surf = TTF_RenderText_Solid(font, text.c_str(), color);
	drawSurface(x, y, surf, screen);
	SDL_FreeSurface(surf);
}

void drawNormalText(std::string text, int x, int y) {
	drawText(text, x, y, font, textColor);
}

void drawWaitingText(std::string text, int x, int y) {
	drawText(text, x, y, fontWaiting, textColor);
}

void drawWinnerText(std::string text, int x, int y) {
	drawText(text, x, y, fontWinner, textColor);
}

////////////////////////////////////////////////////////////////////////////////
// Load Image
////////////////////////////////////////////////////////////////////////////////

SDL_Surface *loadImage(std::string filename) {
	SDL_Surface* loadedImage = NULL;
	SDL_Surface* image = NULL;

	loadedImage = IMG_Load(filename.c_str());

	if(!loadedImage) {
		std::cerr << "Error: Unable to load image " << filename << ": " << SDL_GetError() << std::endl;
		exit(1);
	} else {
		image = SDL_DisplayFormatAlpha(loadedImage);
		SDL_FreeSurface(loadedImage);
	}

	return image;
}

////////////////////////////////////////////////////////////////////////////////
// Load font
////////////////////////////////////////////////////////////////////////////////

TTF_Font* loadFont(std::string filename, int size) {
	TTF_Font* font;
	font = TTF_OpenFont(filename.c_str(), size);

	if(!font) {
		std::cerr << "Error: Unable to load font " << filename << ": " << TTF_GetError() << std::endl;
		exit(1);
	}

	return font;
}

////////////////////////////////////////////////////////////////////////////////
// Cloud Class
////////////////////////////////////////////////////////////////////////////////

enum types {
	human,
	ai,
	raincloud,
};

class Cloud {
	public:
		Cloud(float pX, float pY, float vX, float vY, float V);
		~Cloud();
		void draw();
		void show();
		void drawName();
		void drawVapor();
		void drawVelocity();
		void drawPosition();
		void drawWind();
		std::string getType();

	//private:
		int player;
		float px, py; // The point (px,py) is the position of the cloud
		float vx, vy; // The vector [vx,vy] is the velocity of the cloud
		float vapor; // The amount of vapor in the cloud
		float radius() { return sqrt(vapor);} // radius is square root of vapor

		int windX1, windY1, windX2, windY2;
		Uint32 windColor;

		bool alive;
		std::string name;
		std::string color;
		SDL_Surface *vaporAmount;

		types type;
};

Cloud::Cloud(float pX, float pY, float vX, float vY, float V) {
	px = pX;
	py = pY;
	vx = vX;
	vy = vY;
	vapor = V;
	vaporAmount = NULL;
}

Cloud::~Cloud() {
	SDL_FreeSurface(vaporAmount);
}

std::string Cloud::getType() {
	if(type == ai)
		return "AI";
	else if(type == human)
		return "Human";
	else
		return "Unkown";
}

void Cloud::draw() {
	Uint32 color;

	if(player == 1)
		color = 0x000000FF; // blue
	else if(player == 2)
		color = 0x00FF0000; // red
	if(player == 3)
		color = 0x00FF7F00; // orange
	else if(player == 4)
		color = 0x00800080; // purple

	if(type == raincloud)
		color = 0x007F7F7F; // gray

	drawCircle(screen, px, py, radius(), color);
}

void Cloud::drawWind() {
	if((windX1 != 0) || (windY1 != 0)) {
		drawLine(screen, windX1, windY1, windX2, windY2, windColor);

		std::stringstream ss;
		ss << "WIND(" << windX2-windX1 << ", " << windY2-windY1 << ")";
		std::string s = ss.str();

		drawNormalText(s, windX2, windY2);
	}
}

void Cloud::drawName() {
	drawNormalText(name, px - name.length()*2, py + radius() + 5);
}

void Cloud::drawVapor() {
	std::stringstream vss;
	vss << std::fixed << std::setprecision(2) << vapor;
	std::string vs = vss.str();

	drawNormalText(vs, px - vs.length()*2, py - radius() - 10);
}

void Cloud::drawVelocity() {
	std::stringstream vxss;
	vxss << "vx: " << std::fixed << std::setprecision(2) << vx; // to desimaler
	std::string vxs = vxss.str();

	std::stringstream vyss;
	vyss << "vy: " << std::fixed << std::setprecision(2) << vy; // to desimaler
	std::string vys = vyss.str();

	drawNormalText(vxs, px + radius() + 10, py - 5);

	drawNormalText(vys, px + radius() + 10, py + 5);
}

void Cloud::drawPosition() {
	std::stringstream pxss;
	pxss << "px: " << (int)px;
	std::string pxs = pxss.str();

	std::stringstream pyss;
	pyss << "py: " << (int)py;
	std::string pys = pyss.str();

	drawNormalText(pxs, px - radius() - 10 - pxs.length() * 6, py - 5);

	drawNormalText(pys, px - radius() - 10 - pxs.length() * 6, py + 5);
}

void Cloud::show() {
	double diamenter = radius() * 2.6; // .6 pga skyene ikke fyller hele bildet!
	double zoomx = diamenter  / (float)gray->w;
	double zoomy = diamenter / (float)gray->h;

	SDL_Surface *cloudImage;
	if(color == "gray")
		cloudImage = zoomSurface(gray, zoomx, zoomy, SMOOTHING_OFF);
	else if(color == "blue")
		cloudImage = zoomSurface(blue, zoomx, zoomy, SMOOTHING_OFF);
	else if(color == "red")
		cloudImage = zoomSurface(red, zoomx, zoomy, SMOOTHING_OFF);

	drawSurface(px - diamenter / 2, py - diamenter / 2, cloudImage, screen);
	SDL_FreeSurface(cloudImage);
}

Cloud *cloud[MAX_CLOUDS];


////////////////////////////////////////////////////////////////////////////////
// Usage
////////////////////////////////////////////////////////////////////////////////

void usage() {
	std::cout << "Usage: ./cloudwarsx -m [deathmatch, timelimit] -1 [ai, human] -2 [ai, human]" << std::endl;
	std::cout << "\t-m gamemode\tdeathmatch / timelimit" << std::endl;
	std::cout << "\t-s seconds\ttime limit in seconds" << std::endl;
	std::cout << "\t-1 ai / human\tplayer 1" << std::endl;
	std::cout << "\t-2 ai / human\tplayer 2" << std::endl;
	std::cout << "\t-3\t\tplayer 3 - only ai" << std::endl;
	std::cout << "\t-4\t\tplayer 4 - only ai" << std::endl;
	std::cout << "\t-l filename\tlevel filename" << std::endl;
	std::cout << "\t-r\t\tenable retromode (no gfx)" << std::endl;
	std::cout << "\t-x width\tset width" << std::endl;
	std::cout << "\t-y height\tset height" << std::endl;
	std::cout << "\t-f\t\tenable fullscreen" << std::endl;
	std::cout << "\t-p port\t\ttcp port for server" << std::endl;
	std::cout << "\t-n\t\tno sound" << std::endl;
	std::cout << "\t-d\t\tdebug mode" << std::endl;
	std::cout << "\t-v\t\tshow the version" << std::endl;
	exit(1);
}

//Return the distance between the two points
double distance(int x1, int y1, int x2, int y2) {
	return sqrt(pow( x2 - x1, 2) + pow(y2 - y1, 2));
}

bool checkCollision(Cloud &A, Cloud &B) {
	//If the distance between the centers of the circles is less than the sum of their radii
	if(distance(A.px, A.py, B.px, B.py) < (A.radius() + B.radius())) {
		//The circles have collided
		return true;
	}

	//If not
	return false;
}

int wind(int player, int x, int y) {

	// draw wind line
	if(debug) {
		cloud[player]->windX1 = cloud[player]->px;
		cloud[player]->windY1 = cloud[player]->py;
		cloud[player]->windX2 = x+cloud[player]->windX1;
		cloud[player]->windY2 = y+cloud[player]->windY1;
	}

	// The strength of the wind is calculated as sqrt(x*x+y*y).
	float strength = sqrt(x * x + y * y);

	// This value is not allowed to be less than 1.0 or greater than vapor/2.
	// If this happens, the WIND command is ignored.
	if((strength < 1.0) || (strength > cloud[player]->vapor / 2)) {
		if(debug)
			cloud[player]->windColor = 0x00FF0000; // red
		return 1; // IGNORE

	} else {
		// The vapor property of the thunderstorm will be reduced by strength.
		cloud[player]->vapor -= strength;

		// If the thunderstorm's amount of vapor goes below 1.0, the player dies
		// and is removed from the player list. The player's client can be
		// immediately disconnected with no prior warning.
		if(cloud[player]->vapor <= 1.0) {
			std::cout << "Vapor amount to low. Die!" << std::endl;
		}

		// The vector [(x / radius)*5, (y / radius)*5] is added to the velocity
		// of the thunderstorm.
		cloud[player]->vx += (x / cloud[player]->radius()) * 5;
		cloud[player]->vy += (y / cloud[player]->radius()) * 5;

		// The vector [wx, wy] is calculated as [x / strength, y / strength].
		float wx = x / strength;
		float wy = y / strength;

		// Let vector [vx, vy] represent the velocity of the thunderstorm.
		int vx = cloud[player]->vx;
		int vy = cloud[player]->vy;

		// A new raincloud is spawned with vapor equal to strength
		float raincloud_radius = sqrt(strength);

		// The distance to spawn the new raincloud at is calculated as:
		// (int)((storm_radius + raincloud_radius) * 1.1)
		int distance = (cloud[player]->radius() + raincloud_radius) * 1.1;

		// The position of the new raincloud is set to
		// [(int)(px - wx * distance), (int)(py - wy * distance)]
		int cpx = cloud[player]->px - wx * distance;
		int cpy = cloud[player]->py - wy * distance;

		// with velocity
		// [-(x / strength)*20 + vx, -(y / strength)*20 + vy]
		float cvx = -(x / strength) * 20 + vx;
		float cvy = -(y / strength) * 20 + vy;

		for(int i = 0; i < MAX_CLOUDS; i++) {
			if(!cloud[i]->alive) {
				cloud[i] = new Cloud(cpx, cpy, cvx, cvy, strength);
				cloud[i]->alive = true;
				cloud[i]->type = raincloud;
				cloud[i]->color = "gray";
				break;
			}
		}

		if(debug)
			cloud[player]->windColor = 0x0000FF00;

		return 0; // OK
	}
}

void wind(int player, std::string way) {
	int vaporAmount = 25;

	if(way == "up") {
		cloud[player]->vapor -= vaporAmount;
		cloud[player]->vy -= 10;

		for(int i = 0; i < MAX_CLOUDS; i++) {
			if(!cloud[i]->alive) {
				cloud[i] = new Cloud(cloud[player]->px, cloud[player]->py + cloud[player]->radius() + vaporAmount, -cloud[player]->vx, -cloud[player]->vy, vaporAmount);
				cloud[i]->alive = true;
				cloud[i]->type = raincloud;
				cloud[i]->color = "gray";
				break;
			}
		}
	}

	else if(way == "down") {
		cloud[player]->vapor -= vaporAmount;
		cloud[player]->vy += 10;

		for(int i = 0; i < MAX_CLOUDS; i++) {
			if(!cloud[i]->alive) {
				cloud[i] = new Cloud(cloud[player]->px, cloud[player]->py - cloud[player]->radius() - vaporAmount, -cloud[player]->vx, -cloud[player]->vy, vaporAmount);
				cloud[i]->alive = true;
				cloud[i]->type = raincloud;
				cloud[i]->color = "gray";
				break;
			}
		}
	}

	else if(way == "left") {
		cloud[player]->vapor -= vaporAmount;
		cloud[player]->vx -= 10;

		for(int i = 0; i < MAX_CLOUDS; i++) {
			if(!cloud[i]->alive) {
				cloud[i] = new Cloud(cloud[player]->px + cloud[player]->radius() + vaporAmount, cloud[player]->py, -cloud[player]->vx, -cloud[player]->vy, vaporAmount);
				cloud[i]->alive = true;
				cloud[i]->type = raincloud;
				cloud[i]->color = "gray";
				break;
			}
		}
	}

	else if(way == "right") {
		cloud[player]->vapor -= vaporAmount;
		cloud[player]->vx += 10;

		for(int i = 0; i < MAX_CLOUDS; i++) {
			if(!cloud[i]->alive) {
				cloud[i] = new Cloud(cloud[player]->px - cloud[player]->radius() - vaporAmount, cloud[player]->py, -cloud[player]->vx, -cloud[player]->vy, vaporAmount);
				cloud[i]->alive = true;
				cloud[i]->type = raincloud;
				cloud[i]->color = "gray";
				break;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Server Thread
////////////////////////////////////////////////////////////////////////////////
int server(void *data) {
	IPaddress serverIP;
	TCPsocket serverSocket;
	TCPsocket clientSocket[MAX_CLIENTS];
	bool socketIsFree[MAX_CLIENTS];

	char buffer[BUFFER_SIZE];
	int receivedByteCount = 0;

	bool playersConnected = false;

	int timer = SDL_GetTicks();
	int tries[MAX_CLIENTS];

	for(int i = 0; i < MAX_CLIENTS; i++) {
		tries[i] = 0;
	}

	if(SDLNet_Init()) {
		std::cerr << "Error: Unable to initialize SDL_net: " << SDLNet_GetError << std::endl;
		exit(1);
	}

	SDLNet_SocketSet socketSet = SDLNet_AllocSocketSet(MAX_SOCKETS);
 
	for(int loop = 0; loop < MAX_CLIENTS; loop++) {
		clientSocket[loop] = NULL;
		socketIsFree[loop] = true;
	}

	std::cout << "Starting server on port " << port << std::endl;
 
	SDLNet_ResolveHost(&serverIP, NULL, port);
	serverSocket = SDLNet_TCP_Open(&serverIP);
	SDLNet_TCP_AddSocket(socketSet, serverSocket);

	std::cout << "Waiting for client(s) to connect..." << std::endl;
 
	do {
		int numActiveSockets = SDLNet_CheckSockets(socketSet, 0);

		if(numActiveSockets != 0) {
			std::cout << "There are currently " << numActiveSockets << " socket(s) with data to be processed." << std::endl;
		}

		int serverSocketActivity = SDLNet_SocketReady(serverSocket);

		memset(&buffer[0], 0, sizeof(buffer));

		if(serverSocketActivity != 0) {
			if(clientCount < MAX_CLIENTS) {
				int freeSpot = -99;

				for(int loop = 0; loop < MAX_CLIENTS; loop++) {
					if(socketIsFree[loop] == true) {
						socketIsFree[loop] = false;
						freeSpot = loop;
						break;
					}
				}

				clientSocket[freeSpot] = SDLNet_TCP_Accept(serverSocket);
				SDLNet_TCP_AddSocket(socketSet, clientSocket[freeSpot]);
				clientCount++;

				std::cout << "Client connected. There are now " << clientCount << " client(s) connected." << std::endl << std::endl;

			} else {
				std::cout << "Maximum client count reached - rejecting client connection" << std::endl;
			}
		}


		for(int clientNumber = 0; clientNumber < MAX_CLIENTS; clientNumber++) {
			int clientSocketActivity = SDLNet_SocketReady(clientSocket[clientNumber]);

			if(clientSocketActivity != 0) {
				receivedByteCount = SDLNet_TCP_Recv(clientSocket[clientNumber], buffer, BUFFER_SIZE);

				if(receivedByteCount <= 0) {
					std::cout << "Client " << clientNumber << " disconnected." << std::endl << std::endl;
					SDLNet_TCP_DelSocket(socketSet, clientSocket[clientNumber]);
					SDLNet_TCP_Close(clientSocket[clientNumber]);
					clientSocket[clientNumber] = NULL;
					socketIsFree[clientNumber] = true;
					clientCount--;

					std::cout << "Server is now connected to: " << clientCount << " client(s)." << std::endl << std::endl;

				} else {
					std::cout << "Received: " << buffer << " from client number: " << clientNumber << std::endl;


					std::vector<std::string> v;
					v.clear();
					std::string s = buffer;

					if(std::string::npos != s.find(" ")) {
						split(s, ' ', v);
					} else {
						v.push_back(s);
					}

					s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());

					// NAME
					if(v[0] == "NAME") {
						std::cout << "Client " << clientNumber << " name: " << v[1] << std::endl;
						cloud[clientNumber]->name = v[1];
						++playerCount;
					}

					// START
					if(!playersConnected) {
						if(clientCount == aiPlayers) {
							for(int i = 0; i < aiPlayers; i++) {
								std::cout << "Sending: START to client " << i << std::endl;
								strcpy(buffer, "START\n");
								int msgLength = strlen(buffer);
								SDLNet_TCP_Send(clientSocket[i], (void *)buffer, msgLength);
							}
							playersConnected = true;
						}
					}

					// Check for abusive clients (max 10 tries pr. sec)
					for(int i = 0; i < MAX_CLIENTS; i++) {
						if(tries[i] > 10) {
							std::cout << "client " << i << " GET_STATE max tries exceeded (>10) - setting vapor to 0" << std::endl;
							cloud[i]->vapor = 0;
						}
					}

					// Reset the get state counter each second
					if((SDL_GetTicks() - timer) > 1000) {
						for(int i = 0; i < MAX_CLIENTS; i++) {
							tries[i] = 0;
							timer = SDL_GetTicks(); // reset the timer
						}
					}

					// GET_STATE
					if(s == "GET_STATE") {

						++tries[clientNumber];

						std::stringstream begin;
						begin << "BEGIN_STATE " << iteration << std::endl;
						std::string Begin = begin.str();

						strcpy(buffer, Begin.c_str());
						int msgLength = strlen(buffer);
						SDLNet_TCP_Send(clientSocket[clientNumber], (void *)buffer, msgLength);

						// YOU x\n
						std::stringstream you;
						you << "YOU " << clientNumber << std::endl;
						std::string You = you.str();
						
						strcpy(buffer, You.c_str());
						msgLength = strlen(buffer);
						SDLNet_TCP_Send(clientSocket[clientNumber], (void *)buffer, msgLength);

						// THUNDERSTORM px py vx vy vapor\n
						for(int i = 0; i < 2; i++) {
							std::stringstream thunder;
							thunder << "THUNDERSTORM " << cloud[i]->px << " " << cloud[i]->py << " " << cloud[i]->vx << " " << cloud[i]->vy << " " << cloud[i]->vapor << std::endl;
							std::string foo = thunder.str();

							strcpy(buffer, foo.c_str());
							msgLength = strlen(buffer);
							SDLNet_TCP_Send(clientSocket[clientNumber], (void *)buffer, msgLength);
						}

						// RAINCLOUD x y vx vy vapor\n
						for(int i = 2; i < MAX_CLOUDS; i++) {
							if(cloud[i]->alive) {
								std::stringstream rain;
								rain << "RAINCLOUD " << cloud[i]->px << " " << cloud[i]->py << " " << cloud[i]->vx << " " << cloud[i]->vy << " " << cloud[i]->vapor << std::endl;
								std::string foo = rain.str();

								strcpy(buffer, foo.c_str());
								msgLength = strlen(buffer);
								SDLNet_TCP_Send(clientSocket[clientNumber], (void *)buffer, msgLength);
							}
						}

						strcpy(buffer, "END_STATE\n");
						msgLength = strlen(buffer);
						SDLNet_TCP_Send(clientSocket[clientNumber], (void *)buffer, msgLength);
					}

					// WIND
					else if(v[0] == "WIND") {
						int x,y;
						x = atoi(v[1].c_str());
						y = atoi(v[2].c_str());

						if(wind(clientNumber, x, y)) {
							strcpy(buffer, "IGNORE\n");
							int msgLength = strlen(buffer);
							SDLNet_TCP_Send(clientSocket[clientNumber], (void *)buffer, msgLength);
						} else {
							strcpy(buffer, "OK\n");
							int msgLength = strlen(buffer);
							SDLNet_TCP_Send(clientSocket[clientNumber], (void *)buffer, msgLength);
						}
					}
				}
			}
		}

		SDL_Delay(10);

	} while(!done);

	SDLNet_FreeSocketSet(socketSet);
	SDLNet_TCP_Close(serverSocket);
	SDLNet_Quit();
}

////////////////////////////////////////////////////////////////////////////////
// Random range: -x to x but never 0
////////////////////////////////////////////////////////////////////////////////

int randomRange(int x) {
	int random = 0;
	while(random == 0) {
		random = rand() % (x+x+1) - x;
	}
	return random;
}

////////////////////////////////////////////////////////////////////////////////
// Create cloud
////////////////////////////////////////////////////////////////////////////////
void createCloud(int i, int v) {
	int vx = randomRange(3);
	int vy = randomRange(3);

	int vapor;

	if(v == 0)
		vapor = rand() % 500 + 10;
	else
		vapor = v;

	int radius = sqrt(vapor);
	int px = rand() % width;
	int py = rand() % height;

	// Checking for out of bound position

	// Left
	if(px - radius <= 0)
		px += radius;

	// Right
	if(px + radius > width)
		px -= radius;

	// Top
	if(py - radius <= 0)
		py += radius;

	// Bootom
	if(py + radius > height)
		py -= radius;

	cloud[i] = new Cloud(px, py, vx, vy, vapor);
	cloud[i]->alive = true;
}

////////////////////////////////////////////////////////////////////////////////
// Level functions
////////////////////////////////////////////////////////////////////////////////

void loadLevel(std::string filename) {
	std::ifstream load(filename.c_str());
	std::cout << "Loading file: " << filename << std::endl;

	if(!load) {
		std::cerr << "Error: " << filename << " levelfile not found!" << std::endl;
		exit(1);
	}

	while(!load.eof()) {
		std::string cloudType;
		float px, py;
		float vx, vy;
		float vapor;

		load >> cloudType;
		load >> px;
		load >> py;
		load >> vx;
		load >> vy;
		load >> vapor;

		if( (cloudType != "") || (px) || (py) || (vx) || (vy) || (vapor) ) {
			if(cloudType == "THUNDERSTORM") {
				cloud[thunderCloud] = new Cloud(px, py, vx, vy, vapor);
				cloud[thunderCloud]->alive = true;
				++thunderCloud;
			}

			else if(cloudType == "RAINCLOUD") {
				cloud[rainCloud] = new Cloud(px, py, vx, vy, vapor);
				cloud[rainCloud]->alive = true;
				cloud[rainCloud]->type = raincloud;
				cloud[rainCloud]->color = "gray";
				++rainCloud;
			}
		}
	}

	load.close();
}

////////////////////////////////////////////////////////////////////////////////
// Main
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
	gamemodes gamemode;

////////////////////////////////////////////////////////////////////////////////
// Commandline Arguments
////////////////////////////////////////////////////////////////////////////////

	if(argc <= 1) {
		usage();
	}

	char opt_char=0;
	while((opt_char = getopt(argc, argv, "l:vndm:hs:1:2:34rfx:y:p:")) != -1) {
		switch(opt_char) {
			case 'l':
				loadLevel(optarg);
				level=true;
				break;
			case 'v':
				std::cout << "version: " << version << std::endl;
				exit(1);
				break;

			case 'n':
				nosound = true;
				break;

			case 'm': {
				std::string mode = optarg;
				if(mode == "timelimit")
					gamemode = timelimit;
				else if(mode == "deathmatch")
					gamemode = deathmatch;
				else {
					std::cout << "Error: No game mode!" << std::endl;
					usage();
				}
				break;
			}

			case 's':
				limit = atoi(optarg);
				break;

			case 'd':
				debug=true;
				break;

			// Player 1
			case '1': {
				std::string player = optarg;

				if(player == "ai") {
					if(!level)
						createCloud(0, vaporStart);
					cloud[0]->name = "AI";
					cloud[0]->type = ai;
					cloud[0]->player = 1;
					cloud[0]->color = "blue";
					++aiPlayers;
				} else if(player == "human") {
					if(!level)
						createCloud(0, vaporStart);
					cloud[0]->name = "Player 1";
					cloud[0]->type = human;
					cloud[0]->player = 1;
					cloud[0]->color = "blue";
					++playerCount;
				} else {
					std::cout << "Error: Player 1 not defined!" << std::endl;
					usage();
				}

				std::cout << "Player 1: " << cloud[0]->getType() << std::endl;
				break;
			}

			// Player 2
			case '2': {
				std::string player = optarg;

				if(player == "ai") {
					if(!level)
						createCloud(1, vaporStart);
					cloud[1]->name = "AI";
					cloud[1]->type = ai;
					cloud[1]->player = 2;
					cloud[1]->color = "red";
					++aiPlayers;
				} else if(player == "human") {
					if(!level)
						createCloud(1, vaporStart);
					cloud[1]->name = "Player 2";
					cloud[1]->type = human;
					cloud[1]->player = 2;
					cloud[1]->color = "red";
					++playerCount;
				} else {
					std::cout << "Error: Player 2 not defined!" << std::endl;
					usage();
				}

				std::cout << "Player 2: " << cloud[1]->getType() << std::endl;
				break;
			}

			case '3':
			break;

			case '4':
			break;

			case 'h':
				usage();
				break;
			case 'r':
				retro=true;
				break;

			case 'f':
				fullscreen = true;
				break;

			case 'x':
				width = atoi(optarg);
				break;

			case 'y':
				height = atoi(optarg);
				break;

			case 'p':
				port = atoi(optarg);
				break;

			case '?':
				usage();
				break;

			default:
				usage();
				break;
		}
	}

////////////////////////////////////////////////////////////////////////////////
// Game modes
////////////////////////////////////////////////////////////////////////////////

	if(gamemode == timelimit) {
		std::cout << "Game mode: Timelimit" << std::endl;
		title = title + " - Timelimit";

		if(limit) {
			timeLimit = limit;
			std::cout << "Using time limit: " << timeLimit << std::endl;
		} else {
			timeLimit = defaultTimeLimit;
			std::cout << "Using default time limit: " << timeLimit << std::endl;
		}
	} else if(gamemode == deathmatch) {
		std::cout << "Game mode: Deathmatch" << std::endl;
		title = title + " - Deathmatch";
	}

	title = title + ": " + cloud[0]->getType() + " vs " + cloud[1]->getType();

////////////////////////////////////////////////////////////////////////////////
// Init
////////////////////////////////////////////////////////////////////////////////

	if(debug)
		std::cout << "Debug mode on!" << std::endl;

	int sdlFlags;
	sdlFlags = SDL_SWSURFACE;
	int time = 0;

	if(SDL_Init(SDL_INIT_EVERYTHING)) {
		std::cerr << "Error: Unable to initialize SDL: " << SDL_GetError << std::endl;
		exit(1);
	}

	if(fullscreen) {
		sdlFlags |= SDL_FULLSCREEN;
	} else {
		SDL_putenv((char *)"SDL_VIDEO_CENTERED=center");
	}

	screen = SDL_SetVideoMode(width, height, bpp, sdlFlags);

	if(!screen) {
		std::cerr << "Error: Unable to set video mode: " << SDL_GetError << std::endl;
		exit(1);
	}

	SDL_WM_SetCaption(title.c_str(), title.c_str());

	if((cloud[0]->type == ai) && (cloud[1]->type == ai))
		SDL_ShowCursor(0);

	// Audio
	int audio_rate = 22050;
	Uint16 audio_format = AUDIO_S16SYS;
	int audio_channels = 2;
	int audio_buffers = 4096;

	if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers)) {
		std::cerr << "Error: Unable to set up audio: " << SDL_GetError << std::endl;
		exit(1);
	}

	// Music and sounds
	waitingMusic = Mix_LoadMUS("think.mp3");
	bounceSound = Mix_LoadMUS("bounce.mp3");
	absorbSound = Mix_LoadMUS("absorb.aif");
	music = Mix_LoadWAV("music.wav");
	winnerSound = Mix_LoadWAV("winner.wav");

	if((!waitingMusic) || (!bounceSound) || (!absorbSound) || (!music) || (!winnerSound))
		nosound = true;

	if(nosound)
		std::cout << "Turing off music and sound!" << std::endl;

	// Font
	if(TTF_Init()) {
		std::cerr << "Error: Unable to initialize SDL_ttf: " << TTF_GetError << std::endl;
		exit(1);
	}

	font = loadFont("LiberationMono-Bold.ttf", 10);
	fontWinner = loadFont("LiberationMono-Bold.ttf", 40);
	fontWaiting = loadFont("LiberationMono-Bold.ttf", 25);

	// Images
	background = loadImage("sprites/bg.png");
	blue = loadImage("sprites/blue.png");
	gray = loadImage("sprites/gray.png");
	orange = loadImage("sprites/orange.png");
	purple = loadImage("sprites/purple.png");
	red = loadImage("sprites/red.png");

	if((!background) || (!blue) || (!gray) || (!orange) || (!purple) || (!red)) {
		std::cout << "WARNING: Sprite(s) is missing! You can run: ./install_sprites to download the original graphics." << std::endl;
		retro = true;
	}

	if(retro)
		std::cout << "Going retro! (no gfx)" << std::endl;

	// seed rand
	srand(SDL_GetTicks());

	// init rainclouds randomly
	if(!level) {
		for(int i = 2; i < startClouds; i++) {
			createCloud(i, 0);
			cloud[i]->type = raincloud;
			cloud[i]->color = "gray";
		}
	} else {
		startClouds = rainCloud;
	}

	for(int i = startClouds; i < MAX_CLOUDS; i++) {
		cloud[i] = new Cloud(rand() % width + 1, rand() % height + 1, rand() % 2+1, rand() % 2+1, rand() % 100);
		cloud[i]->alive = false;
		cloud[i]->color = "gray";
	}

////////////////////////////////////////////////////////////////////////////////
// Start server and wait for AIs
////////////////////////////////////////////////////////////////////////////////

	if(cloud[0]->type == ai || cloud[1]->type == ai) {
		thread = SDL_CreateThread(server, NULL);

		std::string waitingP1, waitingP2;

		waitingP1 = "Waiting on player 1 AI to connect...";
		waitingP2 = "Waiting on player 2 AI to connect...";

		// Play music loop
		if(!nosound)
			Mix_PlayMusic(waitingMusic, -1);

		while(playerCount != 2) {
			while(SDL_PollEvent(&event)) {
				if(event.type == SDL_QUIT)
					exit(0);

				if(event.type == SDL_KEYDOWN) {
					switch(event.key.keysym.sym) { 
						case SDLK_ESCAPE:
							exit(0);
							break;
					}
				}
			}

			SDL_FillRect(screen, &screen->clip_rect, SDL_MapRGB(screen->format, 0x00, 0x00, 0x00));

			if((cloud[0]->type == ai) && (cloud[1]->type == ai)) {
				if(playerCount != 1) {
					drawWaitingText(waitingP1, (width/2)-waitingP1.length()*7.5, height/2-20);
				}

				if(playerCount != 2) {
					drawWaitingText(waitingP2, (width/2)-waitingP2.length()*7.5, height/2+20);
				}
			}

			else if(cloud[0]->type == ai) {
				drawWaitingText(waitingP1, (width/2)-waitingP1.length()*7.5, height/2);
			} else if(cloud[1]->type == ai) {
				drawWaitingText(waitingP2, (width/2)-waitingP2.length()*7.5, height/2);
			}

			SDL_Flip(screen);
			SDL_Delay(10);
		}

		// stop music
		if(!nosound)
			Mix_HaltMusic();
	}

////////////////////////////////////////////////////////////////////////////////
// Game loop
////////////////////////////////////////////////////////////////////////////////
	std::cout << "Game start!" << std::endl;
	int timeStart = SDL_GetTicks() / 1000; 

	// Play music loop
	if(!nosound) {
		channel = Mix_PlayChannel(-1, music, -1);
	}

	while(!done) {

////////////////////////////////////////////////////////////////////////////////
// Events and Input
////////////////////////////////////////////////////////////////////////////////

		while(SDL_PollEvent(&event)) {
			if(event.type == SDL_QUIT)
				done = true;

			if(event.type == SDL_KEYDOWN) {
				switch(event.key.keysym.sym) { 
					case SDLK_ESCAPE:
						done = true;
						break;
				}
			}

			if(event.type == SDL_MOUSEBUTTONDOWN) {
				if(event.button.button == SDL_BUTTON_LEFT) {
					int x = event.button.x; 
					int y = event.button.y;

					if(cloud[0]->type == human) {
						int px = x - cloud[0]->px;
						int py = y - cloud[0]->py;
						wind(0, px, py);
					} else if(cloud[1]->type == human) {
						int px = x - cloud[1]->px;
						int py = y - cloud[1]->py;
						wind(1, px, py);
					}
				}
			} 

			// player1 input
			if(event.type == SDL_KEYDOWN) {
				switch(event.key.keysym.sym) {
					case SDLK_UP:
						wind(0, "up");
						break;
					case SDLK_DOWN:
						wind(0, "down");
						break;
					case SDLK_LEFT: 
						wind(0, "left");
						break;
					case SDLK_RIGHT:
						wind(0, "right");
						break;
				}
			}

			// player 2 input
			if(event.type == SDL_KEYDOWN) {
				switch(event.key.keysym.sym) {
					case SDLK_w:
						wind(1, "up");
						break;
					case SDLK_s:
						wind(1, "down");
						break;
					case SDLK_a:
						wind(1, "left");
						break;
					case SDLK_d:
						wind(1, "right");
						break;
				}
			}
		}

////////////////////////////////////////////////////////////////////////////////
// Draw
////////////////////////////////////////////////////////////////////////////////

		// Update title with time if gamemode is timelimit
		if(gamemode == timelimit) {
			time = SDL_GetTicks() / 1000 -timeStart;

			std::stringstream ssLimit;
			ssLimit << timeLimit;
			std::stringstream ssTime;
			ssTime << time;

			std::string title2 = title + " - " + ssTime.str() + "/" + ssLimit.str();
			SDL_WM_SetCaption(title2.c_str(), title2.c_str());
		}

		// Background
		if(retro)
			SDL_FillRect(screen, &screen->clip_rect, SDL_MapRGB(screen->format, 0x00, 0x00, 0x00));
		else
			drawSurface(0, 0, background, screen);

		// Clouds
		for(int i = 0; i < MAX_CLOUDS; i++) {
			if(cloud[i]->alive) {
				if(retro) {
					cloud[i]->draw();
				} else {
					cloud[i]->show();
				}

				if(debug) {
					cloud[i]->drawVapor();
					cloud[i]->drawVelocity();
					cloud[i]->drawPosition();
				}
			}

			if((cloud[i]->type == human) || (cloud[i]->type == ai)) {
				cloud[i]->drawName();

				if(debug)
					cloud[i]->drawWind();
			}
		}

////////////////////////////////////////////////////////////////////////////////
// Moving the clouds and checking for collision between boundaries
////////////////////////////////////////////////////////////////////////////////

		for(int i = 0; i < MAX_CLOUDS; i++) {
			if(cloud[i]->alive) {
				bool collision = false;

				// The velocity is damped to make it more natural.
				cloud[i]->vx *= 0.999;
				cloud[i]->vy *= 0.999;

				// position += velcoity * 0.1 
				cloud[i]->px += cloud[i]->vx * 0.1; // left or right
				cloud[i]->py += cloud[i]->vy * 0.1; //  up or down

				// Collision Left
				if(cloud[i]->px < cloud[i]->radius()) {
					cloud[i]->px = cloud[i]->radius();
					cloud[i]->vx = abs(cloud[i]->vx) * 0.6;
					collision = true;
				}
				
				// Collision Top
				if(cloud[i]->py < cloud[i]->radius()) {
					cloud[i]->py = cloud[i]->radius();
					cloud[i]->vy = abs(cloud[i]->vy) * 0.6;
					collision = true;
				}

				// Collision Right
				if(cloud[i]->px+cloud[i]->radius() > width) {
					cloud[i]->px = width-cloud[i]->radius();
					cloud[i]->vx = -abs(cloud[i]->vx) * 0.6;
					collision = true;
				}

				// Collision Bottom
				if(cloud[i]->py+cloud[i]->radius() > height) {
					cloud[i]->py = height-cloud[i]->radius();
					cloud[i]->vy = -abs(cloud[i]->vy) * 0.6;
					collision = true;
				}

				// Play sound if collision
				if(collision) {
					if(!nosound) {
						Mix_PlayMusic(bounceSound, 0);
					}
				}
			}
		}

////////////////////////////////////////////////////////////////////////////////
// Collision Testing
////////////////////////////////////////////////////////////////////////////////

		for(int i = 0; i < MAX_CLOUDS; i++) {
			if(cloud[i]->alive) {
				for(int j = 0; j < MAX_CLOUDS; j++) {
					if(cloud[j]->alive) {
                        if (i == j) continue;
						while(checkCollision(*cloud[i], *cloud[j] )) {
							if(cloud[i]->vapor < cloud[j]->vapor) {
								cloud[i]->vapor -= absorb;
								cloud[j]->vapor += absorb;
							} else if(cloud[i]->vapor > cloud[j]->vapor) {
								cloud[i]->vapor += absorb;
								cloud[j]->vapor -= absorb;
							} else if(cloud[i]->vapor == cloud[j]->vapor) {
								// random choose between thunderstorms
								int random = rand() % 2;
								if(random == 1) {
									cloud[i]->vapor -= absorb;
									cloud[j]->vapor += absorb;
								} else {
									cloud[i]->vapor += absorb;
									cloud[j]->vapor -= absorb;
								}
							}

							/*
							// Play sound if collision
							if(collision) {
								if(!nosound) {
									Mix_PlayMusic(absorbSound, 0);
									std::cout << "play" << std::endl;
								}
							}
							*/
						}
					}
				}
			}
		}

		for(int i = 2; i < MAX_CLOUDS; i++) {
			if(cloud[i]->alive) {
				if(cloud[i]->vapor <= 1.0) {
					cloud[i]->alive = false;
				}
			}
		}

////////////////////////////////////////////////////////////////////////////////
// Endgame
////////////////////////////////////////////////////////////////////////////////

		if(gamemode == timelimit) {
			if(time >= timeLimit) {
				std::cout << "Time's' up!" << std::endl;
				std::cout << "Player 1 vapor: " << cloud[0]->vapor << std::endl;
				std::cout << "Player 2 vapor: " << cloud[1]->vapor << std::endl;
				done = true;
			}
		}

		if(cloud[0]->vapor <= 1.0) {
			Winner = 2;
			done = true;
		} else if(cloud[1]->vapor <= 1.0) {
			Winner = 1;
			done = true;
		}

////////////////////////////////////////////////////////////////////////////////
// Update
////////////////////////////////////////////////////////////////////////////////

		SDL_Flip(screen);
		SDL_Delay(10);
		++iteration;
	}

////////////////////////////////////////////////////////////////////////////////
// Declare winner
////////////////////////////////////////////////////////////////////////////////

	// Stop game music
	if(!nosound)
		Mix_HaltChannel(channel);

	// Play winning music
	if(!nosound) {
		channel = Mix_PlayChannel(-1, winnerSound, 0);
	}

	// Check for winner in timelimit mode or user exiting
	if(cloud[0]->vapor > cloud[1]->vapor) {
		Winner = 1;
		done = true;
	} else if(cloud[0]->vapor < cloud[1]->vapor) {
		Winner = 2;
		done = true;
	} else if(cloud[0]->vapor == cloud[1]->vapor) {
		Winner = 0;
		done = true;
	}

	std::stringstream winnerSS;

	if(Winner == 0)
		winnerSS << "Draw!";
	else if(Winner == 1)
		winnerSS << cloud[0]->name << " (" << cloud[0]->getType() << ") wins!";
	else if(Winner == 2)
		winnerSS << cloud[1]->name << " (" << cloud[1]->getType() << ") wins!";
	else if(Winner == 3)
		winnerSS << cloud[2]->name << " (" << cloud[2]->getType() << ") wins!";
	else if(Winner == 4)
		winnerSS << cloud[3]->name << " (" << cloud[3]->getType() << ") wins!";

	std::string winnerS = winnerSS.str();
	std::cout << winnerS << std::endl;

	drawWinnerText(winnerS, (width/2)-winnerS.length()*12, height/2);

	SDL_Flip(screen);
	SDL_Delay(2000);

	std::cout << "Game finish!" << std::endl;

////////////////////////////////////////////////////////////////////////////////
// Clean up and exit
////////////////////////////////////////////////////////////////////////////////

	for(int i = 0; i < MAX_CLOUDS; i++) {
		delete cloud[i];
	}

	SDL_FreeSurface(screen);

	SDL_FreeSurface(background);
	SDL_FreeSurface(blue);
	SDL_FreeSurface(gray);
	SDL_FreeSurface(orange);
	SDL_FreeSurface(purple);
	SDL_FreeSurface(red);

	Mix_FreeMusic(waitingMusic);
	Mix_FreeMusic(bounceSound);
	Mix_FreeMusic(absorbSound);
	Mix_FreeChunk(music);
	Mix_FreeChunk(winnerSound);
	Mix_CloseAudio();

	SDL_FreeSurface(winner);
	TTF_CloseFont(font);
	TTF_CloseFont(fontWinner);
	TTF_CloseFont(fontWaiting);
	TTF_Quit(); 

	SDL_KillThread(thread);

	SDL_Quit();
	return 0;
}
