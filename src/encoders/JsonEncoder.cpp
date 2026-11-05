/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "encoders/JsonEncoder.hpp"
#include "core/Utils.hpp"
#include <sstream>
#include <iomanip>

namespace ofx::encoders {

static std::string escapeJson(std::string_view s) {
    std::string res;
    res.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  res += "\\\""; break;
            case '\\': res += "\\\\"; break;
            case '\b': res += "\\b";  break;
            case '\f': res += "\\f";  break;
            case '\n': res += "\\n";  break;
            case '\r': res += "\\r";  break;
            case '\t': res += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 32) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    res += buf;
                } else {
                    res += c;
                }
                break;
        }
    }
    return res;
}

std::string JsonEncoder::encodeToString(const core::Statement& statement, bool pretty) const {
    std::ostringstream ss;
    const std::string ind = pretty ? "  " : "";
    const std::string ind2 = pretty ? "    " : "";
    const std::string ind3 = pretty ? "      " : "";
    const std::string nl = pretty ? "\n" : "";

    ss << "{" << nl;
    ss << ind << "\"statement\": {" << nl;

    // Account
    ss << ind2 << "\"account\": {" << nl;
    ss << ind3 << "\"bankId\": \"" << escapeJson(statement.account.bankId) << "\"," << nl;
    ss << ind3 << "\"bankName\": \"" << escapeJson(statement.account.bankName) << "\"," << nl;
    ss << ind3 << "\"branchId\": \"" << escapeJson(statement.account.branchId) << "\"," << nl;
    ss << ind3 << "\"accountId\": \"" << escapeJson(statement.account.accountId) << "\"," << nl;
    ss << ind3 << "\"accountType\": \"" << escapeJson(statement.account.accountType) << "\"," << nl;
    ss << ind3 << "\"currency\": \"" << escapeJson(statement.account.currency) << "\"" << nl;
    ss << ind2 << "}," << nl;

    // Period
    ss << ind2 << "\"period\": {" << nl;
    ss << ind3 << "\"start\": \"" << escapeJson(statement.periodStart) << "\"," << nl;
    ss << ind3 << "\"end\": \"" << escapeJson(statement.periodEnd) << "\"" << nl;
    ss << ind2 << "}," << nl;

    // Balances & Summary
    ss << std::fixed << std::setprecision(2);
    ss << ind2 << "\"summary\": {" << nl;
    ss << ind3 << "\"initialBalance\": " << statement.initialBalance << "," << nl;
    ss << ind3 << "\"finalBalance\": " << statement.finalBalance << "," << nl;
    ss << ind3 << "\"totalCredits\": " << statement.totalCredits << "," << nl;
    ss << ind3 << "\"totalDebits\": " << statement.totalDebits << "," << nl;
    ss << ind3 << "\"netFlow\": " << statement.netFlow << "," << nl;
    ss << ind3 << "\"creditCount\": " << statement.creditCount << "," << nl;
    ss << ind3 << "\"debitCount\": " << statement.debitCount << "," << nl;
    ss << ind3 << "\"transactionCount\": " << statement.transactions.size() << nl;
    ss << ind2 << "}," << nl;

    // Audit and Integrity
    ss << ind2 << "\"audit\": {" << nl;
    ss << ind3 << "\"status\": \"" << auditStatusToString(statement.auditStatus) << "\"," << nl;
    ss << ind3 << "\"discrepancy\": " << statement.auditDiscrepancy << "," << nl;
    ss << ind3 << "\"statementHashSha256\": \"" << escapeJson(statement.statementHashSha256) << "\"," << nl;
    ss << ind3 << "\"warnings\": [";
    for (size_t i = 0; i < statement.auditWarnings.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << escapeJson(statement.auditWarnings[i]) << "\"";
    }
    ss << "]" << nl;
    ss << ind2 << "}," << nl;

    // Transactions array
    ss << ind2 << "\"transactions\": [" << nl;
    for (size_t i = 0; i < statement.transactions.size(); ++i) {
        const auto& tx = statement.transactions[i];
        ss << ind3 << "{" << nl;
        ss << ind3 << ind << "\"id\": \"" << escapeJson(tx.id.empty() ? std::to_string(i + 1) : tx.id) << "\"," << nl;
        ss << ind3 << ind << "\"date\": \"" << escapeJson(tx.date) << "\"," << nl;
        if (!tx.time.empty()) {
            ss << ind3 << ind << "\"time\": \"" << escapeJson(tx.time) << "\"," << nl;
        }
        ss << ind3 << ind << "\"amount\": " << tx.amount << "," << nl;
        ss << ind3 << ind << "\"type\": \"" << (tx.isCredit() ? "CREDIT" : "DEBIT") << "\"," << nl;
        ss << ind3 << ind << "\"description\": \"" << escapeJson(tx.description) << "\"," << nl;
        ss << ind3 << ind << "\"documentNumber\": \"" << escapeJson(tx.documentNumber) << "\"," << nl;
        ss << ind3 << ind << "\"category\": \"" << escapeJson(tx.category) << "\"," << nl;
        if (!tx.pixKey.empty()) {
            ss << ind3 << ind << "\"pixKey\": \"" << escapeJson(tx.pixKey) << "\"," << nl;
        }
        if (!tx.pixE2eId.empty()) {
            ss << ind3 << ind << "\"pixE2eId\": \"" << escapeJson(tx.pixE2eId) << "\"," << nl;
        }
        if (tx.runningBalance.has_value()) {
            ss << ind3 << ind << "\"runningBalance\": " << tx.runningBalance.value() << "," << nl;
        }
        ss << ind3 << ind << "\"hashSha256\": \"" << escapeJson(tx.hashSha256) << "\"" << nl;
        ss << ind3 << "}" << (i + 1 < statement.transactions.size() ? "," : "") << nl;
    }
    ss << ind2 << "]" << nl;

    ss << ind << "}" << nl;
    ss << "}" << nl;

    return ss.str();
}

bool JsonEncoder::encode(const core::Statement& statement, const std::string& outputFilePath) const {
    std::string json = encodeToString(statement, true);
    return core::Utils::writeFileString(outputFilePath, json);
}

} // namespace ofx::encoders
