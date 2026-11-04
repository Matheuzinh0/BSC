/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "parsers/CsvParser.hpp"
#include "core/Utils.hpp"
#include <map>
#include <iostream>

namespace ofx::parsers {

bool CsvParser::canParse(const std::string& filePath, const uint8_t* /*headerData*/, size_t /*headerSize*/) const {
    if (filePath.size() >= 4 && core::Utils::toLower(filePath.substr(filePath.size() - 4)) == ".csv") {
        return true;
    }
    return false;
}

char CsvParser::detectDelimiter(const std::string& sample) {
    std::map<char, int> counts = {{';', 0}, {',', 0}, {'\t', 0}, {'|', 0}};
    bool inQuotes = false;

    for (size_t i = 0; i < sample.size() && i < 4096; ++i) {
        char c = sample[i];
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (!inQuotes) {
            if (counts.find(c) != counts.end()) {
                counts[c]++;
            }
        }
    }

    char best = ';';
    int maxCount = -1;
    for (auto const& [delim, cnt] : counts) {
        if (cnt > maxCount) {
            maxCount = cnt;
            best = delim;
        }
    }
    return (maxCount > 0) ? best : ';';
}

std::vector<std::vector<std::string>> CsvParser::parseCsvRows(const std::string& content, char delimiter) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> currentRow;
    std::string currentCell;
    bool inQuotes = false;

    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];

        if (c == '"') {
            if (inQuotes && i + 1 < content.size() && content[i + 1] == '"') {
                currentCell.push_back('"');
                i++; // Skip escaped quote
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == delimiter && !inQuotes) {
            currentRow.push_back(core::Utils::trim(currentCell));
            currentCell.clear();
        } else if ((c == '\r' || c == '\n') && !inQuotes) {
            if (c == '\r' && i + 1 < content.size() && content[i + 1] == '\n') {
                i++; // Skip \n in \r\n
            }
            currentRow.push_back(core::Utils::trim(currentCell));
            currentCell.clear();

            // Avoid adding completely empty lines
            bool hasContent = false;
            for (const auto& cell : currentRow) {
                if (!cell.empty()) { hasContent = true; break; }
            }
            if (hasContent) {
                rows.push_back(std::move(currentRow));
            }
            currentRow.clear();
        } else {
            currentCell.push_back(c);
        }
    }

    if (!currentCell.empty() || !currentRow.empty()) {
        currentRow.push_back(core::Utils::trim(currentCell));
        rows.push_back(std::move(currentRow));
    }

    return rows;
}

