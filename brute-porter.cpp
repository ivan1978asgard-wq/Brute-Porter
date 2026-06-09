#include <fstream>
#include <atomic>
#include <algorithm>
#include <iostream>
#include <libssh/libssh.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std;

constexpr int kSshTimeoutSeconds = 5;

struct Stats {
    int check = 0;
    int valid = 0;
    int bad = 0;
};

string trim(const string& value) {
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

vector<string> loadTargets(const string& filePath) {
    vector<string> targets;
    ifstream file(filePath);
    if (!file) {
        cerr << "[-] Can't open target file: " << filePath << endl;
        return targets;
    }

    string line;
    while (getline(file, line)) {
        const string ip = trim(line);
        if (!ip.empty()) targets.push_back(ip);
    }
    return targets;
}

bool authorizeSsh(const string& host, int port, const string& username, const string& password) {
    ssh_session session = ssh_new();
    if (!session) return false;

    ssh_options_set(session, SSH_OPTIONS_HOST, host.c_str());
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    ssh_options_set(session, SSH_OPTIONS_USER, username.c_str());

    const int timeout = kSshTimeoutSeconds;
    ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout);

    bool ok = false;
    if (ssh_connect(session) == SSH_OK) {
        const int auth = ssh_userauth_password(session, nullptr, password.c_str());
        ok = (auth == SSH_AUTH_SUCCESS);
    }

    ssh_disconnect(session);
    ssh_free(session);
    return ok;
}

void printStats(const Stats& stats) {
    cout << "\rcheck: " << stats.check
         << "  valid: " << stats.valid
         << "  bad: " << stats.bad << flush;
}

int main() {
    vector<string> targets;

    cout << "[1] Single host\n";
    cout << "[2] Host list file\n";
    cout << "[?] Choose mode (1-2): ";

    int mode = 0;
    cin >> mode;

    if (mode == 1) {
        string host;
        cout << "[?] Host: ";
        cin >> host;
        host = trim(host);
        if (!host.empty()) targets.push_back(host);
    } else if (mode == 2) {
        string targetFile;
        cout << "[?] Host list file path: ";
        cin >> targetFile;
        targets = loadTargets(targetFile);
    } else {
        cout << "[-] Invalid mode" << endl;
        return 1;
    }

    if (targets.empty()) {
        cout << "[-] No valid targets" << endl;
        return 1;
    }

    int port;
    string username;
    string password;
    int threadCount;

    cout << "[?] SSH port: ";
    cin >> port;
    cout << "[?] Username: ";
    cin >> username;
    cout << "[?] Password: ";
    cin >> password;
    cout << "[?] Thread count: ";
    cin >> threadCount;

    if (threadCount <= 0) {
        cout << "[-] Invalid thread count" << endl;
        return 1;
    }

    atomic<size_t> nextIndex(0);
    const size_t effectiveThreadCount =
        min(static_cast<size_t>(threadCount), targets.size());

    Stats stats;
    mutex statsMutex;
    vector<thread> workers;
    workers.reserve(effectiveThreadCount);

    for (size_t i = 0; i < effectiveThreadCount; i++) {
        workers.emplace_back([&]() {
            while (true) {
                const size_t index = nextIndex.fetch_add(1);
                if (index >= targets.size()) break;

                const bool success = authorizeSsh(targets[index], port, username, password);

                lock_guard<mutex> lock(statsMutex);
                stats.check++;
                if (success) stats.valid++;
                else stats.bad++;
                printStats(stats);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    cout << endl;
    return 0;
}
