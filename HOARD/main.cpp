#include <vector>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <cstdlib>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

const string DB_URL =
"https://hoard-39f9c-default-rtdb.asia-southeast1.firebasedatabase.app";

struct Player {
    string name;
    double units = 0.0;
    double rate = 1.0;
    long long lastSaved = 0;
    double idleBonus = 0.0;
    double doubleInvestChance = 0.0;
};

long long now() {
    return time(nullptr);
}

void saveGame(const Player& p) {
    ofstream f("save.txt");
    f << p.name << "\n"
        << p.units << "\n"
        << p.rate << "\n"
        << now() << "\n"
        << p.idleBonus << "\n"
        << p.doubleInvestChance << "\n";
}

bool loadGame(Player& p) {
    ifstream f("save.txt");
    if (!f) return false;

    long long old;
    f >> p.name >> p.units >> p.rate >> old
        >> p.idleBonus >> p.doubleInvestChance;

    long long elapsed = now() - old;
    double efficiency = 0.10 + min(p.idleBonus, 1.0);
    p.units += p.rate * elapsed * efficiency;
    p.lastSaved = now();
    return true;
}

void idleGain(Player& p) {
    while (true) {
        this_thread::sleep_for(chrono::seconds(1));
        double multiplier = 1.0 + min(p.idleBonus, 1.0);
        p.units += p.rate * multiplier;
    }
}

void invest(Player& p, double amount) {
    if (p.units < amount) {
        cout << "Not enough units.\n";
        return;
    }

    p.units -= amount;

    double gain = (amount * amount) / (amount + 1000.0);
    double rateIncrease = gain * 0.001;

    double roll = (double)rand() / RAND_MAX;
    if (roll < min(p.doubleInvestChance, 0.05)) {
        rateIncrease *= 2.0;
        cout << "Lucky! Investment doubled.\n";
    }

    p.rate += rateIncrease;
}

size_t writeCallback(void* c, size_t s, size_t n, string* out) {
    out->append((char*)c, s * n);
    return s * n;
}

json fetchLeaderboard() {
    CURL* curl = curl_easy_init();
    string response;

    curl_easy_setopt(curl, CURLOPT_URL,
        (DB_URL + "/leaderboard.json").c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (response == "null")
        return json::object();

    return json::parse(response);
}

void uploadScore(const Player& p) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    string url = DB_URL + "/leaderboard/" + p.name + ".json";
    string payload = to_string(p.units);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
}

void showLeaderboard(const json& board) {
    vector<pair<string, double>> entries;

    for (auto& [name, units] : board.items())
        entries.push_back({ name, units });

    sort(entries.begin(), entries.end(),
        [](auto& a, auto& b) {
            return a.second > b.second;
        });

    cout << "\nTOP 10\n";
    for (int i = 0; i < entries.size() && i < 10; i++) {
        cout << i + 1 << ". "
            << entries[i].first << " : "
            << entries[i].second << "\n";
    }
}

void unlockMenu(Player& p) {
    cout << "\nUNLOCKS\n";
    cout << "Idle Bonus: " << p.idleBonus * 100 << "%\n";
    cout << "Double Invest Chance: " << p.doubleInvestChance * 100 << "%\n\n";

    cout << "1. +5% Idle Gain (5000 units)\n";
    cout << "2. +0.05% Double Invest Chance (8000 units)\n";
    cout << "0. Back\n";
    cout << "Choice: ";

    int c;
    cin >> c;

    if (c == 1 && p.idleBonus < 1.0 && p.units >= 5000) {
        p.units -= 5000;
        p.idleBonus += 0.05;
        cout << "Idle gain upgraded.\n";
    }
    else if (c == 2 && p.doubleInvestChance < 0.05 && p.units >= 8000) {
        p.units -= 8000;
        p.doubleInvestChance += 0.0005;
        cout << "Investment luck upgraded.\n";
    }
}

void sanitizeName(string& name) {
    for (char& c : name)
        if (c == ' ') c = '_';
}

bool loadSavedName(string& savedName) {
    ifstream f("save.txt");
    if (!f) return false;
    getline(f, savedName);
    return true;
}

int main() {
    srand((unsigned)time(nullptr));

    Player p;
    json leaderboard = fetchLeaderboard();

    string savedName;
    bool hasSave = loadSavedName(savedName);

    while (true) {
        cout << "Enter player name: ";
        cin >> p.name;
        sanitizeName(p.name);

        if (!leaderboard.contains(p.name))
            break;

        if (hasSave && p.name == savedName)
            break;

        cout << "Name already taken by another player. Try again.\n";
    }

    if (!loadGame(p)) {
        p.units = 0;
        p.rate = 1;
    }

    thread idleThread(idleGain, ref(p));

    while (true) {
        cout << "\n----------------------------------\n";
        cout << "Units: " << p.units << "\n";
        cout << "Rate : " << p.rate << " / sec\n";
        cout << "----------------------------------\n";
        cout << "1. Invest\n";
        cout << "2. View Leaderboard\n";
        cout << "3. Unlocks\n";
        cout << "4. Save and Exit\n";
        cout << "Choice: ";

        int choice;
        cin >> choice;

        if (choice == 1) {
            double amount;
            cout << "Amount: ";
            cin >> amount;
            invest(p, amount);
        }
        else if (choice == 2) {
            leaderboard = fetchLeaderboard();
            showLeaderboard(leaderboard);
            cout << "\nPress Enter...";
            cin.ignore();
            cin.get();
        }
        else if (choice == 3) {
            unlockMenu(p);
        }
        else if (choice == 4) {
            saveGame(p);
            uploadScore(p);
            leaderboard = fetchLeaderboard();
            showLeaderboard(leaderboard);
            break;
        }
        else {
            cout << "Invalid choice.\n";
        }
    }

    idleThread.detach();
    return 0;
}