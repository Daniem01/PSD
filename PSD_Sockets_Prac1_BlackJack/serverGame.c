#include "serverGame.h"
#include <pthread.h>

tPlayer getNextPlayer(tPlayer currentPlayer)
{

	tPlayer next;

	if (currentPlayer == player1)
		next = player2;
	else
		next = player1;

	return next;
}

void initDeck(tDeck *deck)
{

	deck->numCards = DECK_SIZE;

	for (int i = 0; i < DECK_SIZE; i++)
	{
		deck->cards[i] = i;
	}
}

void clearDeck(tDeck *deck)
{

	// Set number of cards
	deck->numCards = 0;

	for (int i = 0; i < DECK_SIZE; i++)
	{
		deck->cards[i] = UNSET_CARD;
	}
}

void printSession(tSession *session)
{

	printf("\n ------ Session state ------\n");

	// Player 1
	printf("%s [bet:%d; %d chips] Deck:", session->player1Name, session->player1Bet, session->player1Stack);
	printDeck(&(session->player1Deck));

	// Player 2
	printf("%s [bet:%d; %d chips] Deck:", session->player2Name, session->player2Bet, session->player2Stack);
	printDeck(&(session->player2Deck));

	// Current game deck
	if (DEBUG_PRINT_GAMEDECK)
	{
		printf("Game deck: ");
		printDeck(&(session->gameDeck));
	}
}

void initSession(tSession *session)
{

	clearDeck(&(session->player1Deck));
	session->player1Bet = 0;
	session->player1Stack = INITIAL_STACK;

	clearDeck(&(session->player2Deck));
	session->player2Bet = 0;
	session->player2Stack = INITIAL_STACK;

	initDeck(&(session->gameDeck));
	session->currentPlayer = player1;
}

unsigned int calculatePoints(tDeck *deck)
{

	unsigned int points;

	// Init...
	points = 0;

	for (int i = 0; i < deck->numCards; i++)
	{

		if (deck->cards[i] % SUIT_SIZE < 9)
			points += (deck->cards[i] % SUIT_SIZE) + 1;
		else
			points += FIGURE_VALUE;
	}

	return points;
}

unsigned int getRandomCard(tDeck *deck)
{

	unsigned int card, cardIndex, i;

	// Get a random card
	cardIndex = rand() % deck->numCards;
	card = deck->cards[cardIndex];

	// Remove the gap
	for (i = cardIndex; i < deck->numCards - 1; i++)
		deck->cards[i] = deck->cards[i + 1];

	// Update the number of cards in the deck
	deck->numCards--;
	deck->cards[deck->numCards] = UNSET_CARD;

	return card;
}

unsigned int askBet(int socket, unsigned int stack)
{
	unsigned int bet = 0;
	unsigned int code = TURN_BET;
	int bytes;
	int betValid = 0;
	while (!betValid)
	{
		// Send TURN_BET
		bytes = send(socket, &code, sizeof(unsigned int), 0);
		printf("Code send \n");
		if (bytes < 0)
			printf("ERROR sending code.\n");

		bytes = send(socket, &stack, sizeof(unsigned int), 0);
		printf("Stack send \n");
		if (bytes < 0)
			printf("ERROR sending stack.\n");

		// Recived bet
		bytes = recv(socket, &bet, sizeof(unsigned int), 0);
		printf("Recive bet \n");
		if (bytes < 0)
			printf("ERROR receiving bet.\n");

		// Validation
		if (bet > 0 && bet <= MAX_BET && bet <= stack)
		{
			printf("Bet is ok \n");
			code = TURN_BET_OK;
			betValid = 1;
		}

		bytes = send(socket, &code, sizeof(unsigned int), 0);
		if (bytes < 0)
		{
			printf("ERROR sending code.\n");
		}
	}

	return bet;
}

void getNewCard(tDeck *deck, tSession *session)
{
	deck->cards[deck->numCards] = getRandomCard(&session->gameDeck);
	deck->numCards++;
}

