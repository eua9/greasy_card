# Greasy Cards — Project Report (CS4328)

## Design overview

The program models **N concurrent players** as **POSIX threads** that share a single `GameState` protected by a **mutex** and **condition variable**. Synchronization is **monitor-style**: at most one thread mutates the shared game at a time; threads that cannot act next **wait** on the condition; any thread that changes the turn or round **broadcasts** so others re-check their role.

- **Rounds:** There is one full round of play per player (`total_rounds = n_players`). The **dealer** is player 1 in round 1, player 2 in round 2, and so on, up to N.
- **Setup:** The dealer shuffles a standard 52-card deck (using the dealer’s `rand_r` state), chooses a **greasy card** uniformly from the shuffled deck, and deals one card to each player from the top of the deck.
- **Turn order:** All players except the dealer take **one** turn in cyclic order, starting with the first player clockwise after the dealer, until someone’s hand includes the greasy card and wins the round, or the deck is exhausted in edge cases.
- **Non-dealer turn:** Draw a second card; if either hand card matches the greasy card, that player wins. Otherwise, **discard** one of the two cards at random, keep the other, append the discard to the **bottom** of the deck, and **consume 1–5** chips; if the bag is short, open a new **full** bag, then take chips. Play passes to the next **non-dealer** clockwise.
- **Output:** The console prints each player’s win/loss for each round. A detailed text log is written to `greasy_cards.log`.

**Randomness:** Each player thread uses a distinct `unsigned int` seed for `rand_r` (`seed + 7919 * (i+1)`) so draws, discards, and chip choices are **deterministic** for a given command-line `seed` and are independent of scheduling order, while remaining statistically varied across players.

## Implementation overview

- **`init_game` / `destroy_game`:** Set up the log file, `state_mutex`, `log_mutex`, and `turn_cv`, and tear them down in reverse.
- **`player_thread`:** A single loop under `state_mutex` that:
  1. Lets the **dealer** run `setup_round_locked` when a new round needs setup.
  2. Has **each** player print their round result once per round, then the dealer can advance the round when all have reported.
  3. Lets the **active non-dealer** run `do_turn_locked` when it is their `current_turn_id`.
  4. Other threads `pthread_cond_wait` until a broadcast.
- **Helpers in `game.c`:** `shuffle_deck` (Fisher–Yates with `rand_r`), `draw_from_deck` / `append_to_deck`, `consume_chips_locked`, and small helpers to format cards and log lines.
- **CLI (`main.c`):** Parses a positive `seed`, `num_players`, and `chips_per_bag`, validates player count, allocates `pthread_t` and per-player `PlayerState` arrays, creates threads, and joins on exit.

## How to compile and run

**Compile** (from the project root, where `Makefile` is):

```bash
make
```

A clean build removes the binary and log file:

```bash
make clean
```

**Run** (three arguments: seed, number of players, chips per new bag):

```bash
./greasy_cards <seed> <num_players> <chips_per_bag>
```

**Example (6 players, 30 chips per bag):**

```bash
./greasy_cards 42 6 30
```

Console output lists each player’s result per round. Full detail is in `greasy_cards.log` in the current directory (overwritten on each run).

## Results: five runs (different seeds, 6 players, 30 chips per bag)

Each run uses seeds **10, 20, 30, 40, 50** with `num_players=6` and `chips_per_bag=30`. The program completes **6 rounds** (one per player as dealer). The table below lists the **round winner** for each run. Every non-winner in that round **lost** the round, as reported on the console and in the log.

| Run | Seed | R1  | R2  | R3  | R4  | R5  | R6  |
|-----|------|-----|-----|-----|-----|-----|-----|
| 1   | 10   | 4   | 4   | 2   | 3   | 2   | 5   |
| 2   | 20   | 5   | 5   | 5   | 6   | 3   | 3   |
| 3   | 30   | 6   | 4   | 6   | 2   | 3   | 2   |
| 4   | 40   | 5   | 3   | 4   | 3   | 2   | 3   |
| 5   | 50   | 6   | 3   | 5   | 6   | 6   | 3   |

**Observed behavior (brief):** Different seeds produce different **greasy** cards, deal order, **discard** and **draw** randomness, and **chip** consumption, so round winners and log length vary. The implementation is **repeatable**: re-running the same `seed` and parameters yields the same **console** and **log** content.

**Commands used for the table:**

```bash
./greasy_cards 10 6 30
./greasy_cards 20 6 30
./greasy_cards 30 6 30
./greasy_cards 40 6 30
./greasy_cards 50 6 30
```

(Each run overwrites `greasy_cards.log`; only the last run’s log file remains on disk until the next run.)
