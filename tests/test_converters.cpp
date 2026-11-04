/**
 * =================================================================================================
 * Projeto: OmniStatement Studio
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "core/Types.hpp"
#include "core/Hasher.hpp"
#include "core/Utils.hpp"
#include "converter/ConverterEngine.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <cmath>

#define ASSERT_TRUE(cond, msg) \
    if (!(cond)) { \
        std::cerr << "[-] ASSERTION FAILED: " << msg << " (" << #cond << ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::exit(1); \
    } else { \
        std::cout << "[+] PASS: " << msg << "\n"; \
    }

void testSha256() {
    std::cout << "\n--- TEST: SHA-256 Hasher ---\n";
    // Known NIST SHA-256 vector for "abc"
    // ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    std::string hash = ofx::core::Sha256::hashString("abc");
    ASSERT_TRUE(hash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 vector for 'abc'");
}

void testAmountAndDateParsing() {
    std::cout << "\n--- TEST: Amount and Date Parsing ---\n";

    ASSERT_TRUE(std::abs(ofx::core::Utils::parseAmount("1.250,50") - 1250.50) < 0.001, "BR amount 1.250,50");
    ASSERT_TRUE(std::abs(ofx::core::Utils::parseAmount("-1250.50") - (-1250.50)) < 0.001, "Negative amount -1250.50");
    ASSERT_TRUE(std::abs(ofx::core::Utils::parseAmount("(350,00)") - (-350.00)) < 0.001, "Parentheses amount (350,00)");
    ASSERT_TRUE(std::abs(ofx::core::Utils::parseAmount("45,90 D") - (-45.90)) < 0.001, "Trailing D debit 45,90 D");
    ASSERT_TRUE(std::abs(ofx::core::Utils::parseAmount("1.000,00 C") - 1000.00) < 0.001, "Trailing C credit 1.000,00 C");

    ASSERT_TRUE(ofx::core::Utils::normalizeDate("15/10/2023") == "2023-10-15", "Date DD/MM/YYYY");
    ASSERT_TRUE(ofx::core::Utils::normalizeDate("2023-10-15") == "2023-10-15", "Date ISO YYYY-MM-DD");
    ASSERT_TRUE(ofx::core::Utils::normalizeDate("20231015120000[-03:EST]") == "2023-10-15", "OFX Timestamp");
}

void testUniversalFormatRoundtrip() {
    std::cout << "\n--- TEST: Universal Any-to-Any Roundtrip ---\n";
    std::filesystem::create_directories("/tmp/ofx_test_sandbox");

    ofx::converter::ConverterEngine engine;

    // Create a canonical sample statement
    ofx::core::Statement original;
    original.account.bankId = "001";
    original.account.bankName = "Banco do Brasil";
    original.account.branchId = "1234";
    original.account.accountId = "56789-0";
    original.account.currency = "BRL";
    original.periodStart = "2023-10-01";
    original.periodEnd = "2023-10-31";

    ofx::core::Transaction t1;
    t1.date = "2023-10-05";
    t1.amount = 5000.00;
    t1.type = ofx::core::TransactionType::CREDIT;
    t1.description = "SALARIO EMPRESA XYZ";
    t1.documentNumber = "DOC001";
    original.transactions.push_back(t1);

    ofx::core::Transaction t2;
    t2.date = "2023-10-10";
    t2.amount = -150.75;
    t2.type = ofx::core::TransactionType::DEBIT;
    t2.description = "PAGAMENTO ENEL ENERGIA";
    t2.documentNumber = "DOC002";
    original.transactions.push_back(t2);

    ofx::core::Transaction t3;
    t3.date = "2023-10-15";
    t3.amount = -89.90;
    t3.type = ofx::core::TransactionType::DEBIT;
    t3.description = "COMPRA SUPERMERCADO ABC";
    t3.documentNumber = "DOC003";
    original.transactions.push_back(t3);

    original.computeStatistics();
    ASSERT_TRUE(original.transactions.size() == 3, "Original statement has 3 transactions");
    ASSERT_TRUE(std::abs(original.totalCredits - 5000.00) < 0.001, "Total credits match");
    ASSERT_TRUE(std::abs(original.totalDebits - 240.65) < 0.001, "Total debits match");

    // 1. Convert to OFX
    std::string ofxFile = "/tmp/ofx_test_sandbox/statement.ofx";
    bool okOfx = engine.convertStatement(original, ofx::core::FileFormat::OFX, ofxFile);
    ASSERT_TRUE(okOfx, "Encode Statement to OFX");
    ASSERT_TRUE(std::filesystem::exists(ofxFile), "OFX file created");

    // Parse back OFX
    auto parsedOfx = engine.parseFile(ofxFile);
    ASSERT_TRUE(parsedOfx != nullptr, "Parse OFX file");
    ASSERT_TRUE(parsedOfx->transactions.size() == 3, "OFX contains 3 transactions");
    ASSERT_TRUE(std::abs(parsedOfx->totalCredits - 5000.00) < 0.001, "OFX credits preserved");

    // 2. Convert from OFX -> CSV
    std::string csvFile = "/tmp/ofx_test_sandbox/statement.csv";
    bool okCsv = engine.convertStatement(*parsedOfx, ofx::core::FileFormat::CSV, csvFile);
    ASSERT_TRUE(okCsv, "Encode OFX to CSV");

    // Parse back CSV
    auto parsedCsv = engine.parseFile(csvFile);
    ASSERT_TRUE(parsedCsv != nullptr, "Parse CSV file");
    ASSERT_TRUE(parsedCsv->transactions.size() == 3, "CSV contains 3 transactions");
    ASSERT_TRUE(std::abs(parsedCsv->totalDebits - 240.65) < 0.001, "CSV debits preserved");

    // 3. Convert from CSV -> XLSX
    std::string xlsxFile = "/tmp/ofx_test_sandbox/statement.xlsx";
    bool okXlsx = engine.convertStatement(*parsedCsv, ofx::core::FileFormat::XLSX, xlsxFile);
    ASSERT_TRUE(okXlsx, "Encode CSV to XLSX");
    ASSERT_TRUE(std::filesystem::file_size(xlsxFile) > 500, "Valid XLSX zip generated");

    // Parse back XLSX
    auto parsedXlsx = engine.parseFile(xlsxFile);
    ASSERT_TRUE(parsedXlsx != nullptr, "Parse XLSX file");
    ASSERT_TRUE(parsedXlsx->transactions.size() == 3, "XLSX contains 3 transactions");
    ASSERT_TRUE(std::abs(parsedXlsx->totalCredits - 5000.00) < 0.001, "XLSX credits preserved");

    // 4. Convert from XLSX -> QIF
    std::string qifFile = "/tmp/ofx_test_sandbox/statement.qif";
    bool okQif = engine.convertStatement(*parsedXlsx, ofx::core::FileFormat::QIF, qifFile);
    ASSERT_TRUE(okQif, "Encode XLSX to QIF");

    // Parse back QIF
    auto parsedQif = engine.parseFile(qifFile);
    ASSERT_TRUE(parsedQif != nullptr, "Parse QIF file");
    ASSERT_TRUE(parsedQif->transactions.size() == 3, "QIF contains 3 transactions");

    // 5. Convert to PDF
    std::string pdfFile = "/tmp/ofx_test_sandbox/statement.pdf";
    bool okPdf = engine.convertStatement(*parsedQif, ofx::core::FileFormat::PDF, pdfFile);
    ASSERT_TRUE(okPdf, "Encode QIF to PDF report");
    ASSERT_TRUE(std::filesystem::file_size(pdfFile) > 1000, "Vector PDF generated");

    // Parse back PDF
    auto parsedPdf = engine.parseFile(pdfFile);
    ASSERT_TRUE(parsedPdf != nullptr, "Parse generated PDF statement");
    ASSERT_TRUE(parsedPdf->transactions.size() == 3, "PDF contains all 3 transactions parsed back");

    // Clean up sandbox
    std::filesystem::remove_all("/tmp/ofx_test_sandbox");
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " EXECUTANDO BATERIA DE TESTES DE CONVERSAO UNIVERSAL\n";
    std::cout << "========================================================\n";

    testSha256();
    testAmountAndDateParsing();
    testUniversalFormatRoundtrip();

    std::cout << "\n[================ TODOS OS TESTES PASSARAM! ================]\n";
    return 0;
}
