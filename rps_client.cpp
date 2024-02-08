#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
//#include <pthread.h>
#include <cctype>
#include <vector>
#include <string>
#include <sstream>
#include <limits>
#include <assert.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 4321
#define BUFFER_SIZE 1024

//rules
    //status
#define FAIL 0x11                   //login fail
#define SUCCESS 0x22                //login succ
#define READY_TO_BATTLE 0x33        //waiting status for player
#define IN_BATTLE 0x44              //player is in a battle
#define NOT_EXIST 0x55              //player not exist
#define REFUSED 0x66                //player refuse to battle
    //command
#define LOGIN 0x01                  //login command
#define SHOW_ALL_PLAYERS 0x02       //show all players command
#define SHOW_RANKING 0x03           //show ranking command
#define BATTLE_REQ 0x04             //battle request command
#define BATTLING 0x05               //battling command
#define EXIT_MSG 0x06               //player exit message from server
#define WAITING_RESP 0x07           //waiting responce command
#define END_BATTLE 0x08             //end battle command
#define LOGIN_MSG 0x09              //player login message from server
    //others
#define PLAYER_LIFE 2               
#define ROCK "ROCK"
#define SCISSORS "SCISSORS"
#define PAPER "PAPER"
#define TEST 0x99

#define RECV_DATA_TIME 5
#define RECV_MSG_TIME 5
    //color
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define GOLD    "\033[37m"
#define RESET   "\033[0m"

using namespace std;

// global data
struct sockaddr_in   addr;            /* server socket address */
int                  timeout;         /* poll timeout */
struct pollfd        fds[5];          /* poll fds */
int                  nfds = 1;        /* The number of all poll-fd */
int                  listen_sd = -1;  /* listen socket fd */
int                  on = 1;          /* 1:non-blocking; 0:blocking */
int                  end_server = false; /* server running */
string myname;



int login_checker(char* buffer, int sockdf);
void show_all_players(char* buffer, int sockfd);
void show_ranking(char* buffer, int sockfd);
void start_battling(char* buffer, int sockfd);
int is_digit(string target);
void loading_message();
void receiving_data();
void receiving_message_green();
void receiving_message_red();
void receiving_message_magenta();
void waiting_response_message();
void exit_message(const string& name);
void login_message(const string& name);
void starting_message();
void login_succ_message();
void battling_with_receiver(char* buffer, int sockfd);
void main_menu();
void game_menu(int round);
void show_all_players_board(vector<string> result);
void show_ranking_board(vector<string> result);
void battle_msg_this_round_win();
void battle_msg_this_game_win();
void battle_msg_this_round_lose();
void battle_msg_this_game_lose();
void battle_msg_same_choice();
void battle_msg_target_in_battle(string target);
void battle_msg_target_not_exist(string target);
void battle_msg_target_refused(string target);

