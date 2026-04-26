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


# @app.route("/add")
# def add_food():

#     food_id = request.args.get("id")
#     name = request.args.get("name")
#     expiry = request.args.get("expiry")
#     owner = request.args.get("owner")

#     if not all([food_id, name, expiry, owner]):
#         return "Missing fields", 400

#     database.add_food(food_id, name, expiry, owner)

#     return {
#         "status": "success",
#         "id": food_id
#     }


# if __name__ == "__main__":
#     app.run(host="0.0.0.0", port=10000)

from flask import Flask, request, jsonify
import database
import requests
import json
import os
import base64

app = Flask(__name__)
database.init_db()

CLAUDE_API_KEY = os.environ.get("CLAUDE_API_KEY", "YOUR_API_KEY_HERE")
CLAUDE_URL = "https://api.anthropic.com/v1/messages"


def call_claude_with_image(image_bytes):
    image_b64 = base64.b64encode(image_bytes).decode("utf-8")

    headers = {
        "x-api-key": CLAUDE_API_KEY,
        "anthropic-version": "2023-06-01",
        "content-type": "application/json"
    }

    data = {
        "model": "claude-3-haiku-20240307",
        "max_tokens": 100,
        "messages": [
            {
                "role": "user",
                "content": [
                    {
                        "type": "image",
                        "source": {
                            "type": "base64",
                            "media_type": "image/jpeg",
                            "data": image_b64
                        }
                    },
                    {
                        "type": "text",
                        "text": """Identify the food and estimate expiry date.
Return ONLY JSON like:
{
  "name": "...",
  "expiry_date": "YYYY-MM-DD"
}
No other text."""
                    }
                ]
            }
        ]
    }

    response = requests.post(CLAUDE_URL, headers=headers, json=data)
    result = response.json()
    text = result["content"][0]["text"].strip()

    try:
        parsed = json.loads(text)
        return parsed["name"], parsed["expiry_date"]
    except:
        return "Unknown", "2026-01-01"


# =============================================
# ESP32 uploads photo here
# =============================================
@app.route("/upload", methods=["POST"])
def upload():
    food_id = request.form.get("id")
    image_file = request.files.get("image")

    if not food_id or not image_file:
        return "Missing id or image", 400

    image_bytes = image_file.read()
    name, expiry = call_claude_with_image(image_bytes)

    database.add_food(food_id, name, expiry, owner="default")

    print(f"Saved: {food_id} = {name}, expires {expiry}")
    return jsonify({"status": "success", "id": food_id, "name": name, "expiry": expiry})


# =============================================
# ESP32 gets next available food ID
# =============================================
@app.route("/next_id", methods=["GET"])
def next_id():
    used_ids = database.get_all_ids()
    for i in range(1, 21):
        food_id = f"FOOD{i:03d}"
        if food_id not in used_ids:
            return jsonify({"food_id": food_id})
    return jsonify({"error": "All QR codes used"}), 400


# =============================================
# Scan QR code → show food info
# =============================================
@app.route("/add", methods=["GET", "POST"])
def add_food():
    if request.method == "GET":
        food_id = request.args.get("id")
        if not food_id:
            return "Missing food ID", 400

        item = database.get_food(food_id)
        if not item:
            return f"""
            <h1>⏳ Not scanned yet</h1>
            <p>Food ID: {food_id}</p>
            <p>This QR code hasn't been used yet.</p>
            """, 404

        from datetime import date
        today = date.today()
        try:
            expiry = date.fromisoformat(item['expiry_date'])
            days_left = (expiry - today).days
            if days_left < 0:
                status = f"⛔ Expired {abs(days_left)} days ago"
                color = "#e74c3c"
            elif days_left <= 3:
                status = f"⚠️ Expires in {days_left} days"
                color = "#f39c12"
            else:
                status = f"✅ {days_left} days left"
                color = "#27ae60"
        except:
            status = "Unknown"
            color = "#888"

        return f"""
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>{item['name']}</title>
            <style>
                body {{ font-family: sans-serif; max-width: 400px; margin: 40px auto; padding: 20px; }}
                h1 {{ font-size: 2rem; margin-bottom: 0.2rem; }}
                .status {{ color: {color}; font-size: 1.2rem; font-weight: bold; margin: 1rem 0; }}
                .info {{ color: #666; margin: 0.5rem 0; }}
            </style>
        </head>
        <body>
            <h1>🍽️ {item['name']}</h1>
            <div class="status">{status}</div>
            <p class="info">📅 Expires: {item['expiry_date']}</p>
            <p class="info">👤 Owner: {item['owner']}</p>
            <p class="info">🏷️ ID: {item['id']}</p>
        </body>
        </html>
        """

    # POST - manual add
    data = request.get_json()
    if not data:
        return "Missing JSON body", 400
    food_id = data.get("id")
    name = data.get("name")
    expiry = data.get("expiry")
    owner = data.get("owner")
    if not all([food_id, name, expiry, owner]):
        return "Missing fields", 400
    database.add_food(food_id, name, expiry, owner)
    return jsonify({"status": "success", "id": food_id})


@app.route("/food/<food_id>")
def food(food_id):
    item = database.get_food(food_id)
    if not item:
        return "Food not found", 404
    return jsonify(item)


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 10000))
    app.run(host="0.0.0.0", port=port)