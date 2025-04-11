#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <mutex>
#include <map>
#include <sstream>
#include <mysql/jdbc.h>

#pragma comment(lib, "ws2_32.lib")

// ���� �� ����� ����
SOCKET serverSocket;
std::vector<SOCKET> clients;
std::map<SOCKET, int> clientUserMap;       // SOCKET -> user_id
std::map<SOCKET, std::string> clientNameMap;    // SOCKET -> username
std::mutex mtx;

sql::mysql::MySQL_Driver* driver;
std::unique_ptr<sql::Connection> con;

void handleClient(SOCKET clientSocket) {
    char buffer[1024];
    int userId = -1;
    std::string username;

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int recvLen = recv(clientSocket, buffer, sizeof(buffer), 0);

        // ���� �Ǵ� ���� ���� Ȯ��
        if (recvLen <= 0) {
            std::cout << "Client disconnected or error occurred: " << WSAGetLastError() << std::endl;
            break;
        }

        std::string msg(buffer);
        std::cout << "[SERVER] Received message: " << msg << std::endl;

        std::istringstream ss(msg);
        std::string cmd;
        getline(ss, cmd, ':');

        if (cmd == "REGISTER") {
            std::string regName, regPass;
            getline(ss, regName, ':');
            getline(ss, regPass);

            try {
                std::unique_ptr<sql::PreparedStatement> checkStmt(
                    con->prepareStatement("SELECT * FROM users WHERE username = ?")
                );
                checkStmt->setString(1, regName);
                std::unique_ptr<sql::ResultSet> checkRes(checkStmt->executeQuery());

                if (checkRes->next()) {
                    std::string response = "This username already exists.";
                    send(clientSocket, response.c_str(), response.length(), 0);
                }
                else {
                    std::unique_ptr<sql::PreparedStatement> insertStmt(
                        con->prepareStatement("INSERT INTO users (username, password) VALUES (?, ?)")
                    );
                    insertStmt->setString(1, regName);
                    insertStmt->setString(2, regPass);
                    insertStmt->executeUpdate();

                    std::string response = "Registration successful!";
                    send(clientSocket, response.c_str(), response.length(), 0);
                }
            }
            catch (sql::SQLException& e) {
                std::cerr << "ȸ������ ����: " << e.what() << std::endl;
                std::string errorMsg = "Database error occurred.";
                send(clientSocket, errorMsg.c_str(), errorMsg.length(), 0);
            }
        }

        else if (cmd == "LOGIN") {
            getline(ss, username, ':');
            std::string pass;
            getline(ss, pass);

            try {
                std::unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement("SELECT * FROM users WHERE username = ? AND password = ?")
                );
                pstmt->setString(1, username);
                pstmt->setString(2, pass);
                std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

                if (res->next()) {
                    userId = res->getInt("user_id");
                    mtx.lock();
                    clients.push_back(clientSocket);
                    clientUserMap[clientSocket] = userId;
                    clientNameMap[clientSocket] = username;
                    mtx.unlock();

                    std::string success = "Login successful! Welcome, " + username;
                    send(clientSocket, success.c_str(), success.length(), 0);
                    std::cout << "�α���: " << username << " (ID: " << userId << ")\n";
                }
                else {
                    std::string fail = "Login failed. Check your credentials.";
                    send(clientSocket, fail.c_str(), fail.length(), 0);
                }
            }
            catch (sql::SQLException& e) {
                std::cerr << "�α��� ����: " << e.what() << std::endl;
                std::string errorMsg = "Database error occurred.";
                send(clientSocket, errorMsg.c_str(), errorMsg.length(), 0);
            }
        }

        else if (cmd == "CHAT") {
            std::string content;
            getline(ss, content);

            if (userId != -1) {
                try {
                    std::unique_ptr<sql::PreparedStatement> pstmt(
                        con->prepareStatement("INSERT INTO message_log (sender_id, content) VALUES (?, ?)")
                    );
                    pstmt->setInt(1, userId);
                    pstmt->setString(2, content);
                    pstmt->executeUpdate();

                    std::string fullMsg = username + ": " + content;

                    mtx.lock();
                    for (SOCKET s : clients) {
                        if (send(s, fullMsg.c_str(), fullMsg.length(), 0) == SOCKET_ERROR) {
                            // �޽��� ���� ���� �� ó��
                            std::cerr << "Failed to send message to a client" << std::endl;
                        }
                    }
                    mtx.unlock();
                }
                catch (sql::SQLException& e) {
                    std::cerr << "ä�� ���� ����: " << e.what() << std::endl;
                }
            }
        }

        else if (cmd == "LOGOUT") {
            mtx.lock();
            clientUserMap.erase(clientSocket);
            clientNameMap.erase(clientSocket);
            clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
            mtx.unlock();

            std::string bye = "You have been logged out.";
            send(clientSocket, bye.c_str(), bye.length(), 0);
            std::cout << "�α׾ƿ�: " << username << std::endl;
            break;
        }
    }

    // ���� ���� ó��
    mtx.lock();
    clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
    clientUserMap.erase(clientSocket);
    clientNameMap.erase(clientSocket);
    mtx.unlock();

    closesocket(clientSocket);
    std::cout << "Ŭ���̾�Ʈ ���� �����\n";
}

int main() {
    try {
        // DB ����
        driver = sql::mysql::get_mysql_driver_instance();
        con.reset(driver->connect("tcp://127.0.0.1:3306", "root", "1234"));
        con->setSchema("hyun_project_db");
        std::cout << "DB connection successful\n";
    }
    catch (sql::SQLException& e) {
        std::cerr << "DB connection failed: " << e.what() << std::endl;
        return 1;
    }

    // Winsock �ʱ�ȭ
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9100);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // ���� ���ε� ���� ó�� �߰�
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Binding failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // ������ ���� ó�� �߰�
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server running on port 9100...\n";

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
            continue;
        }
        std::cout << "Client connected!\n";
        std::thread(handleClient, clientSocket).detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}