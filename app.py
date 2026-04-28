from flask import Flask, request, jsonify, render_template_string
from werkzeug.utils import secure_filename
import database
import os
import uuid
import base64
import binascii

UPLOAD_DIR = os.path.join("static", "uploads")
ALLOWED_IMAGE_EXTENSIONS = {"jpg", "jpeg", "png", "webp"}

app = Flask(__name__, static_folder="static")
os.makedirs(UPLOAD_DIR, exist_ok=True)
database.init_db()

OWNERS = ["John", "Elaine", "Faith"]

DASHBOARD_HTML = r"""
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Plastibot Food Dashboard</title>
  <style>
    :root { --bg:#0f1115; --card:#1b1f2a; --card2:#232838; --text:#f4f7fb; --muted:#aab3c2; --green:#2faa4a; --red:#d64545; --blue:#3b82f6; --line:#333a4d; }
    * { box-sizing: border-box; }
    body { margin:0; font-family: Arial, sans-serif; background:var(--bg); color:var(--text); }
    header { padding:24px 18px 12px; text-align:center; }
    h1 { margin:0; font-size:28px; }
    .sub { color:var(--muted); margin-top:8px; font-size:14px; }
    .toolbar { max-width:1100px; margin:0 auto; padding:12px; display:flex; flex-wrap:wrap; gap:10px; justify-content:center; align-items:center; }
    button, select, input { border-radius:10px; border:1px solid var(--line); padding:10px 12px; font-size:14px; }
    button { cursor:pointer; color:white; background:var(--card2); font-weight:bold; }
    button:hover { filter:brightness(1.12); }
    .active { background:var(--blue); }
    .green { background:var(--green); }
    .red { background:var(--red); }
    .muted { color:var(--muted); }
    .container { max-width:1100px; margin:0 auto; padding:10px 14px 40px; }
    .status { text-align:center; color:var(--muted); margin:8px 0 16px; min-height:20px; }
    .grid { display:grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap:14px; }
    .card { background:var(--card); border:1px solid var(--line); border-radius:16px; padding:14px; box-shadow:0 8px 24px rgba(0,0,0,.22); }
    .imageBox { height:170px; background:#0a0c10; border:1px solid var(--line); border-radius:12px; display:flex; align-items:center; justify-content:center; overflow:hidden; color:var(--muted); margin-bottom:12px; }
    .imageBox img { width:100%; height:100%; object-fit:cover; }
    label { display:block; font-size:12px; color:var(--muted); margin:8px 0 4px; }
    .field { width:100%; background:#10131b; color:var(--text); }
    .row { display:flex; gap:8px; align-items:center; }
    .row > * { flex:1; }
    .actions { display:flex; gap:8px; margin-top:12px; }
    .actions button { flex:1; }
    .empty { text-align:center; color:var(--muted); padding:36px; border:1px dashed var(--line); border-radius:16px; }
    .pill { display:inline-block; padding:4px 8px; border-radius:999px; background:#111827; color:var(--muted); font-size:12px; margin-left:6px; }
    .topline { display:flex; justify-content:space-between; align-items:center; gap:10px; margin-bottom:8px; }
    .id { font-family: monospace; color:#d1e5ff; word-break:break-all; }
    .fileInput { width:100%; color:var(--muted); background:#10131b; }
  </style>
</head>
<body>
  <header>
    <h1>Plastibot Food Dashboard</h1>
    <div class="sub">View food by owner, edit items, upload images, or purge all scanned food records.</div>
  </header>

  <div class="toolbar">
    <button id="btnAll" onclick="setOwner('All')">All</button>
    <button id="btnJohn" onclick="setOwner('John')">John</button>
    <button id="btnElaine" onclick="setOwner('Elaine')">Elaine</button>
    <button id="btnFaith" onclick="setOwner('Faith')">Faith</button>
    <select id="ownerSelect" onchange="setOwner(this.value)">
      <option value="All">All Owners</option>
      <option value="John">John</option>
      <option value="Elaine">Elaine</option>
      <option value="Faith">Faith</option>
    </select>
    <input id="searchBox" placeholder="Search name or ID" oninput="renderCards()" />
    <button onclick="loadFood()">Refresh</button>
    <button class="red" onclick="purgeDatabase()">Purge Database</button>
  </div>

  <div class="container">
    <div class="status" id="status">Loading...</div>
    <div id="cards" class="grid"></div>
  </div>

<script>
let selectedOwner = 'All';
let foodItems = [];
const owners = ['John', 'Elaine', 'Faith'];

function setStatus(msg) {
  document.getElementById('status').innerText = msg;
}

function setOwner(owner) {
  selectedOwner = owner;
  document.getElementById('ownerSelect').value = owner;
  ['All','John','Elaine','Faith'].forEach(o => {
    const btn = document.getElementById('btn' + o);
    if (btn) btn.classList.toggle('active', o === owner);
  });
  loadFood();
}

async function loadFood() {
  setStatus('Loading food...');
  const url = selectedOwner === 'All' ? '/api/foods' : '/api/foods?owner=' + encodeURIComponent(selectedOwner);
  const res = await fetch(url);
  foodItems = await res.json();
  renderCards();
  setStatus(foodItems.length + ' item(s) shown for ' + selectedOwner);
}

function escapeHtml(s) {
  return String(s ?? '').replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
}

function renderCards() {
  const cards = document.getElementById('cards');
  const q = document.getElementById('searchBox').value.toLowerCase().trim();
  const filtered = foodItems.filter(item => {
    const text = `${item.id} ${item.name} ${item.expiry_date} ${item.owner}`.toLowerCase();
    return text.includes(q);
  });

  if (filtered.length === 0) {
    cards.className = '';
    cards.innerHTML = '<div class="empty">No food items found.</div>';
    return;
  }

  cards.className = 'grid';
  cards.innerHTML = filtered.map(item => {
    const image = item.image_url
      ? `<img src="${item.image_url}?t=${Date.now()}" alt="${escapeHtml(item.name)}">`
      : `<span>No image yet</span>`;

    const ownerOptions = owners.map(o => `<option value="${o}" ${o === item.owner ? 'selected' : ''}>${o}</option>`).join('');

    return `
      <div class="card" id="card-${escapeHtml(item.id)}">
        <div class="topline">
          <strong>${escapeHtml(item.name || 'Unknown')}</strong>
          <span class="pill">${escapeHtml(item.owner)}</span>
        </div>
        <div class="imageBox">${image}</div>
        <label>ID / QR Code</label>
        <input class="field" id="id-${escapeHtml(item.id)}" value="${escapeHtml(item.id)}" disabled>
        <label>Name</label>
        <input class="field" id="name-${escapeHtml(item.id)}" value="${escapeHtml(item.name)}">
        <div class="row">
          <div>
            <label>Expiry Date</label>
            <input class="field" id="expiry-${escapeHtml(item.id)}" value="${escapeHtml(item.expiry_date)}">
          </div>
          <div>
            <label>Owner</label>
            <select class="field" id="owner-${escapeHtml(item.id)}">${ownerOptions}</select>
          </div>
        </div>
        <label>Image</label>
        <input class="fileInput" type="file" accept="image/*" id="image-${escapeHtml(item.id)}">
        <div class="actions">
          <button class="green" onclick="saveItem('${escapeHtml(item.id)}')">Save</button>
          <button onclick="uploadImage('${escapeHtml(item.id)}')">Upload Image</button>
          <button class="red" onclick="deleteItem('${escapeHtml(item.id)}')">Delete</button>
        </div>
      </div>`;
  }).join('');
}

async function saveItem(id) {
  const payload = {
    name: document.getElementById('name-' + id).value,
    expiry_date: document.getElementById('expiry-' + id).value,
    owner: document.getElementById('owner-' + id).value,
  };

  const res = await fetch('/api/food/' + encodeURIComponent(id), {
    method: 'PUT',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(payload)
  });

  const data = await res.json();
  setStatus(data.message || data.error || 'Updated');
  await loadFood();
}

async function uploadImage(id) {
  const input = document.getElementById('image-' + id);
  if (!input.files.length) {
    alert('Choose an image first.');
    return;
  }

  const form = new FormData();
  form.append('image', input.files[0]);

  const res = await fetch('/api/food/' + encodeURIComponent(id) + '/image', {
    method: 'POST',
    body: form
  });

  const data = await res.json();
  setStatus(data.message || data.error || 'Image uploaded');
  await loadFood();
}

async function deleteItem(id) {
  if (!confirm('Delete ' + id + '?')) return;

  const res = await fetch('/api/food/' + encodeURIComponent(id), { method: 'DELETE' });
  const data = await res.json();
  setStatus(data.message || data.error || 'Deleted');
  await loadFood();
}

async function purgeDatabase() {
  const phrase = prompt('Type PURGE to delete all food items. Owners will remain.');
  if (phrase !== 'PURGE') return;

  const res = await fetch('/api/purge', { method: 'DELETE' });
  const data = await res.json();
  setStatus(data.message || 'Database purged');
  await loadFood();
}

setOwner('All');
</script>
</body>
</html>
"""


