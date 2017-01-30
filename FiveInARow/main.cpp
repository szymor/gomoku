
/*
TO DO:
	- pingowanie i responsy na pingi
	- czat pomiedzy graczami
	- moze mi cos jeszcze przyjdzie do lba ;]
*/

#include <cstdlib>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "resource.h"

// gra
#define FIELD_WIDTH		15
#define FIELD_HEIGHT	15
#define FIELD_PLAYER1	1
#define FIELD_PLAYER2	2
#define FIELD_EMPTY		0
#define PLAYER_HUMAN	1
#define PLAYER_COMPUTER	2
#define PLAYER_NETWORK	4
#define GAME_LASTING	0
#define GAME_PLAYER1	1		// te 2 stale MUSZA byc
#define	GAME_PLAYER2	2		// takie same jak odpowiedniki FIELD_...
#define GAME_DRAW		4
#define AI_N1			1.0		// priorytety ruchow na nasza korzysc
#define AI_N2			3.0
#define AI_N3			50.0
#define AI_N4			10000.0
#define AI_M1			1.0		// priorytety ruchow na niekorzysc przeciwnika
#define AI_M2			3.0
#define AI_M3			50.0
#define AI_M4			200.0

char whoseTurn;								// czyja teraz kolej (FIELD_PLAYER1 lub FIELD_PLAYER2)
char field[FIELD_WIDTH][FIELD_HEIGHT];		// pole gry
char player1_type;							// kto jest graczem 1 (PLAYER_...)
char player2_type;							// j.w. tyle ze 2
char lockField;

// interfejs
HWND hMainWindow = NULL;
HMENU hMenu = NULL;
HBITMAP hbmCircle = NULL;
HBITMAP hbmCross = NULL;
HBITMAP hbmEmpty = NULL;
char strTitle[256];
char strPlayer1Won[256];
char strPlayer2Won[256];
char strDraw[256];
char strPlayer1Turn[256];
char strPlayer2Turn[256];
char strWaitPeer[256];
char strConnect[256];
char strNetError[256];

// obsluga sieci

#define NETMSG_PING		0
#define NETMSG_RESP		1
#define NETMSG_MOVE		2
#define NETMSG_TALK		3
#define NETMSG_NEWG		4

typedef struct
{
	int type;
	union
	{
		struct
		{
			int posX;
			int posY;
		};
		char data[128];
	};
} SNETMSG;

BOOL isWaiting;
BOOL isConnected;
SOCKET waitingSocket = INVALID_SOCKET;
SOCKET clientSocket = INVALID_SOCKET;
int netPosX;
int netPosY;
BOOL netPosUsed;
char ina[256];			// InterNet Address ;]

void WaitForPeer(void);
void ConnectPeer(void);
void AcceptConnection(void);
void CloseConnection(void);
void ReceiveMessages(void);
void SendNetworkMoveMessage(int posX, int posY);
void NetworkMove(void);
void NetworkError(void);

