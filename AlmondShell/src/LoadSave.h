#pragma once

#include "EventSystem.h"

#include <string>
#include <vector>
#include <map>

namespace almond {

    class SaveSystem {
    public:
        static void SaveGame(const std::string& filename, const std::vector<Event>& events);
        static void LoadGame(const std::string& filename, std::vector<Event>& events);

    private:
        static std::string CompressData(const std::string& data);
        static std::string DecompressData(const std::string& compressedData);
    };

}  // namespace almond
