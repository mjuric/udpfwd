//==================================================== file = udprelay.c =====
//=  A unicast UDP relay/reflector for Windoes or Unix                       =
//============================================================================
//=  Notes:                                                                  =
//=    1) Conditionally compiles for Winsock and BSD sockets.  Manually      =
//=       set the initial #define to WIN or BSD as appropriate.              =
//=    2) Relects or relays as manually set in #define MODE                  =
//=    3) For #define MODE RELAY, IP address must be entered manually        =
//=    4) For #deffine MODE REFLECT, port numbers must be entered manually   =
//=--------------------------------------------------------------------------=
//=  Build: bcc32 udprelay.c or cl udprelay.c wsock32.lib for Winsock        =
//=         gcc udprelay.c -lsocket -lnsl for BSD                            =
//=--------------------------------------------------------------------------=
//=  History:  KJC (06/18/02) - Genesis (from server1.c)                     =
//=            JNS (06/19/02) - Updated, fixed REFLECT and RELAY             =
//============================================================================

//----- Include files -------------------------------------------------------
#include <stdio.h>          // Needed for printf()
#include <stdlib.h>         // Needed for memcpy()
#include <string.h>         // Needed for strcpy()

  #include <sys/types.h>    // Needed for system defined identifiers.
  #include <netinet/in.h>   // Needed for internet address structure.
  #include <sys/socket.h>   // Needed for socket(), bind(), etc...
  #include <arpa/inet.h>    // Needed for inet_ntoa()
  #include <fcntl.h>
  #include <netdb.h>
  #include <unistd.h>

//----- Defines -------------------------------------------------------------
#define FALSE        0  // Boolean false
#define TRUE         1  // Boolean true

#define VERBOSE   TRUE                    // Verbose mode (TRUE or FALSE)

#define PORT_NUM_FROM    5000             // Port number client sends to

//===== Main program ========================================================
int main(void)
{
	unsigned int         server_s;      // Server socket descriptor
	struct sockaddr_in   server_addr;   // Server Internet address
	struct sockaddr_in   client_addr;   // Client Internet address
	struct in_addr       recv_ip_addr;  // Received IP address
	socklen_t            addr_len;      // Internet address length
	int                  retcode;       // Return code
	char                 buffer[30000]; // Datagram buffer
	int                  i;             // Loop counter

	// Create my (server) socket and fill-in address information
	server_s = socket(AF_INET, SOCK_DGRAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM_FROM);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr));

	// Fill-in client's socket information
	client_addr.sin_family = AF_INET;

	// Set addr_len
	addr_len = sizeof(client_addr);

	// Reflect or relay UDP datagrams forever
	printf("BEGIN reflecting packets... \n");
	while(TRUE)
	{
		// Receive a datagram
		retcode = recvfrom(server_s, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_len);

		// Output buffer if verbose is true
		if (VERBOSE == TRUE)
		{
			printf("----------------------------------------------------------- \n");
			printf("Source = %s:%d   ", inet_ntoa(client_addr.sin_addr), (int)ntohs(client_addr.sin_port));
			printf("Message (%d) = ", retcode);
			for (i=0; i<retcode; i++)
				printf("%c", buffer[i]);
			printf("\n");
		}

		// Send the datagram back
		sendto(server_s, buffer, retcode, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
	}

	// Close and clean-up
	close(server_s);
}