// cala reszta ;]
void NewGame(void);
int IsTheGameFinished(void);
int Opponent(void);
void AddMoveValue(double* move, int aux, BOOL goodMoves);
BOOL MakeMove(int x, int y);
void ComputerMove(void);
void EndTurn(void);
BOOL CALLBACK NetPromptDialogFunc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	isWaiting = FALSE;
	isConnected = FALSE;
	netPosUsed = TRUE;
	player1_type = PLAYER_HUMAN;
	player2_type = PLAYER_COMPUTER;
	NewGame();
	lockField = TRUE;

	WSADATA wsadata;
	if( WSAStartup(MAKEWORD(2,2),&wsadata) )
	{
		EnableMenuItem(hMenu,MENU_HVSN,MF_GRAYED);
		EnableMenuItem(hMenu,MENU_NVSH,MF_GRAYED);
	}

	hMenu = LoadMenu(hInstance,MAKEINTRESOURCE(MENU_MAIN));
	hbmCircle = LoadBitmap(hInstance,MAKEINTRESOURCE(PIC_CIRCLE));
	hbmCross = LoadBitmap(hInstance,MAKEINTRESOURCE(PIC_CROSS));
	hbmEmpty = LoadBitmap(hInstance,MAKEINTRESOURCE(PIC_EMPTY));
	LoadString(hInstance,STR_TITLE,strTitle,256);
	LoadString(hInstance,STR_PL1WON,strPlayer1Won,256);
	LoadString(hInstance,STR_PL2WON,strPlayer2Won,256);
	LoadString(hInstance,STR_DRAW,strDraw,256);
	LoadString(hInstance,STR_PL1TURN,strPlayer1Turn,256);
	LoadString(hInstance,STR_PL2TURN,strPlayer2Turn,256);
	LoadString(hInstance,STR_WAIT,strWaitPeer,256);
	LoadString(hInstance,STR_CONNECT,strConnect,256);
	LoadString(hInstance,STR_NETERROR,strNetError,256);

	WNDCLASSEX wce;
	memset(&wce,0,sizeof(wce));
	wce.cbSize = sizeof(wce);
	wce.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wce.hCursor = LoadCursor(NULL,IDC_ARROW);
	wce.hInstance = hInstance;
	wce.lpfnWndProc = MainWindowProc;
	wce.lpszClassName = "FiveInARowMainWindowClass";
	wce.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	wce.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(CAC_ICON));
	RegisterClassEx(&wce);

	RECT rect;
	rect.left = rect.top = 50;
	rect.right = 32*FIELD_WIDTH + 50;
	rect.bottom = 32*FIELD_HEIGHT + 50;
	AdjustWindowRectEx(&rect,WS_SYSMENU | WS_MINIMIZEBOX | WS_CAPTION,TRUE,NULL);
	hMainWindow = CreateWindowEx(NULL,"FiveInARowMainWindowClass",strTitle,WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX | WS_CAPTION,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,NULL,hMenu,hInstance,NULL);

	SetTimer(hMainWindow,1,100,NULL);

	MSG msg;
	while(GetMessage(&msg,NULL,NULL,NULL))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if( !lockField )
		{
			if( ((whoseTurn == FIELD_PLAYER1) && (player1_type == PLAYER_COMPUTER)) || ((whoseTurn == FIELD_PLAYER2) && (player2_type == PLAYER_COMPUTER)) )
			{
				ComputerMove();
				EndTurn();
			}
			if( isConnected && (((whoseTurn == FIELD_PLAYER1) && (player1_type == PLAYER_NETWORK)) || ((whoseTurn == FIELD_PLAYER2) && (player2_type == PLAYER_NETWORK))) )
			{
				NetworkMove();
			}
		}
		if( isWaiting )
		{
			AcceptConnection();
		}
		if( isConnected )
		{
			ReceiveMessages();
		}
	}

	DeleteObject(hbmCircle);
	DeleteObject(hbmCross);
	DeleteObject(hbmEmpty);

	CloseConnection();

	WSACleanup();

	KillTimer(hMainWindow,1);

	return msg.wParam;
}

void WaitForPeer(void)
{
	/*
	WERSJA DLA SYSTEMOW WINDOWS XP I WYZEJ
	addrinfo hints;
	addrinfo* result;
	memset(&hints,0,sizeof(hints));
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if( getaddrinfo(NULL,"1234",&hints,&result) )
	{
		return;
	}
	waitingSocket = socket(result->ai_family,result->ai_socktype,result->ai_protocol);
	if( waitingSocket == INVALID_SOCKET )
	{
		freeaddrinfo(result);
		return;
	}
	if( bind(waitingSocket,result->ai_addr,result->ai_addrlen) == SOCKET_ERROR )
	{
		closesocket(waitingSocket);
		waitingSocket = INVALID_SOCKET;
		freeaddrinfo(result);
		return;
	}
	*/
	waitingSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if( waitingSocket == INVALID_SOCKET )
	{
		return;
	}
	sockaddr_in sai;
	memset(&sai,0,sizeof(sai));
	sai.sin_addr.s_addr = inet_addr("0.0.0.0");
	sai.sin_family = AF_INET;
	sai.sin_port = htons(1234);
	if( bind(waitingSocket,(sockaddr*)&sai,sizeof(sai)) == SOCKET_ERROR )
	{
		closesocket(waitingSocket);
		waitingSocket = INVALID_SOCKET;
		return;
	}

	if( listen(waitingSocket,SOMAXCONN) == SOCKET_ERROR )
	{
		closesocket(waitingSocket);
		waitingSocket = INVALID_SOCKET;
		// freeaddrinfo(result);
		return;
	}
	// freeaddrinfo(result);
	isWaiting = TRUE;
}

