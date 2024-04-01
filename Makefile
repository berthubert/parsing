CXXFLAGS:=-std=gnu++17 -Wall -O1 -MMD -MP

PROGRAMS = hello escaped

all: $(PROGRAMS)

clean:
	rm -f *~ *.o *.d test $(PROGRAMS)

-include *.d

hello: hello.o
	$(CXX) -std=gnu++17 $^ -lfmt -o $@ 

escaped: escaped.o
	$(CXX) -std=gnu++17 $^ -lfmt -o $@ 
