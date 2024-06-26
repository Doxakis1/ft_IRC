NAME = ircserv

S			= src/
I			= inc/

SRC = main.cpp $S/Server.cpp $S/Client.cpp $S/server_helpers.cpp $S/cmds.cpp $S/cmd_helpers.cpp $S/Message.cpp \
$S/Channel.cpp $S/channel_helpers.cpp

FLAGS = -Wall -Wextra -Werror -std=c++17 -g -fsanitize=address
INCLUDES	= -I$I

.PHONY: all clean fclean re

all: $(NAME)

$(NAME): $(SRC)
	@c++ $(FLAGS) $(INCLUDES) -o $(NAME) $(SRC)

clean:
	@rm -f $(NAME)

fclean: clean
	@rm -f $(NAME) client

re: fclean all

debug: FLAGS += -g
debug: re
