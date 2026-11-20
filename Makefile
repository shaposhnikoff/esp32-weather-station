CMAKE = uv tool run --python 3.12 --from cmake==3.30.5 --with ninja==1.11.1.3 cmake
PIO ?= pio
BOARD ?= waveshare-lcd5b

.PHONY: firmware desktop preview clean-firmware

firmware:
	$(PIO) run -e $(BOARD) -j 1

desktop:
	$(CMAKE) -S . -B build-desktop -DWEATHER_DESKTOP=ON -DWEATHER_PREVIEW_NATIVE=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew
	$(CMAKE) --build build-desktop -j 8

preview: desktop
	./build-desktop/weather-preview

clean-firmware:
	$(PIO) run -e $(BOARD) -t clean
