/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for non-commercial purposes only           *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution allowed with this notice.                 *
 *   No obligation to disclose modifications.                 *
 *   See LICENSE file for full terms.                         *
 **************************************************************/
#include "aeventsystem.hpp"

#include <zlib.h>

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

namespace almondnamespace {

    namespace detail {
        [[nodiscard]] inline std::string encode_utf8_char(char32_t codepoint) {
            std::string utf8;
            if (codepoint == 0) {
                return utf8;
            }

            if (codepoint <= 0x7F) {
                utf8.push_back(static_cast<char>(codepoint));
            }
            else if (codepoint <= 0x7FF) {
                utf8.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
                utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else if (codepoint <= 0xFFFF) {
                utf8.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
                utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else {
                utf8.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
                utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }

            return utf8;
        }

        [[nodiscard]] inline char32_t decode_utf8_char(std::string_view utf8) {
            if (utf8.empty()) {
                return 0;
            }

            const unsigned char* bytes = reinterpret_cast<const unsigned char*>(utf8.data());
            const auto size = utf8.size();

            if (bytes[0] <= 0x7F) {
                return bytes[0];
            }
            if ((bytes[0] & 0xE0) == 0xC0 && size >= 2) {
                return static_cast<char32_t>(((bytes[0] & 0x1F) << 6) |
                                             (bytes[1] & 0x3F));
            }
            if ((bytes[0] & 0xF0) == 0xE0 && size >= 3) {
                return static_cast<char32_t>(((bytes[0] & 0x0F) << 12) |
                                             ((bytes[1] & 0x3F) << 6) |
                                             (bytes[2] & 0x3F));
            }
            if ((bytes[0] & 0xF8) == 0xF0 && size >= 4) {
                return static_cast<char32_t>(((bytes[0] & 0x07) << 18) |
                                             ((bytes[1] & 0x3F) << 12) |
                                             ((bytes[2] & 0x3F) << 6) |
                                             (bytes[3] & 0x3F));
            }

            return 0;
        }
    } // namespace detail

    class SaveSystem {
    public:
        static void SaveGame(const std::string& filename, const std::vector<almondnamespace::events::Event>& events) {
           std::ofstream ofs(filename, std::ios::binary);  
           if (!ofs) {  
               std::cerr << "Error opening file for saving!" << std::endl;  
               return;  
           }  

           std::string data;  
           for (const auto& event : events) {  
               data += event_type_to_string(event.type) + ":";  
               for (const auto& pair : event.data) {  
                   data += pair.first + "=" + pair.second + ";";  
               }  
               data += "x=" + std::to_string(event.x) + ";";  
               data += "y=" + std::to_string(event.y) + ";";  
               data += "key=" + std::to_string(event.key) + ";";  

               data += "text=" + detail::encode_utf8_char(event.text) + ";";
               data += "\n";
           }

           std::string compressedData = CompressData(data);  
           ofs.write(compressedData.c_str(), compressedData.size());  
           ofs.close();  
        }

        static void LoadGame(const std::string& filename, std::vector<almondnamespace::events::Event>& events) {
            std::ifstream ifs(filename, std::ios::binary);
            if (!ifs) {
                std::cerr << "Error opening file for loading!" << std::endl;
                return;
            }

            std::string compressedData((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            std::string data = DecompressData(compressedData);

            size_t pos = 0;
            while ((pos = data.find('\n')) != std::string::npos) {
                std::string line = data.substr(0, pos);
                events::Event event;

                size_t typeEnd = line.find(':');
                if (typeEnd == std::string::npos) {
                    data.erase(0, pos + 1);
                    continue;
                }

                event.type = almondnamespace::events::event_type_from(line.substr(0, typeEnd));
                std::string details = line.substr(typeEnd + 1);

                size_t semicolonPos;
                while ((semicolonPos = details.find(';')) != std::string::npos) {
                    std::string keyValue = details.substr(0, semicolonPos);
                    size_t equalPos = keyValue.find('=');
                    if (equalPos != std::string::npos) {
                        std::string key = keyValue.substr(0, equalPos);
                        std::string value = keyValue.substr(equalPos + 1);

                        if (key == "x") {
                            event.x = std::stof(value);
                        }
                        else if (key == "y") {
                            event.y = std::stof(value);
                        }
                        else if (key == "key") {
                            event.key = std::stoi(value);
                        }
                        else if (key == "text") {
                            event.text = detail::decode_utf8_char(value);
                        }
                        else {
                            event.data[key] = value;
                        }
                    }
                    details.erase(0, semicolonPos + 1);
                }

                events.push_back(event);
                data.erase(0, pos + 1);
            }

            ifs.close();
        }

    private:
        static std::string CompressData(const std::string& data) {
            uLongf compressedSize = compressBound(static_cast<uLong>(data.size()));
            std::vector<Bytef> compressedData(compressedSize);
            if (compress(compressedData.data(), &compressedSize, reinterpret_cast<const Bytef*>(data.data()), static_cast<uLong>(data.size())) != Z_OK) {
                return data;
            }
            compressedData.resize(compressedSize);
            return std::string(reinterpret_cast<char*>(compressedData.data()), compressedSize);
        }

        static std::string DecompressData(const std::string& compressedData) {
            uLongf decompressedSize = static_cast<uLong>(compressedData.size()) * 4;
            std::vector<Bytef> decompressedData(decompressedSize);
            while (uncompress(decompressedData.data(), &decompressedSize, reinterpret_cast<const Bytef*>(compressedData.data()), static_cast<uLong>(compressedData.size())) == Z_BUF_ERROR) {
                decompressedSize *= 2;
                decompressedData.resize(decompressedSize);
            }
            return std::string(reinterpret_cast<char*>(decompressedData.data()), decompressedSize);
        }
    };

}  // namespace almond
