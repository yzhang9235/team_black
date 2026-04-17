import qrcode
import os

OUTPUT = "qr_output"
os.makedirs(OUTPUT, exist_ok=True)

DPI = 300
SIZE = 300 


def create_qr(food_id):

    qr = qrcode.QRCode(
        version=1,
        error_correction=qrcode.constants.ERROR_CORRECT_H, 
        box_size=10,
        border=4
    )

    qr.add_data(food_id)
    qr.make(fit=True)

    img = qr.make_image(fill="black", back="white").convert("RGB")

    img = img.resize((SIZE, SIZE))

    filename = f"{OUTPUT}/{food_id}.png"
    img.save(filename, dpi=(DPI, DPI))

    print("generated:", filename)


def main():

    for i in range(1, 21):
        food_id = f"FOOD{str(i).zfill(3)}"
        create_qr(food_id)


if __name__ == "__main__":
    main()