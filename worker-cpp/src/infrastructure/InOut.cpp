#include "InOut.h"
#include <iostream>

std::string InOut::readNextJob() {
    std::string line;
    std::getline(std::cin, line);
    return line;
}

void InOut::writeResult(const std::string& resultJson) {
    std::cout << resultJson << std::endl;
}

bool InOut::isShutdownCommand(const std::string& input) const {
    return input == "shutdown";
}
