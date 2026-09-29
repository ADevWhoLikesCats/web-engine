CC         := emcc
TARGET     := triangle
SRC        := src/main.c src/asset.c src/framebuffer.c src/pass.c src/capture.c src/sh.c src/sg.c src/texture.c src/mesh.c src/ground.c src/shadow.c src/probe_grid.c src/tri_grid.c src/instanced_mesh.c src/cgltf_impl.c src/stb_image_impl.c
SHELL_HTML := shell/index.html
OUT        := $(TARGET).html

CFLAGS  := -std=gnu11 -O3 -Wall -Wextra -Wno-unused-parameter -Ithird_party -Isrc

LDFLAGS := \
    -sUSE_GLFW=3 \
    -sFULL_ES3=1 \
    -sMIN_WEBGL_VERSION=2 \
    -sMAX_WEBGL_VERSION=2 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sINITIAL_MEMORY=256MB \
    -sSTACK_SIZE=1MB \
    --preload-file shaders@/shaders \
    --preload-file assets@/assets \
    --shell-file $(SHELL_HTML)

.PHONY: all clean serve run

all: $(OUT)

$(OUT): $(SRC) $(wildcard src/*.h) $(SHELL_HTML) $(wildcard shaders/*) $(wildcard assets/*)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(TARGET).js $(TARGET).wasm $(TARGET).data $(TARGET).html

serve:
	@echo "http://localhost:8000/$(TARGET).html"
	@python3 -m http.server 8000

run: all serve
