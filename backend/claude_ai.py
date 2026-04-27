import base64
import requests
import re, json

CLAUDE_API_KEY = "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"
CLAUDE_URL = "https://api.anthropic.com/v1/messages"


def analyze_image(image_bytes):

    img_b64 = base64.b64encode(image_bytes).decode()

    headers = {
        "x-api-key": CLAUDE_API_KEY,
        "anthropic-version": "2023-06-01",
        "content-type": "application/json"
    }

    data = {
        "model": "claude-3-haiku-20240307",
        "max_tokens": 200,
        "messages": [{
            "role": "user",
            "content": [
                {
                    "type": "image",
                    "source": {
                        "type": "base64",
                        "media_type": "image/jpeg",
                        "data": img_b64
                    }
                },
                {
                    "type": "text",
                    "text": (
                        "Look at this food item. "
                        "Return ONLY a raw JSON object with exactly two fields: "
                        "\"name\" (string, the food name) and "
                        "\"expiry_date\" (string, format YYYY-MM-DD). "
                        "No markdown, no code blocks, no explanation. "
                        "Example: {\"name\": \"Milk\", \"expiry_date\": \"2026-05-01\"}"
                    )
                }
            ]
        }]
    }

    res = requests.post(CLAUDE_URL, headers=headers, json=data)

    text = res.json()["content"][0]["text"]


    text = res.json()["content"][0]["text"].strip()
    text = re.sub(r"^```json\s*", "", text)
    text = re.sub(r"```$", "", text).strip()

    try:
        return json.loads(text)
    except:
        return {"name": "Unknown", "expiry_date": "2026-01-01"}