#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include "utils.h"

namespace fs = std::filesystem;
const std::string basePath = "./module_template/";

namespace {
    constexpr const char* kDefaultRepository = "RogerAngell99/MagiskHluda";
    const std::array<const char*, 4> kRequiredBinaries = {
        "bin/florida-arm.gz",
        "bin/florida-arm64.gz",
        "bin/florida-x86.gz",
        "bin/florida-x64.gz",
    };

    std::string trim(std::string value)
    {
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), value.end());
        return value;
    }

    std::string getRepositorySlug()
    {
        const char* repository = std::getenv("MAGISKHLUDA_REPOSITORY");
        if (repository != nullptr && repository[0] != '\0')
        {
            return repository;
        }

        repository = std::getenv("GITHUB_REPOSITORY");
        if (repository != nullptr && repository[0] != '\0')
        {
            return repository;
        }

        return kDefaultRepository;
    }

    std::string getConfiguredReleaseTag()
    {
        const std::array<const char*, 3> envNames = {
            "MAGISKHLUDA_RELEASE_TAG",
            "MAGISKHLUDA_FLORIDA_TAG",
            "GITHUB_REF_NAME",
        };

        for (const auto* envName : envNames)
        {
            const char* envValue = std::getenv(envName);
            if (envValue != nullptr && envValue[0] != '\0')
            {
                return envValue;
            }
        }

        std::ifstream currentTag("currentTag.txt");
        if (currentTag)
        {
            std::string tag;
            std::getline(currentTag, tag);
            tag = trim(tag);
            if (!tag.empty())
            {
                return tag;
            }
        }

        throw std::runtime_error(
            "Missing release tag. Set MAGISKHLUDA_RELEASE_TAG before generating metadata.");
    }
}

void utils::initializeReleaseMetadata()
{
    latestTag = trim(getConfiguredReleaseTag());
    if (latestTag.empty())
    {
        throw std::runtime_error("Resolved release tag is empty");
    }

    std::ofstream("currentTag.txt") << latestTag;
}

void utils::validateServerArtifacts()
{
    for (const auto* binaryPath : kRequiredBinaries)
    {
        if (!fs::exists(binaryPath))
        {
            throw std::runtime_error("Missing expected compiled server artifact: " + std::string(binaryPath));
        }
    }
}

void utils::createModuleProps()
{
    std::ofstream moduleProps(basePath + "module.prop");
    const auto repository = getRepositorySlug();

    if (!moduleProps)
    {
        throw std::runtime_error("Failed to open module.prop for writing");
    }

    std::string versionCode = latestTag;
    versionCode.erase(std::remove(versionCode.begin(), versionCode.end(), '.'), versionCode.end());
    moduleProps << "id=magisk-hluda\n"
        << "name=Frida(Florida) Server on Boot\n"
        << "version=" << latestTag.substr(0, latestTag.find('-')) << '\n'
        << "versionCode=" << versionCode << '\n'
        << "author=The Community - Ylarod - Exo1i\n"
        << "description=Runs a stealthier frida-server on boot\n"
        << "updateJson=https://github.com/" << repository << "/releases/latest/download/update.json";
}

void utils::createUpdateJson()
{
    std::string versionCode = latestTag;
    versionCode.erase(std::remove(versionCode.begin(), versionCode.end(), '.'), versionCode.end());
    const auto repository = getRepositorySlug();

    std::ofstream updateJson("update.json");
    if (!updateJson)
    {
        throw std::runtime_error("Failed to open update.json for writing");
    }

    updateJson << "{\n"
        << R"(  "version": ")" << latestTag << "\",\n"
        << "  \"versionCode\": " << versionCode << ",\n"
        << R"(  "zipUrl": "https://github.com/)" << repository << R"(/releases/download/)"
        << latestTag << "/Magisk-Florida-Universal-" << latestTag << ".zip\",\n"
        << R"(  "changelog": "https://github.com/)" << repository << "/releases/tag/" << latestTag << "\""
        << "\n}\n";
}
