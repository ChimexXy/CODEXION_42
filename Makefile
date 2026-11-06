NAME = codexion
CC = cc
CFLAGS = -Wall -Wextra -Werror -pthread -g
SRC = src/main.c src/parsing.c src/timing.c src/logging.c \
      src/heap.c src/heap_pop.c src/dongle.c src/acquire.c src/fifo_start.c \
      src/grant.c src/grant_wait.c src/worker.c src/worker_phases.c \
      src/monitor.c src/edf_schedule.c src/init.c src/cleanup.c src/threads.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean fclean re test

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME)

%.o: %.c src/codexion.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

test: $(NAME)
	python3 tests/check.py
