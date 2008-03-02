all: reflect send

reflect: reflect.cpp

send: send.cpp

clean:
	rm -f *.o reflect send
