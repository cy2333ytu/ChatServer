#include "group.hpp"
#include "user.hpp"
#include "public.hpp"
#include "json.hpp"

#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <functional>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>
using namespace std;
using json = nlohmann::json;
using namespace ccy;
// Record information of the current logged-in user
User g_currentUser;

// Record the friend list information of the currently logged-in user
vector<User> g_currentUserFriendList;

// Record the group list information of the currently logged-in user
vector<Group> g_currentUserGroupList;

// Control the main menu program
bool isMainMenuRunning = false;

// Semaphore for communication between read and write threads
sem_t rwsem;

// Record login status
atomic_bool g_isLoginSuccess{false};

// Receive thread
void readTaskHandler(int clientfd);
// Get system time (used for adding time information to chat messages)
string getCurrentTime();
// Main chat menu program
void mainMenu(int);
// Display basic information of the currently logged-in user
void showCurrentUserData();

// Chat client program implementation, main thread used for sending messages,
// and a child thread used for receiving messages
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "Command invalid! Example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // Parse IP and port passed through command line arguments
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // Create client socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "Socket create error" << endl;
        exit(-1);
    }

    // Fill in server information (IP + port) that the client needs to connect to
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // Connect the client to the server
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "Connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // Initialize semaphore for communication between read and write threads
    sem_init(&rwsem, 0, 0);

    // Connection to the server successful, start the receiving thread
    std::thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    // Main thread responsible for receiving user input and sending data
    for (;;)
    {
        // Display main menu options: login, register, quit
        cout << "========================" << endl;
        cout << "1. Login" << endl;
        cout << "2. Register" << endl;
        cout << "3. Quit" << endl;
        cout << "========================" << endl;
        cout << "Choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // Read and discard any remaining newline characters in the buffer

        switch (choice)
        {
        case 1: // Login business
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "UserID:";
            cin >> id;
            cin.get(); // Read and discard any remaining newline characters in the buffer
            cout << "User Password:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            g_isLoginSuccess = false;

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "Send login msg error:" << request << endl;
            }

            sem_wait(&rwsem); // Wait for the semaphore, notified by the child thread after processing the login response

            if (g_isLoginSuccess)
            {
                // Enter the main chat menu
                isMainMenuRunning = true;
                mainMenu(clientfd);
            }
        }
        break;
        case 2: // Register business
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "Username:";
            cin.getline(name, 50);
            cout << "User Password:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "Send reg msg error:" << request << endl;
            }

            sem_wait(&rwsem); // Wait for the semaphore, notified by the child thread after processing the registration response
        }
        break;
        case 3: // Quit business
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        default:
            cerr << "Invalid input!" << endl;
            break;
        }
    }

    return 0;
}

// Process response logic for registration
void doRegResponse(json &responsejs)
{
    if (0 != responsejs["errno"].get<int>()) // Registration failed
    {
        cerr << "Name is already exist, register error!" << endl;
    }
    else // Registration successful
    {
        cout << "Name register success, UserID is " << responsejs["id"]
             << ", do not forget it!" << endl;
    }
}

