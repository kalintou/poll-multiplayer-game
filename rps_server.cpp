/*
monitoring players
game status picture
send feedback to each player every game

username should be unique


*/

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
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <assert.h>


#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 4321
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MAX_POLL_FDS     1000

//rules
    //status
#define FAIL 0x11                   //login fail
#define SUCCESS 0x22                //login succ
#define READY_TO_BATTLE 0x33                //waiting status for player
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

struct Player {
    string name;
    int sfd;
    int status; // 0:waiting 1:battling
    int points;
};

class Game {
public:
    string p1;
    string p2;
    int p1_life;
    int p2_life;
    string p1_choice = "X";
    string p2_choice = "X";
    int round;
    int which_player(string name) {
        return p1 == name ? 1 : 2;
    }
    int find_game_by_player(string name) {
        return (p1 == name || p2 == name);
    }
    void update_choice(int player, string choice) {
        if(player == 1) p1_choice = choice;
        else if(player == 2) p2_choice = choice;
    }
    int go_result() {
        return (p1_choice != "X" && p2_choice != "X") ? 1 : 0;
    }
    void restart_round() {
        p1_choice = "X";
        p2_choice = "X";
    }
    int get_winner() {
        if(p1_choice == p2_choice) return 3;
        if(p1_choice == ROCK && p2_choice == SCISSORS)
            return 1;
        if(p1_choice == SCISSORS && p2_choice == PAPER)
            return 1;
        if(p1_choice == PAPER && p2_choice == ROCK)
            return 1;
        if(p1_choice == ROCK && p2_choice == PAPER)
            return 2;
        if(p1_choice == SCISSORS && p2_choice == ROCK)
            return 2;
        if(p1_choice == PAPER && p2_choice == SCISSORS)
            return 2;
        return -1;
    }
    int go_end() {
        return (!p1_life || !p2_life);
    }
};

// global data
struct sockaddr_in   addr;            /* server socket address */
int                  timeout;         /* poll timeout */
struct pollfd        fds[MAX_POLL_FDS];/* poll fds */
int                  nfds = 1;        /* The number of all poll-fd */
int                  listen_sd = -1;  /* listen socket fd */
int                  on = 1;          /* 1:non-blocking; 0:blocking */
int                  end_server = false; /* server running */

vector<Player> player;
vector<Game> game;

void add_client(int fd);
void del_client(int del_i);
void clean_client(void);
int player_name_exist(string target);
int play_in_battle(string target);
int find_player_fd_by_name(string target);
string find_player_name_by_fd(int sockfd);
void update_player_to_game(string target);
void update_player_to_not_game(string target);
Game* update_game(string name, string choice);
void del_game(Game* g);
void one_point_for_player(string p);
static bool cmp(const Player& p1, Player& p2) {
    return p1.points > p2.points;
}




