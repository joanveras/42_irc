NAME = ircserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -Iinclude

SRCS = main.cpp $(wildcard src/*.cpp)
OBJDIR = .objs
OBJS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRCS))

RM = rm -f

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

$(OBJDIR)/%.o: %.cpp | $(OBJDIR)/%/..
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# phony target used to create directories for object files
$(OBJDIR)/%/..:
	@mkdir -p $(OBJDIR)

clean:
	$(RM) $(OBJS)
	rm -rf $(OBJDIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
