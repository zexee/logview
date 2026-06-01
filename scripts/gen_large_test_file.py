#!/usr/bin/env python3
"""Generate a large test log file."""
import sys
import random

LEVELS = ["DEBUG", "INFO", "WARN", "ERROR", "FATAL"]
IPS = [f"192.168.{random.randint(0,255)}.{random.randint(1,254)}" for _ in range(20)]
MESSAGES = {
    "DEBUG": ["variable x={}", "entering function foo", "cache hit for key={}", "heap size: {}"],
    "INFO": ["request from {}", "connection established", "user {} logged in", "processed {} items"],
    "WARN": ["disk usage {}%", "memory threshold reached: {}MB", "retry attempt {} for {}", "slow query: {}ms"],
    "ERROR": ["connection timeout 503", "auth failure for user {}", "file not found: /tmp/{}.log", "null pointer in module {}"],
    "FATAL": ["out of memory", "unrecoverable disk error", "segfault at address 0x{}", "kernel panic"],
}

num_lines = int(sys.argv[1]) if len(sys.argv) > 1 else 1_000_000

random.seed(42)
with open("test_log.txt", "w") as f:
    for i in range(num_lines):
        level = random.choice(LEVELS)
        msg = random.choice(MESSAGES[level])
        ip = random.choice(IPS)
        val = random.randint(1, 99999)
        f.write(f"{level} {msg.format(ip, val)}\n")

print(f"Generated {num_lines} lines in test_log.txt")
