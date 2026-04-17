import qrcode
from urllib.parse import urlparse, parse_qs
import os
import re

INPUT_FILE = "qrCodes.txt"
OUTPUT_FOLDER = "qr_output"
DPI = 300
PIXELS = 300  # 1 inch at 300 DPI


def make_safe_filename(url):
    parsed = urlparse(url)
    domain = parsed.netloc.replace("www.", "")
    path = parsed.path.strip("/").replace("/", "_")

    if path:
        name = f"{domain}_{path}"
    else:
        name = domain

    name = re.sub(r'[^a-zA-Z0-9._-]', '_', name)
    return name if name else "qr_code"

def create_qr_code(url, filename):
    qr = qrcode.QRCode(
        version=1,
        error_correction=qrcode.constants.ERROR_CORRECT_H,
        box_size=10,
        border=4,
    )
    qr.add_data(url)
    qr.make(fit=True)

    img = qr.make_image(fill_color="black", back_color="white").convert("RGB")

    # Resize to exactly 300x300 pixels
    img = img.resize((PIXELS, PIXELS))

    # Save with DPI metadata
    img.save(filename, dpi=(DPI, DPI))



def main():
    os.makedirs(OUTPUT_FOLDER, exist_ok=True)
    used_names = {}

    with open(INPUT_FILE, "r", encoding="utf-8") as file:
        for line in file:
            url = line.strip()
            if not url:
                continue

            if not url.startswith(("http://", "https://")):
                url = "https://" + url

            base_name = make_safe_filename(url)

            if base_name in used_names:
                used_names[base_name] += 1
                final_name = f"{base_name}_{used_names[base_name]}.png"
            else:
                used_names[base_name] = 1
                final_name = f"{base_name}.png"

            output_path = os.path.join(OUTPUT_FOLDER, final_name)
            create_qr_code(url, output_path)
            print(f"Saved: {output_path}")

if __name__ == "__main__":
    main()