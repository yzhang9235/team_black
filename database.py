import os
import sqlite3

DB_NAME = "food.db"
OWNERS = ["John", "Elaine", "Faith"]


def get_connection():
    conn = sqlite3.connect(DB_NAME)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    conn = get_connection()
    c = conn.cursor()

    c.execute("""
    CREATE TABLE IF NOT EXISTS food (
        id TEXT PRIMARY KEY,
        name TEXT,
        expiry_date TEXT,
        owner TEXT,
        image_filename TEXT
    )
    """)

    # Migration for older food.db files that only had 4 columns.
    c.execute("PRAGMA table_info(food)")
    columns = [row[1] for row in c.fetchall()]
    if "image_filename" not in columns:
        c.execute("ALTER TABLE food ADD COLUMN image_filename TEXT")

    conn.commit()
    conn.close()


def row_to_food(row):
    if not row:
        return None

    image_filename = row["image_filename"] if "image_filename" in row.keys() else None

    return {
        "id": row["id"],
        "name": row["name"],
        "expiry_date": row["expiry_date"],
        "owner": row["owner"],
        "image_filename": image_filename,
        "image_url": f"/static/uploads/{image_filename}" if image_filename else None,
    }


def get_food(food_id):
    conn = get_connection()
    c = conn.cursor()
    c.execute("SELECT * FROM food WHERE id=?", (food_id,))
    row = c.fetchone()
    conn.close()
    return row_to_food(row)


def get_all_food(owner=None):
    conn = get_connection()
    c = conn.cursor()

    if owner and owner != "All":
        c.execute("SELECT * FROM food WHERE owner=? ORDER BY name", (owner,))
    else:
        c.execute("SELECT * FROM food ORDER BY owner, name")

    rows = c.fetchall()
    conn.close()
    return [row_to_food(row) for row in rows]


def add_food(food_id, name, expiry_date, owner, image_filename=None):
    conn = get_connection()
    c = conn.cursor()

    existing = get_food(food_id)
    if image_filename is None and existing:
        image_filename = existing.get("image_filename")

    c.execute("""
    INSERT OR REPLACE INTO food (id, name, expiry_date, owner, image_filename)
    VALUES (?, ?, ?, ?, ?)
    """, (food_id, name, expiry_date, owner, image_filename))

    conn.commit()
    conn.close()


def update_food(food_id, name, expiry_date, owner):
    conn = get_connection()
    c = conn.cursor()
    c.execute("""
    UPDATE food
    SET name=?, expiry_date=?, owner=?
    WHERE id=?
    """, (name, expiry_date, owner, food_id))
    changed = c.rowcount
    conn.commit()
    conn.close()
    return changed > 0


def update_food_image(food_id, image_filename):
    conn = get_connection()
    c = conn.cursor()
    c.execute("UPDATE food SET image_filename=? WHERE id=?", (image_filename, food_id))
    changed = c.rowcount
    conn.commit()
    conn.close()
    return changed > 0


def delete_food(food_id):
    item = get_food(food_id)
    conn = get_connection()
    c = conn.cursor()
    c.execute("DELETE FROM food WHERE id=?", (food_id,))
    changed = c.rowcount
    conn.commit()
    conn.close()
    return changed > 0, item


def purge_food():
    conn = get_connection()
    c = conn.cursor()
    c.execute("DELETE FROM food")
    deleted_count = c.rowcount
    conn.commit()
    conn.close()
    return deleted_count