int main(int argc, char *argv[]) {
    int sockfd, sendbytes, recvbytes, rc;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];



    //Create an AF_INET stream socket to recvive and send data
    if((sockfd = socket(AF_INET, SOCK_STREAM,0)) < 0) {
        cerr << "socket";
        exit(1);
    }

    //Set the server address
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(SERVER_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP); 
    bzero(&(serv_addr.sin_zero),8);

    if((connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(struct sockaddr))) < 0) {
        cerr << "connect failed!";
        exit(1);
    }
    cout << GREEN << "connect successful!" << RESET << endl;

    //Welcome message
    system("figlet -f small RSP GAME!");

    //login checker
    int checker_loop = 1;
    while(checker_loop) {
        checker_loop = login_checker(buffer,sockfd);
    }

    
    
    fds[0].fd = 0;
    fds[0].events = POLLIN;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    timeout = (3*60*1000);
    //timeout = -1;

    while(1) {
        main_menu();
        rc = poll(fds, 2, timeout);

        if (rc < 0) {
            cerr << "  poll() failed";
            break;
        }

        if(rc == 0) {
            system("clear");
            cout << YELLOW << "  poll() timed out..." << RESET << endl;
        }
        
        //client input
        if(fds[0].revents & POLLIN) {

            int opt;
            while(1) {
                cin >> opt;
                if (cin.fail()) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    cout << "Invalid input. Try again\n";
                    continue;
                }
                if (opt == 1 || opt == 2 || opt == 3 || opt == 4) break;
            }

            
            switch (opt){
                case 1: {
                    show_all_players(buffer,sockfd);
                    break;
                }
                case 2: {
                    show_ranking(buffer,sockfd);
                    break;    
                }
                case 3: {
                    start_battling(buffer,sockfd);
                    break;
                }
                case 4: {
                    cout << GREEN << "Exit game." << RESET << endl;
                    close(sockfd);
                    exit(0);
                    break;
                }
                default: {
                    cout << YELLOW << "Invalid option, Try again." << RESET << endl;
                    break;
                }  
            }
		}
        //message from server
	    if(fds[1].revents & POLLIN)
		{
            memset(buffer, 0, sizeof(buffer));
            rc = read(fds[1].fd, buffer, sizeof(buffer));
			if(rc < 0){
                cerr << "error recv";
				return -1;
			}
			else if(0 == rc) {
				cout << RED << "server down!!!" << RESET << endl;
				break;
            }
            //assert(buffer[0] == EXIT_MSG);
            if(buffer[0] == EXIT_MSG) {
                receiving_message_magenta();
                string exit_name(buffer);
                exit_name = exit_name.substr(1,exit_name.size());

                exit_message(exit_name);

            }
            else if(buffer[0] == LOGIN_MSG) {
                receiving_message_green();
                string login_name(buffer);
                login_name = login_name.substr(1,login_name.size());
                
                login_message(login_name);
            }
            else if(buffer[0] == BATTLE_REQ) {
                receiving_message_magenta();
                string target_name(buffer);
                target_name = target_name.substr(1,target_name.size());
                cout << target_name << " want to battle to you (y/n): ";
                string o;
                while(1) {
                    cin >> o;
                    if (cin.fail()) {
                        cin.clear();
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        continue;
                    }
                    if (o == "y" || o == "n") break;
                    else cout << YELLOW << "Invalid input. Try again" << RESET << endl;
                }

                memset(buffer,0,sizeof(buffer));
                if(o == "y") {
                    system("clear");
                    buffer[0] = WAITING_RESP;
                    buffer[1] = READY_TO_BATTLE;
                    string tmp = myname + ",";
                    strcpy(buffer+2, tmp.c_str());
                    strcpy(buffer+tmp.size()+2, target_name.c_str());
                    if((sendbytes = send(fds[1].fd, buffer, strlen(buffer),0)) < 0) {
                        cerr << "send failed";
                        exit(1);
                    } else{
                        cout << "You start a BATTLE to " << target_name << "\n";
                    }
                    
                    //start battle, here
                    battling_with_receiver(buffer, sockfd);


                }
                else if(o == "n") {
                    buffer[0] = WAITING_RESP;
                    buffer[1] = REFUSED;
                    strcpy(buffer+2, target_name.c_str());
                    if((sendbytes = send(fds[1].fd, buffer, strlen(buffer),0)) < 0) {
                        cerr << "send failed";
                        exit(1);
                    } else{
                        cout << "You refused a BATTLE to " << target_name << "\n";
                    }
                }
            }
            //cout << buffer << endl;
		}
    }
    
    return 0;
}

