include makeinclude

OBJ	= main.o source.o version.o

all: swupd.exe
	mkapp swupd

version.cpp:
	mkversion version.cpp

swupd.exe: $(OBJ)
	$(LD) $(LDFLAGS) -o swupd.exe $(OBJ) $(LIBS)

clean:
	rm -f *.o *.exe
	rm -rf swupd.app
	rm -f swupd

SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $<