def allowed_image(filename):
    return "." in filename and filename.rsplit(".", 1)[1].lower() in ALLOWED_IMAGE_EXTENSIONS

@app.route("/exists", methods=["POST"])
def food_exists():
    data = request.get_json()

    if not data:
        return jsonify({"error": "Missing JSON body"}), 400

    food_id = data.get("id")

    if not food_id:
        return jsonify({"error": "Missing id"}), 400

    item = database.get_food(food_id)

    return jsonify({
        "status": "success",
        "exists": item is not None,
        "item": item
    })


@app.route("/")
def dashboard():
    return render_template_string(DASHBOARD_HTML)


@app.route("/health")
def health():
    return jsonify({"status": "running", "message": "Plastibot database server is running"})


@app.route("/food/<food_id>")
def food(food_id):
    item = database.get_food(food_id)
    if not item:
        return jsonify({"error": "Food not found"}), 404
    return jsonify(item)


@app.route("/add", methods=["POST"])
def add_food():
    data = request.get_json()
    if not data:
        return jsonify({"error": "Missing JSON body"}), 400

    food_id = data.get("id")
    name = data.get("name")
    expiry = data.get("expiry") or data.get("expiry_date")
    owner = data.get("owner")

    if not all([food_id, name, expiry, owner]):
        return jsonify({
            "error": "Missing fields",
            "required": ["id", "name", "expiry_date", "owner"],
            "received": data
        }), 400

    database.add_food(food_id, name, expiry, owner)

    return jsonify({
        "status": "success",
        "id": food_id,
        "name": name,
        "expiry_date": expiry,
        "owner": owner
    })