int login_checker(char* buffer, int sockfd) {
    int sendbytes, recvbytes = 0;

    cout << "================\n";
    string playername;

    while (true) {
        cout << "Please enter your name: ";
        getline(std::cin, playername);
        if (playername.empty() || playername == " " || playername == "  ") {
            cout << "Invalid input. Please try again." << endl;
        } 
        else {
            int no = 1;
            for(int i = 0; i < (int)playername.size(); i++) {
                if(playername[i] == ' ') {//Invali name found
                    no = 0;
                    cout << YELLOW << "Invalid input. Please try again." << RESET << endl;
                    break;
                }
            }
            if(no) break;
        }
    }
    buffer[0] = LOGIN;
    strcpy(buffer+1, playername.c_str());
    if((sendbytes = send(sockfd, buffer, strlen(buffer), 0)) < 0) {
        cerr << "send failed";
        exit(1);
    } //else{
    //    cout << "send successful! " << sendbytes << "\n";
    //}
    recvbytes = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if(recvbytes < 0) {
        cout << "recv error: " << recvbytes << "\n";
        exit(1);
    }
    else if(recvbytes == 0) {
        cout << "The connection has been closed by the server.";
        exit(1);
    }
    else {
        assert(buffer[0] == LOGIN);
        //if(buffer[0] == EXIT_MSG) continue;
        if(buffer[1] == SUCCESS) {

            //loading message
            loading_message();
            
            //login message;
            login_succ_message();

            myname = playername;
            return 0;
        }
        else if(buffer[1] == FAIL) {
            if(strlen(buffer) == 3) {
                cout << "You cannot choose this name, Try again...\n";
                return 1;
            }
            else {
                cout << "Player name existed, Try again...\n";
                return 1;
            }
        }
    }
    return 1;
}

void show_all_players(char* buffer, int sockfd) {
    int sendbytes, recvbytes;
    memset(buffer,0,sizeof(buffer));
    buffer[0] = SHOW_ALL_PLAYERS;
    
    if((sendbytes = send(sockfd, buffer, strlen(buffer), 0)) < 0) {
        cerr << "send failed";
        exit(1);
    } 
    //else cout << "send successful! " << sendbytes << "\n";
    
    memset(buffer,0,sizeof(buffer));
    //recving message
    receiving_data();
    system("clear");

    recvbytes = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if(recvbytes < 0) {
        cout << "recv error: " << recvbytes << "\n";
        exit(1);
    }
    else if(recvbytes == 0) {
        cout << "The connection has been closed by the server.";
        exit(1);
    }
    else {
        //separate data to name and status
        assert(buffer[0] == SHOW_ALL_PLAYERS);

        int len = (int)buffer[1];

        string recvdata(buffer);
        string cor;
        for(int _ = 2; _ < len+2; _++) {
            if(recvdata[_] >= 0x2b && recvdata[_] <= 0x7a)
                cor += recvdata[_];
            else break;
        }
        
        //recvdata = recvdata.substr(1,recvdata.size());
        vector<string> result;
        stringstream ss(cor);
        string item;
        while(getline(ss,item,',')) result.push_back(item);
        
        show_all_players_board(result);
    }
}

void show_ranking(char* buffer, int sockfd) {
    int sendbytes, recvbytes;
    memset(buffer,0,sizeof(buffer));
    buffer[0] = SHOW_RANKING;
    
    if((sendbytes = send(sockfd, buffer, strlen(buffer), 0)) < 0) {
        cerr << "send failed";
        exit(1);
    } 
    //else cout << "send successful! " << sendbytes << "\n";
    
    memset(buffer,0,sizeof(buffer));

    //recving message();
    receiving_data();
    system("clear");


    recvbytes = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if(recvbytes < 0) {
        cout << "recv error: " << recvbytes << "\n";
        exit(1);
    }
    else if(recvbytes == 0) {
        cout << "The connection has been closed by the server.";
        exit(1);
    }
    else {
        //separate data to name and status
        assert(buffer[0] == SHOW_RANKING);
        
        string recvdata(buffer);
        string cor;
        for(int _ = 2; _ < (int)buffer[1]+2; _++) {
            if( recvdata[_] >= 0x2b && recvdata[_] <= 0x7a)
                cor += recvdata[_];
            else break;
        }
        
        
        //string recvdata(buffer);
        //recvdata = recvdata.substr(1,recvdata.size());

        vector<string> result;
        stringstream ss(cor);
        string item;
        while(getline(ss,item,',')) result.push_back(item);
        
        show_ranking_board(result);

    }
}

