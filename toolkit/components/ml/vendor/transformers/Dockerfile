# Use the latest Ubuntu image
FROM ubuntu:latest

# Install required dependencies
RUN apt-get update && apt-get install -y curl git && \
  curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
  apt-get install -y nodejs && \
  apt-get clean

# Set the working directory to /tmp/transformers.js
WORKDIR /tmp/transformers.js

# Default command when running the container
CMD ["sh", "-c", "npm ci && npm run build:dev && npm run build:prod && npm run build"]
