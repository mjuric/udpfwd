all: reflect send udpfwd

udpfwd: udpfwd.cpp common.cpp

reflect: reflect.cpp common.cpp
	$(CXX) $(CXXFLAGS) $? $(LDFLAGS) -o $@

send: send.cpp common.cpp
	$(CXX) $(CXXFLAGS) $? $(LDFLAGS) -o $@

clean:
	rm -f *.o reflect send udpfwd
