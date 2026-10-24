TARGET = myshell

CXX = g++
CXXFLAGS = -Wall -g -std=c++11

all: $(TARGET)

$(TARGET): command.o tokenizer.o
	$(CXX) $(CXXFLAGS) -o $(TARGET) command.o tokenizer.o

command.o: command.cc command.h tokenizer.h
	$(CXX) $(CXXFLAGS) -c command.cc

tokenizer.o: tokenizer.cc tokenizer.h
	$(CXX) $(CXXFLAGS) -c tokenizer.cc

clean:
	rm -f $(TARGET) *.o
