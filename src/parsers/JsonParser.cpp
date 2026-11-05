/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "parsers/JsonParser.hpp"
#include "core/Utils.hpp"
#include <regex>
#include <algorithm>

namespace ofx::parsers {

bool JsonParser::canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const {
    std::string_view header(reinterpret_cast<const char*>(headerData), std::min(headerSize, size_t(256)));
    size_t firstNonSpace = header.find_first_not_of(" \t\r\n");
    if (firstNonSpace != std::string_view::npos) {
        char first = header[firstNonSpace];
        if (first == '{' || first == '[') {
            return true;
        }
    }
    return filePath.size() >= 5 && core::Utils::toLower(filePath.substr(filePath.size() - 5)) == ".json";
}

static std::string extractJsonString(std::string_view obj, std::string_view key) {
    std::string pattern = "\"" + std::string(key) + "\"";
    size_t kPos = obj.find(pattern);
    if (kPos == std::string_view::npos) return "";

    size_t colon = obj.find(':', kPos + pattern.size());
    if (colon == std::string_view::npos) return "";

    size_t qOpen = obj.find('"', colon + 1);
    if (qOpen == std::string_view::npos) return "";

    size_t qClose = qOpen + 1;
    while (qClose < obj.size()) {
        if (obj[qClose] == '\\') {
            qClose += 2;
            continue;
        }
        if (obj[qClose] == '"') break;
        qClose++;
    }

    if (qClose >= obj.size()) return "";

    std::string_view valView = obj.substr(qOpen + 1, qClose - (qOpen + 1));
    std::string unescaped;
    unescaped.reserve(valView.size());
    for (size_t i = 0; i < valView.size(); ++i) {
        if (valView[i] == '\\' && i + 1 < valView.size()) {
            char next = valView[++i];
            if (next == 'n') unescaped.push_back('\n');
            else if (next == 'r') unescaped.push_back('\r');
            else if (next == 't') unescaped.push_back('\t');
            else if (next == '"') unescaped.push_back('"');
            else if (next == '\\') unescaped.push_back('\\');
            else unescaped.push_back(next);
        } else {
            unescaped.push_back(valView[i]);
        }
    }
    return unescaped;
}

static double extractJsonNumber(std::string_view obj, std::string_view key, double defaultVal = 0.0) {
    std::string pattern = "\"" + std::string(key) + "\"";
    size_t kPos = obj.find(pattern);
    if (kPos == std::string_view::npos) return defaultVal;

    size_t colon = obj.find(':', kPos + pattern.size());
    if (colon == std::string_view::npos) return defaultVal;

    size_t valStart = obj.find_first_of("-0123456789.", colon + 1);
    if (valStart == std::string_view::npos) return defaultVal;

    size_t valEnd = obj.find_first_not_of("-0123456789.eE+", valStart);
    if (valEnd == std::string_view::npos) valEnd = obj.size();

    std::string numStr(obj.substr(valStart, valEnd - valStart));
    try {
        return std::stod(numStr);
    } catch (...) {
        return defaultVal;
    }
}

std::shared_ptr<core::Statement> JsonParser::parseFromString(const std::string& jsonContent) const {
    auto statement = std::make_shared<core::Statement>();
    statement->sourceFormat = core::FileFormat::JSON;

    if (jsonContent.empty()) return statement;

    std::string_view root(jsonContent);

    // Extract Account block
    statement->account.bankId = extractJsonString(root, "bankId");
    statement->account.bankName = extractJsonString(root, "bankName");
    statement->account.branchId = extractJsonString(root, "branchId");
    statement->account.accountId = extractJsonString(root, "accountId");
    statement->account.accountType = extractJsonString(root, "accountType");
    statement->account.currency = extractJsonString(root, "currency");
    if (statement->account.currency.empty()) statement->account.currency = "BRL";

    // Period
    statement->periodStart = extractJsonString(root, "start");
    statement->periodEnd = extractJsonString(root, "end");

    // Balances
    statement->initialBalance = extractJsonNumber(root, "initialBalance", 0.0);
    statement->finalBalance = extractJsonNumber(root, "finalBalance", 0.0);
    if (root.find("\"initialBalance\"") != std::string_view::npos) statement->hasInitialBalance = true;
    if (root.find("\"finalBalance\"") != std::string_view::npos) statement->hasFinalBalance = true;

    // Parse transactions array
    size_t txArrayPos = root.find("\"transactions\"");
    size_t scanStart = (txArrayPos != std::string_view::npos) ? root.find('[', txArrayPos) : root.find('[');

    if (scanStart != std::string_view::npos) {
        size_t pos = scanStart + 1;
        while (pos < root.size()) {
            size_t objOpen = root.find('{', pos);
            if (objOpen == std::string_view::npos) break;

            // Find matching closing brace '}' respecting nesting and quotes
            size_t objClose = objOpen + 1;
            int depth = 1;
            bool inQuote = false;

            while (objClose < root.size() && depth > 0) {
                char c = root[objClose];
                if (c == '"' && root[objClose - 1] != '\\') {
                    inQuote = !inQuote;
                } else if (!inQuote) {
                    if (c == '{') depth++;
                    else if (c == '}') depth--;
                }
                objClose++;
            }

            if (depth == 0) {
                std::string_view txObj = root.substr(objOpen, objClose - objOpen);

                core::Transaction tx;
                tx.id = extractJsonString(txObj, "id");
                tx.date = core::Utils::normalizeDate(extractJsonString(txObj, "date"));
                tx.time = extractJsonString(txObj, "time");
                tx.amount = extractJsonNumber(txObj, "amount", 0.0);
                tx.description = extractJsonString(txObj, "description");
                if (tx.description.empty()) tx.description = extractJsonString(txObj, "memo");
                tx.documentNumber = extractJsonString(txObj, "documentNumber");
                if (tx.documentNumber.empty()) tx.documentNumber = extractJsonString(txObj, "checkNumber");
                tx.category = extractJsonString(txObj, "category");
                tx.pixKey = extractJsonString(txObj, "pixKey");
                tx.pixE2eId = extractJsonString(txObj, "pixE2eId");
                
                if (txObj.find("\"runningBalance\"") != std::string_view::npos) {
                    tx.runningBalance = extractJsonNumber(txObj, "runningBalance", 0.0);
                }

                if (tx.amount > 0.0) tx.type = core::TransactionType::CREDIT;
                else if (tx.amount < 0.0) tx.type = core::TransactionType::DEBIT;

                if (!tx.date.empty() && tx.amount != 0.0) {
                    statement->transactions.push_back(std::move(tx));
                }

                pos = objClose;
            } else {
                break;
            }
        }
    }

    statement->computeStatistics();
    return statement;
}

std::shared_ptr<core::Statement> JsonParser::parse(const std::string& filePath) const {
    std::string content = core::Utils::readFileString(filePath);
    auto stmt = parseFromString(content);
    stmt->sourceFilePath = filePath;
    return stmt;
}

} // namespace ofx::parsers
