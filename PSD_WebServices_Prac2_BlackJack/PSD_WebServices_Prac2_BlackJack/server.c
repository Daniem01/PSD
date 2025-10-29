#include "server.h"

/** Shared array that contains all the games. */
tGame games[MAX_GAMES];

void initGame(tGame *game)
{

	// Init players' name
	memset(game->player1Name, 0, STRING_LENGTH);
	memset(game->player2Name, 0, STRING_LENGTH);

	// Alloc memory for the decks
	clearDeck(&(game->player1Deck));
	clearDeck(&(game->player2Deck));
	initDeck(&(game->gameDeck));

	// Bet and stack
	game->player1Bet = 0;
	game->player2Bet = 0;
	game->player1Stack = INITIAL_STACK;
	game->player2Stack = INITIAL_STACK;

	// Game status variables
	game->endOfGame = FALSE;
	game->status = gameEmpty;

	// Threads
	pthread_mutex_init(&game->mutex, NULL);
	pthread_cond_init(&game->cond, NULL);
}

void initServerStructures(struct soap *soap)
{

	if (DEBUG_SERVER)
		printf("Initializing structures...\n");

	// Init seed
	srand(time(NULL));

	// Init each game (alloc memory and init)
	for (int i = 0; i < MAX_GAMES; i++)
	{
		games[i].player1Name = (xsd__string)soap_malloc(soap, STRING_LENGTH);
		games[i].player2Name = (xsd__string)soap_malloc(soap, STRING_LENGTH);
		allocDeck(soap, &(games[i].player1Deck));
		allocDeck(soap, &(games[i].player2Deck));
		allocDeck(soap, &(games[i].gameDeck));
		initGame(&(games[i]));
	}
}

void initDeck(blackJackns__tDeck *deck)
{

	deck->__size = DECK_SIZE;

	for (int i = 0; i < DECK_SIZE; i++)
		deck->cards[i] = i;
}

void clearDeck(blackJackns__tDeck *deck)
{

	// Set number of cards
	deck->__size = 0;

	for (int i = 0; i < DECK_SIZE; i++)
		deck->cards[i] = UNSET_CARD;
}

tPlayer calculateNextPlayer(tPlayer currentPlayer)
{
	return ((currentPlayer == player1) ? player2 : player1);
}

unsigned int getRandomCard(blackJackns__tDeck *deck)
{

	unsigned int card, cardIndex, i;

	// Get a random card
	cardIndex = rand() % deck->__size;
	card = deck->cards[cardIndex];

	// Remove the gap
	for (i = cardIndex; i < deck->__size - 1; i++)
		deck->cards[i] = deck->cards[i + 1];

	// Update the number of cards in the deck
	deck->__size--;
	deck->cards[deck->__size] = UNSET_CARD;

	return card;
}

unsigned int calculatePoints(blackJackns__tDeck *deck)
{

	unsigned int points = 0;

	for (int i = 0; i < deck->__size; i++)
	{

		if (deck->cards[i] % SUIT_SIZE < 9)
			points += (deck->cards[i] % SUIT_SIZE) + 1;
		else
			points += FIGURE_VALUE;
	}

	return points;
}

void copyGameStatusStructure(blackJackns__tBlock *status, char *message, blackJackns__tDeck *newDeck, int newCode)
{

	// Copy the message
	memset((status->msgStruct).msg, 0, STRING_LENGTH);
	strcpy((status->msgStruct).msg, message);
	(status->msgStruct).__size = strlen((status->msgStruct).msg);

	// Copy the deck, only if it is not NULL
	if (newDeck->__size > 0)
		memcpy((status->deck).cards, newDeck->cards, DECK_SIZE * sizeof(unsigned int));
	else
		(status->deck).cards = NULL;

	(status->deck).__size = newDeck->__size;

	// Set the new code
	status->code = newCode;
}

