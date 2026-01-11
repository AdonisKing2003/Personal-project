# Cross-compilation Makefile for Pi4

TARGET = blog_server
SOURCE = markdown_blog_server.c

# These should be set by your environment after sourcing
CC ?= aarch64-linux-gnu-gcc
SYSROOT = $(SDKTARGETSYSROOT)

CFLAGS = -Wall -O2 -pthread --sysroot=$(SYSROOT)
LDFLAGS = -lcmark-gfm -lcmark-gfm-extensions

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
	@echo "✅ Cross-compilation complete!"
	@file $(TARGET)

check:
	@echo "=== Verifying ARM binary ==="
	@file $(TARGET)
	@echo ""
	@echo "=== Checking dependencies ==="
	@readelf -d $(TARGET) | grep NEEDED

deploy:
ifndef PI4_IP
	@echo "❌ Error: Set PI4_IP variable"
	@echo "Usage: make deploy PI4_IP=192.168.1.100"
	@exit 1
endif
	@echo "📤 Deploying to Pi4..."
	scp $(TARGET) pi@$(PI4_IP):~/
	@echo "✅ Deployed to pi@$(PI4_IP):~/"

clean:
	rm -f $(TARGET)

.PHONY: all check deploy clean