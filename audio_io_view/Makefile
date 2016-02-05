PROGRAM = audio_if_view
UNAME = ${shell uname}

ifeq ($(UNAME),Darwin)
CC = g++
#LDLIBS = -L/opt/local/lib
LDADD = -lsndfile -lportaudio
#CFLAGS = -I/opt/local/include -O2 -Wall
CFLAGS = -O2 -Wall
OUTDIR = build
OBJDIR = build
INSTDIR = ../bin
endif

./$(OUTDIR)/$(PROGRAM): ./$(OBJDIR)/main.o
	$(CC) $(CFLAGS) ./$(OBJDIR)/main.o -o ./$(OUTDIR)/$(PROGRAM) $(LDLIBS) $(LDADD)
./$(OBJDIR)/main.o: ./main.cpp
	$(CC) $(CFLAGS) -c ./main.cpp -o $(OBJDIR)/main.o
install:
	cp ./$(OUTDIR)/$(PROGRAM) $(INSTDIR)
clean:
	rm ./$(OUTDIR)/$(PROGRAM)
