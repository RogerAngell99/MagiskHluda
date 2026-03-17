#include "utils.h"
#include <iostream>

std::string utils::latestTag;

int main() {
    try {
        utils::initializeReleaseMetadata();
        utils::validateServerArtifacts();
        utils::createModuleProps();
        utils::createUpdateJson();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
