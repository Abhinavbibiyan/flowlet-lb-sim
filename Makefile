CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Iinclude
LDFLAGS  :=

SRCDIR := src
OBJDIR := build
BINDIR := bin

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
BIN  := $(BINDIR)/flowlet_sim

.PHONY: all clean run plot help

all: dirs $(BIN)

dirs:
	@mkdir -p $(OBJDIR) $(BINDIR) data

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "\n✓ Build successful → $(BIN)"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	@mkdir -p data
	./$(BIN) --flows 300 --spines 4 --leaves 4 --hosts 4 --load 0.6 --duration 0.05

plot: run
	python3 scripts/plot.py

dashboard: run
	python3 scripts/dashboard.py

clean:
	rm -rf $(OBJDIR) $(BINDIR) data/*.csv

help:
	@./$(BIN) --help
