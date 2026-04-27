#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/game.h"

static const char *rank_to_text(int rank) {
    switch (rank) {
        case 1:
            return "A";
        case 11:
            return "J";
        case 12:
            return "Q";
        case 13:
            return "K";
        default: {
            static char buf[4][3];
            static int slot = 0;
            slot = (slot + 1) % 4;
            snprintf(buf[slot], sizeof(buf[slot]), "%d", rank);
            return buf[slot];
        }
    }
}

static const char *suit_to_text(Suit suit) {
    switch (suit) {
        case HEARTS:
            return "Hearts";
        case DIAMONDS:
            return "Diamonds";
        case CLUBS:
            return "Clubs";
        case SPADES:
            return "Spades";
        default:
            return "Unknown";
    }
}

static const char *card_to_text(Card card) {
    static char buf[8][24];
    static int slot = 0;
    slot = (slot + 1) % 8;
    snprintf(buf[slot], sizeof(buf[slot]), "%s of %s", rank_to_text(card.rank), suit_to_text(card.suit));
    return buf[slot];
}

static void log_line(GameState *game, const char *fmt, ...) {
    pthread_mutex_lock(&game->log_mutex);
    va_list args;
    va_start(args, fmt);
    vfprintf(game->log_file, fmt, args);
    va_end(args);
    fputc('\n', game->log_file);
    fflush(game->log_file);
    pthread_mutex_unlock(&game->log_mutex);
}

static void print_round_result(int player_id, int round, bool won) {
    printf("Player %d %s round %d\n", player_id, won ? "won" : "lost", round);
    fflush(stdout);
}

static int next_non_dealer_player(GameState *game, int start_id) {
    int candidate = start_id;
    for (int i = 0; i < game->n_players; i++) {
        candidate++;
        if (candidate > game->n_players) {
            candidate = 1;
        }
        if (candidate != game->dealer_id) {
            return candidate;
        }
    }
    return game->dealer_id;
}

static Card draw_from_deck(GameState *game) {
    if (game->deck_size <= 0) {
        Card fallback = {1, HEARTS};
        return fallback;
    }
    Card card = game->deck[0];
    for (int i = 1; i < game->deck_size; i++) {
        game->deck[i - 1] = game->deck[i];
    }
    game->deck_size--;
    return card;
}

static void append_to_deck(GameState *game, Card card) {
    if (game->deck_size < DECK_SIZE) {
        game->deck[game->deck_size++] = card;
    }
}

static void log_deck(GameState *game) {
    char line[512];
    int offset = snprintf(line, sizeof(line), "DECK:");
    for (int i = 0; i < game->deck_size && offset < (int)sizeof(line) - 4; i++) {
        offset += snprintf(line + offset, sizeof(line) - (size_t)offset, " %s", card_to_text(game->deck[i]));
    }
    log_line(game, "%s", line);
}

static void shuffle_deck(Card *deck, int size, unsigned int *rng_state) {
    for (int i = size - 1; i > 0; i--) {
        int j = (int)(rand_r(rng_state) % (unsigned int)(i + 1));
        Card tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }
}

static void setup_round_locked(GameState *game, PlayerState *players, PlayerState *dealer) {
    for (int i = 0; i < DECK_SIZE; i++) {
        game->deck[i].rank = (i % RANKS) + 1;
        game->deck[i].suit = (Suit)(i / RANKS);
    }
    game->deck_size = DECK_SIZE;
    shuffle_deck(game->deck, game->deck_size, &dealer->rng_state);

    game->greasy_card = game->deck[(int)(rand_r(&dealer->rng_state) % (unsigned int)game->deck_size)];
    log_line(game, "PLAYER %d: Greasy card is %s", dealer->id, card_to_text(game->greasy_card));

    for (int i = 0; i < game->n_players; i++) {
        players[i].hand_size = 1;
        players[i].hand[0] = draw_from_deck(game);
        log_line(game, "PLAYER %d: hand %s", players[i].id, card_to_text(players[i].hand[0]));
    }

    game->chips_left = game->chips_per_bag;
    log_line(game, "BAG: %d Chips left", game->chips_left);
    log_deck(game);

    game->winner_id = 0;
    game->round_complete = false;
    game->results_reported_count = 0;
    game->current_turn_id = next_non_dealer_player(game, game->dealer_id);
    game->round_setup_done = true;
}

static void consume_chips_locked(GameState *game, PlayerState *player) {
    int to_eat = (int)(rand_r(&player->rng_state) % 5U) + 1;
    if (game->chips_left < to_eat) {
        game->chips_left = game->chips_per_bag;
        log_line(game, "PLAYER %d: opens a new bag", player->id);
        log_line(game, "BAG: %d Chips left", game->chips_left);
    }
    game->chips_left -= to_eat;
    log_line(game, "PLAYER %d: eats %d chips", player->id, to_eat);
    log_line(game, "BAG: %d Chips left", game->chips_left);
}