std::shared_ptr<core::Statement> CsvParser::parse(const std::string& filePath) const {
    auto statement = std::make_shared<core::Statement>();
    statement->sourceFilePath = filePath;
    statement->sourceFormat = core::FileFormat::CSV;

    std::string content = core::Utils::readFileString(filePath);
    if (content.empty()) return statement;

    char delimiter = detectDelimiter(content);
    auto rows = parseCsvRows(content, delimiter);
    if (rows.empty()) return statement;

    int dateCol = -1;
    int descCol = -1;
    int docCol = -1;
    int amountCol = -1;
    int debitCol = -1;
    int creditCol = -1;
    int balanceCol = -1;
    int typeCol = -1;

    size_t startRow = 0;

    // Inspect first row for column headers
    const auto& headers = rows[0];
    for (size_t col = 0; col < headers.size(); ++col) {
        std::string h = core::Utils::toUpper(headers[col]);
        if (h.find("DATA") != std::string::npos || h.find("DATE") != std::string::npos || h == "DT") {
            dateCol = static_cast<int>(col);
        } else if (h.find("HISTORICO") != std::string::npos || h.find("HISTÓRICO") != std::string::npos ||
                   h.find("DESCRICAO") != std::string::npos || h.find("DESCRIÇÃO") != std::string::npos ||
                   h.find("MEMO") != std::string::npos || h.find("DESCRIPTION") != std::string::npos) {
            descCol = static_cast<int>(col);
        } else if (h.find("DOCUMENTO") != std::string::npos || h.find("DOC") != std::string::npos ||
                   h.find("FITID") != std::string::npos || h.find("CHECKNUM") != std::string::npos ||
                   h.find("OPERACAO") != std::string::npos || h.find("OPERAÇÃO") != std::string::npos ||
                   h.find("IDENTIFICADOR") != std::string::npos) {
            docCol = static_cast<int>(col);
        } else if (h.find("DEBITO") != std::string::npos || h.find("DÉBITO") != std::string::npos ||
                   h.find("SAIDA") != std::string::npos || h.find("SAÍDA") != std::string::npos ||
                   h.find("TARIFA") != std::string::npos) {
            debitCol = static_cast<int>(col);
        } else if (h.find("CREDITO") != std::string::npos || h.find("CRÉDITO") != std::string::npos ||
                   h.find("ENTRADA") != std::string::npos) {
            creditCol = static_cast<int>(col);
        } else if (h.find("VALOR") != std::string::npos || h.find("AMOUNT") != std::string::npos ||
                   h.find("LANCAMENTO") != std::string::npos || h.find("LANÇAMENTO") != std::string::npos ||
                   h.find("LÍQUIDO") != std::string::npos || h.find("LIQUIDO") != std::string::npos) {
            amountCol = static_cast<int>(col);
        } else if (h.find("SALDO") != std::string::npos || h.find("BALANCE") != std::string::npos) {
            balanceCol = static_cast<int>(col);
        } else if (h.find("TIPO") != std::string::npos || h == "D/C" || h == "C/D") {
            typeCol = static_cast<int>(col);
        }
    }

    if (dateCol != -1 || descCol != -1 || amountCol != -1) {
        startRow = 1; // Header found
    } else {
        // Fallback heuristic: 0 = Date, 1 = Desc, 2 = Doc, 3 = Amount
        dateCol = 0;
        descCol = 1;
        if (headers.size() >= 4) {
            docCol = 2;
            amountCol = 3;
        } else if (headers.size() >= 3) {
            amountCol = 2;
        }
        startRow = 0;
    }

    for (size_t r = startRow; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (row.empty()) continue;

        core::Transaction tx;
        if (dateCol >= 0 && dateCol < static_cast<int>(row.size())) {
            tx.date = core::Utils::normalizeDate(row[dateCol]);
        }
        if (descCol >= 0 && descCol < static_cast<int>(row.size())) {
            tx.description = row[descCol];
        }
        if (docCol >= 0 && docCol < static_cast<int>(row.size())) {
            tx.documentNumber = row[docCol];
        }

        if (debitCol >= 0 && creditCol >= 0) {
            double deb = 0.0, cred = 0.0;
            if (debitCol < static_cast<int>(row.size())) deb = std::abs(core::Utils::parseAmount(row[debitCol]));
            if (creditCol < static_cast<int>(row.size())) cred = std::abs(core::Utils::parseAmount(row[creditCol]));
            if (cred > 0.0) tx.amount = cred;
            else if (deb > 0.0) tx.amount = -deb;
        } else if (amountCol >= 0 && amountCol < static_cast<int>(row.size())) {
            tx.amount = core::Utils::parseAmount(row[amountCol]);
            if (typeCol >= 0 && typeCol < static_cast<int>(row.size())) {
                std::string t = core::Utils::toUpper(row[typeCol]);
                if ((t == "D" || t == "DEBITO" || t == "DÉBITO") && tx.amount > 0.0) {
                    tx.amount = -tx.amount;
                }
            }
        }

        if (balanceCol >= 0 && balanceCol < static_cast<int>(row.size())) {
            tx.runningBalance = core::Utils::parseAmount(row[balanceCol]);
        }

        if (tx.amount > 0.0) tx.type = core::TransactionType::CREDIT;
        else if (tx.amount < 0.0) tx.type = core::TransactionType::DEBIT;

        if (!tx.date.empty() && tx.amount != 0.0) {
            statement->transactions.push_back(std::move(tx));
        }
    }

    statement->computeStatistics();
    return statement;
}

} // namespace ofx::parsers