void start_battling(char* buffer, int sockfd) {
    int sendbytes, recvbytes;
    memset(buffer,0,sizeof(buffer));
    buffer[0] = BATTLE_REQ;
    cout << "Enter player name: ";
    string target;
    cin.ignore(100, '\n');
    getline(cin, target);
    strcpy(buffer+1, target.c_str());
    //target != myname

    if((sendbytes = send(sockfd, buffer, strlen(buffer), 0)) < 0) {
        cerr << "send failed";
        exit(1);
    } 
    else cout << "You send a BATTLE request to " << target << "\n";
    
    receiving_data();

    recvbytes = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if(recvbytes < 0) {
        cout << "recv error: " << recvbytes << "\n";
        exit(1);
    }
    else if(recvbytes == 0) {
        cout << "The connection has been closed by the server.";
        exit(1);
    }
    else {
        assert(buffer[0] == BATTLE_REQ || buffer[0] == WAITING_RESP);
        if(buffer[1] == READY_TO_BATTLE) {//OK to start battle
            int end_game = 1;
            int round = 1;
            system("clear");
            starting_message();
            while(end_game) {
                game_menu(round);

                memset(buffer,0,sizeof(buffer));
                buffer[0] = BATTLING;
                
                int opt;

                while(1) {
                    cin >> opt;
                    if (cin.fail()) {
                        cin.clear();
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        cout << YELLOW << "Invalid input. Try again" << RESET << endl;
                        continue;
                    }
                    if (opt == 1 || opt == 2 || opt == 3) break;
                }
                
                if(opt == 1) {
                    string tmp = myname + "," + "ROCK";
                    strcpy(buffer+1,tmp.c_str());
                }
                else if(opt == 2) {
                    string tmp = myname + "," + "SCISSORS";
                    strcpy(buffer+1,tmp.c_str());
                }
                else if(opt == 3)  {
                    string tmp = myname + "," + "PAPER";
                    strcpy(buffer+1,tmp.c_str());
                }
                if((sendbytes = send(sockfd, buffer, strlen(buffer), 0)) < 0) {
                    cerr << "send failed";
                    exit(1);
                } 
                //else cout << "send successful! " << sendbytes << "\n";
                
                receiving_data();

                recvbytes = recv(sockfd, buffer, BUFFER_SIZE, 0);
                if(recvbytes < 0) {
                    cout << "recv error: " << recvbytes << "\n";
                    exit(1);
                }
                else if(recvbytes == 0) {
                    cout << "The connection has been closed by the server.";
                    exit(1);
                }
                else {

                    string recvdata(buffer);
                    recvdata = recvdata.substr(1,recvdata.size());
                    vector<string> result;
                    stringstream ss(recvdata);
                    string item;
                    while(getline(ss,item,',')) result.push_back(item);
                    string winner = result[0];
                    string p1 = result[1];
                    string p2 = result[2];
                    
                    system("clear");

                    if(winner == myname){
                        if(buffer[0] == END_BATTLE) {
                            end_game = 0;
                            battle_msg_this_game_win();
                        }
                        else battle_msg_this_round_win();   
                    }
                    else if (winner == "3") {
                        battle_msg_same_choice();
                    }
                    else {
                        if(buffer[0] == END_BATTLE) {
                            end_game = 0;
                            battle_msg_this_game_lose();
                        }
                        else battle_msg_this_round_lose();
                    }
                }
                round++;
            }
        }
        else if(buffer[1] == IN_BATTLE){//target is in a battle
            //receiving_data();
            battle_msg_target_in_battle(target);
        }
        else if(buffer[1] == NOT_EXIST) {//target name not exist
            //receiving_data();
            battle_msg_target_not_exist(target);
        }
        else if(buffer[1] == REFUSED){//target name not exist
            //receiving_data();
            battle_msg_target_refused(target);
        } 
    }
}

int is_digit(string target) {
    bool only_digits = true;
    for (char c : target) {
        if (!isdigit(c)) {
            only_digits = false;
            break;
        }
    }
    return only_digits;
}

