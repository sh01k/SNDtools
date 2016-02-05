PROGRAM = play_multifilter
UNAME = ${shell uname}

ifeq ($(UNAME),Darwin)
CC = g++
#LDLIBS = -L/opt/local/lib
LDADD = -lsndfile -lportaudio -lfftw3 -lboost_thread-mt -lboost_system
#CFLAGS = -I/opt/local/include -O2 -Wall
CFLAGS = -O2 -Wall
OUTDIR = build
OBJDIR = build
INSTDIR = ../bin
endif

./$(OUTDIR)/$(PROGRAM): ./$(OBJDIR)/main.o ./$(OBJDIR)/convIR.o
	$(CC) $(CFLAGS) ./$(OBJDIR)/main.o ./$(OBJDIR)/convIR.o -o ./$(OUTDIR)/$(PROGRAM) $(LDLIBS) $(LDADD)
./$(OBJDIR)/main.o: ./main.cpp
	$(CC) $(CFLAGS) -c ./main.cpp -o $(OBJDIR)/main.o
./$(OBJDIR)/convIR.o: ./convIR.cpp
	$(CC) $(CFLAGS) -c ./convIR.cpp -o ./$(OBJDIR)/convIR.o
install:
	cp ./$(OUTDIR)/$(PROGRAM) $(INSTDIR)
clean:
	rm ./$(OBJDIR)/main.o
	rm ./$(OBJDIR)/convIR.o
	rm ./$(OUTDIR)/$(PROGRAM)
