const revealEls = document.querySelectorAll('.reveal');
const io = new IntersectionObserver((entries) => {
  entries.forEach(e => {
    if (e.isIntersecting) {
      e.target.classList.add('visible');
      io.unobserve(e.target);
    }
  });
}, { threshold: 0.12 });
revealEls.forEach(el => io.observe(el));

const API = '../backend';

async function loadInventory() {
  const loading = document.getElementById('inv-loading');
  const table   = document.getElementById('inv-table');
  const empty   = document.getElementById('inv-empty');
  const tbody   = document.getElementById('inv-body');

  if (!tbody) return; 

  try {
    const res  = await fetch(`${API}/food_get.php`, { credentials: 'include' });
    const data = await res.json();

    loading.style.display = 'none';

    if (!data || data.length === 0) {
      empty.style.display = 'block';
      return;
    }

    table.style.display = 'table';
    tbody.innerHTML = data.map(item => `
      <tr data-id="${item.item_id}">
        <td>${item.item_name}</td>
        <td><span class="cat-badge">${item.category || '—'}</span></td>
        <td>${item.quantity || '—'}</td>
        <td>${item.date_added ? new Date(item.date_added).toLocaleDateString() : '—'}</td>
        <td>
          <div class="inv-actions">
            <button class="btn-icon" onclick="deleteItem(${item.item_id})">🗑 Delete</button>
          </div>
        </td>
      </tr>
    `).join('');
  } catch (err) {
    loading.textContent = 'Could not load inventory.';
    console.error(err);
  }
}

async function deleteItem(id) {
  if (!confirm('Remove this item?')) return;
  try {
    const form = new FormData();
    form.append('item_id', id);
    await fetch(`${API}/food_delete.php`, { method: 'POST', body: form, credentials: 'include' });
    loadInventory();
  } catch (err) {
    console.error(err);
  }
}

const addBtn    = document.getElementById('add-item-btn');
const addForm   = document.getElementById('add-form');
const cancelBtn = document.getElementById('cancel-item');
const submitBtn = document.getElementById('submit-item');
const formMsg   = document.getElementById('form-msg');

if (addBtn) {
  addBtn.addEventListener('click', () => {
    addForm.style.display = addForm.style.display === 'none' ? 'block' : 'none';
  });
}

if (cancelBtn) {
  cancelBtn.addEventListener('click', () => {
    addForm.style.display = 'none';
    formMsg.textContent = '';
  });
}

if (submitBtn) {
  submitBtn.addEventListener('click', async () => {
    const name     = document.getElementById('item-name').value.trim();
    const category = document.getElementById('item-category').value;
    const quantity = document.getElementById('item-quantity').value.trim();
    const expiry   = document.getElementById('item-expiry').value;

    if (!name) { formMsg.textContent = 'Please enter an item name.'; return; }

    const form = new FormData();
    form.append('item_name', name);
    form.append('category', category);
    form.append('quantity', quantity);
    form.append('expiration_date', expiry);

    try {
      const res = await fetch(`${API}/food_add.php`, { method: 'POST', body: form, credentials: 'include' });
      const data = await res.json();
      if (data.success) {
        formMsg.textContent = '✓ Item added!';
        document.getElementById('item-name').value = '';
        document.getElementById('item-quantity').value = '';
        document.getElementById('item-expiry').value = '';
        loadInventory();
        setTimeout(() => { addForm.style.display = 'none'; formMsg.textContent = ''; }, 1000);
      } else {
        formMsg.textContent = data.message || 'Something went wrong.';
      }
    } catch (err) {
      formMsg.textContent = 'Could not connect to server.';
      console.error(err);
    }
  });
}

loadInventory();