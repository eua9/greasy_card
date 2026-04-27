#ifndef GAME_H
#define GAME_H

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

#define DECK_SIZE 52
#define RANKS 13

typedef enum {
    HEARTS = 0,
    DIAMONDS = 1,
    CLUBS = 2,
    SPADES = 3
} Suit;

typedef struct {
    int rank;
    Suit suit;
} Card;

typedef struct {
    int id;
    Card hand[2];
    int hand_size;
    unsigned int rng_state;
    int last_reported_round;
} PlayerState;

typedef struct {
    int seed;
    int n_players;
    int total_rounds;
    int chips_per_bag;

    Card deck[DECK_SIZE];
    int deck_size;
    Card greasy_card;

    int current_round;
    int dealer_id;
    int current_turn_id;
    int winner_id;
    int results_reported_count;

    bool round_setup_done;
    bool round_complete;
    bool game_done;

    int chips_left;
    PlayerState *players;

    FILE *log_file;
    pthread_mutex_t state_mutex;
    pthread_mutex_t log_mutex;
    pthread_cond_t turn_cv;
} GameState;

typedef struct {
    GameState *game;
    PlayerState *player;
} ThreadArgs;

int init_game(GameState *game, int seed, int n_players, int chips_per_bag, const char *log_path);
void destroy_game(GameState *game);
void *player_thread(void *arg);

#endif
