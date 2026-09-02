# VeloraOS Python package example
import sys
import platform

print("=== Hello from VeloraOS Python package ===")
print("Python:", sys.version)
print("Platform:", platform.platform())
print("Executable:", sys.executable)
print()
print("Packages can live in apps/ or VelFS/Applications/User/")
print("manifest.json type=python, entry=main.py")