void AcceptConnection(void)
{
	timeval tm;
	tm.tv_sec = 0;
	tm.tv_usec = 100;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(waitingSocket,&fds);
	if( select(0,&fds,NULL,NULL,&tm) == 1 )
	{
		clientSocket = accept(waitingSocket,NULL,NULL);
		closesocket(waitingSocket);
		waitingSocket = INVALID_SOCKET;

		lockField = FALSE;
		SetWindowText(hMainWindow,strPlayer1Turn);

		isWaiting = FALSE;
		isConnected = TRUE;
	}
}

void ConnectPeer(void)
{
	clientSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if( clientSocket == INVALID_SOCKET )
	{
		return;
	}
	
	DialogBox(GetModuleHandle(NULL),MAKEINTRESOURCE(DLG_NETPROMPT),hMainWindow,NetPromptDialogFunc);

	sockaddr_in sai;
	memset(&sai,0,sizeof(sai));
	sai.sin_addr.s_addr = inet_addr(ina);
	sai.sin_family = AF_INET;
	sai.sin_port = htons(1234);
	if( connect(clientSocket,(sockaddr*)&sai,sizeof(sai)) == SOCKET_ERROR )
	{
		closesocket(clientSocket);
		clientSocket = INVALID_SOCKET;
		return;
	}

	isConnected = TRUE;
}

void CloseConnection(void)
{
	if( isWaiting )
	{
		closesocket(waitingSocket);
		waitingSocket = INVALID_SOCKET;
	}
	if( isConnected )
	{
		closesocket(clientSocket);
		clientSocket = INVALID_SOCKET;
	}
	isWaiting = FALSE;
	isConnected = FALSE;
	netPosUsed = TRUE;
	SetWindowText(hMainWindow,strTitle);
}

void ReceiveMessages(void)
{
	timeval tm;
	tm.tv_sec = 0;
	tm.tv_usec = 100;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(clientSocket,&fds);
	if( select(0,&fds,NULL,NULL,&tm) == 1 )
	{
		SNETMSG buff;
		if( recv(clientSocket,(char*)&buff,sizeof(buff),0) == SOCKET_ERROR )
		{
			NetworkError();
		}
		switch( buff.type )
		{
		case NETMSG_MOVE:
			{
				netPosX = buff.posX;
				netPosY = buff.posY;
				netPosUsed = FALSE;
				break;
			}
		case NETMSG_PING:
			{
				memset(&buff,0,sizeof(buff));
				buff.type = NETMSG_RESP;
				if( send(clientSocket,(char*)&buff,sizeof(buff),0) == SOCKET_ERROR )
				{
					NetworkError();
				}
				break;
			}
		case NETMSG_RESP:
			{
				break;
			}
		case NETMSG_TALK:
			{
				break;
			}
		case NETMSG_NEWG:
			{
				NewGame();
				SetWindowText(hMainWindow,strPlayer1Turn);
				InvalidateRect(hMainWindow,NULL,FALSE);
				UpdateWindow(hMainWindow);
				break;
			}
		}
	}
}

void SendNetworkMoveMessage(int posX, int posY)
{
	SNETMSG buff;
	memset(&buff,0,sizeof(buff));
	buff.type = NETMSG_MOVE;
	buff.posX = posX;
	buff.posY = posY;
	if( send(clientSocket,(char*)&buff,sizeof(buff),0) == SOCKET_ERROR )
	{
		NetworkError();
	}
}

void NetworkMove(void)
{
	if( !netPosUsed )
	{
		MakeMove(netPosX,netPosY);
		netPosUsed = TRUE;
		EndTurn();
	}
}