void waiting_response_message() {
    printf("waiting");
    for(int i = 0; i < 3; i++) {
        fflush(stdout);
        sleep(1);
        printf(".");
        fflush(stdout);
    }
    cout << "\n";
}

void battling_with_receiver(char* buffer, int sockfd) {
    int sendbytes, recvbytes;
    int end_game = 1;
    int round = 1;
    system("clear");
    starting_message();
    while(end_game) {
        
        game_menu(round);

        memset(buffer,0,sizeof(buffer));
        buffer[0] = BATTLING;
        
        int opt;
        while(1) {
            cin >> opt;
            if (cin.fail()) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                continue;
            }
            if (opt == 1 || opt == 2 || opt == 3) break;
            else cout << "Invalid input. Try again\n";
        }
        if(opt == 1) {
            string tmp = myname + "," + "ROCK";
            strcpy(buffer+1,tmp.c_str());
        }
        else if(opt == 2) {
            string tmp = myname + "," + "SCISSORS";
            strcpy(buffer+1,tmp.c_str());
        }
        else if(opt == 3)  {
            string tmp = myname + "," + "PAPER";
            strcpy(buffer+1,tmp.c_str());
        }

        if((sendbytes = send(sockfd, buffer, strlen(buffer), 0)) < 0) {
            cerr << "send failed";
            exit(1);
        } 
        //else cout << "send successful! " << sendbytes << "\n";
        
        receiving_data();

        recvbytes = recv(sockfd, buffer, BUFFER_SIZE, 0);
        cout << buffer << endl;
        if(recvbytes < 0) {
            cout << "recv error: " << recvbytes << "\n";
            exit(1);
        }
        else if(recvbytes == 0) {
            cout << "The connection has been closed by the server.";
            exit(1);
        }
        else {
            string recvdata(buffer);
            recvdata = recvdata.substr(1,recvdata.size());
            vector<string> result;
            stringstream ss(recvdata);
            string item;
            while(getline(ss,item,',')) result.push_back(item);
            string winner = result[0];
            string p1 = result[1];
            string p2 = result[2];
            system("clear");

            if(winner == myname){
                if(buffer[0] == END_BATTLE) {
                    end_game = 0;
                    battle_msg_this_game_win();
                }
                else battle_msg_this_round_win(); 
            }
            else if (winner == "3") {
                battle_msg_same_choice();
            }
            else {
                if(buffer[0] == END_BATTLE) {
                    end_game = 0;
                    battle_msg_this_game_lose();
                }
                else battle_msg_this_round_lose();
            }
        }
        round++;
    }
}

void main_menu() {
    cout << "╭─────────────────────╮\n";
    cout << "│ 1. Show all players │\n";
    cout << "│ 2. Show ranking     │\n";
    cout << "│ 3. Start battle     │\n";
    cout << "│ 4. Exit             │\n";
    cout << "╰─────────────────────╯\n";
    //cout << "Waiting on poll()...\n";
}

void game_menu(int round) {
    
    cout << "╭────────────────╮\n";
    cout << "│    ROUND " << round << "     │\n";
    cout << "│ 1. Rock        │\n";
    cout << "│ 2. Scissors    │\n";
    cout << "│ 3. Paper       │\n";
    cout << "╰────────────────╯\n";
}

void loading_message() {
    const int WIDTH = RECV_DATA_TIME;
    cout << "Loading Game: ";
    for (int i = 0; i <= WIDTH; ++i) {
        cout << GREEN << "▌" << RESET;
        cout.flush();
        usleep(500000);
    }
    cout << "\n";
    system("clear");
}

void starting_message() {
    const int WIDTH = RECV_DATA_TIME;
    cout << "Starting Game: ";
    for (int i = 0; i <= WIDTH; ++i) {
        cout << CYAN << "▌" << RESET;
        cout.flush();
        usleep(500000);
    }
    cout << "\n";
    system("clear");
}

void receiving_data() {
    const int WIDTH = RECV_DATA_TIME;
    cout << "Receiving Data: ";
    for (int i = 0; i <= WIDTH; ++i) {
        cout << YELLOW << "▌" << RESET;
        cout.flush();
        usleep(500000);
    }
    cout << "\n";
    system("clear");
}

