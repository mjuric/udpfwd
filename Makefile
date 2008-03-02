all: reflect send relay

relay: relay.cpp

reflect: reflect.cpp

send: send.cpp

clean:
	rm -f *.o reflect send relay
