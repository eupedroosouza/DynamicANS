import kagglehub
import argparse
import os
import cv2
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

def int_to_bits(valor, num_bits=32):
    binario_str = format(valor, f'0{num_bits}b')
    return [int(b) for b in binario_str]


# https://github.com/fernando-bertoldi/Projeto-ANS/blob/main/Bit_Planes.py
def bit_plane_slicing(image_path, name, dir):
    print(f"Generating bitplane to: {name}")

    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
    img_color = cv2.imread(image_path, cv2.IMREAD_COLOR)

    if img is None or img_color is None:
        print("Image load fail.")
        return

    img_rgb = cv2.cvtColor(img_color, cv2.COLOR_BGR2RGB)

    planes_visual = []
    planes_binary = {}

    for i in range(8):
        plane_mask = np.full(img.shape, 2 ** i, np.uint8)
        res = cv2.bitwise_and(img, plane_mask)

        binary_matrix = (res > 0).astype(np.uint8)

        planes_binary[i] = binary_matrix

        planes_visual.append(binary_matrix * 255)

    final_dir = f'{dir}/{name}'
    Path(final_dir).mkdir(parents=True, exist_ok=True)
    for bit in range(8):

        buffers = [[] for _ in range(4)]

        bp = planes_binary[bit]
        lines, columns = bp.shape
        # Applied 2D context to improve entropy coding
        # 4 contexts by up and left:
        # 00 - up and left are white
        # 01 - left is white, up is black
        # 10 - left is black, up is white
        # 11 - up and left are black
        for y in range(lines):
            for x in range(columns):
                left = bp[y, x - 1] if x > 0 else 0
                up = bp[y - 1, x] if y > 0 else 0

                ctx = (left << 1) | up
                real_bit = bp[y, x]
                buffers[ctx].append(real_bit)


        data = []

        for i in range(4):
            buffer_size = len(buffers[i])
            bits = int_to_bits(buffer_size, 32)
            data.extend(bits)

        for i in range(4):
            # append buffers size to reconstruct if necessary
            data.extend(buffers[i])

        filename = os.path.join(final_dir, f'{bit}.txt')
        np.savetxt(filename, np.array(data).reshape(1, -1), fmt='%d', delimiter='')
        print(f"Bitplane {bit} saved successfully on: {filename}")

    fig = plt.figure(figsize=(12, 10))
    fig.suptitle(f'{name}', fontsize=18, fontweight='bold')

    gs = fig.add_gridspec(3, 4)
    ax_color = fig.add_subplot(gs[0, 1])
    ax_color.imshow(img_rgb)
    ax_color.set_title('Original')
    ax_color.axis('off')

    ax_gray = fig.add_subplot(gs[0, 2])
    ax_gray.imshow(img, cmap='gray')
    ax_gray.set_title('Grayscale')
    ax_gray.axis('off')

    for i in range(8):
        row = 1 + (i // 4)
        col = i % 4
        ax_bp = fig.add_subplot(gs[row, col])
        ax_bp.imshow(planes_visual[i], cmap='gray')
        ax_bp.set_title(f'Bitplane {i}')
        ax_bp.axis('off')

    plt.tight_layout()
    plt.subplots_adjust(top=0.92)

    png_filename = os.path.join(final_dir, f'{name}_bitplane.png')
    plt.savefig(png_filename, dpi=300, bbox_inches='tight')
    plt.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", "-d", required=True, help="Save directory")

    args = parser.parse_args()

    # https://www.kaggle.com/datasets/sherylmehta/kodak-dataset
    print("Dataset: sherylmehta/kodak-dataset")
    dataset_path = kagglehub.dataset_download("sherylmehta/kodak-dataset")
    print("Path to dataset files:", dataset_path)

    search = os.scandir(dataset_path)

    for entry in search:
        if not entry.is_file():
            continue
        name: str = os.path.splitext(entry.name)[0]
        bit_plane_slicing(entry.path, name, args.dir)
        pass


if __name__ == "__main__":
    main()
