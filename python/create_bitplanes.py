import kagglehub
import argparse
import os
import cv2
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path


# https://github.com/fernando-bertoldi/Projeto-ANS/blob/main/Bit_Planes.py
def bit_plane_slicing(image_path, name, dir):
    print(f"Generating bitplane to: {name}")
    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)

    if img is None:
        print("Image load fail.")
        return

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
        filename = os.path.join(final_dir, f'{bit}.txt')
        np.savetxt(filename, planes_binary[bit].reshape(1, -1), fmt='%d', delimiter='')
        print(f"Bitplane {bit} saved successfully on: {filename}")

    plt.figure(figsize=(12, 8))
    for i in range(8):
        plt.subplot(2, 4, i + 1)
        plt.imshow(planes_visual[i], cmap='gray')
        plt.title(f'Bitplane {i}')
        plt.axis('off')

    plt.tight_layout()
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
