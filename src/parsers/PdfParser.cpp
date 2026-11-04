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
                    // Look for BT (Begin Text) operator outside string literals
                    const char* bt = nullptr;
                    const char* bScan = content + cPos;
                    bool inParen = false;
                    while (bScan + 1 < content + cSize) {
                        if (*bScan == '\\' && inParen) { bScan += 2; continue; }
                        if (*bScan == '(') inParen = true;
                        else if (*bScan == ')') inParen = false;
                        else if (!inParen && bScan[0] == 'B' && bScan[1] == 'T') {
                            bool leftOk = (bScan == content || std::isspace(static_cast<unsigned char>(bScan[-1])));
                            bool rightOk = (bScan + 2 >= content + cSize || std::isspace(static_cast<unsigned char>(bScan[2])));
                            if (leftOk && rightOk) {
                                bt = bScan;
                                break;
                            }
                        }
                        bScan++;
                    }
                    if (!bt) break;

                    // Look for matching ET (End Text) operator outside string literals
                    const char* et = nullptr;
                    const char* eScan = bt + 2;
                    inParen = false;
                    while (eScan + 1 < content + cSize) {
                        if (*eScan == '\\' && inParen) { eScan += 2; continue; }
                        if (*eScan == '(') inParen = true;
                        else if (*eScan == ')') inParen = false;
                        else if (!inParen && eScan[0] == 'E' && eScan[1] == 'T') {
                            bool leftOk = (eScan == content || std::isspace(static_cast<unsigned char>(eScan[-1])));
                            bool rightOk = (eScan + 2 >= content + cSize || std::isspace(static_cast<unsigned char>(eScan[2])));
                            if (leftOk && rightOk) {
                                et = eScan;
                                break;
                            }
                        }
                        eScan++;
                    }
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

                            // If followed by line break operators (' or T* or explicit newline)
                            size_t nextP = block.find('(', bPos);
                            std::string_view between = (nextP == std::string_view::npos) ? block.substr(bPos) : block.substr(bPos, nextP - bPos);
                            if (between.find('\'') != std::string_view::npos || between.find("T*") != std::string_view::npos) {
                                fullText += "\n";
                            }
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
    std::string upperText = core::Utils::toUpper(text);
    std::string headerArea = upperText.substr(0, std::min<size_t>(upperText.size(), 2500));

    struct BankMap {
        const char* key;
        const char* code;
        const char* name;
    };

    static const BankMap kBanks[] = {
        {"BANCO DO BRASIL", "001", "Banco do Brasil S.A."},
        {"AUTOATENDIMENTO BB", "001", "Banco do Brasil S.A."},
        {"SANTANDER", "033", "Banco Santander Brasil"},
        {"CAIXA ECONOMICA", "104", "Caixa Economica Federal"},
        {"CAIXA ECONÔMICA", "104", "Caixa Economica Federal"},
        {"BRADESCO", "237", "Banco Bradesco S.A."},
        {"ITAU", "341", "Itau Unibanco S.A."},
        {"ITAÚ", "341", "Itau Unibanco S.A."},
        {"NUBANK", "260", "Nu Pagamentos - Nubank"},
        {"NU PAGAMENTOS", "260", "Nu Pagamentos - Nubank"},
        {"BANCO INTER", "077", "Banco Inter S.A."},
        {"INTER", "077", "Banco Inter S.A."},
        {"C6 BANK", "336", "Banco C6 S.A."},
        {"SICOOB", "756", "Bancoob - Sicoob"},
        {"SICREDI", "748", "Banco Sicredi"},
        {"UNICRED", "136", "Unicred do Brasil"},
        {"BTG PACTUAL", "208", "Banco BTG Pactual"},
        {"SAFRA", "422", "Banco Safra"},
        {"BANRISUL", "041", "Banco do Estado do Rio Grande do Sul"},
        {"BANCO DO NORDESTE", "004", "Banco do Nordeste do Brasil"},
        {"BNB", "004", "Banco do Nordeste do Brasil"},
        {"BANCO DA AMAZONIA", "003", "Banco da Amazonia S.A."},
        {"PAGBANK", "290", "PagBank - PagSeguro"},
        {"PAGSEGURO", "290", "PagBank - PagSeguro"},
        {"MERCADO PAGO", "323", "Mercado Pago IP Ltda"},
        {"PICPAY", "380", "PicPay Servicos S.A."},
        {"STONE", "197", "Stone Pagamentos"},
        {"XP INVESTIMENTOS", "102", "XP Investimentos"},
        {"BANCO ORIGINAL", "212", "Banco Original"},
        {"DAYCOVAL", "707", "Banco Daycoval"},
        {"BANCO BMG", "318", "Banco BMG"},
        {"BMG", "318", "Banco BMG"},
        {"BANCO PAN", "623", "Banco Pan"}
    };

    auto matchesBankKey = [](const std::string& haystack, const std::string& key) -> bool {
        size_t pos = 0;
        while ((pos = haystack.find(key, pos)) != std::string::npos) {
            bool leftOk = (pos == 0 || !std::isalnum(static_cast<unsigned char>(haystack[pos - 1])));
            size_t endPos = pos + key.size();
            bool rightOk = (endPos == haystack.size() || !std::isalnum(static_cast<unsigned char>(haystack[endPos])));
            if (leftOk && rightOk) {
                return true;
            }
            pos = endPos;
        }
        return false;
    };

    bool bankFound = false;
    // Pass 1: Header area check
    for (const auto& b : kBanks) {
        if (matchesBankKey(headerArea, b.key)) {
            statement.account.bankId = b.code;
            statement.account.bankName = b.name;
            bankFound = true;
            break;
        }
    }

    // Pass 2: Full text fallback
    if (!bankFound) {
        for (const auto& b : kBanks) {
            if (matchesBankKey(upperText, b.key)) {
                statement.account.bankId = b.code;
                statement.account.bankName = b.name;
                bankFound = true;
                break;
            }
        }
    }

    if (!bankFound) {
        std::regex dynBankRegex(R"((?:Banco|Institui[cç][aã]o|Cooperativa)[\s\:\-]+([A-Za-z0-9\s]{3,25}))", std::regex::icase);
        std::smatch match;
        if (std::regex_search(text, match, dynBankRegex)) {
            statement.account.bankName = core::Utils::trim(match[1].str());
            statement.account.bankId = "999";
        } else {
            statement.account.bankId = "999";
            statement.account.bankName = "Extrato Bancario Universal";
        }
    }

    // Extract Agência and Conta
    std::regex agRegex(R"((?:Ag[êe]ncia|Ag\.?|Prefix|Prefixo|Ponto de Atendimento|PA)[\s\:\-]*([0-9]{3,5}(?:-[0-9X])?))", std::regex::icase);
    std::smatch agMatch;
    if (std::regex_search(text, agMatch, agRegex)) {
        statement.account.branchId = agMatch[1].str();
    }

    std::regex ccRegex(R"((?:Conta|C\/C|Conta Corrente|Cta|Cta Corrente|No\. Conta)[\s\:\-]*([0-9]{4,14}(?:-[0-9X])?))", std::regex::icase);
    std::smatch ccMatch;
    if (std::regex_search(text, ccMatch, ccRegex)) {
        statement.account.accountId = ccMatch[1].str();
    }

    if (statement.account.accountId.empty()) statement.account.accountId = "000000";
    if (statement.account.branchId.empty()) statement.account.branchId = "0001";
    statement.account.accountType = "CHECKING";
}

