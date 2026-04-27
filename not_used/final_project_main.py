import requests
import subprocess
import time

# ========== API ==========
CLAUDE_API_KEY = "YOUR_API_KEY_HERE"
CLAUDE_URL = "https://api.anthropic.com/v1/messages"

# ========== 2. picture ==========
# can be changed to ESP taking photoes
def capture_image():
    print("Capturing image...")
    # #for testing
    # return "A carton of milk"
    ESP32_IP = "10.243.118.208"  
    response = requests.get(f"http://{ESP32_IP}/jpg", timeout=5)
    return response.content

# ========== call Claude API ==========
def call_claude(description):
    image_b64 = base64.b64encode(image_bytes).decode("utf-8")
    
    headers = {
        "x-api-key": CLAUDE_API_KEY,
        "anthropic-version": "2023-06-01",
        "content-type": "application/json"
    }

    prompt = f"""
    Identify the food and estimate expiry date.
    Return ONLY JSON like:
    {{
      "name": "...",
      "expiry_date": "YYYY-MM-DD"
    }}

    Food description: {description}
    """
# generate data 
    data = {
        "model": "claude-3-haiku-20240307",
        "max_tokens": 100,
        "messages": [
            {"role": "user", "content": prompt}
        ]
    }

    response = requests.post(CLAUDE_URL, headers=headers, json=data)
    result = response.json()

    text = result["content"][0]["text"]
    print("Claude response:", text)

    return text

# ========== 4. analyze JSON ==========
import json
def parse_response(text):
    try:
        data = json.loads(text)
        return data["name"], data["expiry_date"]
    except:
        print("JSON parse error, fallback")
        return "Unknown", "2026-01-01"

# ========== 5. connect to database ==========
# def save_to_cpp(name, expiry, owner="Elaine"):
#     food_id = str(int(time.time()))

#     subprocess.run([
#         "./addFood",
#         food_id,
#         name,
#         expiry,
#         owner
#     ])

# import requests

def save_to_db(name, expiry, owner="Elaine"):
    # food_id = get_next_food_id()
    
    if not food_id:
        print("all qrcode has been used")
        return
    
    data = {
        "id": food_id,
        "name": name,
        "expiry": expiry,
        "owner": owner
    }
    requests.post("https://team-black-1.onrender.com/add", json=data)
    print(f"✅{food_id}, please attach qrcode on your food")

# ========== main ==========
def main():
    desc = capture_image()
    response_text = call_claude(desc)

    name, expiry = parse_response(response_text)

    print("Parsed:", name, expiry)

    # save_to_cpp(name, expiry)
    save_to_db(name, expiry)

    print("Saved to database!")

if __name__ == "__main__":
    main()