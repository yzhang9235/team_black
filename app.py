from flask import Flask, request
import subprocess
import os

app = Flask(__name__)

def load_database(filename):
    db = []
    with open(filename, "r") as f:
        lines = f.readlines()[1:]

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


def get_food_by_id(food_id):
    for item in load_database("food.csv"):
        if item["id"] == food_id:
            return item
    return None


@app.route("/food/<food_id>")
def food(food_id):
    item = get_food_by_id(food_id)
    if not item:
        return "Food not found", 404
    return item


@app.route("/add")
def add_food():
    food_id = request.args.get("id")

    if not food_id:
        return "Missing id", 400

    item = get_food_by_id(food_id)

    if not item:
        return "Food not found", 404

    subprocess.run([
        "./addFood",
        item["id"],
        item["name"],
        item["expiry_date"],
        item["owner"]
    ])

    return {"status": "success", "food": item}


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 10000))
    app.run(host="0.0.0.0", port=port)