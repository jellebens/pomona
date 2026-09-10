"""Entry point: python -m demeter --config /config/config.yaml"""

from __future__ import annotations

import argparse
import logging
import sys

from . import __version__, config
from .runtime import Runtime


def main() -> int:
    parser = argparse.ArgumentParser(prog="demeter")
    parser.add_argument("--config", default="/config/config.yaml")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        stream=sys.stdout,
    )
    cfg = config.load(args.config)
    logging.getLogger("demeter").info(
        "starting demeter %s (mode=%s)", __version__, cfg.mode
    )
    Runtime(cfg).run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
