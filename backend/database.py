import sqlite3

DB_NAME = "food.db"


# =========================
# init database
# =========================
def init_db():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()

    c.execute("""
    CREATE TABLE IF NOT EXISTS food (
        id TEXT PRIMARY KEY,
        name TEXT,
        expiry_date TEXT,
        owner TEXT
    )
    """)

    conn.commit()
    conn.close()


# =========================
# insert / update food
# =========================
def add_food(food_id, name, expiry_date, owner):
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()

    c.execute("""
    INSERT OR REPLACE INTO food VALUES (?, ?, ?, ?)
    """, (food_id, name, expiry_date, owner))

    conn.commit()
    conn.close()


# =========================
# get food by id
# =========================
def get_food(food_id):
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()

    c.execute("SELECT * FROM food WHERE id=?", (food_id,))
    row = c.fetchone()

    conn.close()

    if not row:
        return None

    return {
        "id": row[0],
        "name": row[1],
        "expiry_date": row[2],
        "owner": row[3]
    }


# =========================
# get all ids (for QR system)
# =========================
def get_all_ids():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()

    c.execute("SELECT id FROM food")
    rows = c.fetchall()

    conn.close()

    return [r[0] for r in rows]
# import sqlite3

# DB_NAME = "food.db"


# def init_db():
#     conn = sqlite3.connect(DB_NAME)
#     c = conn.cursor()

#     c.execute("""
#     CREATE TABLE IF NOT EXISTS food (
#         id TEXT PRIMARY KEY,
#         name TEXT,
#         expiry_date TEXT,
#         owner TEXT
#     )
#     """)

#     conn.commit()
#     conn.close()


# def get_food(food_id):
#     conn = sqlite3.connect(DB_NAME)
#     c = conn.cursor()

#     c.execute("SELECT * FROM food WHERE id=?", (food_id,))
#     row = c.fetchone()

#     conn.close()

#     if row:
#         return {
#             "id": row[0],
#             "name": row[1],
#             "expiry_date": row[2],
#             "owner": row[3]
#         }

#     return None


# def add_food(food_id, name, expiry_date, owner):
#     conn = sqlite3.connect(DB_NAME)
#     c = conn.cursor()

#     c.execute("""
#     INSERT OR REPLACE INTO food VALUES (?, ?, ?, ?)
#     """, (food_id, name, expiry_date, owner))

#     conn.commit()
#     conn.close()

# def get_all_ids():
#     conn = sqlite3.connect(DB_NAME)
#     c = conn.cursor()
#     c.execute("SELECT id FROM food")
#     rows = c.fetchall()
#     conn.close()
#     return [row[0] for row in rows]