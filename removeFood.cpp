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

vector<Food> loadDatabase(const string &filename) {
    vector<Food> db;
    ifstream file(filename);

    string line;
    getline(file, line); // skip header

    while (getline(file, line)) {
        stringstream ss(line);
        Food f;

        getline(ss, f.id, ',');
        getline(ss, f.name, ',');
        getline(ss, f.expiry_date, ',');
        getline(ss, f.owner, ',');

        db.push_back(f);
    }

    return db;
}

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
    if (argc != 2) {
        cerr << "Usage: ./removeFood id\n";
        return 1;
    }

    string targetId = argv[1];

    vector<Food> db = loadDatabase("food.csv");
    vector<Food> newDb;

    bool found = false;

    for (int i = 0; i < db.size(); i++) {
        if (db[i].id == targetId) {
            found = true; // skip this one
        } else {
            newDb.push_back(db[i]);
        }
    }

    saveDatabase(newDb, "food.csv");

    if (found) {
        cout << "Food removed successfully.\n";
    } else {
        cout << "Food not found.\n";
    }

    return 0;
}


//检查link有没有link到之前的东西
//啊不要delete