int main(int argc, char *argv[]) {

    /*
    Player p1;
    p1.name = "dummy1";
    p1.sfd = -1;
    p1.status = 1;
    p1.points = 1;
    Player p2;
    p2.name = "dummy2";
    p2.sfd = -1;
    p2.status = 0;
    p2.points = 123;
    Player p3;
    p3.name = "dummy3";
    p3.sfd = -1;
    p3.status = 1;
    p3.points = 33;
    player.push_back(p1);
    player.push_back(p2);
    player.push_back(p3);
    */

    int rc;
    char buffer[BUFFER_SIZE];
    int current_size = 0;
    int new_sd = -1;//socket fd for new connection
    int sendbytes;
    int recvbytes;

    //Create and AF_INET stream socket to receive data
    listen_sd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_sd < 0) {
        cerr << "socket failed";
        exit(-1);
    }

    //set socket descriptor to be reuseable
    rc = setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    if(rc < 0) {
        cerr << "setsockopt failed";
        close(listen_sd);
        exit(-1);
    }

    //set socket to be nonblocking
    rc = ioctl(listen_sd, FIONBIO, (char *)&on);
    if(rc < 0) {
        cerr << "ioctl failed";
        close(listen_sd);
        exit(-1);
    }

    //bind the socket
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(SERVER_PORT);

    rc = bind(listen_sd, (struct sockaddr *)&addr, sizeof(addr));
    if(rc < 0) {
        cerr << "bind failed";
        close(listen_sd);
        exit(-1);
    }

    //set the listen backlog
    rc = listen(listen_sd, MAX_POLL_FDS - 1);
    if(rc < 0) {
        cerr << "listen failed";
        close(listen_sd);
        exit(-1);
    }

    //initialize the poll fds array
    memset(fds, 0, sizeof(fds));

    //initialize fds[0] socket
    fds[0].fd = listen_sd;
    fds[0].events = POLLIN;

    //set the poll timeout
    timeout = (3*60*1000);

    while(end_server == false) {

        //for(int i = 1; i <= (int)player.size(); i++) {
        //    cout << "player" << i << ": " << player[i-1].points << endl;
        //}

        //Call poll() and wait 3 minutes for it to complete.    
        cout << "Waiting on poll()...\n";
        rc = poll(fds, nfds, timeout);

        if (rc < 0) {
        cerr << RED << "  poll() failed" << RESET;
        break;
        }

        if(rc == 0) {
            cout << YELLOW << "  poll() timed out.\n" << RESET;
        }

        
        //One or more descriptors are readable.         
        //Need to determine which ones they are.                         
        current_size = nfds;
        for(int i = 0; i < current_size; i++) {
            /*
                Loop through to find the descriptors that returned
                POLLIN and determine whether it's the listening      
                or the active connection.                             
            */   

            //  If revents equal to 0,it means connection is inactive.
            //  So,find the next descriptors.
            if(fds[i].revents == 0) continue;
            
            //  If revents is not POLLIN, it's an unexpected result, 
            //  log and end the server. 
            if(fds[i].revents != POLLIN) {
                cout << "  Error! revents = " << fds[i].revents << "\n";
                //end_server = true;
                del_client(i);
                break;
            }

            // it's the listening scoket
            if(fds[i].fd == listen_sd) {
                do {
                    new_sd = accept(listen_sd, NULL, NULL);
                    if(new_sd < 0) {
                        if(errno != EWOULDBLOCK) {
                            cerr << "  accept() failed";
                            end_server = true;
                        }
                        break;
                    }
                    //add the new connection to poll array
                    add_client(new_sd);
                } while(new_sd != -1);
            }
            //  it's NOT the listening scoket, 
            //  therefore an existing connection must be readable
            else {
                //  Receive all incoming data on this socket           
                //  before we loop back and call poll again.           
                while(1) {
                    /*
                        Receive data on this connection until the      
                        recv fails with EWOULDBLOCK. If any other       
                        failure occurs, we will close the                
                        connection.                                      
                    */
                    memset(buffer,0,sizeof(buffer));
                    rc = recv(fds[i].fd, buffer, sizeof(buffer), 0);
                    if(rc < 0) {
                        if(errno != EWOULDBLOCK) {
                            cerr << "  recv() failed";
                            del_client(i);
                        }
                        //cout << "  errno == EWOULDBLOCK.\n";
                        break;
                    }

                    
                    // Check to see if the connection has been closed by the client                             
                    if (rc == 0) {
                        string exit_name = find_player_name_by_fd(fds[i].fd);

                        //if user name is valid 
                        if(!exit_name.empty() && exit_name != " " && exit_name != "  ") {
                            cout << RED << "The connection has been closed by " << find_player_name_by_fd(fds[i].fd) << RESET << endl;
                            memset(buffer,0,sizeof(buffer));
                            buffer[0] = EXIT_MSG;
                            strcpy(buffer+1,find_player_name_by_fd(fds[i].fd).c_str());
                            for(int k = 0; k < (int)player.size(); k++) {//sfd -1 is 
                                if((player[k].sfd != fds[i].fd) && player[k].sfd != -1 && player[k].status == 0) {
                                    rc = write(player[k].sfd, buffer, sizeof(buffer));
                                    if(rc < 0) cerr << "  send() failed";
                                    else cout << GREEN << "send message to " << player[k].name << " successfully" << RESET << "\n";
                                }
                            }
                        }

                        del_client(i);//also del client in vec
                        break;
                    }

                    // data was received
                    int opt = buffer[0];
                    switch (opt){
                        case LOGIN: {
                            //login checker
                            // data was received
                            assert(buffer[0] == LOGIN);
                            string login_name(buffer);
                            login_name = login_name.substr(1,login_name.size());
                            //login fail, name existed
                            if(player_name_exist(login_name)) {
                                cout << "Player name existed, send back...\n";
                                memset(buffer,0,sizeof(buffer));
                                buffer[0] = LOGIN;
                                buffer[1] = FAIL;
                                rc = send(fds[i].fd, buffer, strlen(buffer), 0);
                                if(rc < 0) {
                                    cerr << "  send() failed";
                                    //del_client(i);
                                    break;
                                }
                            } 
                            /*
                            else if(login_name == "" || login_name == " ") {
                                cout << "Player cannot choose this name, send back...\n";
                                memset(buffer,0,sizeof(buffer));
                                buffer[0] = LOGIN;
                                buffer[1] = FAIL;
                                buffer[1] = FAIL;
                                rc = send(fds[i].fd, buffer, strlen(buffer), 0);
                                if(rc < 0) {
                                    cerr << "  send() failed";
                                    //del_client(i);
                                    break;
                                }
                            }
                            */
                            else {//login OK
                                cout << CYAN << "New player: " << login_name << " Login" << RESET << endl;
                                memset(buffer,0,sizeof(buffer));
                                buffer[0] = LOGIN;
                                buffer[1] = SUCCESS;
                                rc = send(fds[i].fd, buffer, strlen(buffer), 0);
                                if(rc < 0) {
                                    cerr << "  send() failed";
                                    //del_client(i);
                                    break;
                                }
                                Player new_player;
                                new_player.name = login_name;
                                new_player.sfd = fds[i].fd;
                                new_player.status = 0;
                                new_player.points = 0;
                                player.push_back(new_player);

                                //send player login message to all
                                //cout << "send login message to all\n";
                                memset(buffer,0,sizeof(buffer));
                                buffer[0] = LOGIN_MSG;
                                strcpy(buffer+1, login_name.c_str());

                                for(int k = 0; k < (int)player.size(); k++) {
                                    if(player[k].name != login_name && player[k].status != 1 && player[k].sfd != -1) {
                                        rc = write(player[k].sfd, buffer, strlen(buffer));
                                        if(rc < 0) {
                                            cerr << "   send() failed";
                                        }
                                        else cout << GREEN << "send player login successful message to " << player[k].name << RESET << endl;
                                    }
                                }


                            }                
                            break;
                        }
                        case SHOW_ALL_PLAYERS: {
                            memset(buffer,0,sizeof(buffer));
                            int count = 0;
                            string pkt;
                            for(int j = 0; j < (int)player.size(); j++) {
                                pkt += player[j].name; 
                                pkt += to_string(player[j].status);
                                if(j != (int)player.size()-1) {
                                    pkt += ','; count++;
                                }

                                count += player[j].name.size();
                                count += 1;

                            }
                 
                            buffer[0] = SHOW_ALL_PLAYERS;
                            buffer[1] = (char)count;
                            strcpy(buffer+2,pkt.c_str());

                            if((sendbytes = send(fds[i].fd, buffer, strlen(buffer), 0)) < 0) {
                                cerr << "SHOW ALL PLAYERS send failed..., exit\n";
                                exit(1);
                            } else{
                                cout << CYAN << "SHOW ALL PLAYERS from " << find_player_name_by_fd(fds[i].fd) <<\
                                     " and send back OK." << RESET << endl;
                            }

                            break;
                        }
                        case SHOW_RANKING: {
                            memset(buffer,0,sizeof(buffer));

                            vector<Player> tmp = player;
                            sort(tmp.begin(), tmp.end(), cmp);
                            string pkt;
                            for(int j = 0; j < (int)tmp.size(); j++) {
                                pkt += tmp[j].name;
                                if(j != (int)tmp.size()-1) pkt += ',';
                            }
                            
                            int count = 0;
                            count += tmp.size()-1;
                            for(int j = 0; j < (int)tmp.size(); j++) {
                                count += tmp[j].name.size();
                            }

                            buffer[0] = SHOW_RANKING;
                            buffer[1] = (char)count;

                            strcpy(buffer+2,pkt.c_str());

                            if((sendbytes = send(fds[i].fd, buffer, strlen(buffer), 0)) < 0) {
                                cerr << "SHOW RANKING send failed..., exit\n";
                                exit(1);
                            } else{
                                cout << CYAN << "SHOW RANKING from " << find_player_name_by_fd(fds[i].fd) <<\
                                     " and send back OK." << RESET << endl;
                            }
                            break;    
                        }
                        case BATTLE_REQ: {
                            assert(buffer[0] == BATTLE_REQ);
                            string target_name(buffer);
                            target_name = target_name.substr(1,target_name.size());
                            //if target do not exist
                            memset(buffer,0,sizeof(buffer));
                            if(!player_name_exist(target_name)) {
                                buffer[0] = BATTLE_REQ;
                                buffer[1] = NOT_EXIST;
                                if((sendbytes = send(fds[i].fd, buffer, strlen(buffer), 0)) < 0) {
                                    cerr << "send failed";
                                    exit(1);
                                } else{
                                    cout << CYAN << "send BATTLE_REQ and NOT_EXIST" << RESET << endl;
                                }
                                break;
                            }
                            //if target is in a battle
                            else if(play_in_battle(target_name)) {
                                buffer[0] = BATTLE_REQ;
                                buffer[1] = IN_BATTLE;
                                if((sendbytes = send(fds[i].fd, buffer, strlen(buffer), 0)) < 0) {
                                    cerr << "send failed";
                                    exit(1);
                                } else{
                                    cout << CYAN << "BATTLE_REQ and IN_BATTLE" << RESET << endl;
                                }
                                break;
                            }
                            //send a battle request to target
                            else {
                                buffer[0] = BATTLE_REQ;
                                string myname = find_player_name_by_fd(fds[i].fd);

                                strcpy(buffer+1,myname.c_str());
                                int target_fd = find_player_fd_by_name(target_name);
                                assert(target_fd != -1);
                                if((sendbytes = send(target_fd, buffer, strlen(buffer),0)) < 0) {
                                    cerr << "send failed";
                                    exit(1);
                                } else{
                                    cout << CYAN << "BATTLE_REQ and send request to target" << RESET << endl;
                                }
                            }
                            break;
                        }
                        case WAITING_RESP: {
                            assert(buffer[0] == WAITING_RESP);
                            assert(buffer[1] == REFUSED || buffer[1] == READY_TO_BATTLE);
                            if(buffer[1] == REFUSED) {
                                cout << "The target player refused...\n";
                                string reply_name(buffer);
                                reply_name = reply_name.substr(2,reply_name.size());
                                int target_fd = find_player_fd_by_name(reply_name);
                                memset(buffer,0,sizeof(buffer));
                                buffer[0] = WAITING_RESP;
                                buffer[1] = REFUSED;
                                if((sendbytes = write(target_fd, buffer, sizeof(buffer))) < 0) {
                                    cerr << "send failed";
                                    exit(1);
                                } else{
                                    cout << GREEN << "send refused message to " << reply_name << RESET << endl;
                                }
                            }
                            else if(buffer[1] == READY_TO_BATTLE) {
                                cout << "The target player OK for a new BATTLE\n";
                                
                                string recvdata(buffer);
                                recvdata = recvdata.substr(2,recvdata.size());
                                vector<string> result;
                                stringstream ss(recvdata);
                                string item;
                                while(getline(ss,item,',')) result.push_back(item);
                                string receiver = result[0];
                                string sender = result[1];

                                int target_fd = find_player_fd_by_name(sender);
                                memset(buffer,0,sizeof(buffer));
                                buffer[0] = WAITING_RESP;
                                buffer[1] = READY_TO_BATTLE;
                                if((sendbytes = write(target_fd, buffer, sizeof(buffer))) < 0) {
                                    cerr << "send failed";
                                    exit(1);
                                } else{
                                    cout << GREEN << "Start battle between " << receiver << " and " << sender << RESET << endl;
                                }

                                //add two player to a game
                                Game newGame;
                                newGame.p1 = receiver;
                                newGame.p1_life = PLAYER_LIFE;
                                newGame.p2 = sender;
                                newGame.p2_life = PLAYER_LIFE;
                                game.push_back(newGame);

                                //update player status
                                update_player_to_game(receiver);
                                update_player_to_game(sender);

                            }
                                      
                            break;
                        }
                        case BATTLING: {
                            cout << "recv: " << buffer << endl;
                            string recvdata(buffer);
                            recvdata = recvdata.substr(1,recvdata.size());
                            vector<string> result;
                            stringstream ss(recvdata);
                            string item;
                            while(getline(ss,item,',')) result.push_back(item);
                            string player_name = result[0];
                            string player_choice = result[1];
                            //need to send
                            Game* curGame;
                            if(curGame = update_game(player_name,player_choice)) {
                                memset(buffer,0,sizeof(buffer));
                                int winner = curGame->get_winner();
                                string senddata = "";
                                if(winner == 1) {//p1 win
                                    senddata += curGame->p1 + "," + curGame->p1 + "," + curGame->p2;
                                    curGame->p2_life--;
                                }
                                else if (winner == 2) {//p2 win
                                    senddata += curGame->p2 + "," + curGame->p1 + "," + curGame->p2;
                                    curGame->p1_life--;
                                }
                                else if(winner == 3) {//no winner, again
                                    senddata += "3,"+curGame->p1 + "," + curGame->p2;
                                }
                                
                                if(curGame->go_end()) {
                                    buffer[0] = END_BATTLE;
                                    //delete game
                                    del_game(curGame);
                                    if(winner == 1) 
                                        one_point_for_player(curGame->p1);
                                    else if (winner == 2)
                                        one_point_for_player(curGame->p2);
                                }
                                else {
                                    buffer[0] = BATTLING;
                                    curGame->restart_round();
                                }
                                strcpy(buffer+1,senddata.c_str());

                                int p1_fd = find_player_fd_by_name(curGame->p1);
                                int p2_fd = find_player_fd_by_name(curGame->p2);
                                //send to p1
                                if((sendbytes = send(p1_fd, buffer, strlen(buffer), 0)) < 0) {
                                    cerr << "send failed";
                                    exit(1);
                                } 
                                else cout << GREEN << "send battle message to p1 " << RESET << endl;
                                //send to p2
                                if((sendbytes = send(p2_fd, buffer, strlen(buffer), 0)) < 0) {
                                    cerr << "send failed";
                                    exit(1);
                                } 
                                else cout << GREEN << "send battle message to p2 " << RESET << endl;                            }
                            break;
                        }
                        default: {
                            //cout << "Invalid option, Try again\n";
                            break;
                        }  
                    }
                    
                    // Echo the data back to the client
                    /*
                    rc = send(fds[i].fd, buffer, len, 0);
                    if(rc < 0) {
                        cerr << "  send() failed";
                        del_client(i);
                        break;
                    }
                    */
                }
            }
        }
    }
    
    // Clean up all of the sockets that are open                
    clean_client();
    return 0;
}









