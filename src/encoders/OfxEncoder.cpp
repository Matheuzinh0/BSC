/**
 * =================================================================================================
 * Projeto: OmniStatement Studio
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "encoders/OfxEncoder.hpp"
#include "core/Utils.hpp"
#include <sstream>
#include <iomanip>

namespace ofx::encoders {

static std::string toOfxTimestamp(std::string_view isoDate) {
    std::string s = core::Utils::trim(isoDate);
    std::string clean;
    for (char c : s) {
        if (std::isdigit(c)) clean.push_back(c);
    }
    if (clean.size() >= 8) {
        return clean.substr(0, 8) + "120000[-03:EST]";
    }
    return "20240101120000[-03:EST]";
}

bool OfxEncoder::encode(const core::Statement& statement, const std::string& outputFilePath) const {
    std::ostringstream ss;

    // Standard OFX 1.02 Header
    ss << "OFXHEADER:100\r\n";
    ss << "DATA:OFXSGML\r\n";
    ss << "VERSION:102\r\n";
    ss << "SECURITY:NONE\r\n";
    ss << "ENCODING:USASCII\r\n";
    ss << "CHARSET:1252\r\n";
    ss << "COMPRESSION:NONE\r\n";
    ss << "OLDFILEUID:NONE\r\n";
    ss << "NEWFILEUID:NONE\r\n\r\n";

    ss << "<OFX>\r\n";
    ss << "<SIGNONMSGSRSV1>\r\n";
    ss << "<SONRS>\r\n";
    ss << "<STATUS>\r\n";
    ss << "<CODE>0\r\n";
    ss << "<SEVERITY>INFO\r\n";
    ss << "</STATUS>\r\n";
    ss << "<DTSERVER>" << toOfxTimestamp(statement.periodEnd.empty() ? "20240101" : statement.periodEnd) << "\r\n";
    ss << "<LANGUAGE>POR\r\n";
    ss << "<FI>\r\n";
    ss << "<ORG>" << (statement.account.bankName.empty() ? "OmniConverter" : statement.account.bankName) << "\r\n";
    ss << "<FID>" << (statement.account.bankId.empty() ? "999" : statement.account.bankId) << "\r\n";
    ss << "</FI>\r\n";
    ss << "</SONRS>\r\n";
    ss << "</SIGNONMSGSRSV1>\r\n";

    ss << "<BANKMSGSRSV1>\r\n";
    ss << "<STMTTRNRS>\r\n";
    ss << "<TRNUID>1001\r\n";
    ss << "<STATUS>\r\n";
    ss << "<CODE>0\r\n";
    ss << "<SEVERITY>INFO\r\n";
    ss << "</STATUS>\r\n";
    ss << "<STMTRS>\r\n";
    ss << "<CURDEF>" << (statement.account.currency.empty() ? "BRL" : statement.account.currency) << "\r\n";

    ss << "<BANKACCTFROM>\r\n";
    ss << "<BANKID>" << (statement.account.bankId.empty() ? "999" : statement.account.bankId) << "\r\n";
    ss << "<BRANCHID>" << (statement.account.branchId.empty() ? "0001" : statement.account.branchId) << "\r\n";
    ss << "<ACCTID>" << (statement.account.accountId.empty() ? "000000" : statement.account.accountId) << "\r\n";
    ss << "<ACCTTYPE>" << (statement.account.accountType.empty() ? "CHECKING" : statement.account.accountType) << "\r\n";
    ss << "</BANKACCTFROM>\r\n";

    ss << "<BANKTRANLIST>\r\n";
    ss << "<DTSTART>" << toOfxTimestamp(statement.periodStart.empty() ? "20240101" : statement.periodStart) << "\r\n";
    ss << "<DTEND>" << toOfxTimestamp(statement.periodEnd.empty() ? "20240101" : statement.periodEnd) << "\r\n";

    for (size_t i = 0; i < statement.transactions.size(); ++i) {
        const auto& tx = statement.transactions[i];
        ss << "<STMTTRN>\r\n";
        ss << "<TRNTYPE>" << (tx.isCredit() ? "CREDIT" : "DEBIT") << "\r\n";
        ss << "<DTPOSTED>" << toOfxTimestamp(tx.date) << "\r\n";
        ss << "<TRNAMT>" << std::fixed << std::setprecision(2) << tx.amount << "\r\n";

        std::string fitId = !tx.id.empty() ? tx.id : (!tx.documentNumber.empty() ? tx.documentNumber : std::to_string(i + 1));
        ss << "<FITID>" << fitId << "\r\n";
        if (!tx.documentNumber.empty()) {
            ss << "<CHECKNUM>" << tx.documentNumber << "\r\n";
        }
        ss << "<MEMO>" << (!tx.description.empty() ? tx.description : "Transacao") << "\r\n";
        ss << "</STMTTRN>\r\n";
    }

    ss << "</BANKTRANLIST>\r\n";

    double ledgerBal = statement.hasFinalBalance ? statement.finalBalance : statement.netFlow;
    ss << "<LEDGERBAL>\r\n";
    ss << "<BALAMT>" << std::fixed << std::setprecision(2) << ledgerBal << "\r\n";
    ss << "<DTASOF>" << toOfxTimestamp(statement.periodEnd.empty() ? "20240101" : statement.periodEnd) << "\r\n";
    ss << "</LEDGERBAL>\r\n";

    ss << "</STMTRS>\r\n";
    ss << "</STMTTRNRS>\r\n";
    ss << "</BANKMSGSRSV1>\r\n";
    ss << "</OFX>\r\n";

    return core::Utils::writeFileString(outputFilePath, ss.str());
}

} // namespace ofx::encoders
