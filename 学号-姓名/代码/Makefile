CXX = g++
CXXFLAGS = -O3 -std=c++17 -fopenmp -Wall -Wextra -Wpedantic
BIN = ../../temp/bin
CORE = serial.cpp parallel.cpp io.cpp
.PHONY: all test sanitize clean
all: $(BIN)/serial $(BIN)/parallel $(BIN)/benchmark $(BIN)/verify
$(BIN):
	mkdir -p $@
$(BIN)/serial: serial.cpp io.cpp sobel.hpp | $(BIN)
	$(CXX) $(CXXFLAGS) serial.cpp io.cpp -o $@
$(BIN)/parallel: parallel.cpp io.cpp sobel.hpp | $(BIN)
	$(CXX) $(CXXFLAGS) parallel.cpp io.cpp -o $@
$(BIN)/benchmark: benchmark.cpp $(CORE) sobel.hpp | $(BIN)
	$(CXX) $(CXXFLAGS) -DSOBEL_LIBRARY benchmark.cpp $(CORE) -o $@
$(BIN)/verify: verify.cpp $(CORE) sobel.hpp | $(BIN)
	$(CXX) $(CXXFLAGS) -DSOBEL_LIBRARY verify.cpp $(CORE) -o $@
test: $(BIN)/verify
	$(BIN)/verify
sanitize: | $(BIN)
	$(CXX) -O1 -g -std=c++17 -fopenmp -fsanitize=address,undefined -fno-omit-frame-pointer -DSOBEL_LIBRARY verify.cpp $(CORE) -o $(BIN)/verify_sanitize
	$(BIN)/verify_sanitize
clean:
	rm -f $(BIN)/serial $(BIN)/parallel $(BIN)/benchmark $(BIN)/verify $(BIN)/verify_sanitize
