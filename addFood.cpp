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

        getline(ss, f.id, ',');
        getline(ss, f.name, ',');
        getline(ss, f.expiry_date, ',');
        getline(ss, f.owner, ',');

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

// receive data from python program
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
}