@app.route("/all")
def all_food():
    return jsonify(database.get_all_food())


@app.route("/api/owners")
def api_owners():
    return jsonify(OWNERS)

@app.route("/api/image_base64", methods=["POST"])
def api_upload_image_base64_json():
    data = request.get_json()

    if not data:
        return jsonify({"error": "Missing JSON body"}), 400

    food_id = data.get("id")
    image_b64 = data.get("image_base64")

    if not food_id:
        return jsonify({"error": "Missing id"}), 400

    if not image_b64:
        return jsonify({"error": "Missing image_base64"}), 400

    item = database.get_food(food_id)
    if not item:
        return jsonify({"error": "Food not found"}), 404

    if "," in image_b64:
        image_b64 = image_b64.split(",", 1)[1]

    try:
        image_bytes = base64.b64decode(image_b64)
    except (binascii.Error, ValueError):
        return jsonify({"error": "Invalid base64 image"}), 400

    filename = f"{secure_filename(food_id)}-{uuid.uuid4().hex[:8]}.jpg"
    image_path = os.path.join(UPLOAD_DIR, filename)

    with open(image_path, "wb") as f:
        f.write(image_bytes)

    old_image = item.get("image_filename")
    if old_image:
        old_path = os.path.join(UPLOAD_DIR, old_image)
        if os.path.exists(old_path):
            os.remove(old_path)

    database.update_food_image(food_id, filename)

    return jsonify({
        "status": "success",
        "message": f"Image uploaded for {food_id}",
        "image_url": f"/static/uploads/{filename}"
    })

@app.route("/api/foods")
def api_foods():
    owner = request.args.get("owner")
    return jsonify(database.get_all_food(owner=owner))

