CFLAGS = -MMD -Wall -pedantic --std=c99
CXXFLAGS = -MMD -Wall -pedantic --std=c++11
LIBS = -lm
CC = gcc
CXX = g++
OFILES = $(patsubst %.c,%.o,$(wildcard *.c)) $(patsubst %.cpp,%.o,$(wildcard *.cpp))
DFILES = $(patsubst %.c,%.d,$(wildcard *.c)) $(patsubst %.cpp,%.d,$(wildcard *.cpp))
HFILES = $(wildcard *.h *.hpp)
PROG = cachesim
TARBALL = $(if $(USER),$(USER),gburdell3)-proj1.tar.gz

ifdef SANITIZE
CFLAGS += -fsanitize=address
CXXFLAGS += -fsanitize=address
LIBS += -fsanitize=address
endif

ifdef DEBUG
CFLAGS += -DDEBUG
CXXFLAGS += -DDEBUG
endif

ifdef FAST
CFLAGS += -O2
CXXFLAGS += -O2
else
CFLAGS += -g
CXXFLAGS += -g
endif

.PHONY: all validate submit clean

all: $(PROG)

$(PROG): $(OFILES)
	$(CXX) -o $@ $^ $(LIBS)

%.o: %.c $(HFILES)
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp $(HFILES)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

validate_undergrad: $(PROG)
	@./validate_undergrad.sh

validate_grad: $(PROG)
	@./validate_grad.sh

submit: clean
	tar --exclude=project1_*.pdf -czhvf $(TARBALL) run.sh Makefile $(wildcard *.pdf *.cpp *.c *.hpp *.h)
	@echo
	@echo 'submission tarball written to' $(TARBALL)
	@echo 'please decompress it yourself and make sure it looks right!'

clean:
	rm -f $(TARBALL) $(PROG) $(OFILES) $(DFILES)

-include $(DFILES)

# if you're a student, ignore this
-include ta-rules.mk