void receiving_message_green() {
    const int WIDTH = 5;
    cout << "Receiving Data: ";
    for (int i = 0; i <= WIDTH; ++i) {
        cout << GREEN << "▌" << RESET;
        cout.flush();
        usleep(500000);
    }
    cout << "\n";
    system("clear");
}

void receiving_message_red() {
    const int WIDTH = RECV_MSG_TIME;
    cout << "Receiving Data: ";
    for (int i = 0; i <= WIDTH; ++i) {
        cout << RED << "▌" << RESET;
        cout.flush();
        usleep(500000);
    }
    cout << "\n";
    system("clear");
}

void receiving_message_magenta() {
    const int WIDTH = RECV_MSG_TIME;
    cout << "Receiving Data: ";
    for (int i = 0; i <= WIDTH; ++i) {
        cout << MAGENTA << "▌" << RESET;
        cout.flush();
        usleep(500000);
    }
    cout << "\n";
    system("clear");
}

void exit_message(const string& name) {

    int box_width = 40;
    string show = name;
    show += " has EXITED the game.";
    //line 1
    cout << YELLOW << "╔";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╗" << endl;

    //line 2
    cout << "║ ";
    cout << show;
    for(int i = 0; i < box_width - (int)show.size() - 1; i++) 
        cout << " ";
    cout << "║" << endl;

    //line 3
    cout << "╚";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╝" << RESET << endl;

}

void login_message(const string& name) {
    int box_width = 40;
    string show = name;
    show += " Login The Game.";
    //line 1
    cout << CYAN << "╔";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╗" << endl;

    //line 2
    cout << "║ ";
    cout << show;
    for(int i = 0; i < box_width - (int)show.size() - 1; i++) 
        cout << " ";
    cout << "║" << endl;

    //line 3
    cout << "╚";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╝" << RESET << endl;
       
}

void login_succ_message() {
    int box_width = 40;
    string show;
    show += " Login Successfully!";
    //line 1
    cout << GREEN << "╔";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╗" << endl;

    //line 2
    cout << "║ ";
    cout << show;
    for(int i = 0; i < box_width - (int)show.size() - 1; i++) 
        cout << " ";
    cout << "║" << endl;

    //line 3
    cout << "╚";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╝" << RESET << endl;
}

void show_all_players_board(vector<string> result) {
    cout << "╔═══════════════════════════════════╗" << endl;
    cout << "║            PLAYER LIST            ║" << endl;
    cout << "╠═══════════════════════════════════╣" << endl;

    int len = 38;

    for (int i = 0; i < (int)result.size(); i++) {

        string player_name = result[i].substr(0,result[i].size()-1);
        string status = result[i].substr(result[i].size()-1,result[i].size());
        if(status == "1") status = "battling";
        else if(status == "0") status = "waiting";

        // line 1
        string line = "║     Player " + to_string(i+1) + ": " + player_name + " | " + status;
        cout << line;
        for (int j = 0; j < len-(int)line.size(); j++) {
            cout << " ";
        }
        cout << "║" << endl;
        
        // line 2
        //cout << "║                          ║" << endl

    }

    cout << "╚═══════════════════════════════════╝" << endl;
}

void show_ranking_board(vector<string> result) {
    cout << "╔═════════════════════════════╗" << endl;
    cout << "║        RANKING BOARD        ║" << endl;
    cout << "╠═════════════════════════════╣" << endl;

    for (int i = 0; i < (int)result.size(); i++) {
        // 将排名的前三名颜色标注为黄色、银色和铜色
        string rankColor;
        if (i == 0) {
            rankColor = RED;
        } else if (i == 1) {
            rankColor = BLUE;
        } else if (i == 2) {
            rankColor = YELLOW;
        } else {
            rankColor = "\033[0m";
        }

        // 输出排名
        cout << "║          " << rankColor << i + 1 << ". " << result[i] << " " << "\033[0m";
        int spaceNum = 22 - result[i].length() - to_string(222).length() - 4;
        for (int j = 0; j < spaceNum; j++) {
            cout << " ";
        }
        cout << "║" << endl;
    }

    cout << "╚═════════════════════════════╝" << endl;
}

