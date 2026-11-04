/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "parsers/PdfParser.hpp"
#include "core/Utils.hpp"
#include "miniz/miniz.h"
#include <regex>
#include <iostream>
#include <cstring>

namespace ofx::parsers {

bool PdfParser::canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const {
    if (headerSize >= 4 && headerData[0] == '%' && headerData[1] == 'P' && headerData[2] == 'D' && headerData[3] == 'F') {
        return true;
    }
    return filePath.size() >= 4 && 
           core::Utils::toLower(filePath.substr(filePath.size() - 4)) == ".pdf";
}

std::string PdfParser::extractRawTextFromPdf(const std::vector<uint8_t>& pdfBytes) const {
    std::string fullText;
    const size_t totalSize = pdfBytes.size();
    if (totalSize < 10) return "";

    const char* data = reinterpret_cast<const char*>(pdfBytes.data());
    size_t pos = 0;

    while (pos < totalSize) {
        // Search for "stream" keyword
        const char* streamStart = strstr(data + pos, "stream");
        if (!streamStart) break;

        size_t streamWordPos = streamStart - data;
        
        // Find preceding dictionary "<<" before "stream"
        size_t dictEnd = streamWordPos;
        size_t dictStart = (dictEnd >= 500) ? (dictEnd - 500) : 0;
        std::string_view dictView(data + dictStart, dictEnd - dictStart);
        bool isFlate = (dictView.find("/FlateDecode") != std::string_view::npos);

        // Advance past "stream" and newline (\r\n or \n)
        const char* streamDataStart = streamStart + 6;
        if (*streamDataStart == '\r') streamDataStart++;
        if (*streamDataStart == '\n') streamDataStart++;

        // Search for "endstream"
        const char* streamEnd = strstr(streamDataStart, "endstream");
        if (!streamEnd) break;

        size_t streamLen = streamEnd - streamDataStart;
        if (streamLen > 0) {
            std::vector<uint8_t> uncompressed;
            if (isFlate) {
                // Decompress using miniz
                mz_ulong uncompLen = streamLen * 10 + 4096;
                uncompressed.resize(uncompLen);
                int status = mz_uncompress(uncompressed.data(), &uncompLen, 
                                           reinterpret_cast<const unsigned char*>(streamDataStart), streamLen);
                if (status == MZ_OK) {
                    uncompressed.resize(uncompLen);
                } else {
                    // Try with larger buffer if needed
                    uncompLen = streamLen * 25 + 65536;
                    uncompressed.resize(uncompLen);
                    status = mz_uncompress(uncompressed.data(), &uncompLen, 
                                           reinterpret_cast<const unsigned char*>(streamDataStart), streamLen);
                    if (status == MZ_OK) {
                        uncompressed.resize(uncompLen);
                    } else {
                        uncompressed.clear();
                    }
                }
            } else {
                uncompressed.assign(streamDataStart, streamDataStart + streamLen);
            }

            if (!uncompressed.empty()) {
                const char* content = reinterpret_cast<const char*>(uncompressed.data());
                size_t cSize = uncompressed.size();
                size_t cPos = 0;

                while (cPos < cSize) {
                    // Look for BT (Begin Text)
                    const char* bt = strstr(content + cPos, "BT");
                    if (!bt || (bt - content) >= (int)cSize) break;

                    const char* et = strstr(bt, "ET");
                    if (!et) et = content + cSize;

                    std::string_view block(bt, et - bt);

                    // Extract text strings inside parentheses (text) Tj or within TJ arrays
                    size_t bPos = 0;
                    while (bPos < block.size()) {
                        size_t pOpen = block.find('(', bPos);
                        if (pOpen == std::string_view::npos) break;

                        size_t pClose = pOpen + 1;
                        int nested = 1;
                        while (pClose < block.size() && nested > 0) {
                            if (block[pClose] == '\\') {
                                pClose += 2;
                                continue;
                            }
                            if (block[pClose] == '(') nested++;
                            else if (block[pClose] == ')') nested--;
                            pClose++;
                        }

                        if (nested == 0) {
                            std::string token(block.substr(pOpen + 1, (pClose - 1) - (pOpen + 1)));
                            // Unescape basic PDF characters
                            std::string cleaned;
                            for (size_t i = 0; i < token.size(); ++i) {
                                if (token[i] == '\\' && i + 1 < token.size()) {
                                    char next = token[++i];
                                    if (next == 'n') cleaned.push_back('\n');
                                    else if (next == 'r') cleaned.push_back('\r');
                                    else if (next == 't') cleaned.push_back('\t');
                                    else cleaned.push_back(next);
                                } else {
                                    cleaned.push_back(token[i]);
                                }
                            }
                            fullText += cleaned + " ";
                            bPos = pClose;
                        } else {
                            bPos = pOpen + 1;
                        }
                    }

                    fullText += "\n";
                    cPos = (et - content) + 2;
                }
            }
        }

        pos = (streamEnd - data) + 9;
    }

    return fullText;
}

void PdfParser::parseBankMetadata(const std::string& text, core::Statement& statement) const {
    // Detect Bank
    std::string upper = core::Utils::toUpper(text);
    if (upper.find("BANCO DO BRASIL") != std::string::npos || upper.find("AUTOATENDIMENTO BB") != std::string::npos) {
        statement.account.bankId = "001";
        statement.account.bankName = "Banco do Brasil";
    } else if (upper.find("ITAU") != std::string::npos || upper.find("ITAÚ") != std::string::npos) {
        statement.account.bankId = "341";
        statement.account.bankName = "Banco Itaú";
    } else if (upper.find("BRADESCO") != std::string::npos) {
        statement.account.bankId = "237";
        statement.account.bankName = "Banco Bradesco";
    } else if (upper.find("SANTANDER") != std::string::npos) {
        statement.account.bankId = "033";
        statement.account.bankName = "Banco Santander";
    } else if (upper.find("CAIXA") != std::string::npos) {
        statement.account.bankId = "104";
        statement.account.bankName = "Caixa Econômica Federal";
    } else if (upper.find("NUBANK") != std::string::npos || upper.find("NU PAGAMENTOS") != std::string::npos) {
        statement.account.bankId = "260";
        statement.account.bankName = "Nu Pagamentos S.A.";
    } else if (upper.find("BANCO INTER") != std::string::npos || upper.find("INTER") != std::string::npos) {
        statement.account.bankId = "077";
        statement.account.bankName = "Banco Inter";
    } else {
        statement.account.bankId = "999";
        statement.account.bankName = "Extrato Bancário Universal";
    }

    // Extract Agência and Conta
    std::regex agRegex(R"((?:Ag[êe]ncia|Ag\.?|Prefix|Prefixo)[\s\:\-]*([0-9]{3,5}(?:-[0-9X])?))", std::regex::icase);
    std::smatch match;
    if (std::regex_search(text, match, agRegex)) {
        statement.account.branchId = match[1].str();
    }

    std::regex ccRegex(R"((?:Conta|C\/C|Conta Corrente|Cta)[\s\:\-]*([0-9]{4,12}(?:-[0-9X])?))", std::regex::icase);
    if (std::regex_search(text, match, ccRegex)) {
        statement.account.accountId = match[1].str();
    }

    if (statement.account.accountId.empty()) {
        statement.account.accountId = "000000";
    }
    if (statement.account.branchId.empty()) {
        statement.account.branchId = "0001";
    }
    statement.account.accountType = "CHECKING";
}

void PdfParser::parseTransactionsFromText(const std::string& text, core::Statement& statement) const {
    std::vector<std::string> lines = core::Utils::split(text, '\n');

    // Regex 1: DD/MM/YYYY Description Amount [C/D] [Balance]
    std::regex txRegex1(R"((\d{2,4}[\/\-]\d{2}[\/\-]\d{2,4})\s+(.+?)\s+([\+\-]?\s*(?:R\$\s*)?[\d\.\,]+(?:\s*[CD])?)(?:\s+([\d\.\,]+(?:\s*[CD])?))?\s*$)");
    // Regex 2: DD/MM Description Amount
    std::regex txRegex2(R"((\d{2}[\/\-]\d{2}(?:[\/\-]\d{2,4})?)\s+(.+?)\s+([\+\-]?\s*(?:R\$\s*)?[\d\.\,]+(?:\s*[CD])?)\s*$)");

    int currentYear = 2024;

    for (const auto& rawLine : lines) {
        std::string line = core::Utils::trim(rawLine);
        if (line.size() < 8) continue;

        // Skip obvious header or footer lines
        std::string upLine = core::Utils::toUpper(line);
        if (upLine.find("SALDO ANTERIOR") != std::string::npos || upLine.find("SALDO INICIAL") != std::string::npos) {
            std::regex balRegex(R"([\+\-]?\s*(?:R\$\s*)?[\d\.\,]+(?:\s*[CD])?)");
            std::smatch bMatch;
            if (std::regex_search(line, bMatch, balRegex)) {
                statement.initialBalance = core::Utils::parseAmount(bMatch[0].str());
                statement.hasInitialBalance = true;
            }
            continue;
        }
        if (upLine.find("SALDO FINAL") != std::string::npos || upLine.find("SALDO ATUAL") != std::string::npos) {
            std::regex balRegex(R"([\+\-]?\s*(?:R\$\s*)?[\d\.\,]+(?:\s*[CD])?)");
            std::smatch bMatch;
            if (std::regex_search(line, bMatch, balRegex)) {
                statement.finalBalance = core::Utils::parseAmount(bMatch[0].str());
                statement.hasFinalBalance = true;
            }
            continue;
        }

        std::smatch match;
        if (std::regex_search(line, match, txRegex1)) {
            core::Transaction tx;
            std::string dateStr = match[1].str();
            tx.date = core::Utils::normalizeDate(dateStr);
            if (tx.date.size() == 5) { // DD/MM without year
                tx.date = std::to_string(currentYear) + "-" + dateStr.substr(3, 2) + "-" + dateStr.substr(0, 2);
            }

            tx.description = core::Utils::trim(match[2].str());
            tx.amount = core::Utils::parseAmount(match[3].str());

            if (match[4].matched && !match[4].str().empty()) {
                tx.runningBalance = core::Utils::parseAmount(match[4].str());
            }

            if (tx.amount > 0.0) tx.type = core::TransactionType::CREDIT;
            else if (tx.amount < 0.0) tx.type = core::TransactionType::DEBIT;

            if (!tx.date.empty() && tx.amount != 0.0) {
                statement.transactions.push_back(std::move(tx));
            }
        } else if (std::regex_search(line, match, txRegex2)) {
            core::Transaction tx;
            std::string dateStr = match[1].str();
            tx.date = core::Utils::normalizeDate(dateStr);
            if (tx.date.size() == 5) {
                tx.date = std::to_string(currentYear) + "-" + dateStr.substr(3, 2) + "-" + dateStr.substr(0, 2);
            }

            tx.description = core::Utils::trim(match[2].str());
            tx.amount = core::Utils::parseAmount(match[3].str());

            if (tx.amount > 0.0) tx.type = core::TransactionType::CREDIT;
            else if (tx.amount < 0.0) tx.type = core::TransactionType::DEBIT;

            if (!tx.date.empty() && tx.amount != 0.0) {
                statement.transactions.push_back(std::move(tx));
            }
        }
    }
}

std::shared_ptr<core::Statement> PdfParser::parse(const std::string& filePath) const {
    auto statement = std::make_shared<core::Statement>();
    statement->sourceFilePath = filePath;
    statement->sourceFormat = core::FileFormat::PDF;

    std::vector<uint8_t> bytes = core::Utils::readFileBinary(filePath);
    if (bytes.empty()) return statement;

    std::string text = extractRawTextFromPdf(bytes);
    parseBankMetadata(text, *statement);
    parseTransactionsFromText(text, *statement);
    statement->computeStatistics();

    return statement;
}

} // namespace ofx::parsers