void NetworkError(void)
{
	CloseConnection();
	lockField = TRUE;
	CheckMenuItem(hMenu,MENU_HVSH,MF_UNCHECKED);
	CheckMenuItem(hMenu,MENU_HVSC,MF_CHECKED);
	CheckMenuItem(hMenu,MENU_CVSH,MF_UNCHECKED);
	CheckMenuItem(hMenu,MENU_HVSN,MF_UNCHECKED);
	CheckMenuItem(hMenu,MENU_NVSH,MF_UNCHECKED);
	player1_type = PLAYER_HUMAN;
	player2_type = PLAYER_COMPUTER;
	SetWindowText(hMainWindow,strNetError);
}

void NewGame()
{
	whoseTurn = FIELD_PLAYER1;
	memset(&field,FIELD_EMPTY,sizeof(field));
	lockField = FALSE;
}

// zwraca GAME_...
int IsTheGameFinished()
{
	int return_value;
	int aux;

	// sprawdzanie czy aktualnie stawiajacy znaczek gracz wygral
	return_value = GAME_DRAW;
	for(int i = 0; i < FIELD_WIDTH; ++i)
		for(int j = 0; j < FIELD_HEIGHT; ++j)
		{
			// sprawdzanie czy remis
			if( field[i][j] == FIELD_EMPTY )
				return_value = GAME_LASTING;

			// sprawdzanie linii pionowych
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i][j+k] == whoseTurn )
					++aux;
				if( aux == 5 )
					return whoseTurn;
				if( j+k+1 == FIELD_HEIGHT )
					break;
			}

			// sprawdzanie linii poziomych
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i+k][j] == whoseTurn )
					++aux;
				if( aux == 5 )
					return whoseTurn;
				if( i+k+1 == FIELD_WIDTH )
					break;
			}

			// sprawdzanie linii skosnych '\'
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i+k][j+k] == whoseTurn )
					++aux;
				if( aux == 5 )
					return whoseTurn;
				if( (i+k+1 == FIELD_WIDTH) || (j+k+1 == FIELD_HEIGHT) )
					break;
			}

			// sprawdzanie linii skosnych '/'
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i-k][j+k] == whoseTurn )
					++aux;
				if( aux == 5 )
					return whoseTurn;
				if( (i-k-1 == -1) || (j+k+1 == FIELD_HEIGHT) )
					break;
			}
		}

	return return_value;
}

int Opponent()
{
	return (whoseTurn == FIELD_PLAYER1) ? FIELD_PLAYER2 : FIELD_PLAYER1;
}

void AddMoveValue(double* move, int aux, BOOL goodMoves)
{
	if(goodMoves)
	{
		switch(aux)
		{
		case 0:
			break;
		case 1:
			{
				*move += AI_N1;
				break;
			}
		case 2:
			{
				*move += AI_N2;
				break;
			}
		case 3:
			{
				*move += AI_N3;
				break;
			}
		case 4:
			{
				*move += AI_N4;
				break;
			}
		}
	}
	else
	{
		switch(aux)
		{
		case 0:
			break;
		case 1:
			{
				*move += AI_M1;
				break;
			}
		case 2:
			{
				*move += AI_M2;
				break;
			}
		case 3:
			{
				*move += AI_M3;
				break;
			}
		case 4:
			{
				*move += AI_M4;
				break;
			}
		}
	}
}

BOOL MakeMove(int x, int y)
{
	if( field[x][y] == FIELD_EMPTY )
	{
		field[x][y] = whoseTurn;
		return TRUE;
	}
	else
		return FALSE;
}