int main(int argc, char *argv[])
{

	int socketfd;					   /** Socket descriptor */
	struct sockaddr_in serverAddress;  /** Server address structure */
	unsigned int port;				   /** Listening port */
	struct sockaddr_in player1Address; /** Client address structure for player 1 */
	struct sockaddr_in player2Address; /** Client address structure for player 2 */
	int socketPlayer1;				   /** Socket descriptor for player 1 */
	int socketPlayer2;				   /** Socket descriptor for player 2 */
	unsigned int clientLength;		   /** Length of client structure */
	tThreadArgs *threadArgs;		   /** Thread parameters */
	pthread_t threadID;				   /** Thread ID */
	tString message;				   // el mensaje q se va a transmitir
	int bytes;						   // para comprobar que se transmiten bien las cosas
	tSession gameSession;
	unsigned int gameOver = 0;

	// Seed
	srand(time(0));
	initSession(&gameSession);
	// Check arguments
	if (argc != 2)
	{
		fprintf(stderr, "ERROR wrong number of arguments\n");
		fprintf(stderr, "Usage:\n$>%s port\n", argv[0]);
		exit(1);
	}

	// Create the socket
	socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check
	if (socketfd < 0)
		showError("ERROR while opening socket");

	// Init server structure
	memset(&serverAddress, 0, sizeof(serverAddress));

	// Get listening port
	port = atoi(argv[1]);

	// Fill server structure
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(port);

	// Bind
	if (bind(socketfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
		showError("ERROR while binding");

	// Listen
	listen(socketfd, 10);

	// Get length of client structure
	clientLength = sizeof(player1Address);

	// Accept!
	socketPlayer1 = accept(socketfd, (struct sockaddr *)&player1Address, &clientLength);
	socketPlayer2 = accept(socketfd, (struct sockaddr *)&player2Address, &clientLength);

	// Check accept result
	if (socketPlayer1 < 0 || socketPlayer2 < 0)
		showError("ERROR while accepting");

	// Player 1 escribe
	// Init and read message
	memset(message, 0, STRING_LENGTH); // Setea todo a 0 para que no haya basura
	bytes = recv(socketPlayer1, message, STRING_LENGTH - 1, 0);

	// Check read bytes
	if (bytes < 0)
	{
		showError("ERROR while reading from socket");
	}

	// Save in gameSession Player1's name
	strcpy(gameSession.player1Name, message);

	// Show ando cartas2ERROmessage
	printf("Player 1: %s\n", message);

	// Get the message length
	memset(message, 0, STRING_LENGTH);
	strcpy(message, "Welcome to BlackJack online");
	bytes = send(socketPlayer1, message, strlen(message), 0);

	// Check bytes sent
	if (bytes < 0)
		showError("ERROR while writing to socket");

	// Player 2 escribe
	// Init and read message
	memset(message, 0, STRING_LENGTH);
	bytes = recv(socketPlayer2, message, STRING_LENGTH - 1, 0);

	// Check read bytes
	if (bytes < 0)
	{
		showError("ERROR while reading from socket");
	}

	// Save in gameSession Player2's name
	strcpy(gameSession.player2Name, message);

	// Show message
	printf("Player 2: %s\n", message);

	// Get the message length
	memset(message, 0, STRING_LENGTH);
	strcpy(message, "Welcome to BlackJack online");
	bytes = send(socketPlayer2, message, strlen(message), 0);

	// Check bytes sent
	if (bytes < 0)
		showError("ERROR while writing to socket");

	// Deal the cards and send it to the players
	getNewCard(&gameSession.player1Deck, &gameSession);
	getNewCard(&gameSession.player1Deck, &gameSession);
	getNewCard(&gameSession.player2Deck, &gameSession);
	getNewCard(&gameSession.player2Deck, &gameSession);

	bytes = send(socketPlayer1, &gameSession.player1Deck, sizeof(tDeck), 0);
	if (bytes < 0)
	{
		printf("ERROR while sending deck");
	}
	bytes = send(socketPlayer2, &gameSession.player2Deck, sizeof(tDeck), 0);
	if (bytes < 0)
	{
		printf("ERROR while sending deck");
	}

	// Starts the game
	while (!gameOver)
	{
		int currentSocket, passiveSocket;
		unsigned int currentBet, currentStack;
		tDeck currentDeck;

		// Select socket and stack
		if (gameSession.currentPlayer == player1)
		{
			currentSocket = socketPlayer1;
			currentStack = gameSession.player1Stack;
			passiveSocket = socketPlayer2;
			currentDeck = gameSession.player1Deck;
		}
		else
		{
			currentSocket = socketPlayer2;
			currentStack = gameSession.player2Stack;
			passiveSocket = socketPlayer1;
			currentDeck = gameSession.player2Deck;
		}

		// Does 2 times the bet (fuera con los hilos creo)
		for (int i = 0; i < 2; i++)
		{
			// Bet
			currentBet = askBet(currentSocket, currentStack);
			printf("Player make a bet of %d \n", currentBet);
			// Save the bet
			if (gameSession.currentPlayer == player1)
			{
				gameSession.player1Bet = currentBet;
			}

			else
			{
				gameSession.player2Bet = currentBet;
			}
			// Player change
			gameSession.currentPlayer = getNextPlayer(gameSession.currentPlayer);
			// Select socket and stack
			if (gameSession.currentPlayer == player1)
			{
				currentSocket = socketPlayer1;
				currentStack = gameSession.player1Stack;
				passiveSocket = socketPlayer2;
				currentDeck = gameSession.player1Deck;
			}
			else
			{
				currentSocket = socketPlayer2;
				currentStack = gameSession.player2Stack;
				passiveSocket = socketPlayer1;
				currentDeck = gameSession.player2Deck;
			}
		}

		// Game phase
		for (int i = 0; i < 2; i++)
		{
			if (gameSession.currentPlayer == player1)
			{
				currentSocket = socketPlayer1;
				passiveSocket = socketPlayer2;
				currentDeck = gameSession.player1Deck;
			}
			else
			{
				currentSocket = socketPlayer2;
				passiveSocket = socketPlayer1;
				currentDeck = gameSession.player2Deck;
			}

			makePlay(currentSocket, passiveSocket, &currentDeck, &gameSession);

			// Save
			if (gameSession.currentPlayer == player1)
				gameSession.player1Deck = currentDeck;
			else
				gameSession.player2Deck = currentDeck;

			// Next player
			gameSession.currentPlayer = getNextPlayer(gameSession.currentPlayer);
		}
	}

	prinf("bye bye");
	
	// Close sockets
	close(socketPlayer1);
	close(socketPlayer2);
	close(socketfd);

	return 0;
}

void makePlay(int usedSocket, int otherSocket, tDeck *deck, tSession *session)
{
    unsigned int action = 0;
    unsigned int points = 0;
    unsigned int codeUsed = TURN_PLAY;
    unsigned int codeOther = TURN_PLAY_WAIT;
    int playing = 1;

    while (playing)
    {
        points = calculatePoints(deck);

        // Send info to active player
        send(usedSocket, &codeUsed, sizeof(unsigned int), 0);
        send(usedSocket, &points, sizeof(unsigned int), 0);
        send(usedSocket, deck, sizeof(tDeck), 0);

        // Send info to passive player
        send(otherSocket, &codeOther, sizeof(unsigned int), 0);
        send(otherSocket, &points, sizeof(unsigned int), 0);
        send(otherSocket, deck, sizeof(tDeck), 0);

        // Wait for active player action
        if (recv(usedSocket, &action, sizeof(unsigned int), 0) <= 0)
        {
            printf("Error receiving player action.\n");
            return;
        }

        if (action == TURN_PLAY_HIT)
        {
            getNewCard(deck, session);
            points = calculatePoints(deck);

            if (points > 21)
            {
                // Player bust
                codeUsed = TURN_PLAY_OUT;
                codeOther = TURN_PLAY_RIVAL_DONE;

                // Send final state
                send(usedSocket, &codeUsed, sizeof(unsigned int), 0);
                send(usedSocket, &points, sizeof(unsigned int), 0);
                send(usedSocket, deck, sizeof(tDeck), 0);

                send(otherSocket, &codeOther, sizeof(unsigned int), 0);
                send(otherSocket, &points, sizeof(unsigned int), 0);
                send(otherSocket, deck, sizeof(tDeck), 0);

                playing = 0;
            }
            else
            {
                // Continue turn
                codeUsed = TURN_PLAY;
                codeOther = TURN_PLAY_WAIT;
            }
        }
        else if (action == TURN_PLAY_STAND)
        {
            // Player stands
            codeUsed = TURN_PLAY_WAIT;
            codeOther = TURN_PLAY_RIVAL_DONE;

            send(usedSocket, &codeUsed, sizeof(unsigned int), 0);
            send(usedSocket, &points, sizeof(unsigned int), 0);
            send(usedSocket, deck, sizeof(tDeck), 0);

            send(otherSocket, &codeOther, sizeof(unsigned int), 0);
            send(otherSocket, &points, sizeof(unsigned int), 0);
            send(otherSocket, deck, sizeof(tDeck), 0);

            playing = 0;
        }
        else
        {
            printf("Unknown action received: %u\n", action);
            playing = 0;
        }
    }
}


