FROM ubuntu:latest

RUN apt update
RUN apt install -y gcc gdb make git binutils libc6-dev

WORKDIR /app