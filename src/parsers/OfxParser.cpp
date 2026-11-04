/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "parsers/OfxParser.hpp"
#include "core/Utils.hpp"
#include <algorithm>
#include <regex>

namespace ofx::parsers {

bool OfxParser::canParse(const std::string& filePath, const uint8_t* headerData, size_t headerSize) const {
    std::string_view header(reinterpret_cast<const char*>(headerData), std::min(headerSize, size_t(512)));
    if (header.find("OFXHEADER") != std::string_view::npos ||
        header.find("<OFX>") != std::string_view::npos ||
        header.find("<ofx>") != std::string_view::npos) {
        return true;
    }
    return filePath.size() >= 4 && core::Utils::toLower(filePath.substr(filePath.size() - 4)) == ".ofx";
}

std::string OfxParser::extractTagValue(std::string_view content, std::string_view tag) const {
    std::string openTag = "<" + std::string(tag) + ">";
    size_t start = content.find(openTag);
    if (start == std::string_view::npos) {
        // Try uppercase/lowercase
        std::string openUpper = "<" + core::Utils::toUpper(tag) + ">";
        start = content.find(openUpper);
        if (start == std::string_view::npos) return "";
    }

    start += openTag.size();
    
    // Check if there is a closing tag </TAG>
    std::string closeTag = "</" + std::string(tag) + ">";
    size_t end = content.find(closeTag, start);
    if (end != std::string_view::npos) {
        return core::Utils::trim(content.substr(start, end - start));
    }

    // SGML style: tag ends at newline or at next '<'
    end = start;
    while (end < content.size() && content[end] != '<' && content[end] != '\r' && content[end] != '\n') {
        end++;
    }

    return core::Utils::trim(content.substr(start, end - start));
}

void OfxParser::parseTransactions(std::string_view ofxContent, core::Statement& statement) const {
    // Locate each <STMTTRN> block
    std::string openBlock = "<STMTTRN>";
    size_t curPos = 0;

    while (curPos < ofxContent.size()) {
        size_t bStart = ofxContent.find(openBlock, curPos);
        if (bStart == std::string_view::npos) {
            bStart = ofxContent.find("<stmttrn>", curPos);
            if (bStart == std::string_view::npos) break;
        }

        bStart += openBlock.size();
        size_t bEnd = ofxContent.find("</STMTTRN>", bStart);
        if (bEnd == std::string_view::npos) {
            bEnd = ofxContent.find("</stmttrn>", bStart);
            if (bEnd == std::string_view::npos) {
                // Find next <STMTTRN> or </BANKTRANLIST>
                size_t nextTrn = ofxContent.find("<STMTTRN>", bStart);
                size_t endList = ofxContent.find("</BANKTRANLIST>", bStart);
                bEnd = std::min(nextTrn, endList);
                if (bEnd == std::string_view::npos) bEnd = ofxContent.size();
            }
        }

        std::string_view block = ofxContent.substr(bStart, bEnd - bStart);

        core::Transaction tx;
        std::string trnType = extractTagValue(block, "TRNTYPE");
        std::string dtPosted = extractTagValue(block, "DTPOSTED");
        std::string trnAmt = extractTagValue(block, "TRNAMT");
        std::string fitId = extractTagValue(block, "FITID");
        std::string name = extractTagValue(block, "NAME");
        std::string memo = extractTagValue(block, "MEMO");
        std::string checkNum = extractTagValue(block, "CHECKNUM");

        tx.date = core::Utils::normalizeDate(dtPosted);
        tx.amount = core::Utils::parseAmount(trnAmt);
        tx.id = fitId;
        tx.documentNumber = !checkNum.empty() ? checkNum : fitId;

        if (!name.empty() && !memo.empty()) {
            tx.description = name + " - " + memo;
        } else if (!name.empty()) {
            tx.description = name;
        } else {
            tx.description = memo;
        }

        std::string typeUpper = core::Utils::toUpper(trnType);
        if (typeUpper == "CREDIT" || typeUpper == "DEP") tx.type = core::TransactionType::CREDIT;
        else if (typeUpper == "DEBIT" || typeUpper == "PAYMENT") tx.type = core::TransactionType::DEBIT;
        else if (typeUpper == "XFER") tx.type = core::TransactionType::TRANSFER;
        else if (typeUpper == "FEE") tx.type = core::TransactionType::FEE;
        else if (typeUpper == "INT") tx.type = core::TransactionType::INTEREST;
        else tx.type = (tx.amount > 0.0) ? core::TransactionType::CREDIT : core::TransactionType::DEBIT;

        if (!tx.date.empty() && tx.amount != 0.0) {
            statement.transactions.push_back(std::move(tx));
        }

        curPos = bEnd;
    }
}

std::shared_ptr<core::Statement> OfxParser::parse(const std::string& filePath) const {
    auto statement = std::make_shared<core::Statement>();
    statement->sourceFilePath = filePath;
    statement->sourceFormat = core::FileFormat::OFX;

    std::string content = core::Utils::readFileString(filePath);
    if (content.empty()) return statement;

    statement->account.bankId = extractTagValue(content, "BANKID");
    statement->account.branchId = extractTagValue(content, "BRANCHID");
    statement->account.accountId = extractTagValue(content, "ACCTID");
    statement->account.accountType = extractTagValue(content, "ACCTTYPE");
    statement->account.currency = extractTagValue(content, "CURDEF");
    if (statement->account.currency.empty()) statement->account.currency = "BRL";

    std::string dtStart = extractTagValue(content, "DTSTART");
    std::string dtEnd = extractTagValue(content, "DTEND");
    statement->periodStart = core::Utils::normalizeDate(dtStart);
    statement->periodEnd = core::Utils::normalizeDate(dtEnd);

    std::string balAmt = extractTagValue(content, "BALAMT");
    if (!balAmt.empty()) {
        statement->finalBalance = core::Utils::parseAmount(balAmt);
        statement->hasFinalBalance = true;
    }

    parseTransactions(content, *statement);
    statement->computeStatistics();

    return statement;
}

} // namespace ofx::parsers