int blackJackns__register(struct soap *soap, blackJackns__tMessage playerName, int *result)
{

	playerName.msg[playerName.__size] = 0;

	if (DEBUG_SERVER)
		printf("[Register] Registering new player -> [%s]\n", playerName.msg);

	// Busca una room en la que haya hueco
	for (int i = 0; i < MAX_GAMES; i++)
	{
		pthread_mutex_lock(&games[i].mutex);

		if (games[i].status == gameEmpty)
		{
			strncpy(games[i].player1Name, playerName.msg, STRING_LENGTH - 1);
			games[i].status = gameWaitingPlayer;
			*result = i;
			pthread_cond_wait(&games[i].cond, &games[i].mutex);
			pthread_mutex_unlock(&games[i].mutex);
			return SOAP_OK;
		}

		if (games[i].status == gameWaitingPlayer)
		{
			strncpy(games[i].player2Name, playerName.msg, STRING_LENGTH - 1);
			games[i].status = gameReady;
			*result = i;
			games[i].currentPlayer = (rand() % 2 == 0) ? player1 : player2;
			pthread_cond_signal(&games[i].cond);
			pthread_mutex_unlock(&games[i].mutex);
			return SOAP_OK;
		}

		pthread_mutex_unlock(&games[i].mutex);
	}

	*result = -1;
	return SOAP_OK;
}

int blackJackns__betInfo(struct soap *soap, blackJackns__tMessage playerName, int gameId, int *result)
{

	unsigned int stack;
	playerName.msg[playerName.__size] = 0;
	//Debug por fallo: Despues del register no pasa nada
    printf("[Server] betInfo called | Player: %s | Game ID: %d\n", playerName.msg, gameId);

	if (DEBUG_SERVER)
	{
		printf("[Bet] Entering betInfo() | Player: %s | Game ID: %d\n", playerName.msg, gameId);
	}

	// Mutex
	pthread_mutex_lock(&games[gameId].mutex);

	// Conseguimos el stack del jugador que toque
	if (strcmp(playerName.msg, games[gameId].player1Name) == 0)
	{
		stack = games[gameId].player1Stack;
	}
	else if (strcmp(playerName.msg, games[gameId].player2Name) == 0)
	{
		stack = games[gameId].player2Stack;
	}
	// El jugador no concuerda con ninguno de los 2.
	else
	{
		printf("Error: Player name doesn't match: %s \n", playerName.msg);
		pthread_mutex_unlock(&games[gameId].mutex);
		*result = -1;
		return SOAP_OK;
	}

	// Cerramos y devolvemos
	pthread_mutex_unlock(&games[gameId].mutex);

	if (DEBUG_SERVER)
	{
		printf("[Bet] Exiting betInfo() | Stack: %u | Default bet: %u\n", stack, DEFAULT_BET);
	}

	*result = stack;
	return SOAP_OK;
}

int main(int argc, char **argv)
{

	struct soap soap;
	struct soap *tsoap;
	pthread_t tid;
	int port;
	SOAP_SOCKET m, s;

	if (argc != 2)
	{
		printf("Usage: %s port\n", argv[0]);
		exit(0);
	}

	port = atoi(argv[1]);

	soap_init(&soap);
	initServerStructures(&soap);

	m = soap_bind(&soap, NULL, port, MAX_GAMES);
	if (!soap_valid_socket(m))
	{
		printf("Error doing bind \n");
		exit(1);
	}
	printf("Server is ON \n");

	while (1)
	{
		s = soap_accept(&soap);
		if (!soap_valid_socket(s))
		{
			if (soap.errnum)
			{
				soap_print_fault(&soap, stderr);
				exit(1);
			}
			printf("Connection interrupted. \n");
			break;
		}

		printf("Connection accepted. \n");
		tsoap = soap_copy(&soap);
		pthread_create(&tid, NULL, (void *(*)(void *))soap_serve, (void *)tsoap);
		pthread_detach(tid);
	}

	soap_done(&soap);
	return 0;
}