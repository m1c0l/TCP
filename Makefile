CXX=g++
CXXOPTIMIZE= -g
CXXFLAGS= -g -Wall -pthread -std=c++11 $(CXXOPTIMIZE)
DISTDIR= CS118Project2

CLASSES = 

all: server client

web-server: $(CLASSES:=.cpp) server.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

web-client: $(CLASSES:=.cpp) client.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

$(CLASSES:=.cpp): $(CLASSES:=.h)

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM web-server web-client *.tar.gz $(DISTDIR)

dist: clean
	tar cvf - --transform='s|^|$(DISTDIR)/|' *.cpp *.h *.pdf Makefile Vagrantfile | gzip -9 > $(DISTDIR).tar.gz

.PHONY: clean dist

