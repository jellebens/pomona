# Demeter — pomona autodosing controller.
# The k3s cluster is arm64: build with
#   docker buildx build --platform linux/arm64 --provenance=false \
#     -t jellebens/pomona-demeter:<version> --push controller/
FROM python:3.12-slim

WORKDIR /app
COPY pyproject.toml ./
COPY demeter ./demeter
RUN pip install --no-cache-dir .

# Non-root, read-only-rootfs friendly (no writes outside /tmp).
USER 1000:1000

ENTRYPOINT ["python", "-m", "demeter"]
CMD ["--config", "/config/config.yaml"]