void PdfParser::parseTransactionsFromText(const std::string& text, core::Statement& statement) const {
    std::vector<std::string> lines = core::Utils::split(text, '\n');

    std::regex dateRegex(R"(\b(\d{1,2}[\/\-\.]\d{1,2}(?:[\/\-\.]\d{2,4})?|\d{4}[\/\-]\d{1,2}[\/\-]\d{1,2})\b)");
    std::regex amtRegex(R"((?:[\+\-]\s*)?(?:R\$\s*)?\(?\d{1,3}(?:\.\d{3})*,\d{2}\)?(?:\s*[CD\-])?(?!\/|\d)|(?:[\+\-]\s*)?(?:R\$\s*)?\(?\d{1,3}(?:,\d{3})*\.\d{2}\)?(?:\s*[CD\-])?(?!\/|\d))");

    int currentYear = 2024;

    for (const auto& rawLine : lines) {
        std::string line = core::Utils::trim(rawLine);
        if (line.size() < 4) continue;

        std::string upLine = core::Utils::toUpper(line);

        // Filter initial / final balances from table
        if (upLine.find("SALDO ANTERIOR") != std::string::npos || 
            upLine.find("SALDO INICIAL") != std::string::npos ||
            upLine.find("SALDO DEVEDOR ANTERIOR") != std::string::npos) {
            auto it = std::sregex_iterator(line.begin(), line.end(), amtRegex);
            if (it != std::sregex_iterator()) {
                statement.initialBalance = core::Utils::parseAmount(it->str());
                statement.hasInitialBalance = true;
            }
            continue;
        }

        if (upLine.find("SALDO FINAL") != std::string::npos || 
            upLine.find("SALDO ATUAL") != std::string::npos ||
            upLine.find("SALDO DISPONIVEL") != std::string::npos ||
            upLine.find("SALDO DISPONÍVEL") != std::string::npos) {
            auto it = std::sregex_iterator(line.begin(), line.end(), amtRegex);
            if (it != std::sregex_iterator()) {
                statement.finalBalance = core::Utils::parseAmount(it->str());
                statement.hasFinalBalance = true;
            }
            continue;
        }

        // Ignore common header lines
        if (upLine.find("DATA") != std::string::npos && 
            (upLine.find("HISTORICO") != std::string::npos || upLine.find("HISTÓRICO") != std::string::npos || upLine.find("VALOR") != std::string::npos)) {
            continue;
        }
        if (upLine.find("PAGINA ") != std::string::npos || upLine.find("PÁGINA ") != std::string::npos) {
            continue;
        }
        if (upLine.find("OUVIDORIA") != std::string::npos || upLine.find("SAC 0800") != std::string::npos) {
            continue;
        }

        // Check for Date match
        std::smatch dMatch;
        bool hasDate = std::regex_search(line, dMatch, dateRegex);

        // Check for Amount matches
        auto amtBegin = std::sregex_iterator(line.begin(), line.end(), amtRegex);
        auto amtEnd = std::sregex_iterator();
        std::vector<std::smatch> amtMatches;
        for (auto it = amtBegin; it != amtEnd; ++it) {
            amtMatches.push_back(*it);
        }

        if (hasDate && !amtMatches.empty()) {
            core::Transaction tx;
            std::string dateStr = dMatch[0].str();
            tx.date = core::Utils::normalizeDate(dateStr);
            if (tx.date.size() == 5) {
                // DD/MM without year -> synthesize current year
                tx.date = std::to_string(currentYear) + "-" + dateStr.substr(3, 2) + "-" + dateStr.substr(0, 2);
            }

            // Determine amount and optional running balance
            if (amtMatches.size() == 1) {
                tx.amount = core::Utils::parseAmount(amtMatches[0].str());
            } else if (amtMatches.size() == 2) {
                tx.amount = core::Utils::parseAmount(amtMatches[0].str());
                tx.runningBalance = core::Utils::parseAmount(amtMatches[1].str());
            } else if (amtMatches.size() >= 3) {
                // Columns [Débito, Crédito, Saldo]
                double val1 = std::abs(core::Utils::parseAmount(amtMatches[0].str()));
                double val2 = std::abs(core::Utils::parseAmount(amtMatches[1].str()));
                if (val2 > 0.0) {
                    tx.amount = val2;
                } else if (val1 > 0.0) {
                    tx.amount = -val1;
                }
                tx.runningBalance = core::Utils::parseAmount(amtMatches[2].str());
            }

            if (tx.amount > 0.0) tx.type = core::TransactionType::CREDIT;
            else if (tx.amount < 0.0) tx.type = core::TransactionType::DEBIT;

            // Extract Description by masking out date and amount regions
            std::string textOnly = line;
            // Mask date
            textOnly.replace(dMatch.position(), dMatch.length(), std::string(dMatch.length(), ' '));
            // Mask amounts
            for (const auto& m : amtMatches) {
                textOnly.replace(m.position(), m.length(), std::string(m.length(), ' '));
            }

            // Collapse whitespace and trim
            std::string cleanDesc;
            bool inSpace = false;
            for (char c : textOnly) {
                if (std::isspace(static_cast<unsigned char>(c))) {
                    if (!inSpace) { cleanDesc.push_back(' '); inSpace = true; }
                } else {
                    cleanDesc.push_back(c);
                    inSpace = false;
                }
            }
            cleanDesc = core::Utils::trim(cleanDesc);

            // Check if leading token is a document number (e.g. DOC1234, 001234)
            auto tokens = core::Utils::split(cleanDesc, ' ');
            if (tokens.size() > 1) {
                std::string firstTok = tokens[0];
                bool isDocLike = (firstTok.size() >= 3 && firstTok.size() <= 12 &&
                                  (std::all_of(firstTok.begin(), firstTok.end(), [](char c){ return std::isdigit(c); }) ||
                                   firstTok.rfind("DOC", 0) == 0));
                if (isDocLike) {
                    tx.documentNumber = firstTok;
                    cleanDesc = cleanDesc.substr(firstTok.size());
                    cleanDesc = core::Utils::trim(cleanDesc);
                }
            }

            tx.description = cleanDesc;

            if (!tx.date.empty() && tx.amount != 0.0) {
                statement.transactions.push_back(std::move(tx));
            }
        } else if (!hasDate && amtMatches.empty() && !statement.transactions.empty()) {
            // Multi-line continuation: line has text but no dates and no amounts
            if (line.size() > 2 && line.size() < 120 &&
                upLine.find("TOTAL") == std::string::npos &&
                upLine.find("EXTRATO") == std::string::npos &&
                upLine.find("BANCO") == std::string::npos &&
                upLine.find("AGENCIA") == std::string::npos &&
                upLine.find("CONTA") == std::string::npos) {
                statement.transactions.back().description += " " + line;
            }
        }
    }
}

std::shared_ptr<core::Statement> PdfParser::parseFromText(const std::string& text) const {
    auto statement = std::make_shared<core::Statement>();
    statement->sourceFormat = core::FileFormat::PDF;

    parseBankMetadata(text, *statement);
    parseTransactionsFromText(text, *statement);
    statement->computeStatistics();

    return statement;
}

std::shared_ptr<core::Statement> PdfParser::parse(const std::string& filePath) const {
    std::vector<uint8_t> bytes = core::Utils::readFileBinary(filePath);
    if (bytes.empty()) {
        auto statement = std::make_shared<core::Statement>();
        statement->sourceFilePath = filePath;
        statement->sourceFormat = core::FileFormat::PDF;
        return statement;
    }

    std::string text = extractRawTextFromPdf(bytes);
    auto statement = parseFromText(text);
    statement->sourceFilePath = filePath;
    return statement;
}

} // namespace ofx::parsers
