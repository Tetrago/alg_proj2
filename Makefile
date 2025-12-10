CXX     ?= g++
TARGETS ?= flow greedy

.PHONY: all
all: $(TARGETS)

.PHONY: clean
clean:
	rm -f $(TARGETS)

%: %.cpp
	$(CXX) -o $@ $<