void battle_msg_this_round_win() {
    int box_width = 40;
    string show;
    show += " This Round You Win!";
    //line 1
    cout << YELLOW << "╔";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╗" << endl;

    //line 2
    cout << "║ ";
    cout << show;
    int len = box_width - show.size() - 1;
    for(int i = 0; i < len; i++) 
        cout << " ";
    cout << "║" << endl;

    //line 3
    cout << "╚";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╝" << RESET << endl;
}

void battle_msg_this_game_win() {
    int box_width = 40;
    string show;
    show += " You Win The Game!!! Congra!";
    //line 1
    cout << GREEN << "╔";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╗" << endl;

    //line 2
    cout << "║ ";
    cout << show;
    int len = box_width - show.size() - 1;
    for(int i = 0; i < len; i++) 
        cout << " ";
    cout << "║" << endl;

    //line 3
    cout << "╚";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╝" << RESET << endl;
}

void battle_msg_this_round_lose() {
    int box_width = 40;
    string show;
    show += " You Lose This Round...";
    //line 1
    cout << CYAN << "╔";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╗" << endl;

    //line 2
    cout << "║ ";
    cout << show;
    int len = box_width - show.size() - 1;
    for(int i = 0; i < len; i++) 
        cout << " ";
    cout << "║" << endl;

    //line 3
    cout << "╚";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╝" << RESET << endl;
}
void battle_msg_this_game_lose() {
    int box_width = 40;
    string show;
    show += " You Lose This Game T T...";
    //line 1
    cout << BLUE << "╔";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╗" << endl;

    //line 2
    cout << "║ ";
    cout << show;
    int len = box_width - show.size() - 1;
    for(int i = 0; i < len; i++) 
        cout << " ";
    cout << "║" << endl;

    //line 3
    cout << "╚";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╝" << RESET << endl;
}
void battle_msg_same_choice() {
    int box_width = 40;
    string show;
    show += " Same choice, again...";
    //line 1
    cout << MAGENTA << "╔";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╗" << endl;

    //line 2
    cout << "║ ";
    cout << show;
    int len = box_width - show.size() - 1;
    for(int i = 0; i < len; i++)  
        cout << " ";
    cout << "║" << endl;

    //line 3
    cout << "╚";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╝" << RESET << endl;
}

void battle_msg_target_in_battle(string target) {
    int box_width = 40;
    string show = target;
    show += " Target is in a battle, Try later";
    //line 1
    cout << YELLOW << "╔";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╗" << endl;

    //line 2
    cout << "║ ";
    cout << show;
    int len = box_width - show.size() - 1;
    for(int i = 0; i < len; i++) 
        cout << " ";
    cout << "║" << endl;

    //line 3
    cout << "╚";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╝" << RESET << endl;
}
void battle_msg_target_not_exist(string target) {
    int box_width = 40;
    string show = target;
    show += " Target not EXIST! Try again...";
    //line 1
    cout << YELLOW << "╔";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╗" << endl;

    //line 2
    cout << "║ ";
    cout << show;
    int len = box_width - show.size() - 1;
    for(int i = 0; i < len; i++) 
        cout << " ";
    cout << "║" << endl;

    //line 3
    cout << "╚";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╝" << RESET << endl;
}
void battle_msg_target_refused(string target) {
    int box_width = 40;
    string show = target;
    show += " Target REFUSED request! T T...";
    //line 1
    cout << YELLOW << "╔";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╗" << endl;

    //line 2
    cout << "║ ";
    cout << show;
    int len = box_width - show.size() - 1;
    for(int i = 0; i < len; i++) 
        cout << " ";
    cout << "║" << endl;

    //line 3
    cout << "╚";
    for(int i = 0; i < box_width; i++) 
        cout << "═";
    cout << "╝" << RESET << endl;
}
