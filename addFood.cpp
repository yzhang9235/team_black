#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <string>

using namespace std;

struct Food {
    string id;
    string name;
    string expiry_date;
    string owner;
};

// trim helper (fixes hidden formatting issues)
string trim(const string &s) {
    int start = 0;
    int end = s.size() - 1;

    while (start <= end && s[start] == ' ') start++;
    while (end >= start && s[end] == ' ') end--;

    return s.substr(start, end - start + 1);
}

// read database
vector<Food> loadDatabase(const string &filename) {
    vector<Food> db;
    ifstream file(filename);

    if (!file.is_open()) return db;

    string line;
    getline(file, line); // skip header

    while (getline(file, line)) {
        stringstream ss(line);
        Food f;

        string id, name, expiry, owner;

        if (!getline(ss, id, ',')) continue;
        if (!getline(ss, name, ',')) continue;
        if (!getline(ss, expiry, ',')) continue;
        if (!getline(ss, owner, ',')) continue;

        f.id = trim(id);
        f.name = trim(name);
        f.expiry_date = trim(expiry);
        f.owner = trim(owner);

        db.push_back(f);
    }

    return db;
}

// save data
void saveDatabase(const vector<Food> &db, const string &filename) {
    ofstream file(filename);

    file << "id,name,expiry_date,owner\n";

    for (int i = 0; i < db.size(); i++) {
        file << db[i].id << ","
             << db[i].name << ","
             << db[i].expiry_date << ","
             << db[i].owner << "\n";
    }
}

int main(int argc, char *argv[]) {

    if (argc != 5) {
        cerr << "Usage: ./addFood id name expiry owner\n";
        return 1;
    }

    Food newFood;
    newFood.id = argv[1];
    newFood.name = argv[2];
    newFood.expiry_date = argv[3];
    newFood.owner = argv[4];

    vector<Food> db = loadDatabase("food.csv");

    db.push_back(newFood);

    saveDatabase(db, "food.csv");

    cout << "Food saved: " << newFood.name << endl;

    return 0;
}n