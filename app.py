import subprocess
from flask import Flask, request

app = Flask(__name__)

def load_database(filename):
    db = []
    with open(filename, "r") as f:
        lines = f.readlines()[1:]  # skip header

    for line in lines:
        parts = line.strip().split(",")
        if len(parts) == 4:
            db.append({
                "id": parts[0],
                "name": parts[1],
                "expiry_date": parts[2],
                "owner": parts[3]
            })

    return db


@app.route("/add")
def add_food():
    food_id = request.args.get("id")

    if not food_id:
        return "Missing id", 400

    db = load_database("food.csv")

    name = expiry = owner = None

    for item in db:
        if item["id"] == food_id:
            name = item["name"]
            expiry = item["expiry_date"]
            owner = item["owner"]
            break

    if not name:
        return "Food not found", 404

    subprocess.run([
        "./addFood",
        food_id,
        name,
        expiry,
        owner
    ])

    return f"Food name: {name}, Expiry: {expiry}, Owner: {owner}"

if __name__ == "__main__":
    app.run(debug=True)