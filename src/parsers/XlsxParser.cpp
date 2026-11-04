/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "parsers/XlsxParser.hpp"
#include "core/Utils.hpp"
#include "miniz/miniz.h"
#include <cstring>
#include <regex>
#include <iostream>

namespace ofx::parsers {

bool XlsxParser::canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const {
    // ZIP magic: PK\x03\x04
    if (headerSize >= 4 && headerData[0] == 0x50 && headerData[1] == 0x4B &&
        headerData[2] == 0x03 && headerData[3] == 0x04) {
        return filePath.size() >= 5 && core::Utils::toLower(filePath.substr(filePath.size() - 5)) == ".xlsx";
    }
    return filePath.size() >= 5 && core::Utils::toLower(filePath.substr(filePath.size() - 5)) == ".xlsx";
}

std::vector<std::string> XlsxParser::parseSharedStrings(const std::string& xml) const {
    std::vector<std::string> shared;
    if (xml.empty()) return shared;

    // Scan for <si>...</si> and extract <t>...</t> inside
    size_t pos = 0;
    while (pos < xml.size()) {
        size_t siStart = xml.find("<si>", pos);
        if (siStart == std::string::npos) {
            siStart = xml.find("<si ", pos);
            if (siStart == std::string::npos) break;
        }

        size_t siEnd = xml.find("</si>", siStart);
        if (siEnd == std::string::npos) break;

        std::string_view siBlock(xml.data() + siStart, siEnd - siStart);

        std::string textValue;
        size_t tPos = 0;
        while (tPos < siBlock.size()) {
            size_t tStart = siBlock.find("<t", tPos);
            if (tStart == std::string_view::npos) break;

            size_t closeTag = siBlock.find('>', tStart);
            if (closeTag == std::string_view::npos) break;

            size_t tEnd = siBlock.find("</t>", closeTag);
            if (tEnd == std::string_view::npos) break;

            textValue += siBlock.substr(closeTag + 1, tEnd - (closeTag + 1));
            tPos = tEnd + 4;
        }

        shared.push_back(textValue);
        pos = siEnd + 5;
    }

    return shared;
}

static int columnLettersToIndex(std::string_view col) {
    int idx = 0;
    for (char c : col) {
        if (c >= 'A' && c <= 'Z') {
            idx = idx * 26 + (c - 'A' + 1);
        } else if (c >= 'a' && c <= 'z') {
            idx = idx * 26 + (c - 'a' + 1);
        }
    }
    return idx > 0 ? idx - 1 : 0;
}

std::vector<std::vector<std::string>> XlsxParser::parseSheetRows(const std::string& xml, const std::vector<std::string>& sharedStrings) const {
    std::vector<std::vector<std::string>> rows;
    if (xml.empty()) return rows;

    size_t pos = 0;
    while (pos < xml.size()) {
        size_t rowStart = xml.find("<row ", pos);
        if (rowStart == std::string::npos) {
            rowStart = xml.find("<row>", pos);
            if (rowStart == std::string::npos) break;
        }

        size_t rowEnd = xml.find("</row>", rowStart);
        if (rowEnd == std::string::npos) break;

        std::string_view rowBlock(xml.data() + rowStart, rowEnd - rowStart);
        std::vector<std::string> currentRow;

        // Parse cells <c r="A1" ...>
        size_t cPos = 0;
        while (cPos < rowBlock.size()) {
            size_t cellStart = rowBlock.find("<c ", cPos);
            if (cellStart == std::string_view::npos) {
                cellStart = rowBlock.find("<c>", cPos);
                if (cellStart == std::string_view::npos) break;
            }

            size_t cellEnd = rowBlock.find("</c>", cellStart);
            size_t selfClose = rowBlock.find("/>", cellStart);
            bool isSelfClosing = (selfClose != std::string_view::npos && (cellEnd == std::string_view::npos || selfClose < cellEnd));

            if (isSelfClosing) {
                cPos = selfClose + 2;
                continue;
            }
            if (cellEnd == std::string_view::npos) break;

            std::string_view cellTag(rowBlock.data() + cellStart, cellEnd - cellStart);

            // Determine column index from r="A1"
            size_t rAttr = cellTag.find("r=\"");
            int colIdx = -1;
            if (rAttr != std::string_view::npos) {
                size_t rValStart = rAttr + 3;
                size_t rValEnd = cellTag.find('"', rValStart);
                if (rValEnd != std::string_view::npos) {
                    std::string_view cellRef = cellTag.substr(rValStart, rValEnd - rValStart);
                    std::string colLetters;
                    for (char ch : cellRef) {
                        if (std::isalpha(ch)) colLetters.push_back(ch);
                        else break;
                    }
                    colIdx = columnLettersToIndex(colLetters);
                }
            }

            bool isShared = (cellTag.find("t=\"s\"") != std::string_view::npos);
            bool isInline = (cellTag.find("t=\"inlineStr\"") != std::string_view::npos);

            std::string cellVal;
            if (isInline) {
                size_t tStart = cellTag.find("<t>");
                if (tStart != std::string_view::npos) {
                    size_t tEnd = cellTag.find("</t>", tStart);
                    if (tEnd != std::string_view::npos) {
                        cellVal = cellTag.substr(tStart + 3, tEnd - (tStart + 3));
                    }
                }
            } else {
                size_t vStart = cellTag.find("<v>");
                if (vStart != std::string_view::npos) {
                    size_t vEnd = cellTag.find("</v>", vStart);
                    if (vEnd != std::string_view::npos) {
                        std::string valStr(cellTag.substr(vStart + 3, vEnd - (vStart + 3)));
                        if (isShared) {
                            try {
                                size_t idx = std::stoul(valStr);
                                if (idx < sharedStrings.size()) {
                                    cellVal = sharedStrings[idx];
                                }
                            } catch (...) {}
                        } else {
                            cellVal = valStr;
                        }
                    }
                }
            }

            if (colIdx >= 0) {
                if (currentRow.size() <= static_cast<size_t>(colIdx)) {
                    currentRow.resize(colIdx + 1);
                }
                currentRow[colIdx] = cellVal;
            } else {
                currentRow.push_back(cellVal);
            }

            cPos = cellEnd + 4;
        }

        if (!currentRow.empty()) {
            rows.push_back(std::move(currentRow));
        }

        pos = rowEnd + 6;
    }

    return rows;
}

std::shared_ptr<core::Statement> XlsxParser::parse(const std::string& filePath) const {
    auto statement = std::make_shared<core::Statement>();
    statement->sourceFilePath = filePath;
    statement->sourceFormat = core::FileFormat::XLSX;

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, filePath.c_str(), 0)) {
        return statement;
    }

    // 1. Read xl/sharedStrings.xml if present
    std::vector<std::string> sharedStrings;
    int ssIdx = mz_zip_reader_locate_file(&zip, "xl/sharedStrings.xml", nullptr, 0);
    if (ssIdx >= 0) {
        size_t size = 0;
        char* buf = static_cast<char*>(mz_zip_reader_extract_to_heap(&zip, ssIdx, &size, 0));
        if (buf) {
            std::string ssXml(buf, size);
            mz_free(buf);
            sharedStrings = parseSharedStrings(ssXml);
        }
    }

    // 2. Read xl/worksheets/sheet1.xml
    int sheetIdx = mz_zip_reader_locate_file(&zip, "xl/worksheets/sheet1.xml", nullptr, 0);
    if (sheetIdx < 0) {
        // Fallback search for any sheet
        for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip); ++i) {
            mz_zip_archive_file_stat stat;
            if (mz_zip_reader_file_stat(&zip, i, &stat)) {
                if (strstr(stat.m_filename, "worksheets/sheet")) {
                    sheetIdx = i;
                    break;
                }
            }
        }
    }

    std::vector<std::vector<std::string>> rows;
    if (sheetIdx >= 0) {
        size_t size = 0;
        char* buf = static_cast<char*>(mz_zip_reader_extract_to_heap(&zip, sheetIdx, &size, 0));
        if (buf) {
            std::string sheetXml(buf, size);
            mz_free(buf);
            rows = parseSheetRows(sheetXml, sharedStrings);
        }
    }

    mz_zip_reader_end(&zip);

    if (rows.empty()) return statement;

    // Detect column indexes matching headers
    int dateCol = -1, descCol = -1, docCol = -1, amountCol = -1;
    int debitCol = -1, creditCol = -1, balanceCol = -1, typeCol = -1;
    size_t startRow = 0;

    const auto& headers = rows[0];
    for (size_t col = 0; col < headers.size(); ++col) {
        std::string h = core::Utils::toUpper(headers[col]);
        if (h.find("DATA") != std::string::npos || h.find("DATE") != std::string::npos || h == "DT") {
            dateCol = static_cast<int>(col);
        } else if (h.find("HISTORICO") != std::string::npos || h.find("HISTÓRICO") != std::string::npos ||
                   h.find("DESCRICAO") != std::string::npos || h.find("DESCRIÇÃO") != std::string::npos ||
                   h.find("MEMO") != std::string::npos) {
            descCol = static_cast<int>(col);
        } else if (h.find("DOCUMENTO") != std::string::npos || h.find("DOC") != std::string::npos ||
                   h.find("FITID") != std::string::npos) {
            docCol = static_cast<int>(col);
        } else if (h.find("DEBITO") != std::string::npos || h.find("DÉBITO") != std::string::npos) {
            debitCol = static_cast<int>(col);
        } else if (h.find("CREDITO") != std::string::npos || h.find("CRÉDITO") != std::string::npos) {
            creditCol = static_cast<int>(col);
        } else if (h.find("VALOR") != std::string::npos || h.find("AMOUNT") != std::string::npos) {
            amountCol = static_cast<int>(col);
        } else if (h.find("SALDO") != std::string::npos || h.find("BALANCE") != std::string::npos) {
            balanceCol = static_cast<int>(col);
        } else if (h.find("TIPO") != std::string::npos || h == "D/C") {
            typeCol = static_cast<int>(col);
        }
    }

    if (dateCol != -1 || descCol != -1 || amountCol != -1) {
        startRow = 1;
    } else {
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
            std::string dRaw = row[dateCol];
            // Check if Excel serialized date as numeric serial days (e.g. 45230)
            if (!dRaw.empty() && std::all_of(dRaw.begin(), dRaw.end(), [](char c) { return std::isdigit(c) || c == '.'; })) {
                try {
                    double serial = std::stod(dRaw);
                    if (serial > 10000 && serial < 80000) {
                        // Excel epoch: 1899-12-30
                        int days = static_cast<int>(serial);
                        // Approximate calendar calculation
                        std::tm epoch = {};
                        epoch.tm_year = 1900 - 1900;
                        epoch.tm_mday = 1;
                        std::time_t t = std::mktime(&epoch) + (days - 2) * 86400;
                        std::tm* dt = std::gmtime(&t);
                        if (dt) {
                            char buf[32];
                            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", dt->tm_year + 1900, dt->tm_mon + 1, dt->tm_mday);
                            tx.date = buf;
                        }
                    } else {
                        tx.date = core::Utils::normalizeDate(dRaw);
                    }
                } catch (...) {
                    tx.date = core::Utils::normalizeDate(dRaw);
                }
            } else {
                tx.date = core::Utils::normalizeDate(dRaw);
            }
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
