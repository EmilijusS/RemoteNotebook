#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <windows.h>
#include <tchar.h>
#include <errno.h>
#include <winsock2.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 2048

void initialiseWinsock();

SOCKET createAndBindMasterSocket(int port);

void startServer(SOCKET masterSocket);

void incomingConnection(SOCKET masterSocket, SOCKET clientSocket[]);

void incomingData(SOCKET *s);

void scenarios(SOCKET s, char *buffer);

char* newConnection(char *buffer);

char* userInformation(char *buffer);

char* newNote(char *buffer);

char* getNote(char *buffer);

int formPath(char *buffer, char path[], int *usernameLength, int *noteTitleLength);

int main(int argc , char *argv[])
{
    SOCKET masterSocket;
    int port;

    if( argc != 2 )
    {
        printf("Port number must be passed as an argument");
        return 0;
    }

    if((port = (int)strtol(argv[1], NULL, 10)) == 0)
    {
        printf("Could not parse arguments");
        return 0;
    }

    initialiseWinsock();

    masterSocket = createAndBindMasterSocket(port);

    //Listen to incoming connections
    listen(masterSocket , 3);

    printf("Waiting for incoming connections...\n");

    startServer(masterSocket);

    closesocket(masterSocket);
    WSACleanup();

    return 0;
}

