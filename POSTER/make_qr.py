import qrcode
from qrcode.constants import ERROR_CORRECT_H

url = "https://github.com/MikaDinnus/bird-sounds-pcasoti"
output_file = "bird_sounds_qr.png"

qr = qrcode.QRCode(
    version=None,
    error_correction=ERROR_CORRECT_H,
    box_size=12,
    border=4,
)
qr.add_data(url)
qr.make(fit=True)

img = qr.make_image(fill_color="white", back_color="transparent").convert("RGBA")
img.save(output_file)

print(f"Gespeichert als: {output_file}")
