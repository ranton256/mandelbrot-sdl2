CC=g++
FLAGS= -Wall # -pg

MYNAME=mandelbrot

# TODO: comment this line out to get a production release
# FLAGS += -g

FLAGS+= -D_GNU_SOURCE=1 -D_THREAD_SAFE
FLAGS+= -std=c++0x
FLAGS+= -I/usr/include/SDL2
FLAGS+= -I/usr/local/include/SDL2
FLAGS+= -I/usr/local/include
LFLAGS=-lm 
LFLAGS+= -lpthread
LFLAGS+= -v
LFLAGS+= -L/usr/local/lib
LFLAGS+=-lSDL2

PROG=$(MYNAME)
all: $(PROG)

SRCS := $(wildcard *.cpp)
	
OBJS := $(SRCS:.cpp=.o)

all: $(PROG) 

clean:
	rm -f *.o $(OBJS) $(PROG) 
	rm -f gmon.out core core.*

$(PROG): $(OBJS)
	$(CC) $(FLAGS) $(OBJS) $(LFLAGS) -o $(PROG)


%.o : %.cpp 
	@echo CPP ----- $*.cpp -----
	$(CC) $(FLAGS) -c $*.cpp -o $*.o