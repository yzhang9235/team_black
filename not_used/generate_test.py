"""
Quick test script — run this to register a user and generate their QR code
without starting the full Flask server.

Usage:
    python generate_test_qr.py
"""

from database import init_db, add_user, get_user
from qr_generator import generate_qr_for_user
import uuid

def main():
    init_db()

    name = input("Enter user name: ").strip()
    if not name:
        print("Name cannot be empty.")
        return

    user_id = str(uuid.uuid4())[:8]
    add_user(user_id, name)

    food_url = f"http://localhost:5000/user/{user_id}/foods"
    qr_path = generate_qr_for_user(user_id, name, food_url)

    print(f"\n✅ User registered!")
    print(f"   ID       : {user_id}")
    print(f"   Name     : {name}")
    print(f"   URL      : {food_url}")
    print(f"   QR code  : {qr_path}")

if __name__ == "__main__":
    main()