// Add the new socket fd to poll fds array
void add_client(int fd) {
    //set the socket to be nonblocking
    int rc = ioctl(fd, FIONBIO, (char *)&on);
    if(rc < 0) {
        cerr << "ioctl failed";
        close(listen_sd);
        exit(-1);
    }

    fds[nfds].fd = fd;
    fds[nfds].events = POLLIN;

    nfds++;
    if(nfds > MAX_POLL_FDS) nfds = MAX_POLL_FDS;
}


// Delete the socket fd based on the index(del_i)
void del_client(int del_i){
    int tmp = fds[del_i].fd;
    close(fds[del_i].fd);
    for(int j = del_i; j < nfds; j++) {
        fds[j].fd = fds[j+1].fd;
    }
    nfds--;

    //del in vec
    
    for(int j = 0; j < (int)player.size(); j++) {
        if(player[j].sfd == tmp) {
            player.erase(player.begin()+j);
            break;
        }
    }

    //bug?
    for(int j = 0; j < (int)player.size(); j++) {
        if(player[j].name == "") {
            player.erase(player.begin()+j);
            break;
        }
    }
}


// Clean up all of the sockets that are open                

void clean_client(void){
  for (int i = 0; i < nfds; i++) {
    if(fds[i].fd >= 0) close(fds[i].fd);
  }
}

