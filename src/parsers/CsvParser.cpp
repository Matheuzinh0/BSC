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
    bool headerFound = false;

    // Scan up to 25 rows for table headers, extracting any metadata along the way
    size_t maxHeaderScan = std::min(rows.size(), size_t(25));
    for (size_t r = 0; r < maxHeaderScan; ++r) {
        const auto& candidateRow = rows[r];

        // Check if row has metadata (e.g. Banco, Agência, Conta, Saldo)
        for (const auto& cell : candidateRow) {
            std::string upCell = core::Utils::toUpper(cell);
            if (upCell.find("BANCO:") != std::string::npos || upCell.find("BANCO DO BRASIL") != std::string::npos ||
                upCell.find("ITAU") != std::string::npos || upCell.find("BRADESCO") != std::string::npos ||
                upCell.find("SANTANDER") != std::string::npos || upCell.find("NUBANK") != std::string::npos) {
                if (statement->account.bankName.empty()) statement->account.bankName = cell;
            }
            if (upCell.find("AGENCIA:") != std::string::npos || upCell.find("AGÊNCIA:") != std::string::npos) {
                statement->account.branchId = cell;
            }
            if (upCell.find("CONTA:") != std::string::npos || upCell.find("CONTA CORRENTE:") != std::string::npos) {
                statement->account.accountId = cell;
            }
            if (upCell.find("SALDO ANTERIOR:") != std::string::npos || upCell.find("SALDO INICIAL:") != std::string::npos) {
                statement->initialBalance = core::Utils::parseAmount(cell);
                statement->hasInitialBalance = true;
            }
        }

        // Test if candidateRow represents column headers
        int dCol = -1, sCol = -1, aCol = -1, debCol = -1, credCol = -1, balCol = -1, tCol = -1, dcCol = -1;

        for (size_t col = 0; col < candidateRow.size(); ++col) {
            std::string h = core::Utils::toUpper(candidateRow[col]);
            if (h.find("DATA") != std::string::npos || h.find("DATE") != std::string::npos || h == "DT") {
                dCol = static_cast<int>(col);
            } else if (h.find("HISTORICO") != std::string::npos || h.find("HISTÓRICO") != std::string::npos ||
                       h.find("DESCRICAO") != std::string::npos || h.find("DESCRIÇÃO") != std::string::npos ||
                       h.find("MEMO") != std::string::npos || h.find("DESCRIPTION") != std::string::npos ||
                       h.find("DETALHE") != std::string::npos) {
                sCol = static_cast<int>(col);
            } else if (h.find("DOCUMENTO") != std::string::npos || h.find("DOC") != std::string::npos ||
                       h.find("FITID") != std::string::npos || h.find("CHECKNUM") != std::string::npos ||
                       h.find("OPERACAO") != std::string::npos || h.find("OPERAÇÃO") != std::string::npos ||
                       h.find("IDENTIFICADOR") != std::string::npos) {
                dcCol = static_cast<int>(col);
            } else if (h.find("DEBITO") != std::string::npos || h.find("DÉBITO") != std::string::npos ||
                       h.find("SAIDA") != std::string::npos || h.find("SAÍDA") != std::string::npos ||
                       h.find("TARIFA") != std::string::npos) {
                debCol = static_cast<int>(col);
            } else if (h.find("CREDITO") != std::string::npos || h.find("CRÉDITO") != std::string::npos ||
                       h.find("ENTRADA") != std::string::npos) {
                credCol = static_cast<int>(col);
            } else if (h.find("VALOR") != std::string::npos || h.find("AMOUNT") != std::string::npos ||
                       h.find("LANCAMENTO") != std::string::npos || h.find("LANÇAMENTO") != std::string::npos ||
                       h.find("LÍQUIDO") != std::string::npos || h.find("LIQUIDO") != std::string::npos) {
                aCol = static_cast<int>(col);
            } else if (h.find("SALDO") != std::string::npos || h.find("BALANCE") != std::string::npos) {
                balCol = static_cast<int>(col);
            } else if (h.find("TIPO") != std::string::npos || h == "D/C" || h == "C/D") {
                tCol = static_cast<int>(col);
            }
        }

        // A valid header row must at least define a date column and an amount/debit/credit/description column
        if (dCol != -1 && (aCol != -1 || debCol != -1 || credCol != -1 || sCol != -1)) {
            dateCol = dCol;
            descCol = sCol;
            amountCol = aCol;
            debitCol = debCol;
            creditCol = credCol;
            balanceCol = balCol;
            docCol = dcCol;
            typeCol = tCol;
            startRow = r + 1;
            headerFound = true;
            break;
        }
    }

    if (!headerFound) {
        // Fallback heuristic: 0 = Date, 1 = Desc, 2 = Doc, 3 = Amount
        const auto& headers = rows[0];
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
