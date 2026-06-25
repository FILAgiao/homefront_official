#!/usr/bin/env python3
"""
Batch image generation through an OpenAI-compatible proxy endpoint.

Each line in the input JSONL should contain at least:
  - prompt
  - out

Optional per-line overrides:
  - model
  - size
  - quality
  - background
  - output_format
  - n
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

DEFAULT_API_KEY = "sk-qsaTDGDAVPBfGJboIqJq8HVealfyAlmQJhr8IsgMZlWgi3T5"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Batch-generate images through the local proxy image script."
    )
    parser.add_argument("--input", required=True, help="Input JSONL file.")
    parser.add_argument("--out-dir", required=True, help="Output directory.")
    parser.add_argument("--api-key", help="API key. Defaults to QWEAPI_KEY or OPENAI_API_KEY.")
    parser.add_argument("--base-url", default="https://qweapi.com", help="Proxy base URL.")
    parser.add_argument("--default-model", default="gpt-image-2", help="Default model.")
    parser.add_argument(
        "--default-size", default="1024x1536", help="Default image size."
    )
    parser.add_argument(
        "--default-quality",
        default="high",
        choices=["low", "medium", "high", "auto"],
        help="Default quality.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Print jobs only.")
    return parser


def load_jobs(path: Path) -> list[dict]:
    jobs: list[dict] = []
    for index, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.strip():
            continue
        try:
            jobs.append(json.loads(line))
        except json.JSONDecodeError as exc:
            raise SystemExit(f"Invalid JSON on line {index}: {exc}") from exc
    return jobs


def main() -> int:
    args = build_parser().parse_args()
    input_path = Path(args.input)
    out_dir = Path(args.out_dir)
    script_path = Path(__file__).with_name("generate_image_proxy.py")
    api_key = (
        args.api_key
        or os.environ.get("QWEAPI_KEY")
        or os.environ.get("OPENAI_API_KEY")
        or DEFAULT_API_KEY
    )

    if not api_key and not args.dry_run:
        raise SystemExit("Missing API key. Use --api-key or set QWEAPI_KEY / OPENAI_API_KEY.")

    jobs = load_jobs(input_path)
    out_dir.mkdir(parents=True, exist_ok=True)

    for index, job in enumerate(jobs, start=1):
        prompt = job.get("prompt")
        out_name = job.get("out")
        if not prompt or not out_name:
            raise SystemExit(f"Job {index} must include prompt and out.")

        out_path = out_dir / out_name
        cmd = [
            sys.executable,
            str(script_path),
            "--prompt",
            prompt,
            "--out",
            str(out_path),
            "--base-url",
            job.get("base_url", args.base_url),
            "--model",
            job.get("model", job.get("default_model", args.default_model)),
            "--size",
            job.get("size", args.default_size),
            "--quality",
            job.get("quality", args.default_quality),
            "--background",
            job.get("background", "auto"),
            "--output-format",
            job.get("output_format", "png"),
            "--n",
            str(job.get("n", 1)),
        ]

        if api_key:
            cmd.extend(["--api-key", api_key])

        if args.dry_run:
            print(" ".join(json.dumps(part, ensure_ascii=False) for part in cmd))
            continue

        print(f"[{index}/{len(jobs)}] Generating {out_name} ...")
        result = subprocess.run(cmd, check=False)
        if result.returncode != 0:
            return result.returncode

    print(f"Finished {len(jobs)} image jobs into {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
