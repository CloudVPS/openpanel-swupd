# This file is part of OpenPanel - The Open Source Control Panel
# OpenPanel is free software: you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation, using version 3 of the License.
#
# Please note that use of the OpenPanel trademark may be subject to additional 
# restrictions. For more information, please visit the Legal Information 
# section of the OpenPanel website on http://www.openpanel.com/

include makeinclude

OBJ	= main.o source.o version.o

all: swupd.exe
	grace mkapp swupd

version.cpp:
	grace mkversion version.cpp

swupd.exe: $(OBJ)
	$(LD) $(LDFLAGS) -o swupd.exe $(OBJ) $(LIBS)

clean:
	rm -f *.o *.exe
	rm -rf swupd.app
	rm -f swupd

SUFFIXES: .cpp .o
.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $<
