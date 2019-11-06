#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

#define NUM_PLAYERS 3
#define NUM_CARDS 52


// Thread Global variables:
pthread_mutex_t mutex_useDeck = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_dealerExit = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_win1 = PTHREAD_COND_INITIALIZER;
pthread_t playerThread[NUM_PLAYERS];
pthread_t dealerThread;

// Other Globals:
FILE * fileIO;
int roundNumber = 1;
int numberOfRounds = 3;
int turn = 0;

// a card deck for shuffling and dealing
int deck[NUM_CARDS];

//top and bottom of deck
int *topCard;
int *bottomCard;

// the seed for rand() passed from the command line
int seed = 0;

// flag to indicate a player has won
bool win = false;

//hand container for each hand
struct hand{
   int card1, card2;
};

// hands for the players
struct hand player1, player2, player3;

// function prototypes
void *dealer_thread(void *arg);
void *player_thread(void *playerId);
void useTheDeck(long, struct hand);
void randSeed();
void buildDeck();
void shuffleDeck();
void dealCards();
void playRound();
void parseArgs(char *[]);
void printDeck();

int main(int argc, char *argv[]){
   // open the log file
   fileIO = fopen("log.txt", "a");

   //parse console arguments
   srand(atoi(argv[1]));

   // builds the deck
   buildDeck();

   // launch a roundNumber
   // inc the roundNumber counter to next roundNumber
   // reset the win flag
   while( roundNumber <= numberOfRounds ){
      playRound();
      roundNumber++;
      win = false;
   }

   // close the log file
   fclose(fileIO);

   //close program
   exit(EXIT_SUCCESS);
}

// builds a deck of 4 suits with 13 cards each
void buildDeck(){
   int cardVal = 0, card = 0, i;
   while( (card < NUM_CARDS) && (cardVal < (NUM_CARDS/4)) ){
      for(i = 0; i < 4; i++ ){
         deck[card] = cardVal;
         card++;
      }
      cardVal++;
   }
   // point to top and bottom of deck
   topCard = deck;
   bottomCard = deck + (NUM_CARDS-1);
}

// launch dealer and player threads for the current roundNumber
void playRound(){
   printf("ROUND: %d _ _ _ _ _ _ _ _ _\n", roundNumber);
   fprintf(fileIO, "ROUND: %d _ _ _ _ _ _ _ _ _\n", roundNumber);

   // create dealer thread
   int retD = pthread_create(&dealerThread, NULL, dealer_thread, NULL);

   // create player threads
   int retP;
   long i;
   for(i = 1; i <= NUM_PLAYERS; i++ ){
      retP = pthread_create(&playerThread[i], NULL, player_thread, (void *)i);
   }

   int j;
   // join threads so that function waits until all threads complete
   pthread_join(dealerThread, NULL);
   for( j = 0; j < 3; j++ ){
      pthread_join(playerThread[j], NULL);
   }
}

// this function is for the dealer thread
void *dealer_thread(void *arg){
   // identify the dealer as player 0
   long pId = 0;

   // set/reset turn val to indicate it's the dealer's turn
   turn = 0;

   // dealer gets a NULL hand
   struct hand dealerHand;
   useTheDeck(pId, dealerHand);

   // leave the dealer thread and lock the exit
   pthread_mutex_lock(&mutex_dealerExit);
      while( !win ){
            pthread_cond_wait(&cond_win1, &mutex_dealerExit);
      }

   //unlock the exit
   pthread_mutex_unlock(&mutex_dealerExit);

   //write to file
   fprintf(fileIO, "DEALER: exits round: \n");
   pthread_exit(NULL);
}