void initialiseWinsock()
{
    WSADATA WSAData;

    printf("\nInitialising Winsock...");
    if (WSAStartup(MAKEWORD(2,2), &WSAData) != 0)
    {
        printf("Failed. Error Code : %d",WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    printf("Initialised.\n");

}

SOCKET createAndBindMasterSocket(int port)
{
    SOCKET masterSocket;
    struct sockaddr_in serverAddress;

    //Create a socket
    if((masterSocket = socket(AF_INET , SOCK_STREAM , 0 )) == INVALID_SOCKET)
    {
        printf("Could not create socket : %d" , WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    printf("Socket created.\n");

    //Prepare the serverAddress structure
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    //Bind
    if( bind(masterSocket ,(struct sockaddr *)&serverAddress , sizeof(serverAddress)) == SOCKET_ERROR)
    {
        printf("Bind failed with error code : %d" , WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    printf("Bind done.\n");

    return masterSocket;
}

void startServer(SOCKET masterSocket)
{
    SOCKET clientSocket[MAX_CLIENTS] = {0};
    int activity, i;
    //set of socket descriptors
    fd_set socketSet;

    while(TRUE)
    {
        //clear the socket fd set
        FD_ZERO(&socketSet);

        //add master socket to fd set
        FD_SET(masterSocket, &socketSet);

        //add child sockets to fd set
        for (  i = 0 ; i < MAX_CLIENTS ; i++)
        {
            if(clientSocket[i] > 0)
            {
                FD_SET(clientSocket[i] , &socketSet);
            }
        }

        //wait for an activity on any of the sockets, timeout is NULL , so wait indefinitely
        activity = select( 0 , &socketSet , NULL , NULL , NULL);

        if ( activity == SOCKET_ERROR )
        {
            printf("select call failed with error code : %d" , WSAGetLastError());
            return;
        }

        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(masterSocket , &socketSet))
        {
            incomingConnection(masterSocket, clientSocket);
        }

        //else it's some IO operation on some other socket
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            if (FD_ISSET( clientSocket[i] , &socketSet))
            {
                incomingData(&clientSocket[i]);
            }
        }
    }
}

void incomingConnection(SOCKET masterSocket, SOCKET clientSocket[])
{
    SOCKET newSocket;
    struct sockaddr_in address;
    int i, addressLength = sizeof(struct sockaddr_in);

    if ((newSocket = accept(masterSocket , (struct sockaddr *)&address, &addressLength)) == INVALID_SOCKET)
    {
        printf("Could not accept connection.\n");
        return;
    }

    //inform user of socket number - used in send and receive commands
    printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , newSocket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

    //add new socket to array of sockets
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clientSocket[i] == 0)
        {
            clientSocket[i] = newSocket;
            printf("Adding to list of sockets at index %d \n" , i);
            break;
        }
    }
}

void incomingData(SOCKET *s)
{
    struct sockaddr_in address;
    int readLength, addressLength = sizeof(struct sockaddr_in);
    //1 extra for null character, string termination
    char *buffer = (char*) malloc((BUFFER_SIZE + 1) * sizeof(char));

    if(buffer == NULL)
    {
        printf("Out of memory\n");
        return;
    }

    //get details of the client
    getpeername(*s, (struct sockaddr*)&address, &addressLength);

    //Check if it was for closing , and also read the incoming message
    //recv does not place a null terminator at the end of the string (whilst printf %s assumes there is one).
    readLength = recv(*s , buffer, BUFFER_SIZE, 0);

    if( readLength == SOCKET_ERROR)
    {
        int error_code = WSAGetLastError();
        if(error_code == WSAECONNRESET)
        {
            //Somebody disconnected , get his details and print
            printf("Host disconnected unexpectedly , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

            //Close the socket and mark as 0 in list for reuse
            closesocket(*s);
            *s = 0;
        }
        else
        {
            printf("recv failed with error code : %d" , error_code);
        }
    }
    else if ( readLength == 0)
    {
        //Somebody disconnected , get his details and print
        printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

        //Close the socket and mark as 0 in list for reuse
        closesocket(*s);
        *s = 0;
    }
    //Echo back the message that came in
    else
    {
        //add null character, if you want to use with printf/puts or other string handling functions
        buffer[readLength] = '\0';
        printf("%s:%d - %s \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port), buffer);
        scenarios(*s, buffer);
    }

    free(buffer);

}

void scenarios(SOCKET s, char *buffer)
{
    char* command = (char*) malloc(3 * sizeof(char));
    char* message;

    strncpy(command, buffer, 3);

    if(strcmp(command, "CNN") == 0)
    {
        message = newConnection(buffer);
    }
    else if(strcmp(command, "INF") == 0)
    {
        message = userInformation(buffer);
    }
    else if(strcmp(command, "NEW") == 0)
    {
        message = newNote(buffer);
    }
    else if(strcmp(command, "REQ") == 0)
    {
        message = getNote(buffer);
    }
    else
    {
        message = "ERR";
    }

    if( send(s, message, strlen(message), 0) != strlen(message) )
    {
        printf("Message was not sent succesfully.\n");
    }

    free(message);
}

char* newConnection(char *buffer)
{
    char* username = &buffer[4];
    char path[100] = "Notes\\";

    if(_mkdir(strcat(path, username)) == 0)
    {
        return "NEW";
    }
    else if(errno == EEXIST)
    {
        return "OLD";
    }
    else
    {
        return "ERR";
    }
}

char* userInformation(char *buffer)
{
    HANDLE hFind;
    WIN32_FIND_DATA data;
    char* username = &buffer[4];
    char* message = (char*) malloc((BUFFER_SIZE + 1) * sizeof(char));
    char path[100] = "Notes\\";


 //   printf("Path: %s\n", strcat(strcat(path, username), "\\*.*"));

    hFind = FindFirstFile(strcat(strcat(path, username), "\\*.*"), &data);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        strcpy(message, "INF|");

        do
        {
            // these appear everytime
            if(strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0)
            {
                //printf("File: %s\n", data.cFileName);
                strcat(message, data.cFileName);
                //Cut .txt ending
                message[strlen(message) - 4] = '\0';
                strcat(message, "|");
            }
        }
        while (FindNextFile(hFind, &data));

      FindClose(hFind);
    }
    else
    {
        strcpy(message, "ERR");
    }

    return message;

}

char* newNote(char *buffer)
{
    int usernameLength, noteTitleLength;
    char path[100] = "Notes\\";
    FILE *file;

    formPath(buffer, path, &usernameLength, &noteTitleLength);

    file = fopen(path, "w");
    fprintf(file, "%s", &buffer[6 + usernameLength + noteTitleLength]);

    fclose(file);

    return "SCC";
}

char* getNote(char *buffer)
{
    int usernameLength, noteTitleLength;
    char path[100] = "Notes\\";
    char *message = (char*) malloc((BUFFER_SIZE + 1) * sizeof(char));
    char fileBuffer[110];
    FILE *file;

    formPath(buffer, path, &usernameLength, &noteTitleLength);

    printf("%s\n", path);

    file = fopen(path, "r");

    if(file == NULL)
    {
        strcpy(message, "ERR");
    }
    else
    {
            strcpy(message, "REQ|");
        strcat(message, &buffer[5 + usernameLength]);
        strcat(message, "|");
        fgets(fileBuffer, 110, file);
        strcat(message, fileBuffer);
        strcat(message, "|");
    }

    return message;

}

int formPath(char *buffer, char path[], int *usernameLength, int *noteTitleLength)
{
    int i;

    for(i = 4; buffer[i]; ++i)
    {
        if(buffer[i] == '|')
        {
            *usernameLength = i - 4;
            break;
        }
    }

    for(++i; buffer[i]; ++i)
    {
        if(buffer[i] == '|')
        {
            *noteTitleLength = i - *usernameLength - 5;
            break;
        }
    }

    strncat(path, &buffer[4], *usernameLength);
    strcat(path, "\\");
    strncat(path, &buffer[5 + *usernameLength], *noteTitleLength);
    strcat(path, ".txt");

    return 1;
}




