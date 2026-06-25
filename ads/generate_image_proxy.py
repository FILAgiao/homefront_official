#!/usr/bin/env python3
"""
Generate images through an OpenAI-compatible proxy endpoint.

Examples:
  python generate_image_proxy.py ^
    --prompt "Luxury villa garden at sunset with hidden irrigation" ^
    --out output\\imagegen\\hero.png

  python generate_image_proxy.py ^
    --prompt-file tmp\\imagegen\\hero_prompt.txt ^
    --size 1024x1536 ^
    --quality high ^
    --out output\\imagegen\\hero.png
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import sys
from pathlib import Path
from urllib import error, request


DEFAULT_BASE_URL = "https://qweapi.com"
DEFAULT_MODEL = "gpt-image-2"
DEFAULT_API_KEY = "sk-qsaTDGDAVPBfGJboIqJq8HVealfyAlmQJhr8IsgMZlWgi3T5"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate an image through an OpenAI-compatible proxy."
    )
    parser.add_argument("--prompt", help="Prompt text.")
    parser.add_argument("--prompt-file", help="Read prompt text from a file.")
    parser.add_argument("--out", required=True, help="Output image path.")
    parser.add_argument("--api-key", help="API key. Defaults to QWEAPI_KEY or OPENAI_API_KEY.")
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL, help="Proxy base URL.")
    parser.add_argument("--model", default=DEFAULT_MODEL, help="Image model id.")
    parser.add_argument("--size", default="1024x1536", help="Image size, for example 1024x1536.")
    parser.add_argument(
        "--quality",
        default="high",
        choices=["low", "medium", "high", "auto"],
        help="Image quality.",
    )
    parser.add_argument(
        "--background",
        default="auto",
        help="Background mode if the proxy supports it.",
    )
    parser.add_argument(
        "--output-format",
        default="png",
        choices=["png", "jpeg", "webp"],
        help="Returned image format.",
    )
    parser.add_argument(
        "--n",
        type=int,
        default=1,
        help="Number of images to request. Script saves the first image only.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the request payload without sending it.",
    )
    return parser


def load_prompt(args: argparse.Namespace) -> str:
    if args.prompt and args.prompt_file:
        raise SystemExit("Use either --prompt or --prompt-file, not both.")
    if args.prompt:
        return args.prompt.strip()
    if args.prompt_file:
        return Path(args.prompt_file).read_text(encoding="utf-8").strip()
    raise SystemExit("You must provide --prompt or --prompt-file.")


def resolve_api_key(args: argparse.Namespace) -> str:
    api_key = (
        args.api_key
        or os.environ.get("QWEAPI_KEY")
        or os.environ.get("OPENAI_API_KEY")
        or DEFAULT_API_KEY
    )
    if not api_key:
        raise SystemExit("Missing API key. Use --api-key or set QWEAPI_KEY / OPENAI_API_KEY.")
    return api_key


def build_payload(args: argparse.Namespace, prompt: str) -> dict:
    return {
        "model": args.model,
        "prompt": prompt,
        "size": args.size,
        "quality": args.quality,
        "background": args.background,
        "output_format": args.output_format,
        "n": args.n,
    }


def extract_image_bytes(response_json: dict) -> bytes:
    data = response_json.get("data")
    if not isinstance(data, list) or not data:
        raise ValueError("Response does not contain a non-empty data array.")

    first = data[0]
    b64 = first.get("b64_json")
    if b64:
        return base64.b64decode(b64)

    url = first.get("url")
    if url:
        with request.urlopen(url) as image_resp:
            return image_resp.read()

    raise ValueError("Response item does not contain b64_json or url.")


def save_bytes(out_path: Path, payload: bytes) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(payload)


def post_json(url: str, api_key: str, payload: dict) -> dict:
    body = json.dumps(payload).encode("utf-8")
    req = request.Request(
        url,
        data=body,
        method="POST",
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        },
    )
    with request.urlopen(req) as resp:
        raw = resp.read().decode("utf-8")
    return json.loads(raw)


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    prompt = load_prompt(args)
    api_key = resolve_api_key(args)
    payload = build_payload(args, prompt)

    if args.dry_run:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
        return 0

    endpoint = args.base_url.rstrip("/") + "/v1/images/generations"
    out_path = Path(args.out)

    try:
        response_json = post_json(endpoint, api_key, payload)
        image_bytes = extract_image_bytes(response_json)
        save_bytes(out_path, image_bytes)
    except error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        print(f"HTTP {exc.code}: {detail}", file=sys.stderr)
        return 1
    except error.URLError as exc:
        print(f"Network error: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:  # noqa: BLE001
        print(f"Failed: {exc}", file=sys.stderr)
        return 1

    print(f"Saved image to {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
