/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "core/Types.hpp"
#include "core/Hasher.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace ofx::core {

void Statement::computeStatistics() {
    totalCredits = 0.0;
    totalDebits = 0.0;
    creditCount = 0;
    debitCount = 0;

    std::string minDate = "9999-99-99";
    std::string maxDate = "0000-00-00";

    Sha256 stmtHasher;
    stmtHasher.update(account.bankId);
    stmtHasher.update(account.accountId);

    for (auto& tx : transactions) {
        // Compute transaction hash if not already set
        if (tx.hashSha256.empty()) {
            std::ostringstream ss;
            ss << tx.date << "|" << std::fixed << std::setprecision(2) << tx.amount 
               << "|" << tx.documentNumber << "|" << tx.description;
            tx.hashSha256 = Sha256::hashString(ss.str());
        }

        stmtHasher.update(tx.hashSha256);

        if (tx.amount > 0.0) {
            totalCredits += tx.amount;
            creditCount++;
            if (tx.type == TransactionType::OTHER) tx.type = TransactionType::CREDIT;
        } else if (tx.amount < 0.0) {
            totalDebits += std::abs(tx.amount);
            debitCount++;
            if (tx.type == TransactionType::OTHER) tx.type = TransactionType::DEBIT;
        }

        if (!tx.date.empty()) {
            if (tx.date < minDate) minDate = tx.date;
            if (tx.date > maxDate) maxDate = tx.date;
        }
    }

    netFlow = totalCredits - totalDebits;

    if (periodStart.empty() && minDate != "9999-99-99") {
        periodStart = minDate;
    }
    if (periodEnd.empty() && maxDate != "0000-00-00") {
        periodEnd = maxDate;
    }

    statementHashSha256 = stmtHasher.finalize();
}

} // namespace ofx::core
