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
    #for testing
    return "A carton of milk"

# ========== call Claude API ==========
def call_claude(description):
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

# ========== 5. run C++ program ==========
def save_to_cpp(name, expiry, owner="Elaine"):
    food_id = str(int(time.time()))

    subprocess.run([
        "./addFood",
        food_id,
        name,
        expiry,
        owner
    ])

# ========== main ==========
def main():
    desc = capture_image()
    response_text = call_claude(desc)

    name, expiry = parse_response(response_text)

    print("Parsed:", name, expiry)

    save_to_cpp(name, expiry)

    print("Saved to database!")

if __name__ == "__main__":
    main()