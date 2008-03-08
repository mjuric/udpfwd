.f.o:
	$(F77) $(FFLAGS) -I../orbfit/src/include -I./include -c $*.f -o $*.o

.cpp.o:
	$(CXX) $(CXXCFLAGS) -I../include -c $*.cpp -o $*.o


ALL=reflect send udpfwd

all: $(ALL)


udpfwd: udpfwd.o common.o
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

reflect: reflect.o common.o
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

send: send.o common.o
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o $(ALL)
