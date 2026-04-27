# Greasy Cards (CS4328 Project 2)

POSIX-threaded C implementation of the Greasy Cards game.

## Build

```bash
make
```

## Run

```bash
./greasy_cards <seed> <num_players> <chips_per_bag>
```

Example:

```bash
./greasy_cards 12345 6 30
```

## Output

- Console: each player prints whether they won/lost every round.
- Log file: `greasy_cards.log` contains detailed gameplay actions (draw/discard/deck/chips/round outcomes).

## Suggested report runs

Run the program 5 times with different seeds, 6 players, and 30 chips per bag:

```bash
./greasy_cards 10 6 30
./greasy_cards 20 6 30
./greasy_cards 30 6 30
./greasy_cards 40 6 30
./greasy_cards 50 6 30
```