// Process response logic for login
void doLoginResponse(json &responsejs)
{
    if (0 != responsejs["errno"].get<int>()) // Login failed
    {
        cerr << responsejs["errmsg"] << endl;
        g_isLoginSuccess = false;
    }
    else // Login successful
    {
        // Record the current user's ID and name
        g_currentUser.setId(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);

        // Record the friend list information of the current user
        if (responsejs.contains("friends"))
        {
            // Initialize
            g_currentUserFriendList.clear();

            vector<string> vec = responsejs["friends"];
            for (string &str : vec)
            {
                json js = json::parse(str);
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }

        // Record the group list information of the current user
        if (responsejs.contains("groups"))
        {
            // Initialize
            g_currentUserGroupList.clear();

            vector<string> vec1 = responsejs["groups"];
            for (string &groupstr : vec1)
            {
                json grpjs = json::parse(groupstr);
                Group group;
                group.setId(grpjs["id"].get<int>());
                group.setName(grpjs["groupname"]);
                group.setDesc(grpjs["groupdesc"]);

                vector<string> vec2 = grpjs["users"];
                for (string &userstr : vec2)
                {
                    GroupUser user;
                    json js = json::parse(userstr);
                    user.setId(js["id"].get<int>());
                    user.setName(js["name"]);
                    user.setState(js["state"]);
                    user.setRole(js["role"]);
                    group.getUsers().push_back(user);
                }

                g_currentUserGroupList.push_back(group);
            }
        }

        // Show basic information of the logged-in user
        showCurrentUserData();

        // Display offline messages for the current user - personal chat messages or group messages
        if (responsejs.contains("offlinemsg"))
        {
            vector<string> vec = responsejs["offlinemsg"];
            for (string &str : vec)
            {
                json js = json::parse(str);
                // time + [id] + name + " said: " + xxx
                if (ONE_CHAT_MSG == js["msgid"].get<int>())
                {
                    cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                         << " said: " << js["msg"].get<string>() << endl;
                }
                else
                {
                    cout << "Group message [" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                         << " said: " << js["msg"].get<string>() << endl;
                }
            }
        }

        g_isLoginSuccess = true;
    }
}

// Subthread - Receive thread
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0); // Blocking
        if (-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }

        // Receive data forwarded by ChatServer, deserialize to generate a json data object
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == msgtype)
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue;
        }

        if (GROUP_CHAT_MSG == msgtype)
        {
            cout << "Group message [" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue;
        }

        if (LOGIN_MSG_ACK == msgtype)
        {
            doLoginResponse(js); // Process the business logic of login response
            sem_post(&rwsem);    // Notify the main thread that login result processing is complete
            continue;
        }

        if (REG_MSG_ACK == msgtype)
        {
            doRegResponse(js);
            sem_post(&rwsem); // Notify the main thread that registration result processing is complete
            continue;
        }
    }
}

// Display basic information of the currently logged-in user
void showCurrentUserData()
{
    cout << "======================Login User======================" << endl;
    cout << "Current login user => ID:" << g_currentUser.getId() << " Name:" << g_currentUser.getName() << endl;
    cout << "----------------------Friend List---------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------------Group List----------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
}

// "help" command handler
void help(int fd = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void loginout(int, string);

// System-supported client command list
unordered_map<string, string> commandMap = {
    {"help", "Display all supported commands, format: help"},
    {"chat", "One-to-one chat, format: chat:friendid:message"},
    {"addfriend", "Add friend, format: addfriend:friendid"},
    {"creategroup", "Create group, format: creategroup:groupname:groupdesc"},
    {"addgroup", "Join group, format: addgroup:groupid"},
    {"groupchat", "Group chat, format: groupchat:groupid:message"},
    {"loginout", "Logout, format: loginout"}};

// Registering client command handlers
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

// Main chat menu program
void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; // Store command
        int idx = commandbuf.find(":");
        if (-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "Invalid input command!" << endl;
            continue;
        }

        // Call the event handling callback for the respective command, mainMenu is closed to modification, adding new functionality does not require modifying this function
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx)); // Call the command processing method
    }
}

// "help" command handler
void help(int, string)
{
    cout << "Show command list >>> " << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

// "addfriend" command handler
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "Send addfriend msg error -> " << buffer << endl;
    }
}

// "chat" command handler
void chat(int clientfd, string str)
{
    int idx = str.find(":"); // friendid:message
    if (-1 == idx)
    {
        cerr << "Chat command invalid!" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "Send chat msg error -> " << buffer << endl;
    }
}

// "creategroup" command handler groupname:groupdesc
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "Creategroup command invalid!" << endl;
        return;
    }

    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "Send creategroup msg error -> " << buffer << endl;
    }
}

// "addgroup" command handler
void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "Send addgroup msg error -> " << buffer << endl;
    }
}

// "groupchat" command handler groupid:message
void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "Groupchat command invalid!" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "Send groupchat msg error -> " << buffer << endl;
    }
}

// "loginout" command handler
void loginout(int clientfd, string)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "Send loginout msg error -> " << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}

// Get system time (chat messages need to add timestamp information)
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}