void ComputerMove()
{
	double moves[FIELD_WIDTH][FIELD_HEIGHT];
	int aux, tempx, tempy;
	double aux2;
	
	// czyszczenie tablicy ruchow
	for(int i = 0; i < FIELD_WIDTH; ++i)
		for(int j = 0; j < FIELD_HEIGHT; ++j)
			moves[i][j] = 0.0;

	// przegladanie ruchow pionowych
	for(int i = 0; i < FIELD_WIDTH; ++i)
		for(int j = 0; j < FIELD_HEIGHT-4; ++j)
		{
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i][j+k] == whoseTurn )
					++aux;
				if( field[i][j+k] == Opponent() )
				{
					aux = 0;
					break;
				}
			}
			for(int k = 0; k < 5; ++k)
				if( field[i][j+k] == FIELD_EMPTY )
					AddMoveValue(&moves[i][j+k],aux,TRUE);
		}

	// przegladanie ruchow poziomych
	for(int i = 0; i < FIELD_WIDTH-4; ++i)
		for(int j = 0; j < FIELD_HEIGHT; ++j)
		{
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i+k][j] == whoseTurn )
					++aux;
				if( field[i+k][j] == Opponent() )
				{
					aux = 0;
					break;
				}
			}
			for(int k = 0; k < 5; ++k)
				if( field[i+k][j] == FIELD_EMPTY )
					AddMoveValue(&moves[i+k][j],aux,TRUE);
		}

	// przegladanie ruchow skosnych '\'
	for(int i = 0; i < FIELD_WIDTH-4; ++i)
		for(int j = 0; j < FIELD_HEIGHT-4; ++j)
		{
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i+k][j+k] == whoseTurn )
					++aux;
				if( field[i+k][j+k] == Opponent() )
				{
					aux = 0;
					break;
				}
			}
			for(int k = 0; k < 5; ++k)
				if( field[i+k][j+k] == FIELD_EMPTY )
					AddMoveValue(&moves[i+k][j+k],aux,TRUE);
		}

	// przegladanie ruchow skosnych '/'
	for(int i = 4; i < FIELD_WIDTH; ++i)
		for(int j = 0; j < FIELD_HEIGHT-4; ++j)
		{
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i-k][j+k] == whoseTurn )
					++aux;
				if( field[i-k][j+k] == Opponent() )
				{
					aux = 0;
					break;
				}
			}
			for(int k = 0; k < 5; ++k)
				if( field[i-k][j+k] == FIELD_EMPTY )
					AddMoveValue(&moves[i-k][j+k],aux,TRUE);
		}


	// teraz to samo, tyle ze dla ruchow przeciwnika

	// przegladanie ruchow pionowych
	for(int i = 0; i < FIELD_WIDTH; ++i)
		for(int j = 0; j < FIELD_HEIGHT-4; ++j)
		{
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i][j+k] == Opponent() )
					++aux;
				if( field[i][j+k] == whoseTurn )
				{
					aux = 0;
					break;
				}
			}
			for(int k = 0; k < 5; ++k)
				if( field[i][j+k] == FIELD_EMPTY )
					AddMoveValue(&moves[i][j+k],aux,FALSE);
		}

	// przegladanie ruchow poziomych
	for(int i = 0; i < FIELD_WIDTH-4; ++i)
		for(int j = 0; j < FIELD_HEIGHT; ++j)
		{
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i+k][j] == Opponent() )
					++aux;
				if( field[i+k][j] == whoseTurn )
				{
					aux = 0;
					break;
				}
			}
			for(int k = 0; k < 5; ++k)
				if( field[i+k][j] == FIELD_EMPTY )
					AddMoveValue(&moves[i+k][j],aux,FALSE);
		}

	// przegladanie ruchow skosnych '\'
	for(int i = 0; i < FIELD_WIDTH-4; ++i)
		for(int j = 0; j < FIELD_HEIGHT-4; ++j)
		{
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i+k][j+k] == Opponent() )
					++aux;
				if( field[i+k][j+k] == whoseTurn )
				{
					aux = 0;
					break;
				}
			}
			for(int k = 0; k < 5; ++k)
				if( field[i+k][j+k] == FIELD_EMPTY )
					AddMoveValue(&moves[i+k][j+k],aux,FALSE);
		}

	// przegladanie ruchow skosnych '/'
	for(int i = 4; i < FIELD_WIDTH; ++i)
		for(int j = 0; j < FIELD_HEIGHT-4; ++j)
		{
			aux = 0;
			for(int k = 0; k < 5; ++k)
			{
				if( field[i-k][j+k] == Opponent() )
					++aux;
				if( field[i-k][j+k] == whoseTurn )
				{
					aux = 0;
					break;
				}
			}
			for(int k = 0; k < 5; ++k)
				if( field[i-k][j+k] == FIELD_EMPTY )
					AddMoveValue(&moves[i-k][j+k],aux,FALSE);
		}

	// wnioskowanie i ostateczny ruch
	aux2 = 0.0;
	tempx = tempy = 0;
	for(int i = 0; i < FIELD_WIDTH; ++i)
		for(int j = 0; j < FIELD_HEIGHT; ++j)
			if( moves[i][j] > aux2 )
			{
				aux2 = moves[i][j];
				tempx = i;
				tempy = j;
			}
	if( !MakeMove(tempx,tempy) )
		for(int i = 0; i < FIELD_WIDTH; ++i)
			for(int j = 0; j < FIELD_HEIGHT; ++j)
				if( MakeMove(i,j) )
					return;
}

