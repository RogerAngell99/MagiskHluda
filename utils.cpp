#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <unordered_map>
#include "restclient-cpp/connection.h"
#include "restclient-cpp/restclient.h"
#include "rapidjson/document.h"
#include "utils.h"

namespace fs = std::filesystem;
const std::string basePath = "./module_template/";
namespace {
    constexpr const char* kDefaultRepository = "RogerAngell99/MagiskHluda";
    const std::array<std::string, 4> kArchitectures = {"arm", "arm64", "x86", "x86_64"};
    std::unordered_map<std::string, std::string> g_downloadUrls;

    struct ParsedUrl
    {
        std::string baseUrl;
        std::string path;
    };

    ParsedUrl parseUrl(const std::string& url)
    {
        const auto schemePos = url.find("://");
        if (schemePos == std::string::npos)
        {
            throw std::runtime_error("Invalid URL: missing scheme in " + url);
        }

        const auto pathPos = url.find('/', schemePos + 3);
        if (pathPos == std::string::npos)
        {
            return {url, "/"};
        }

        return {url.substr(0, pathPos), url.substr(pathPos)};
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

    std::string getExpectedAssetName(const std::string& tag, const std::string& architecture)
    {
        return "florida-server-" + tag + "-android-" + architecture + ".gz";
    }

    bool loadReleaseMetadata(const rapidjson::Value& release)
    {
        if (!release.IsObject())
        {
            return false;
        }

        if (release.HasMember("draft") && release["draft"].IsBool() && release["draft"].GetBool())
        {
            return false;
        }

        if (release.HasMember("prerelease") && release["prerelease"].IsBool() && release["prerelease"].GetBool())
        {
            return false;
        }

        if (!release.HasMember("tag_name") || !release["tag_name"].IsString())
        {
            return false;
        }

        if (!release.HasMember("assets") || !release["assets"].IsArray())
        {
            return false;
        }

        const std::string tag = release["tag_name"].GetString();
        std::unordered_map<std::string, std::string> releaseDownloadUrls;

        for (const auto& asset : release["assets"].GetArray())
        {
            if (!asset.IsObject() || !asset.HasMember("name") || !asset["name"].IsString() ||
                !asset.HasMember("browser_download_url") || !asset["browser_download_url"].IsString())
            {
                continue;
            }

            releaseDownloadUrls.emplace(asset["name"].GetString(), asset["browser_download_url"].GetString());
        }

        std::unordered_map<std::string, std::string> serverDownloadUrls;
        for (const auto& architecture : kArchitectures)
        {
            const auto assetName = getExpectedAssetName(tag, architecture);
            const auto assetIt = releaseDownloadUrls.find(assetName);
            if (assetIt == releaseDownloadUrls.end())
            {
                return false;
            }

            serverDownloadUrls.emplace(architecture, assetIt->second);
        }

        utils::latestTag = tag;
        g_downloadUrls = std::move(serverDownloadUrls);
        std::ofstream("currentTag.txt") << utils::latestTag;
        return true;
    }
}

void utils::initializeReleaseMetadata()
{
    const std::string url = "https://api.github.com/repos/Ylarod/Florida/releases?per_page=20";
    RestClient::Response response = RestClient::get(url);

    if (response.code != 200)
    {
        throw std::runtime_error("HTTP Error: " + std::to_string(response.code) + " " + response.body);
    }

    rapidjson::Document d;
    d.Parse(response.body.c_str());

    if (!d.IsArray())
    {
        throw std::runtime_error("Invalid JSON response: expected releases array");
    }

    const char* requestedTag = std::getenv("MAGISKHLUDA_FLORIDA_TAG");
    if (requestedTag != nullptr && requestedTag[0] != '\0')
    {
        for (const auto& release : d.GetArray())
        {
            if (!release.IsObject() || !release.HasMember("tag_name") || !release["tag_name"].IsString())
            {
                continue;
            }

            if (release["tag_name"].GetString() == std::string(requestedTag) && loadReleaseMetadata(release))
            {
                return;
            }
        }

        throw std::runtime_error("Requested Florida release tag is unavailable or missing required assets: " +
            std::string(requestedTag));
    }

    for (const auto& release : d.GetArray())
    {
        if (loadReleaseMetadata(release))
        {
            return;
        }
    }

    throw std::runtime_error("No Florida release with all required server assets was found");
}

void download(const std::string& aarch)
{
    auto start = std::chrono::system_clock::now();

    std::cout << "Starting To Downloaded florida for arch: " + aarch + "\n";

    const auto assetIt = g_downloadUrls.find(aarch);
    if (assetIt == g_downloadUrls.end())
    {
        throw std::runtime_error("Missing download URL for architecture: " + aarch);
    }

    const auto parsedUrl = parseUrl(assetIt->second);

    std::unique_ptr<RestClient::Connection> pConnection(new RestClient::Connection(parsedUrl.baseUrl));
    pConnection->FollowRedirects(true);
    RestClient::Response response = pConnection->get(parsedUrl.path);

    if (response.code != 200)
    {
        throw std::runtime_error("Download failed: " + std::to_string(response.code) + " " + response.body);
    }
    std::string filename;
    if (aarch == "x86_64")
        filename = "bin/florida-x64.gz";
    else
        filename = "bin/florida-" + aarch + ".gz";

    std::ofstream downloadedFile(filename, std::ios::out | std::ios::binary);

    if (!downloadedFile)
    {
        throw std::runtime_error("Failed to open file for writing: " + filename);
    }

    downloadedFile.write(response.body.c_str(), response.body.length());

    auto end = std::chrono::system_clock::now();

    std::chrono::duration<double> elapsed_seconds = end - start;
    std::cout
        << "Successfully Downloaded florida for arch: " + aarch + ". Took " + to_string(elapsed_seconds.count()) +
        "s\n";
}

void utils::downloadServers()
{
    fs::create_directories("./bin");

    for (const auto& aarch : kArchitectures)
    {
        try
        {
            download(aarch);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error downloading " << aarch << ": " << e.what() << std::endl;
            throw std::runtime_error("Error downloading " + aarch + ": " + e.what());
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

    string versionCode = latestTag;
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
    string versionCode = latestTag;
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
