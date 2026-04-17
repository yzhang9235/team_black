from flask import Flask, request, jsonify
import database

app = Flask(__name__)

database.init_db()


@app.route("/food/<food_id>")
def food(food_id):

    item = database.get_food(food_id)

    if not item:
        return "Food not found", 404

    return jsonify(item)


@app.route("/add")
def add_food():

    food_id = request.args.get("id")
    name = request.args.get("name")
    expiry = request.args.get("expiry")
    owner = request.args.get("owner")

    if not all([food_id, name, expiry, owner]):
        return "Missing fields", 400

    database.add_food(food_id, name, expiry, owner)

    return {
        "status": "success",
        "id": food_id
    }


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=10000)