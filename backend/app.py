from flask import Flask, request, jsonify
import database
import claude_ai
import uuid

app = Flask(__name__)

# Initialize database on server start
database.init_db()


# =====================================================
# Health Check Endpoint
# =====================================================
@app.route("/", methods=["GET"])
def home():
    """
    Simple health check to verify server is running.
    """
    return jsonify({"status": "backend running"})


# =====================================================
# Get Food by ID (Used by QR Code Lookup)
# =====================================================
@app.route("/food/<food_id>", methods=["GET"])
def get_food(food_id):
    """
    Retrieve a food item from the database by its ID.

    Args:
        food_id (str): Unique ID of the food item

    Returns:
        JSON object with food data or error message
    """
    item = database.get_food(food_id)

    if not item:
        return jsonify({"error": "Food not found"}), 404

    return jsonify(item)


# =====================================================
# Add Food Manually (Optional / Debug Use)
# =====================================================
@app.route("/add", methods=["POST"])
def add_food():
    """
    Add a food item manually using JSON input.

    Expected JSON format:
    {
        "id": "FOOD001",
        "name": "milk",
        "expiry_date": "2026-01-01",
        "owner": "Elaine"
    }
    """
    data = request.get_json()

    if not data:
        return jsonify({"error": "No JSON received"}), 400

    food_id = data.get("id")
    name = data.get("name")
    expiry_date = data.get("expiry_date")
    owner = data.get("owner", "default")

    # Validate required fields
    if not all([food_id, name, expiry_date]):
        return jsonify({"error": "Missing fields"}), 400

    database.add_food(food_id, name, expiry_date, owner)

    return jsonify({
        "status": "success",
        "id": food_id
    })


# =====================================================
# Upload Endpoint (CORE: ESP32 → AI → Database)
# =====================================================
@app.route("/upload", methods=["POST"])
def upload():
    """
    Receive image from ESP32, analyze using AI,
    and store result in database.

    Expected request:
    - multipart/form-data
        - "image": JPEG file
        - "id": optional food ID
    """

    # 1. Check if image exists in request
    if 'image' not in request.files:
        return jsonify({"error": "No image provided"}), 400

    image_file = request.files['image']
    image_bytes = image_file.read()

    # 2. Get food ID (optional)
    food_id = request.form.get("id")

    if not food_id:
        # Generate random ID if not provided
        food_id = str(uuid.uuid4())[:8]

    # 3. Call AI service (Claude)
    try:
        result = claude_ai.analyze_image(image_bytes)
    except Exception as e:
        return jsonify({"error": "AI processing failed", "detail": str(e)}), 500

    name = result.get("name", "Unknown")
    expiry_date = result.get("expiry_date", "2026-01-01")

    # 4. Save to database
    database.add_food(food_id, name, expiry_date, "default")

    # 5. Return result
    return jsonify({
        "status": "success",
        "id": food_id,
        "name": name,
        "expiry_date": expiry_date
    })


# =====================================================
# Get All Food IDs (Optional: Inventory / QR System)
# =====================================================
@app.route("/all", methods=["GET"])
def get_all_food():
    """
    Return a list of all food IDs in the database.
    Useful for inventory display or debugging.
    """
    ids = database.get_all_ids()

    return jsonify({
        "count": len(ids),
        "items": ids
    })

@app.route("/next_id")
def next_id():
    existing_ids = database.get_all_ids()

    i = 1
    while True:
        new_id = f"FOOD{i:03d}"
        if new_id not in existing_ids:
            return new_id
        i += 1

# =====================================================
# Run Server
# =====================================================
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=10000)



# from flask import Flask, request, jsonify
# import database

# app = Flask(__name__)

# database.init_db()


# @app.route("/food/<food_id>")
# def food(food_id):

#     item = database.get_food(food_id)

#     if not item:
#         return "Food not found", 404

#     return jsonify(item)


# # @app.route("/add")
# # def add_food():

# #     food_id = request.args.get("id")
# #     name = request.args.get("name")
# #     expiry = request.args.get("expiry")
# #     owner = request.args.get("owner")

# #     if not all([food_id, name, expiry, owner]):
# #         return "Missing fields", 400

# #     database.add_food(food_id, name, expiry, owner)

# #     return {
# #         "status": "success",
# #         "id": food_id
# #     }

# @app.route("/add", methods=["POST"])
# def add_food():

#     data = request.get_json()
#     food_id = data["id"]
#     name = data["name"]
#     expiry_date = data["expiry_date"]
#     owner = data["owner"]
#     database.add_food(food_id, name, expiry_date, owner)

#     return jsonify({
#         "status": "success",
#         "id": food_id
#     })




# if __name__ == "__main__":
#     app.run(host="0.0.0.0", port=10000)

