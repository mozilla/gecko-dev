FROM node:23-alpine@sha256:6eae672406a2bc8ed93eab6f9f76a02eb247e06ba82b2f5032c0a4ae07e825ba
ARG UID=1000
ARG GID=1000

RUN apk add -U clang lld wasi-sdk && mkdir /home/node/llhttp

WORKDIR /home/node/llhttp

COPY . .

RUN npm ci

USER node
