from os import environ, listdir, path
from subprocess import run

SHADER_EXTENSIONS = {".vert", ".frag"}
COMPILED_EXTENSION = ".spv"

print("Compiling shaders...")
for file in listdir(environ["SHADERS_DIR"]):
    extension = path.splitext(file)[1]
    if extension in SHADER_EXTENSIONS:
        run([environ["VK_GLSL_VALIDATOR"], "-V", path.join(environ["SHADERS_DIR"], file),
             "-o", path.join(environ["COMPILED_DIR"], file + COMPILED_EXTENSION)])
