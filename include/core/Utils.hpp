/**
 * =================================================================================================
 * Projeto: OmniStatement Studio
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <regex>

namespace ofx::core {

class Utils {
public:
    static inline std::string trim(std::string_view s) {
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) return "";
        size_t last = s.find_last_not_of(" \t\r\n");
        return std::string(s.substr(first, (last - first) + 1));
    }

    static inline std::string toUpper(std::string_view s) {
        std::string res;
        res.reserve(s.size());
        for (char c : s) {
            res.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        return res;
    }

    static inline std::string toLower(std::string_view s) {
        std::string res;
        res.reserve(s.size());
        for (char c : s) {
            res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return res;
    }

    static inline std::vector<std::string> split(std::string_view s, char delimiter) {
        std::vector<std::string> tokens;
        size_t start = 0;
        size_t end = s.find(delimiter);
        while (end != std::string_view::npos) {
            tokens.emplace_back(s.substr(start, end - start));
            start = end + 1;
            end = s.find(delimiter, start);
        }
        tokens.emplace_back(s.substr(start));
        return tokens;
    }

    // Parse financial amounts resiliently (e.g. "R$ 1.250,50 D", "-1250.50", "(50,00)", "150,00-")
    static double parseAmount(std::string_view raw) {
        std::string s = trim(raw);
        if (s.empty()) return 0.0;

        bool isNegative = false;

        // Check parentheses: (100.00) => negative
        if (s.front() == '(' && s.back() == ')') {
            isNegative = true;
            s = s.substr(1, s.size() - 2);
        }

        // Check trailing or leading minus
        if (!s.empty() && s.front() == '-') {
            isNegative = true;
            s = s.substr(1);
        }
        if (!s.empty() && s.back() == '-') {
            isNegative = true;
            s.pop_back();
        }

        // Check trailing 'D' (Débito) or 'C' (Crédito)
        s = trim(s);
        if (!s.empty()) {
            char last = static_cast<char>(std::toupper(static_cast<unsigned char>(s.back())));
            if (last == 'D') {
                isNegative = true;
                s.pop_back();
            } else if (last == 'C') {
                s.pop_back();
            }
        }

        s = trim(s);
        // Strip out non-numeric chars except '.' and ','
        std::string clean;
        clean.reserve(s.size());
        for (char c : s) {
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == ',') {
                clean.push_back(c);
            }
        }

        if (clean.empty()) return 0.0;

        // Determine if comma or dot is the decimal separator
        size_t lastDot = clean.rfind('.');
        size_t lastComma = clean.rfind(',');

        std::string normalized;
        normalized.reserve(clean.size());

        if (lastComma != std::string::npos && (lastDot == std::string::npos || lastComma > lastDot)) {
            // Brazilian format: 1.234,56 -> remove dots, replace comma with dot
            for (size_t i = 0; i < clean.size(); ++i) {
                if (clean[i] == '.') continue;
                if (clean[i] == ',') normalized.push_back('.');
                else normalized.push_back(clean[i]);
            }
        } else {
            // US format: 1,234.56 -> remove commas
            for (size_t i = 0; i < clean.size(); ++i) {
                if (clean[i] == ',') continue;
                normalized.push_back(clean[i]);
            }
        }

        try {
            double val = std::stod(normalized);
            return isNegative ? -val : val;
        } catch (...) {
            return 0.0;
        }
    }

    // Normalizes various date formats to ISO 8601: "YYYY-MM-DD"
    static std::string normalizeDate(std::string_view raw) {
        std::string s = trim(raw);
        if (s.empty()) return "";

        // Remove time if present (e.g. "2023-11-04 14:00:00" or "20231104120000[-03:EST]")
        size_t spaceIdx = s.find(' ');
        if (spaceIdx != std::string::npos) {
            s = s.substr(0, spaceIdx);
        }
        size_t bracketIdx = s.find('[');
        if (bracketIdx != std::string::npos) {
            s = s.substr(0, bracketIdx);
        }

        // OFX raw timestamp: YYYYMMDD...
        if (s.size() >= 8 && std::all_of(s.begin(), s.begin() + 8, [](char c) { return std::isdigit(c); })) {
            std::string y = s.substr(0, 4);
            std::string m = s.substr(4, 2);
            std::string d = s.substr(6, 2);
            return y + "-" + m + "-" + d;
        }

        // Look for DD/MM/YYYY or DD/MM/YY
        std::regex brDateRegex(R"(^(\d{1,2})[\/\-](\d{1,2})[\/\-](\d{2,4}))");
        std::smatch match;
        if (std::regex_search(s, match, brDateRegex)) {
            std::string d = match[1].str();
            std::string m = match[2].str();
            std::string y = match[3].str();
            if (d.size() == 1) d = "0" + d;
            if (m.size() == 1) m = "0" + m;
            if (y.size() == 2) {
                int yi = std::stoi(y);
                y = (yi > 50 ? "19" : "20") + y;
            }
            // Check if format might be YYYY-MM-DD already
            if (match[1].length() == 4) {
                return match[1].str() + "-" + m + "-" + y;
            }
            return y + "-" + m + "-" + d;
        }

        // ISO format YYYY-MM-DD
        std::regex isoDateRegex(R"(^(\d{4})[\/\-](\d{1,2})[\/\-](\d{1,2}))");
        if (std::regex_search(s, match, isoDateRegex)) {
            std::string y = match[1].str();
            std::string m = match[2].str();
            std::string d = match[3].str();
            if (d.size() == 1) d = "0" + d;
            if (m.size() == 1) m = "0" + m;
            return y + "-" + m + "-" + d;
        }

        // Not a recognizable date
        return "";
    }

    static std::string formatCurrency(double val, bool useBrazilian = true) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << std::abs(val);
        std::string num = ss.str();
        
        if (useBrazilian) {
            std::string result;
            size_t dotPos = num.find('.');
            std::string intPart = num.substr(0, dotPos);
            std::string decPart = num.substr(dotPos + 1);

            int count = 0;
            for (int i = static_cast<int>(intPart.size()) - 1; i >= 0; --i) {
                result.push_back(intPart[i]);
                count++;
                if (count == 3 && i > 0) {
                    result.push_back('.');
                    count = 0;
                }
            }
            std::reverse(result.begin(), result.end());
            result += "," + decPart;
            if (val < 0.0) result = "-" + result;
            return result;
        }

        return (val < 0.0 ? "-" : "") + num;
    }

    static std::vector<uint8_t> readFileBinary(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return {};
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            return buffer;
        }
        return {};
    }

    static std::string readFileString(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    static bool writeFileBinary(const std::string& path, const uint8_t* data, size_t size) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        return file.good();
    }

    static bool writeFileString(const std::string& path, const std::string& content) {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << content;
        return file.good();
    }
};

} // namespace ofx::core
