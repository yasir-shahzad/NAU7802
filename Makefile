.PHONY: Nau7802

Nau7802: examples/Example2_CompleteScale/Example2_CompleteScale.cpp
	g++ -std=c++17 examples/Example2_CompleteScale/Example2_CompleteScale.cpp src/NAU7802.cpp -li2c -o bin/Nau7802