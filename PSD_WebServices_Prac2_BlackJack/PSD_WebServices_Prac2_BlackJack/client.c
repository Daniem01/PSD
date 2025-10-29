#include "client.h"

unsigned int readBet()
{

	int isValid, bet = 0;
	xsd__string enteredMove;

	// While player does not enter a correct bet...
	do
	{

		// Init...
		enteredMove = (xsd__string)malloc(STRING_LENGTH);
		bzero(enteredMove, STRING_LENGTH);
		isValid = TRUE;

		printf("Enter a value:");
		fgets(enteredMove, STRING_LENGTH - 1, stdin);
		enteredMove[strlen(enteredMove) - 1] = 0;

		// Check if each character is a digit
		for (int i = 0; i < strlen(enteredMove) && isValid; i++)
			if (!isdigit(enteredMove[i]))
				isValid = FALSE;

		// Entered move is not a number
		if (!isValid)
			printf("Entered value is not correct. It must be a number greater than 0\n");
		else
			bet = atoi(enteredMove);

	} while (!isValid);

	printf("\n");
	free(enteredMove);

	return ((unsigned int)bet);
}

unsigned int readOption()
{

	unsigned int bet;

	do
	{
		printf("What is your move? Press %d to hit a card and %d to stand\n", PLAYER_HIT_CARD, PLAYER_STAND);
		bet = readBet();
		if ((bet != PLAYER_HIT_CARD) && (bet != PLAYER_STAND))
			printf("Wrong option!\n");
	} while ((bet != PLAYER_HIT_CARD) && (bet != PLAYER_STAND));

	return bet;
}

int main(int argc, char **argv)
{

	struct soap soap;				  /** Soap struct */
	char *serverURL;				  /** Server URL */
	blackJackns__tMessage playerName; /** Player name */
	blackJackns__tBlock gameStatus;	  /** Game status */
	unsigned int playerMove;		  /** Player's move */
	int resCode, gameId;			  /** Result and gameId */
	unsigned int gameContinue = 1;	  // Variable para el bucle del juego
	int betInfo;					  // Variable para ver como llega la informacion de la apuesta, da el stack.

	if (argc != 2)
	{
		printf("Usage: %s http://server:port\n", argv[0]);
		return 1;
	}

	serverURL = argv[1];
	soap_init(&soap);

	// Reservamos memoria
	allocClearMessage(&soap, &playerName);
	allocClearBlock(&soap, &gameStatus);

	// Nombre
	printf("Introduce your name: ");
	if (fgets(playerName.msg, STRING_LENGTH, stdin) == NULL)
	{
		fprintf(stderr, "Error reading input.\n");
		soap_end(&soap);
		soap_done(&soap);
		return 1;
	}
	playerName.msg[strcspn(playerName.msg, "\n")] = '\0';
	playerName.__size = strlen(playerName.msg);

	// Lo registramos
	if (soap_call_blackJackns__register(&soap, serverURL, NULL, playerName, &resCode) == SOAP_OK)
	{
		gameId = resCode;
		printf("Registered successfully. Welcome %s, you are in room %d.\n", playerName.msg, gameId);
		printf("--- \n");
		// Pausa para que no salte el betInfo de golpe
		sleep(1);
	}
	else
	{
		soap_print_fault(&soap, stderr);
		soap_end(&soap);
		soap_done(&soap);
		return 1;
	}

	while (gameContinue)
	{
		// Damos info de la apuesta y el stack
		if (soap_call_blackJackns__betInfo(&soap, serverURL, NULL, playerName, gameId, &resCode) == SOAP_OK)
		{
			betInfo = resCode;
			// Comprobamos si la info se mostro bien o no y si es asi le decimos al cliente que la apuesta se hizo.
			if (betInfo != -1)
			{
				// Mensajes que se muestran de informacion al jugador
				printf("There is a default bet: %d \n", 1);
				printf("Your stack is: %d \n", betInfo);
				printf("Your bet is automatically done. \n");
			}
			else
			{
				printf("Theres is a problem doing your bet. \n");
			}
		}
		else
		{
			soap_print_fault(&soap, stderr);
			soap_end(&soap);
			soap_done(&soap);
			return 1;
		}

		gameContinue = 0;
	}
	return 0;
}
