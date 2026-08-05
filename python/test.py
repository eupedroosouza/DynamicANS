import csv
import numpy as np
import os
import re
import subprocess
import time
import yaml
from argparse import ArgumentParser
from pathlib import Path
from tqdm import tqdm


def extract(pattern: str, text: str, flags=0):
    match = re.search(pattern, text, flags)
    return match.group(1) if match else "N/A"


def to_num(val):
    if val == "N/A":
        return val
    try:
        return int(val)
    except ValueError:
        try:
            return float(val)
        except ValueError:
            return val


def main():
    parser = ArgumentParser()
    parser.add_argument("--config", "-cfg", required=False, default="test_config.yml")
    parser.add_argument("--input", "-i", required=True, help="Define the input folder")

    args = parser.parse_args()

    with open(args.config, "r") as file:
        config = yaml.safe_load(file)

    fields = ["range", "adaptative_interval", "memory_usage_by_context", "original_size", "encoded_size",
              "compression_ratio", "compression",
              "encoding_time", "encoding_mem_baseline", "encoding_mem_peak", "encoding_mem_delta",
              "decoding_time", "decoding_mem_baseline", "decoding_mem_peak", "decoding_mem_delta"]
    rows = []

    samples: int = int(config["samples"])
    ranges: list[int] = config["ranges"]
    adaptativeIntervals: list[int] = config["adaptativeIntervals"]

    execPath = Path(config["executable"])
    if not execPath.exists():
        print(f"Executable not found: {execPath}")
        return 1

    for rg in ranges:
        print(f"===== RANGE {rg} =====")
        for adaptativeInterval in adaptativeIntervals:
            samples_context_usage_mem: list[float] = []
            samples_encode_time: list[float] = []
            samples_enc_baseline_mem: list[float] = []
            samples_enc_peak_mem: list[float] = []
            samples_enc_delta_mem: list[float] = []
            samples_decode_time: list[float] = []
            samples_dec_baseline_mem: list[float] = []
            samples_dec_peak_mem: list[float] = []
            samples_dec_delta_mem: list[float] = []

            for i in tqdm(range(0, samples, 1), desc=f"ADAPTATIVE INTERVAL {adaptativeInterval}"):
                command = f"{os.path.normpath(execPath)} --encode --decode --tables ../tables/range{rg}.bin --adaptativeInterval {adaptativeInterval} -i {args.input}"

                try:

                    result = subprocess.run(
                        command,
                        shell=True,
                        capture_output=True,
                        text=True,
                        check=True
                    )

                    stdout = result.stdout.strip()

                    enc_mem_block = extract(r"Encode memory:(.*?)(?:=====|$)", text=stdout, flags=re.DOTALL)
                    if enc_mem_block == "N/A": enc_mem_block = ""
                    dec_mem_block = extract(r"Decode memory:(.*?)(?:=====|$)", text=stdout, flags=re.DOTALL)
                    if dec_mem_block == "N/A": dec_mem_block = ""

                    mem_usage_by_context = to_num(extract(r"usage by context:\s*([\d.]+)", stdout))
                    bins_encoded = to_num(extract(r"Bins encoded:\s*([\d.]+)", stdout))
                    original_size: int = to_num(extract(r"Original size \(bits\):\s*([\d.]+)", stdout))
                    encoded_size: int = to_num(extract(r"Encoded size \(bits\):\s*([\d.]+)", stdout))
                    encode_time: float = to_num(extract(r"Encode time:\s*([\d.]+)", stdout))
                    enc_baseline_mem = to_num(extract(r"baseline\s+:\s+([\d.]+)", text=enc_mem_block))
                    enc_peak_mem = to_num(extract(r"peak\s+:\s+([\d.]+)", text=enc_mem_block))
                    enc_delta_mem = to_num(extract(r"delta\s+:\s+([\d.]+)", text=enc_mem_block))
                    decode_time: float = to_num(extract(r"Decode time:\s*([\d.]+)", stdout))
                    dec_baseline_mem = to_num(extract(r"baseline\s+:\s+([\d.]+)", text=dec_mem_block))
                    dec_peak_mem = to_num(extract(r"peak\s+:\s+([\d.]+)", text=dec_mem_block))
                    dec_delta_mem = to_num(extract(r"delta\s+:\s+([\d.]+)", text=dec_mem_block))

                    samples_context_usage_mem.append(mem_usage_by_context)
                    samples_encode_time.append(encode_time)
                    samples_enc_baseline_mem.append(enc_baseline_mem)
                    samples_enc_peak_mem.append(enc_peak_mem)
                    samples_enc_delta_mem.append(enc_delta_mem)
                    samples_decode_time.append(decode_time)
                    samples_dec_baseline_mem.append(dec_baseline_mem)
                    samples_dec_peak_mem.append(dec_peak_mem)
                    samples_dec_delta_mem.append(dec_delta_mem)

                    compression_ratio = original_size / encoded_size
                    compression = (1.0 - (encoded_size / original_size)) * 100.0


                except subprocess.CalledProcessError as e:
                    print(f"Process returned: {e}")
                    return 1

            mem_usage_by_context = np.array(samples_context_usage_mem).mean()
            encode_time = np.array(samples_encode_time).mean()
            enc_baseline_mem = np.array(samples_enc_baseline_mem).mean()
            enc_peak_mem = np.array(samples_enc_peak_mem).mean()
            enc_delta_mem = np.array(samples_enc_delta_mem).mean()
            decode_time = np.array(samples_decode_time).mean()
            dec_baseline_mem = np.array(samples_dec_baseline_mem).mean()
            dec_peak_mem = np.array(samples_dec_peak_mem).mean()
            dec_delta_mem = np.array(samples_dec_delta_mem).mean()

            rows.append(
                [f"{rg}", f"{adaptativeInterval}", f"{mem_usage_by_context:.4f}", f"{original_size}", f"{encoded_size}", f"{compression_ratio:.2f}",
                 f"{compression:.2f}", f"{encode_time:.6f}", f"{enc_baseline_mem:.4f}", f"{enc_peak_mem:.4f}", f"{enc_delta_mem:.4f}", f"{decode_time:.6f}", f"{dec_baseline_mem:.4f}", f"{dec_peak_mem:.4f}", f"{dec_delta_mem:.4f}"])

    with open(f"test_results_{time.time()}.csv", "w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["input", f"{args.input}"])
        writer.writerow(["samples", f"{samples}"])
        rangesStr = [str(x) for x in ranges]
        joinableRanges = ", ".join(rangesStr)
        writer.writerow(["ranges", f"{joinableRanges}"])
        adaptativeIntervalsStr = [str(x) for x in adaptativeIntervals]
        joinableAdaptativeIntervals = ", ".join(adaptativeIntervalsStr)
        writer.writerow(["adaptative_intervals", f"{joinableAdaptativeIntervals}"])
        writer.writerow(fields)
        for row in rows:
            writer.writerow(row)
            # print("=== ENCODING SUMMARY ===")
            # print(f"Bins to encode: {bins_encoded}")
            # print(f"Original size (bits): {original_size}")
            # print(f"Encoded size (bits): {encoded_size}")
            # print(f"Encode time (seg): {encode_time}")
            # print(f"Compression ratio: {compression_ratio}")
            # print(f"Compression (%): {compression}")
            # print("=== DECODING SUMMARY ===")
            # print(f"Decode time (seg): {decode_time}")

    pass


if __name__ == "__main__":
    main()
