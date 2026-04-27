#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/game.h"

/* Parses a strict positive decimal integer in a safe range; returns the value or -1 after printing an error. */
static int parse_positive_int(const char *value, const char *name) {
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0 || parsed > 1000000) {
        fprintf(stderr, "Invalid %s: %s\n", name, value);
        return -1;
    }
    return (int)parsed;
}

/* CLI entry: validates args, inits the game, spawns one pthread per player with a distinct rand_r seed, joins all threads, and frees resources. */
int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <seed> <num_players> <chips_per_bag>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int seed = parse_positive_int(argv[1], "seed");
    int n_players = parse_positive_int(argv[2], "num_players");
    int chips_per_bag = parse_positive_int(argv[3], "chips_per_bag");
    if (seed < 0 || n_players < 0 || chips_per_bag < 0) {
        return EXIT_FAILURE;
    }
    if (n_players < 2 || n_players > DECK_SIZE - 1) {
        fprintf(stderr, "num_players must be in [2, %d]\n", DECK_SIZE - 1);
        return EXIT_FAILURE;
    }

    GameState game;
    if (init_game(&game, seed, n_players, chips_per_bag, "greasy_cards.log") != 0) {
        fprintf(stderr, "Failed to initialize game state.\n");
        return EXIT_FAILURE;
    }

    pthread_t *threads = calloc((size_t)n_players, sizeof(*threads));
    ThreadArgs *thread_args = calloc((size_t)n_players, sizeof(*thread_args));
    PlayerState *players = calloc((size_t)n_players, sizeof(*players));
    if (threads == NULL || thread_args == NULL || players == NULL) {
        fprintf(stderr, "Allocation failure: %s\n", strerror(errno));
        free(threads);
        free(thread_args);
        free(players);
        destroy_game(&game);
        return EXIT_FAILURE;
    }
    game.players = players;

    for (int i = 0; i < n_players; i++) {
        players[i].id = i + 1;
        players[i].hand_size = 0;
        players[i].rng_state = (unsigned int)(seed + 7919 * (i + 1));
        players[i].last_reported_round = 0;
        thread_args[i].game = &game;
        thread_args[i].player = &players[i];

        if (pthread_create(&threads[i], NULL, player_thread, &thread_args[i]) != 0) {
            fprintf(stderr, "Failed creating thread for player %d\n", i + 1);
            game.game_done = true;
            pthread_mutex_lock(&game.state_mutex);
            pthread_cond_broadcast(&game.turn_cv);
            pthread_mutex_unlock(&game.state_mutex);

            for (int j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            free(threads);
            free(thread_args);
            free(players);
            destroy_game(&game);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < n_players; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(thread_args);
    free(players);
    destroy_game(&game);
    return EXIT_SUCCESS;
}
