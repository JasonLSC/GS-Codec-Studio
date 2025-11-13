#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import cv2
import numpy as np
import argparse
import os


def annotate_image(
    input_path: str,
    u: int,
    v: int,
    h: int,
    w: int,
    out_path: str,
    thickness: int = 4,
    margin: int = 16,
    rect_color=(0, 0, 255),  # BGR: red
):
    """
    Draw a red rectangle highlighting the region and place a x2 zoomed inset
    of the region at the bottom-right corner of the image.

    Args:
        input_path: path to the input image.
        u, v: top-left corner of the highlight region (x=u, y=v).
        h, w: height and width of the highlight region.
        out_path: path to save the annotated output image.
        thickness: rectangle line thickness in pixels.
        margin: margin in pixels around the inset.
        rect_color: BGR color tuple for the rectangle (default red).
    """
    img = cv2.imread(input_path, cv2.IMREAD_UNCHANGED)
    if img is None:
        raise FileNotFoundError(f"Failed to read image: {input_path}")

    # If image has alpha channel, drop it for drawing operations.
    if img.ndim == 3 and img.shape[2] == 4:
        img = cv2.cvtColor(img, cv2.COLOR_BGRA2BGR)

    H, W = img.shape[:2]

    # Sanitize coordinates and clip to image bounds.
    x1 = int(max(0, u))
    y1 = int(max(0, v))
    x2 = int(min(W, u + w))
    y2 = int(min(H, v + h))

    if x2 <= x1 or y2 <= y1:
        raise ValueError("Highlight region is empty or out of bounds after clipping.")

    # Draw highlight rectangle.
    cv2.rectangle(img, (x1, y1), (x2, y2), rect_color, thickness=thickness)

    # Extract ROI and create x2 zoom (with fallback scaling if needed).
    roi = img[y1:y2, x1:x2]
    roi_h, roi_w = roi.shape[:2]

    target_w = roi_w * 2
    target_h = roi_h * 2

    # Compute max allowed size for inset (respecting margins).
    max_inset_w = max(1, W - 2 * margin)
    max_inset_h = max(1, H - 2 * margin)

    scale = min(
        max_inset_w / target_w if target_w > 0 else 1.0,
        max_inset_h / target_h if target_h > 0 else 1.0,
        1.0,
    )
    disp_w = max(1, int(round(target_w * scale)))
    disp_h = max(1, int(round(target_h * scale)))

    inset = cv2.resize(roi, (disp_w, disp_h), interpolation=cv2.INTER_CUBIC)

    # Bottom-right placement with margin.
    br_x2 = W - margin
    br_y2 = H - margin
    br_x1 = br_x2 - disp_w
    br_y1 = br_y2 - disp_h

    # Safety clip if margins are tight.
    if br_x1 < 0:
        shift = -br_x1
        br_x1 += shift
        br_x2 += shift
    if br_y1 < 0:
        shift = -br_y1
        br_y1 += shift
        br_y2 += shift

    # Paste inset.
    img[br_y1:br_y2, br_x1:br_x2] = inset

    # Draw border around inset to distinguish it.
    cv2.rectangle(img, (br_x1, br_y1), (br_x2, br_y2), rect_color, thickness=max(1, thickness - 1))

    # Ensure output directory exists.
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)

    # Save result.
    ok = cv2.imwrite(out_path, img)
    if not ok:
        raise IOError(f"Failed to write output image: {out_path}")


def parse_args():
    p = argparse.ArgumentParser(
        description="Highlight a region and place a x2 zoomed inset at bottom-right."
    )
    p.add_argument(
        "--input",
        required=True,
        help="Comma-separated list of input image paths (e.g. img1.png,img2.png).",
    )
    p.add_argument("--u", type=int, required=True, help="Left (x) of top-left corner.")
    p.add_argument("--v", type=int, required=True, help="Top (y) of top-left corner.")
    p.add_argument("--h", type=int, required=True, help="Height of the region.")
    p.add_argument("--w", type=int, required=True, help="Width of the region.")
    p.add_argument(
        "--output",
        required=False,
        default=None,
        help=(
            "Optional output path for single-image runs. When omitted (or when processing"
            " multiple inputs), results save as <input>_crop.<ext> alongside each source."
        ),
    )
    p.add_argument(
        "--thickness",
        type=int,
        default=4,
        help="Rectangle line thickness (3-5 recommended).",
    )
    p.add_argument("--margin", type=int, default=16, help="Margin for inset placement.")
    return p.parse_args()


def main():
    args = parse_args()
    input_paths = [item.strip() for item in args.input.split(",") if item.strip()]
    if not input_paths:
        raise ValueError("No valid input image paths provided.")

    multiple_inputs = len(input_paths) > 1

    if multiple_inputs and args.output:
        print("Ignoring --output because multiple input images were provided.")

    print(f"Processing {len(input_paths)} image(s)...")

    for input_path in input_paths:
        out_path = args.output if args.output and not multiple_inputs else None
        if out_path is None:
            base_dir = os.path.dirname(input_path)
            base_name = os.path.basename(input_path)
            name, ext = os.path.splitext(base_name)
            if ext == "":
                ext = ".png"
            out_path = os.path.join(base_dir or ".", f"{name}_crop{ext}")

        print(f"Annotating '{input_path}' -> '{out_path}'")
        annotate_image(
            input_path=input_path,
            u=args.u,
            v=args.v,
            h=args.h,
            w=args.w,
            out_path=out_path,
            thickness=args.thickness,
            margin=args.margin,
        )


if __name__ == "__main__":
    main()