// this function is for player's threads
void *player_thread(void *playerId){
   long pId = (long)playerId;

   // assign hands to players based on which roundNumber is being played
   struct hand thisHand;
   if( roundNumber == 1 ){
      if( pId == 1 )
        thisHand = player1;
      else if( pId == 2 )
        thisHand = player2;
      else
        thisHand = player3;
   }
   else if( roundNumber == 2 ){
      if( pId == 2 )
        thisHand = player1;
      else if( pId == 3 )
        thisHand = player2;
      else
        thisHand = player3;
   }
   else if( roundNumber == 3 ){
      if( pId == 3 )
        thisHand = player1;
      else if( pId == 1 )
        thisHand = player2;
      else
        thisHand = player3;
   }

   while( win == 0 ){
	  // lock the deck
      pthread_mutex_lock(&mutex_useDeck);

		 // make players wait their respective turn
         while( pId != turn && win == 0 ){
            pthread_cond_wait(&condition_var, &mutex_useDeck);
         }
      if( win == 0 )
          useTheDeck(pId, thisHand);

	  // unlock the deck
      pthread_mutex_unlock(&mutex_useDeck);
   }
   // leave the player thread
   fprintf(fileIO, "PLAYER %ld: exits round\n", pId);
   pthread_exit(NULL);
}


void useTheDeck(long pId, struct hand thisHand){
   //this is for the dealer
   if( pId == 0 ){
      // shuffles the deck
	  fprintf(fileIO, "DEALER: shuffle\n");
	  shuffleDeck();

	  // deal the cards
      fprintf(fileIO, "DEALER: deal\n");
	  dealCards();
   }
   //else is for the player
   else{
      // show hand
      fprintf(fileIO, "PLAYER %ld: hand %d \n", pId, thisHand.card1);

      // draw a card
      thisHand.card2 = *topCard,
      topCard = topCard + 1;
      fprintf(fileIO, "PLAYER %ld: draws %d \n", pId,thisHand.card2);

      // show hand
      printf("HAND %d %d\n", thisHand.card1, thisHand.card2);
      fprintf(fileIO, "PLAYER %ld: hand %d %d\n", pId, thisHand.card1, thisHand.card2);

      // compare the cards
      if( thisHand.card1 == thisHand.card2 ){
         // if the cards match, then declare a winner
         printf("WINNER!!\n");
         fprintf(fileIO, "PLAYER %ld: wins\n\n", pId);
         win = true;

		 // signal dealer to exit
         pthread_cond_signal(&cond_win1);
      }
      else{
         // if the cards don't match, then discard a card
         printf("No Win/No Match\n");
         // shift cards in deck to make room for discard
         topCard = topCard - 1;
         int *ptr = topCard;
         while( ptr != bottomCard ){
            *ptr = *(ptr + 1);
            ptr = ptr + 1;
         }
         // randomly select discard and put it back in the deck
         int discard = rand()%2;
         if( discard == 0 ){
            fprintf(fileIO, "PLAYER %ld: discards %d \n", pId, thisHand.card1);

			// puts card back in deck
			*bottomCard = thisHand.card1;

			// sets card1 to remaining card value
            thisHand.card1 = thisHand.card2;
         }
         else{
            fprintf(fileIO, "PLAYER %ld: discards %d \n", pId, thisHand.card2);
            *bottomCard = thisHand.card2;
         }
         // print the contents of the deck
         printDeck();
      }
   }
   //increment turn for the next player
   turn ++;

   // if player3 went, move to player1
   if( turn > NUM_PLAYERS ) turn = 1;

   // broadcast that the deck is available
   pthread_cond_broadcast(&condition_var);
}

// dealer swaps current card w/ rand card until all are shuffled
void shuffleDeck(){
   int i;
   for(i = 0; i < (NUM_CARDS - 1); i++ ){
      int randPos = i + (rand() % (NUM_CARDS - i));
      int temp = deck[i];
      deck[i] = deck[randPos];
      deck[randPos] = temp;
   }
}

// the dealer deals one card to each player
void dealCards(){
   //player 1
   player1.card1 = *topCard; topCard = topCard + 1;

   //player 2
   player2.card1 = *topCard; topCard = topCard + 1;

   //player 3
   player3.card1 = *topCard; topCard = topCard + 1;
}

// prints deck to console and log
void printDeck(){
   printf("DECK: ");
   fprintf(fileIO, "DECK: ");
   int *ptr = topCard;
   while( ptr != bottomCard ){
      printf("%d ", *ptr);
      fprintf(fileIO, "%d ", *ptr);
      ptr++;
      if( ptr == bottomCard ){
         printf("%d \n", *ptr);
         fprintf(fileIO, "%d \n", *ptr);
      }
   }
}
