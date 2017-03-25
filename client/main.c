#include<stdio.h>
#include<winsock2.h>

void initialiseWinsock();

SOCKET createSocketandConnect(int port, char *address);

int getUserInput (char *prompt, char *buffer, size_t size);

int sendMessage(SOCKET s, char *message);

int receiveReply(SOCKET s);

void receiveInformation(char serverReply[]);

void start(SOCKET s);

char* login(SOCKET s);

void requestInformation(SOCKET s, char *username);

void newNote(SOCKET s, char *username);

void readNote(SOCKET s, char *username);

int main(int argc , char *argv[])
{
    SOCKET s;
    int port;

    printf("Remote notes v3.1\n");

    if( argc != 3 )
    {
        printf("Port number and server's ip address must be passed as arguments\n");
        return 0;
    }

    if((port = (int)strtol(argv[1], NULL, 10)) == 0)
    {
        printf("Could not parse arguments\n");
        return 0;
    }

    initialiseWinsock();

    s = createSocketandConnect(port, argv[2]);

    start(s);

    closesocket(s);
    WSACleanup();

    return 0;
}

void initialiseWinsock()
{
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        printf("Failed to initialise WinSock. Error Code : %d",WSAGetLastError());
        exit(EXIT_FAILURE);
    }

}

SOCKET createSocketandConnect(int port, char *address)
{
    SOCKET s;
    struct sockaddr_in server;

    if((s = socket(AF_INET , SOCK_STREAM , 0 )) == INVALID_SOCKET)
    {
        printf("Could not create socket : %d" , WSAGetLastError());
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Socket created.\n");


    server.sin_addr.s_addr = inet_addr(address);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    //Connect to remote server
    if (connect(s , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        printf("Coould not connect to the server\n");
        closesocket(s);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Connected\n");

    return s;
}

int getUserInput (char *prompt, char *buffer, size_t size)
{

    int extra;
    char ch;

    // Get line with buffer overrun protection.
    if (prompt != NULL) {
        printf ("%s", prompt);
        fflush (stdout);
    }
    if (fgets (buffer, size, stdin) == NULL)
        return 0;

    // If it was too long, there'll be no newline. In that case, we flush
    // to end of line so that excess doesn't affect the next call.
    if (buffer[strlen(buffer) - 1] != '\n')
    {
        extra = 0;
        while (((ch = getchar()) != '\n') && (ch != EOF))
            extra = 1;
        return (extra == 1) ? 0 : 1;
    }

    // Otherwise remove newline and give string back to caller.
    buffer[strlen(buffer)-1] = '\0';
    return 1;
}

int sendMessage(SOCKET s, char *message)
{
    if(send(s, message, strlen(message), 0) < 0)
    {
        puts("Send failed");
        return 0;
    }

    return 1;
}

int receiveReply(SOCKET s)
{
    int receiveSize;
    char serverReply[2000], command[4];

    //Receive a reply from the server
    if((receiveSize = recv(s , serverReply , 2000 , 0)) == SOCKET_ERROR)
    {
        printf("recv failed\n");
        return 0;
    }

    //Add a NULL terminating character to make it a proper string before printing
    serverReply[receiveSize] = '\0';

    strncpy(command, serverReply, 3);
    command[3] = '\0';

    if(strcmp(command, "NEW") == 0)
    {
        printf("User created succesfully!\n");
    }
    else if(strcmp(command, "OLD") == 0)
    {
        printf("Welcome back!\n");
    }
    else if(strcmp(command, "INF") == 0)
    {
        printf("Your notes:\n");
        receiveInformation(serverReply);
    }
    else if(strcmp(command, "SCC") == 0)
    {
        printf("Success!\n");
    }
    else if(strcmp(command, "REQ") == 0)
    {
        printf("Note ");
        receiveInformation(serverReply);
    }
    else if(strcmp(command, "ERR") == 0)
    {
        printf("Error!\n");
        return 0;
    }
    else
    {
        printf("Server reply unrecognized\n");
        return 0;
    }

    return 1;
}

void receiveInformation(char serverReply[])
{
    int i, beginning;
    char text[2000];

    beginning = 4;

    for(i = 4; serverReply[i] != '\0'; ++i)
    {
        if(serverReply[i] == '|')
        {
            strncpy(text, &serverReply[beginning], i - beginning);
            text[i - beginning] = '\0';
            printf("%s\n", text);
            beginning = i + 1;
        }
    }

}

void start(SOCKET s)
{
    char *username, input[2];

    if((username = login(s)) == NULL)
    {
        return;
    }

    printf("MENU\n");
    printf("1. Your notes\n");
    printf("2. Create new note\n");
    printf("3. Read a note\n");
    printf("4. Exit\n");

    do
    {
        if(!getUserInput("Enter your choice:", input, sizeof(input)))
        {
            printf("Wrong input!");
        }
        else
        {
            if(input[0] == '1')
            {
                requestInformation(s, username);
            }
            else if(input[0] == '2')
            {
                newNote(s, username);
            }
            else if(input[0] == '3')
            {
                readNote(s, username);
            }
        }
    }
    while(input[0] != '4');

    free(username);

}

char* login(SOCKET s)
{
    char *username, message[15];

    username = malloc(sizeof(char) * 11);

    if(!getUserInput("Enter your username (max 10 characters):", username, 11))
    {
        printf("Invalid username!\n");
        return NULL;
    }

    strcpy(message, "CNN|");
    strcat(message, username);

    if(!sendMessage(s, message))
    {
        return NULL;
    }

    if(!receiveReply(s))
    {
        return NULL;
    }

    return username;
}

void requestInformation(SOCKET s, char *username)
{
    char message[20];

    strcpy(message, "INF|");
    strcat(message, username);

    sendMessage(s, message);
    receiveReply(s);
}

void newNote(SOCKET s, char *username)
{
    char message[1100], buffer[1001];

    strcpy(message, "NEW|");
    strcat(message, username);
    strcat(message, "|");

    if(!getUserInput("Enter note title (max 10 characters):", buffer, 10))
    {
        printf("Bad title!\n");
        return;
    }

    strcat(message, buffer);
    strcat(message, "|");

    if(!getUserInput("Enter note (max 1000 characters):", buffer, 1000))
    {
        printf("Bad note!\n");
        return;
    }

    strcat(message, buffer);

    sendMessage(s, message);
    receiveReply(s);
}

void readNote(SOCKET s, char *username)
{
    char message[50], title[11];

    strcpy(message, "REQ|");
    strcat(message, username);
    strcat(message, "|");

    if(!getUserInput("Enter note title:", title, 10))
    {
        printf("Bad title!\n");
        return;
    }

    strcat(message, title);

    sendMessage(s, message);
    receiveReply(s);
}