@app.route("/api/food/<food_id>/image_base64", methods=["POST"])
def api_upload_image_base64(food_id):
    item = database.get_food(food_id)
    if not item:
        return jsonify({"error": "Food not found"}), 404

    data = request.get_json()
    if not data:
        return jsonify({"error": "Missing JSON body"}), 400

    image_b64 = data.get("image_base64")
    if not image_b64:
        return jsonify({"error": "Missing image_base64"}), 400

    # In case a data URL is ever sent, strip the prefix
    if "," in image_b64:
        image_b64 = image_b64.split(",", 1)[1]

    try:
        image_bytes = base64.b64decode(image_b64)
    except (binascii.Error, ValueError):
        return jsonify({"error": "Invalid base64 image"}), 400

    filename = f"{secure_filename(food_id)}-{uuid.uuid4().hex[:8]}.jpg"
    image_path = os.path.join(UPLOAD_DIR, filename)

    with open(image_path, "wb") as f:
        f.write(image_bytes)

    # Remove old image if one exists
    old_image = item.get("image_filename")
    if old_image:
        old_path = os.path.join(UPLOAD_DIR, old_image)
        if os.path.exists(old_path):
            os.remove(old_path)

    database.update_food_image(food_id, filename)

    return jsonify({
        "status": "success",
        "message": f"Image uploaded for {food_id}",
        "image_url": f"/static/uploads/{filename}"
    })


@app.route("/api/food/<food_id>", methods=["GET"])
def api_get_food(food_id):
    item = database.get_food(food_id)
    if not item:
        return jsonify({"error": "Food not found"}), 404
    return jsonify(item)


@app.route("/api/food/<food_id>", methods=["PUT"])
def api_update_food(food_id):
    data = request.get_json()
    if not data:
        return jsonify({"error": "Missing JSON body"}), 400

    name = data.get("name")
    expiry = data.get("expiry_date") or data.get("expiry")
    owner = data.get("owner")

    if not all([name, expiry, owner]):
        return jsonify({"error": "Missing name, expiry_date, or owner"}), 400

    ok = database.update_food(food_id, name, expiry, owner)
    if not ok:
        return jsonify({"error": "Food not found"}), 404

    return jsonify({"status": "success", "message": f"Updated {food_id}"})


@app.route("/api/food/<food_id>", methods=["DELETE"])
def api_delete_food(food_id):
    ok, item = database.delete_food(food_id)
    if not ok:
        return jsonify({"error": "Food not found"}), 404

    # Remove image file if one exists.
    image_filename = item.get("image_filename") if item else None
    if image_filename:
        image_path = os.path.join(UPLOAD_DIR, image_filename)
        if os.path.exists(image_path):
            os.remove(image_path)

    return jsonify({"status": "success", "message": f"Deleted {food_id}"})


@app.route("/api/food/<food_id>/image", methods=["POST"])
def api_upload_image(food_id):
    item = database.get_food(food_id)
    if not item:
        return jsonify({"error": "Food not found"}), 404

    if "image" not in request.files:
        return jsonify({"error": "Missing image file"}), 400

    image = request.files["image"]
    if image.filename == "":
        return jsonify({"error": "Empty filename"}), 400

    if not allowed_image(image.filename):
        return jsonify({"error": "Allowed image types: jpg, jpeg, png, webp"}), 400

    ext = secure_filename(image.filename).rsplit(".", 1)[1].lower()
    filename = f"{secure_filename(food_id)}-{uuid.uuid4().hex[:8]}.{ext}"
    image_path = os.path.join(UPLOAD_DIR, filename)
    image.save(image_path)

    # Remove old image if there was one.
    old_image = item.get("image_filename")
    if old_image:
        old_path = os.path.join(UPLOAD_DIR, old_image)
        if os.path.exists(old_path):
            os.remove(old_path)

    database.update_food_image(food_id, filename)
    return jsonify({"status": "success", "message": f"Image uploaded for {food_id}", "image_url": f"/static/uploads/{filename}"})


@app.route("/api/purge", methods=["DELETE"])
def api_purge():
    deleted_count = database.purge_food()

    # Clear uploaded images too, because those belong to food items.
    for filename in os.listdir(UPLOAD_DIR):
        path = os.path.join(UPLOAD_DIR, filename)
        if os.path.isfile(path):
            os.remove(path)

    return jsonify({
        "status": "success",
        "message": f"Purged {deleted_count} food item(s). Owners were kept.",
        "owners": OWNERS
    })


if __name__ == "__main__":
    port = int(os.environ.get("PORT", 10000))
    app.run(host="0.0.0.0", port=port, debug=True)