void EndTurn()
{
	int game_progress;
	game_progress = IsTheGameFinished();
	if( game_progress != GAME_LASTING )
		lockField = TRUE;
	whoseTurn = Opponent();
	switch( game_progress )
	{
	case GAME_PLAYER1:
		{
			SetWindowText(hMainWindow,strPlayer1Won);
			break;
		}
	case GAME_PLAYER2:
		{
			SetWindowText(hMainWindow,strPlayer2Won);
			break;
		}
	case GAME_DRAW:
		{
			SetWindowText(hMainWindow,strDraw);
			break;
		}
	case GAME_LASTING:
		{
			if( whoseTurn == FIELD_PLAYER1 )
				SetWindowText(hMainWindow,strPlayer1Turn);
			if( whoseTurn == FIELD_PLAYER2 )
				SetWindowText(hMainWindow,strPlayer2Turn);
			break;
		}
	}
	InvalidateRect(hMainWindow,NULL,FALSE);
	UpdateWindow(hMainWindow);
}

BOOL CALLBACK NetPromptDialogFunc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_COMMAND:
		{
			if( LOWORD(wParam) == DLG_BUTTON )
			{
				GetDlgItemText(hWnd,DLG_EDIT,ina,256);
				EndDialog(hWnd,0);
				return TRUE;
			}
			return FALSE;
		}
	}
	return FALSE;
}

LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc, hdcCross, hdcCircle, hdcEmpty;
			HBITMAP hbmOldCross, hbmOldCircle, hbmOldEmpty;
			hdc = BeginPaint(hWnd,&ps);
			hdcCross = CreateCompatibleDC(hdc);
			hdcCircle = CreateCompatibleDC(hdc);
			hdcEmpty = CreateCompatibleDC(hdc);
			hbmOldCross = (HBITMAP)SelectObject(hdcCross,hbmCross);
			hbmOldCircle = (HBITMAP)SelectObject(hdcCircle,hbmCircle);
			hbmOldEmpty = (HBITMAP)SelectObject(hdcEmpty,hbmEmpty);

			// rysowanie pola gry
			for(int i = 0; i < FIELD_WIDTH; ++i)
				for(int j = 0; j < FIELD_HEIGHT; ++j)
				{
					if(field[i][j] == FIELD_PLAYER1)
						BitBlt(hdc,i*32,j*32,32,32,hdcCircle,0,0,SRCCOPY);
					if(field[i][j] == FIELD_PLAYER2)
						BitBlt(hdc,i*32,j*32,32,32,hdcCross,0,0,SRCCOPY);
					if(field[i][j] == FIELD_EMPTY)
						BitBlt(hdc,i*32,j*32,32,32,hdcEmpty,0,0,SRCCOPY);
				}

			SelectObject(hdcCross,hbmOldCross);
			SelectObject(hdcCircle,hbmOldCircle);
			SelectObject(hdcEmpty,hbmOldEmpty);
			EndPaint(hWnd,&ps);
			return 0;
		}
	case WM_LBUTTONDOWN:
		{
			if( !lockField && (((whoseTurn == FIELD_PLAYER1) && (player1_type == PLAYER_HUMAN)) || ((whoseTurn == FIELD_PLAYER2) && (player2_type == PLAYER_HUMAN))) )
			{
				int tempx, tempy;
				tempx = LOWORD(lParam) / 32;
				tempy = HIWORD(lParam) / 32;
				if( MakeMove(tempx,tempy) )
				{
					if( isConnected )
						SendNetworkMoveMessage(tempx, tempy);
					EndTurn();
				}
			}
			return 0;
		}
	case WM_COMMAND:
		{
			switch( LOWORD(wParam) )
			{
			case MENU_NEWGAME:
				{
					NewGame();
					SetWindowText(hWnd,strPlayer1Turn);
					if( isConnected )
					{
						SNETMSG buff;
						memset(&buff,0,sizeof(buff));
						buff.type = NETMSG_NEWG;
						if( send(clientSocket,(char*)&buff,sizeof(buff),0) == SOCKET_ERROR )
						{
							CloseConnection();
							lockField = TRUE;
							SetWindowText(hMainWindow,strNetError);
						}
					}
					InvalidateRect(hWnd,NULL,FALSE);
					UpdateWindow(hWnd);
					break;
				}
			case MENU_QUIT:
				{
					PostQuitMessage(0);
					break;
				}
			case MENU_HVSH:
				{
					CloseConnection();
					CheckMenuItem(hMenu,MENU_HVSH,MF_CHECKED);
					CheckMenuItem(hMenu,MENU_HVSC,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_CVSH,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_HVSN,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_NVSH,MF_UNCHECKED);
					player1_type = PLAYER_HUMAN;
					player2_type = PLAYER_HUMAN;
					SendMessage(hWnd,WM_COMMAND,MENU_NEWGAME,0);
					break;
				}
			case MENU_HVSC:
				{
					CloseConnection();
					CheckMenuItem(hMenu,MENU_HVSH,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_HVSC,MF_CHECKED);
					CheckMenuItem(hMenu,MENU_CVSH,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_HVSN,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_NVSH,MF_UNCHECKED);
					player1_type = PLAYER_HUMAN;
					player2_type = PLAYER_COMPUTER;
					SendMessage(hWnd,WM_COMMAND,MENU_NEWGAME,0);
					break;
				}
			case MENU_CVSH:
				{
					CloseConnection();
					CheckMenuItem(hMenu,MENU_HVSH,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_HVSC,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_CVSH,MF_CHECKED);
					CheckMenuItem(hMenu,MENU_HVSN,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_NVSH,MF_UNCHECKED);
					player1_type = PLAYER_COMPUTER;
					player2_type = PLAYER_HUMAN;
					SendMessage(hWnd,WM_COMMAND,MENU_NEWGAME,0);
					break;
				}
			case MENU_HVSN:
				{
					CloseConnection();
					CheckMenuItem(hMenu,MENU_HVSH,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_HVSC,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_CVSH,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_HVSN,MF_CHECKED);
					CheckMenuItem(hMenu,MENU_NVSH,MF_UNCHECKED);
					player1_type = PLAYER_HUMAN;
					player2_type = PLAYER_NETWORK;
					NewGame();
					lockField = TRUE;
					SetWindowText(hWnd,strWaitPeer);
					WaitForPeer();
					InvalidateRect(hWnd,NULL,FALSE);
					UpdateWindow(hWnd);
					break;
				}
			case MENU_NVSH:
				{
					CloseConnection();
					CheckMenuItem(hMenu,MENU_HVSH,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_HVSC,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_CVSH,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_HVSN,MF_UNCHECKED);
					CheckMenuItem(hMenu,MENU_NVSH,MF_CHECKED);
					player1_type = PLAYER_NETWORK;
					player2_type = PLAYER_HUMAN;
					NewGame();
					lockField = TRUE;
					SetWindowText(hWnd,strConnect);
					ConnectPeer();
					if( !isConnected )
					{
						NetworkError();
					}
					else
					{
						lockField = FALSE;
						SetWindowText(hMainWindow,strPlayer1Turn);
					}
					InvalidateRect(hWnd,NULL,FALSE);
					UpdateWindow(hWnd);
					break;
				}
			}
			return 0;
		}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}