static void do_turn_locked(GameState *game, PlayerState *player) {
    Card drawn = draw_from_deck(game);
    player->hand[1] = drawn;
    player->hand_size = 2;
    log_line(game, "PLAYER %d: draws %s", player->id, card_to_text(drawn));

    bool first_matches = (player->hand[0].rank == game->greasy_card.rank &&
                          player->hand[0].suit == game->greasy_card.suit);
    bool second_matches = (player->hand[1].rank == game->greasy_card.rank &&
                           player->hand[1].suit == game->greasy_card.suit);
    if (first_matches || second_matches) {
        log_line(
            game,
            "PLAYER %d: hand (%s,%s) <> Greasy card is %s",
            player->id,
            card_to_text(player->hand[0]),
            card_to_text(player->hand[1]),
            card_to_text(game->greasy_card)
        );
        log_line(game, "PLAYER %d: wins round %d", player->id, game->current_round);
        game->winner_id = player->id;
        game->round_complete = true;
        return;
    }

    int discard_index = (int)(rand_r(&player->rng_state) % 2U);
    Card kept_card = player->hand[1 - discard_index];
    Card discarded = player->hand[discard_index];
    append_to_deck(game, discarded);
    player->hand[0] = kept_card;
    player->hand_size = 1;

    log_line(game, "PLAYER %d: discards %s at random", player->id, card_to_text(discarded));
    log_line(game, "PLAYER %d: hand %s", player->id, card_to_text(player->hand[0]));
    log_deck(game);

    consume_chips_locked(game, player);

    game->current_turn_id = next_non_dealer_player(game, player->id);
}

int init_game(GameState *game, int seed, int n_players, int chips_per_bag, const char *log_path) {
    memset(game, 0, sizeof(*game));
    game->seed = seed;
    game->n_players = n_players;
    game->total_rounds = n_players;
    game->chips_per_bag = chips_per_bag;
    game->current_round = 1;
    game->dealer_id = 1;
    game->current_turn_id = 2;

    if (pthread_mutex_init(&game->state_mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_mutex_init(&game->log_mutex, NULL) != 0) {
        pthread_mutex_destroy(&game->state_mutex);
        return -1;
    }
    if (pthread_cond_init(&game->turn_cv, NULL) != 0) {
        pthread_mutex_destroy(&game->state_mutex);
        pthread_mutex_destroy(&game->log_mutex);
        return -1;
    }

    game->log_file = fopen(log_path, "w");
    if (game->log_file == NULL) {
        pthread_cond_destroy(&game->turn_cv);
        pthread_mutex_destroy(&game->state_mutex);
        pthread_mutex_destroy(&game->log_mutex);
        return -1;
    }

    return 0;
}

void destroy_game(GameState *game) {
    if (game->log_file != NULL) {
        fclose(game->log_file);
    }
    pthread_cond_destroy(&game->turn_cv);
    pthread_mutex_destroy(&game->state_mutex);
    pthread_mutex_destroy(&game->log_mutex);
}

void *player_thread(void *arg) {
    ThreadArgs *thread_args = (ThreadArgs *)arg;
    GameState *game = thread_args->game;
    PlayerState *self = thread_args->player;

    pthread_mutex_lock(&game->state_mutex);
    while (!game->game_done) {
        if (game->current_round > game->total_rounds) {
            game->game_done = true;
            pthread_cond_broadcast(&game->turn_cv);
            break;
        }

        if (self->id == game->dealer_id && !game->round_setup_done) {
            setup_round_locked(game, game->players, self);
            pthread_cond_broadcast(&game->turn_cv);
        }

        if (game->round_complete && self->last_reported_round < game->current_round) {
            int round = game->current_round;
            bool won = (game->winner_id == self->id);
            self->last_reported_round = round;
            game->results_reported_count++;

            log_line(game, "PLAYER %d: %s round %d", self->id, won ? "won" : "lost", round);
            pthread_mutex_unlock(&game->state_mutex);
            print_round_result(self->id, round, won);
            pthread_mutex_lock(&game->state_mutex);

            if (game->results_reported_count >= game->n_players) {
                pthread_cond_broadcast(&game->turn_cv);
            }
            continue;
        }

        if (self->id == game->dealer_id && game->round_complete &&
            game->results_reported_count >= game->n_players) {
            log_line(game, "PLAYER %d: Round ends", self->id);
            game->current_round++;
            if (game->current_round > game->total_rounds) {
                game->game_done = true;
                pthread_cond_broadcast(&game->turn_cv);
                break;
            }

            game->dealer_id = game->current_round;
            game->round_setup_done = false;
            game->round_complete = false;
            game->winner_id = 0;
            game->results_reported_count = 0;
            game->current_turn_id = next_non_dealer_player(game, game->dealer_id);
            pthread_cond_broadcast(&game->turn_cv);
            continue;
        }

        if (!game->round_complete && game->round_setup_done &&
            self->id == game->current_turn_id && self->id != game->dealer_id) {
            do_turn_locked(game, self);
            pthread_cond_broadcast(&game->turn_cv);
            continue;
        }

        pthread_cond_wait(&game->turn_cv, &game->state_mutex);
    }
    pthread_mutex_unlock(&game->state_mutex);

    return NULL;
}
