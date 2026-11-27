CXX ?= g++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra

test: build/test_energy
	./build/test_energy

build/test_energy: test/test_energy.cpp firmware/AhMeter/energy.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ test/test_energy.cpp

compile:
	arduino-cli compile --fqbn arduino:avr:uno firmware/AhMeter

clean:
	rm -rf build

.PHONY: test compile clean
