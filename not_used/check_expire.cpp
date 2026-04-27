#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>

using namespace std;

struct Food {
    string id;
    string name;
    string expiry_date; // YYYY-MM-DD
    string owner;
};

// =======================
// change YYYY-MM-DD 转成 time_t
// =======================
time_t parseDate(const string &dateStr) {
    tm t = {};
    sscanf(dateStr.c_str(), "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday);

    t.tm_year -= 1900; 
    t.tm_mon -= 1;   

    return mktime(&t);
}

int daysUntilExpiry(const string &expiry) {
    time_t now = time(0);
    time_t exp = parseDate(expiry);

    double diff = difftime(exp, now);
    return diff / (60 * 60 * 24);
}


void checkFoodFile(const string &filename) {
    ifstream file(filename);

    if (!file.is_open()) {
        cerr << "Cannot open file\n";
        return;
    }

    string line;
    getline(file, line); // skip header

    while (getline(file, line)) {
        stringstream ss(line);
        Food f;

        getline(ss, f.id, ',');
        getline(ss, f.name, ',');
        getline(ss, f.expiry_date, ',');
        getline(ss, f.owner, ',');

        int daysLeft = daysUntilExpiry(f.expiry_date);

        cout << "Food: " << f.name << "\n";
        cout << "Owner: " << f.owner << "\n";
        cout << "Expiry: " << f.expiry_date << "\n";

        if (daysLeft < 0) {
            cout << "❌ EXPIRED\n";
        } 
        else if (daysLeft <= 2) {
            cout << "⚠️ Only " << daysLeft << " days left! Eat soon!\n";
        } 
        else {
            cout << "✅ " << daysLeft << " days left\n";
        }

        cout << "----------------------\n";
    }
}

// =======================
// main
// =======================
int main() {
    checkFoodFile("food.csv");
    return 0;
}