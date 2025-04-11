#include <iostream>
#include <winsock2.h>
#include <thread>
#include <string>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

SOCKET clientSocket;
bool isChatting = false;
bool isConnected = false;

// 서버 메시지 수신
void receiveMessages() {
    char buffer[1024];
    while (isChatting) {
        memset(buffer, 0, sizeof(buffer));
        int recvLen = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (recvLen > 0) {
            std::string msg(buffer);
            std::cout << "\nMessage: " << msg << std::endl;
            std::cout << ">> ";
        }
    }
}

// 서버 연결
bool connectToServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9100);

    InetPton(AF_INET, L"127.0.0.1", &serverAddr.sin_addr);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server!" << std::endl;
        return false;
    }

    std::cout << "Successfully connected to server!" << std::endl;
    return true;
}

// 메시지 수신 및 출력
void getResponse() {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::cout << "Server response: " << buffer << std::endl;
}

// 회원가입
void registerUser() {
    std::string username, password;
    std::cout << "\n[Registration]\n";
    std::cout << "Username: ";
    std::getline(std::cin, username);
    std::cout << "Password: ";
    std::getline(std::cin, password);

    std::string regMsg = "REGISTER:" + username + ":" + password;
    send(clientSocket, regMsg.c_str(), regMsg.size(), 0);
    getResponse();
}

// 채팅 시작
void startChat() {
    isChatting = true;
    std::thread recvThread(receiveMessages);

    std::string input;
    while (true) {
        std::cout << ">> ";
        std::getline(std::cin, input);

        if (input == "/exit") {
            isChatting = false;
            break;
        }

        std::string chatMsg = "CHAT:" + input;
        send(clientSocket, chatMsg.c_str(), chatMsg.size(), 0);
    }

    recvThread.join();
}

// 로그인 및 후속 메뉴
void loginUser() {
    std::string username, password;
    std::cout << "\n[Login]\n";
    std::cout << "Username: ";
    std::getline(std::cin, username);
    std::cout << "Password: ";
    std::getline(std::cin, password);

    std::string loginMsg = "LOGIN:" + username + ":" + password;
    send(clientSocket, loginMsg.c_str(), loginMsg.size(), 0);

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::string response(buffer);
    std::cout << "Server response: " << response << std::endl;

    if (response.find("Login successful") != std::string::npos) {
        while (true) {
            std::cout << "\n=== Post-Login Menu ===\n";
            std::cout << "1. Start Chat\n";
            std::cout << "2. Logout\n";
            std::cout << "3. Exit\n";
            std::cout << "Select: ";
            int subChoice;
            std::cin >> subChoice;
            std::cin.ignore();

            if (subChoice == 1) {
                startChat();
            }
            else if (subChoice == 2) {
                std::string logoutMsg = "LOGOUT:";
                send(clientSocket, logoutMsg.c_str(), logoutMsg.length(), 0);
                getResponse();
                break; // Return to main menu
            }
            else if (subChoice == 3) {
                std::cout << "Client shutting down\n";
                closesocket(clientSocket);
                WSACleanup();
                exit(0);
            }
            else {
                std::cout << "Invalid input.\n";
            }
        }
    }
}

int main() {
    if (!connectToServer()) return 1;

    while (true) {
        std::cout << "\n========= Main Menu =========\n";
        std::cout << "1. Register\n";
        std::cout << "2. Login\n";
        std::cout << "0. Exit\n";
        std::cout << "Select: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();

        if (choice == 1) {
            registerUser();
        }
        else if (choice == 2) {
            loginUser();
        }
        else if (choice == 0) {
            std::cout << "Program terminated\n";
            break;
        }
        else {
            std::cout << "Invalid selection.\n";
        }
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}