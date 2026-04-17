import qrcode
import os
import re

INPUT_FILE = "qrCodes.txt"
OUTPUT_FOLDER = "qr_output"
DPI = 300
PIXELS = 300


# =========================
# SAFE FILENAME
# =========================
def make_safe_filename(text):
    text = re.sub(r'[^a-zA-Z0-9._-]', '_', text)
    return text if text else "qr_code"


# =========================
# QR GENERATOR
# =========================
def create_qr(data, filename):
    qr = qrcode.QRCode(
        version=1,
        error_correction=qrcode.constants.ERROR_CORRECT_H,
        box_size=10,
        border=4,
    )

    qr.add_data(data)
    qr.make(fit=True)

    img = qr.make_image(fill_color="black", back_color="white").convert("RGB")
    img = img.resize((PIXELS, PIXELS))
    img.save(filename, dpi=(DPI, DPI))


def load_from_list():
    return [f"FOOD{str(i).zfill(3)}" for i in range(1, 21)]


# =========================
# MAIN
# =========================
def main():

    os.makedirs(OUTPUT_FOLDER, exist_ok=True)

    items = load_from_list()

    used = {}

    for item in items:

        data = item

        base_name = make_safe_filename(item)

        if base_name in used:
            used[base_name] += 1
            filename = f"{base_name}_{used[base_name]}.png"
        else:
            used[base_name] = 1
            filename = f"{base_name}.png"

        output_path = os.path.join(OUTPUT_FOLDER, filename)

        create_qr(data, output_path)
        print(f"Saved: {output_path} -> {data}")


if __name__ == "__main__":
    main()