int player_name_exist(string target) {
    for(int i = 0; i < (int)player.size(); i++) {
        if(player[i].name == target) return 1;
    }
    return 0;
}

int play_in_battle(string target) {
    for(int i = 0; i < (int)player.size(); i++) {
        if(player[i].name == target && player[i].status == 1) return 1;
    }
    return 0;
}

int find_player_fd_by_name(string target) {
    for(int i = 0; i < (int)player.size(); i++) {
        if(player[i].name == target) return player[i].sfd;
    }
    return -1;
}

string find_player_name_by_fd(int sockfd) {
    for(int i = 0; i < (int)player.size(); i++) {
        if(player[i].sfd == sockfd) return player[i].name;
    }
    return "";
}

void update_player_to_game(string target) {
    for(int i = 0; i < (int)player.size(); i++) {
        if(player[i].name == target) {
            player[i].status = 1;
            break;
        }
    }
}

void update_player_to_not_game(string target) {
    for(int i = 0; i < (int)player.size(); i++) {
        if(player[i].name == target) {
            player[i].status = 0;
            break;
        }
    }
}

Game* update_game(string name, string choice) {
    Game* curGame;
    int player = 0;
    for(int i = 0; i < (int)game.size(); i++) {
        if(game[i].find_game_by_player(name)) {
            curGame = &game[i];
            break;
        }
    }
    player = curGame->which_player(name);
    curGame->update_choice(player, choice);
    if(curGame->go_result()) return curGame;
    return NULL;    
}

void del_game(Game* g) {
    update_player_to_not_game(g->p1);
    update_player_to_not_game(g->p2);
    for(int i = 0; i < (int)game.size(); i++) {
        if(&game[i] == g) {
            game.erase(game.begin()+i);
            break;
        }
    }
}

void one_point_for_player(string p) {
    for(int i = 0; i < (int)player.size(); i++) {
        if(player[i].name == p) {
            player[i].points++;
            break;
        }
    }
}