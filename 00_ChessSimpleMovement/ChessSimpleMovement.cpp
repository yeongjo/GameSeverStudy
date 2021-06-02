#include <iostream>
#include <conio.h>
#include <Windows.h>

#define MAPSIZE 8

using namespace std;

char map[MAPSIZE][MAPSIZE];
int playerPosX = 4;
int playerPosY = 4;

int clamp(int value, int min, int max)
{
	return value < min ? min : value > max ? max : value;
}

void gotoxy(int x, int y)
{
	COORD Pos;
	Pos.X = x;
	Pos.Y = y;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), Pos);

}

int main()
{
	while(true)
	{
		gotoxy(0, 0);
		for (size_t i = 0; i < MAPSIZE; i++)
		{
			for (size_t j = 0; j < MAPSIZE; j++)
			{
				map[i][j] = '+';
			}
		}

		map[playerPosY][playerPosX] = 'A';

		for (size_t i = 0; i < MAPSIZE; i++)
		{
			for (size_t j = 0; j < MAPSIZE; j++)
			{
				cout << map[i][j];
			}
			cout << endl;
		}

		switch (_getch())
		{
		case 72: // 위
			playerPosY--;
			break;
		case 80: // 아래
			playerPosY++;
			break;
		case 75: // 왼
			playerPosX--;
			break;
		case 77: // 오른
			playerPosX++;
			break;
		default:
			continue;
			break;
		}
		playerPosX = clamp(playerPosX, 0, MAPSIZE-1);
		playerPosY = clamp(playerPosY, 0, MAPSIZE-1);
	}
}