/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#include "core/Types.hpp"
#include "core/Hasher.hpp"
#include "core/Utils.hpp"
#include "parsers/PdfParser.hpp"
#include "parsers/CsvParser.hpp"
#include "parsers/JsonParser.hpp"
#include "encoders/JsonEncoder.hpp"
#include "ocr/OcrEngine.hpp"
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
    ASSERT_TRUE(ofx::core::Utils::normalizeDate("15 OUT 2023") == "2023-10-15", "Text Month Date 15 OUT 2023");
    ASSERT_TRUE(ofx::core::Utils::normalizeDate("10-JAN-2024") == "2024-01-10", "Text Month Date 10-JAN-2024");
    ASSERT_TRUE(ofx::core::Utils::normalizeDate("05/DEZ/2023") == "2023-12-05", "Text Month Date 05/DEZ/2023");
    ASSERT_TRUE(ofx::core::Utils::normalizeDate("20.11.2023") == "2023-11-20", "Dot Date 20.11.2023");
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

void testLayoutAgnosticPdfParsing() {
    std::cout << "\n--- TEST: Layout-Agnostic PDF Text Parser ---\n";
    ofx::parsers::PdfParser parser;

    // Test Case 1: Banco do Brasil style with document number, trailing C/D, and multi-line continuation
    std::string bbPdfText = 
        "BANCO DO BRASIL - AUTOATENDIMENTO BB\n"
        "Prefixo: 1234-5    Conta Corrente: 98765-4\n"
        "Data        Documento  Historico                       Valor\n"
        "SALDO ANTERIOR                                         1.500,00 C\n"
        "05/10/2023  000452     PIX TRANSF MATHEUS              150,00 D\n"
        "06/10/2023  001099     TED RECEBIDA                    2.500,00 C\n"
        "FAVORECIDO: EMPRESA ALPHA LTDA\n"
        "SALDO ATUAL                                            3.850,00 C\n";

    auto stmtBB = parser.parseFromText(bbPdfText);
    ASSERT_TRUE(stmtBB != nullptr, "Parsed BB statement");
    ASSERT_TRUE(stmtBB->account.bankId == "001", "BB Bank ID is 001");
    ASSERT_TRUE(stmtBB->account.branchId == "1234-5", "BB Branch ID matches");
    ASSERT_TRUE(stmtBB->account.accountId == "98765-4", "BB Account ID matches");
    ASSERT_TRUE(stmtBB->transactions.size() == 2, "BB Transactions count is 2");
    ASSERT_TRUE(stmtBB->transactions[0].documentNumber == "000452", "BB Tx 1 Doc number matched");
    ASSERT_TRUE(std::abs(stmtBB->transactions[0].amount - (-150.00)) < 0.001, "BB Tx 1 amount -150.00");
    ASSERT_TRUE(std::abs(stmtBB->transactions[1].amount - 2500.00) < 0.001, "BB Tx 2 amount +2500.00");
    ASSERT_TRUE(stmtBB->transactions[1].description.find("EMPRESA ALPHA LTDA") != std::string::npos, 
                "Multi-line description stitched successfully");

    // Test Case 2: Nubank / Fintech style with signed amounts and simple layout
    std::string nuPdfText =
        "Nu Pagamentos S.A. - Nubank\n"
        "Agencia: 0001    Conta: 1234567-8\n"
        "Data        Descricao                         Valor\n"
        "01/11/2023  Transferencia enviada pelo Pix    -45,90\n"
        "02/11/2023  Pagamento de boleto internet      -120,00\n"
        "03/11/2023  Transferencia recebida pelo Pix   +800,00\n";

    auto stmtNu = parser.parseFromText(nuPdfText);
    ASSERT_TRUE(stmtNu != nullptr, "Parsed Nubank statement");
    ASSERT_TRUE(stmtNu->account.bankId == "260", "Nubank ID is 260");
    ASSERT_TRUE(stmtNu->account.accountId == "1234567-8", "Nubank Account ID matches");
    ASSERT_TRUE(stmtNu->transactions.size() == 3, "Nubank Transactions count is 3");
    ASSERT_TRUE(std::abs(stmtNu->totalDebits - 165.90) < 0.001, "Nubank total debits 165.90");
    ASSERT_TRUE(std::abs(stmtNu->totalCredits - 800.00) < 0.001, "Nubank total credits 800.00");

    // Test Case 3: 3-column statement (Débito, Crédito, Saldo) commonly used in Cooperatives / Caixa
    std::string coopPdfText =
        "SICOOB - Sistema de Cooperativas\n"
        "Ponto de Atendimento: 0101    No. Conta: 55443-2\n"
        "DATA        HISTORICO                    DEBITO      CREDITO     SALDO\n"
        "10/11/2023  COMPRA CARTAO DEBITO LOJA    100,00      0,00        1.900,00\n"
        "11/11/2023  DEPOSITO EM DINHEIRO         0,00        500,00      2.400,00\n";

    auto stmtCoop = parser.parseFromText(coopPdfText);
    ASSERT_TRUE(stmtCoop != nullptr, "Parsed Sicoob 3-column statement");
    ASSERT_TRUE(stmtCoop->account.bankId == "756", "Sicoob Bank ID is 756");
    ASSERT_TRUE(stmtCoop->transactions.size() == 2, "Sicoob Transactions count is 2");
    ASSERT_TRUE(std::abs(stmtCoop->transactions[0].amount - (-100.00)) < 0.001, "3-column debit recognized");
    ASSERT_TRUE(std::abs(stmtCoop->transactions[1].amount - 500.00) < 0.001, "3-column credit recognized");
    ASSERT_TRUE(stmtCoop->transactions[1].runningBalance.has_value(), "Running balance present");
    ASSERT_TRUE(std::abs(stmtCoop->transactions[1].runningBalance.value() - 2400.00) < 0.001, "Running balance recognized");
}

void testMercadoLivreStatementParsing() {
    std::cout << "\n--- TEST: Mercado Livre / Mercado Pago Statement Parser ---\n";
    ofx::parsers::PdfParser parser;

    // Test Case 1: Mercado Pago with prepositional dates and signed currency (R$ - / R$ +)
    std::string mlPdfText =
        "Mercado Pago Instituicao de Pagamento Ltda.\n"
        "CNPJ: 10.573.521/0001-91\n"
        "Extrato de Movimentacao de Conta\n"
        "Agencia: 0001    Conta: 71829304-1\n"
        "Data | Detalhe | Operacao | Entrada | Saida | Saldo\n"
        "24 de outubro de 2023 Venda de produto #7182930415 R$ 250,00 R$ 0,00 R$ 1.500,00\n"
        "24 de outubro de 2023 Tarifa de venda #7182930415 R$ 0,00 R$ 32,50 R$ 1.467,50\n"
        "25 de outubro de 2023 Envio de Pix para Maria R$ 0,00 R$ 100,00 R$ 1.367,50\n"
        "26 de outubro de 2023 Rendimento de conta R$ 4,85 R$ 0,00 R$ 1.372,35\n";

    auto stmtML = parser.parseFromText(mlPdfText);
    ASSERT_TRUE(stmtML != nullptr, "Parsed Mercado Livre statement");
    ASSERT_TRUE(stmtML->account.bankId == "323", "Mercado Pago Bank ID is 323");
    ASSERT_TRUE(stmtML->transactions.size() == 4, "Mercado Livre Transactions count is 4");
    ASSERT_TRUE(stmtML->transactions[0].date == "2023-10-24", "Preposition date '24 de outubro de 2023' normalized to ISO");
    ASSERT_TRUE(std::abs(stmtML->transactions[0].amount - 250.00) < 0.001, "Tx 1 credit is 250.00");
    ASSERT_TRUE(std::abs(stmtML->transactions[1].amount - (-32.50)) < 0.001, "Tx 2 fee debit is -32.50");
    ASSERT_TRUE(std::abs(stmtML->transactions[2].amount - (-100.00)) < 0.001, "Tx 3 Pix debit is -100.00");
    ASSERT_TRUE(std::abs(stmtML->transactions[3].amount - 4.85) < 0.001, "Tx 4 Yield credit is 4.85");

    // Test Case 2: Mercado Livre with abbreviated preposition dates and signed R$ - notation
    std::string mlText2 =
        "Mercado Livre / Mercado Pago\n"
        "15 de out de 2023 Venda #998811 R$ 450,00\n"
        "16 de out de 2023 Tarifa de envio Mercado Envios R$ -45,90\n"
        "18 de out de 2023 Pagamento com saldo Mercado Pago R$ -120,00\n";

    auto stmtML2 = parser.parseFromText(mlText2);
    ASSERT_TRUE(stmtML2 != nullptr, "Parsed Mercado Livre statement abbreviated format");
    ASSERT_TRUE(stmtML2->account.bankId == "323", "Matched 323 for Mercado Livre");
    ASSERT_TRUE(stmtML2->transactions.size() == 3, "Abbreviated date transactions count is 3");
    ASSERT_TRUE(stmtML2->transactions[0].date == "2023-10-15", "Abbreviated date '15 de out de 2023' normalized");
    ASSERT_TRUE(std::abs(stmtML2->transactions[1].amount - (-45.90)) < 0.001, "R$ -45,90 parsed as negative debit");
    ASSERT_TRUE(std::abs(stmtML2->transactions[2].amount - (-120.00)) < 0.001, "R$ -120,00 parsed as negative debit");

    // Test Case 3: Mercado Pago CSV format
    std::string csvData =
        "DATA;OPERAÇÃO;DETALHE;VALOR BRUTO;TARIFA;VALOR LÍQUIDO;SALDO\n"
        "2023-10-24 14:30:00;7182930415;Venda de produto;250,00;-32,50;217,50;1467,50\n"
        "2023-10-25 09:15:00;7182930416;Envio de dinheiro Pix;0,00;-100,00;-100,00;1367,50\n";
    std::string tmpCsv = "/tmp/test_ml.csv";
    ofx::core::Utils::writeFileString(tmpCsv, csvData);
    ofx::parsers::CsvParser csvParser;
    auto stmtCsv = csvParser.parse(tmpCsv);
    ASSERT_TRUE(stmtCsv != nullptr && stmtCsv->transactions.size() == 2, "Mercado Pago CSV parsed with 2 transactions");
    std::remove(tmpCsv.c_str());
}

void testJsonRoundtrip() {
    std::cout << "\n--- TEST: JSON Universal Format Roundtrip ---\n";
    ofx::converter::ConverterEngine engine;

    ofx::core::Statement stmt;
    stmt.account.bankId = "260";
    stmt.account.bankName = "Nubank";
    stmt.account.branchId = "0001";
    stmt.account.accountId = "123456-7";
    stmt.account.currency = "BRL";

    ofx::core::Transaction tx1;
    tx1.date = "2023-10-15";
    tx1.amount = 1200.50;
    tx1.description = "Pix Recebido Joao";
    tx1.documentNumber = "PIX01";
    stmt.transactions.push_back(tx1);

    ofx::core::Transaction tx2;
    tx2.date = "2023-10-16";
    tx2.amount = -85.90;
    tx2.description = "Uber Viagem Trabalho";
    tx2.documentNumber = "UBR02";
    stmt.transactions.push_back(tx2);

    stmt.computeStatistics();

    std::string jsonFile = "/tmp/test_stmt.json";
    bool okEncode = engine.convertStatement(stmt, ofx::core::FileFormat::JSON, jsonFile);
    ASSERT_TRUE(okEncode, "Encode Statement to JSON");
    ASSERT_TRUE(std::filesystem::exists(jsonFile), "JSON file written");

    auto parsed = engine.parseFile(jsonFile);
    ASSERT_TRUE(parsed != nullptr, "Parse JSON file");
    ASSERT_TRUE(parsed->transactions.size() == 2, "JSON contains 2 transactions");
    ASSERT_TRUE(parsed->account.bankId == "260", "JSON bankId preserved");
    ASSERT_TRUE(std::abs(parsed->totalCredits - 1200.50) < 0.001, "JSON total credits match");
    ASSERT_TRUE(std::abs(parsed->totalDebits - 85.90) < 0.001, "JSON total debits match");
    ASSERT_TRUE(parsed->transactions[0].category == "Pix Recebido", "Category preserved/assigned");
    ASSERT_TRUE(parsed->transactions[1].category == "Transporte / Combustivel", "Category for Uber recognized");

    std::remove(jsonFile.c_str());
}

void testMultiPagePdfGeneration() {
    std::cout << "\n--- TEST: Multi-Page Vector PDF Generation ---\n";
    ofx::converter::ConverterEngine engine;

    ofx::core::Statement largeStmt;
    largeStmt.account.bankId = "341";
    largeStmt.account.bankName = "Itau Unibanco";
    largeStmt.account.branchId = "0456";
    largeStmt.account.accountId = "98765-4";
    largeStmt.account.currency = "BRL";
    largeStmt.periodStart = "2023-10-01";
    largeStmt.periodEnd = "2023-10-31";

    // Generate 85 transactions (spans 3 pages)
    for (int i = 1; i <= 85; ++i) {
        ofx::core::Transaction tx;
        tx.date = (i < 10) ? ("2023-10-0" + std::to_string(i)) : ("2023-10-" + std::to_string(std::min(i, 28)));
        tx.amount = (i % 2 == 0) ? (50.0 + i * 2) : -(20.0 + i);
        tx.description = (i % 2 == 0) ? ("CREDITO OPERACAO #" + std::to_string(i)) : ("PAGAMENTO BOLETO #" + std::to_string(i));
        tx.documentNumber = "DOC" + std::to_string(1000 + i);
        largeStmt.transactions.push_back(tx);
    }

    largeStmt.computeStatistics();
    ASSERT_TRUE(largeStmt.transactions.size() == 85, "Large statement contains 85 transactions");

    std::string pdfPath = "/tmp/multipage_test.pdf";
    bool okPdf = engine.convertStatement(largeStmt, ofx::core::FileFormat::PDF, pdfPath);
    ASSERT_TRUE(okPdf, "Encode large statement to multi-page PDF");
    ASSERT_TRUE(std::filesystem::exists(pdfPath), "PDF file created");

    // Verify PDF content contains multiple pages
    std::string pdfContent = ofx::core::Utils::readFileString(pdfPath);
    ASSERT_TRUE(pdfContent.find("/Count 3") != std::string::npos, "PDF catalog has /Count 3 pages");
    ASSERT_TRUE(pdfContent.find("Pagina 2 de 3") != std::string::npos, "Continuation page 2 running header present");
    ASSERT_TRUE(pdfContent.find("Pagina 3 de 3") != std::string::npos, "Continuation page 3 running header present");
    ASSERT_TRUE(pdfContent.find("INTEGRIDADE CRIPTOGRAFICA SHA-256") != std::string::npos, "Audit badge present on final page");

    // Parse back all 85 transactions
    auto parsedPdf = engine.parseFile(pdfPath);
    ASSERT_TRUE(parsedPdf != nullptr, "Parse multi-page PDF");
    ASSERT_TRUE(parsedPdf->transactions.size() == 85, "All 85 transactions parsed back across all pages");

    std::remove(pdfPath.c_str());
}

void testAuditAndReconciliationEngine() {
    std::cout << "\n--- TEST: Audit & Mathematical Reconciliation Engine ---\n";

    // Test Case 1: Exact reconciliation (Verified)
    ofx::core::Statement stmt1;
    stmt1.hasInitialBalance = true;
    stmt1.initialBalance = 1000.00;
    stmt1.hasFinalBalance = true;
    stmt1.finalBalance = 1350.00;

    ofx::core::Transaction t1;
    t1.date = "2023-10-01";
    t1.amount = 500.00;
    stmt1.transactions.push_back(t1);

    ofx::core::Transaction t2;
    t2.date = "2023-10-02";
    t2.amount = -150.00;
    stmt1.transactions.push_back(t2);

    stmt1.computeStatistics();
    ASSERT_TRUE(stmt1.auditStatus == ofx::core::AuditStatus::VERIFIED, "Exact parity verified (1000 + 500 - 150 = 1350)");
    ASSERT_TRUE(std::abs(stmt1.auditDiscrepancy) < 0.001, "Zero discrepancy");
    ASSERT_TRUE(stmt1.auditWarnings.empty(), "Zero audit warnings");

    // Test Case 2: Discrepancy detection
    ofx::core::Statement stmt2;
    stmt2.hasInitialBalance = true;
    stmt2.initialBalance = 1000.00;
    stmt2.hasFinalBalance = true;
    stmt2.finalBalance = 1500.00; // Declared 1500, but expected is 1350 (diff of 150)
    stmt2.transactions.push_back(t1);
    stmt2.transactions.push_back(t2);

    stmt2.computeStatistics();
    ASSERT_TRUE(stmt2.auditStatus == ofx::core::AuditStatus::DISCREPANCY, "Discrepancy correctly detected");
    ASSERT_TRUE(std::abs(stmt2.auditDiscrepancy - 150.00) < 0.001, "Discrepancy amount is 150.00");
    ASSERT_TRUE(!stmt2.auditWarnings.empty(), "Audit warning generated");

    // Test Case 3: Duplicate transaction detection
    ofx::core::Statement stmt3;
    stmt3.transactions.push_back(t1);
    stmt3.transactions.push_back(t1); // Exact duplicate
    stmt3.computeStatistics();
    ASSERT_TRUE(!stmt3.auditWarnings.empty(), "Duplicate transaction flagged in audit warnings");
}

void testCategorizationAndPixParsing() {
    std::cout << "\n--- TEST: Intelligent Categorization & Pix Parsing ---\n";
    ofx::core::Statement stmt;

    ofx::core::Transaction t1;
    t1.date = "2023-10-10";
    t1.amount = 4500.00;
    t1.description = "SALARIO MENSAL FOLHA DE PAGAMENTO";
    stmt.transactions.push_back(t1);

    ofx::core::Transaction t2;
    t2.date = "2023-10-11";
    t2.amount = -45.90;
    t2.description = "IFOOD RESTAURANTE SABOR";
    stmt.transactions.push_back(t2);

    ofx::core::Transaction t3;
    t3.date = "2023-10-12";
    t3.amount = -180.50;
    t3.description = "CPFL ENERGIA CONTA LUZ";
    stmt.transactions.push_back(t3);

    ofx::core::Transaction t4;
    t4.date = "2023-10-13";
    t4.amount = -12.50;
    t4.description = "TARIFA PACOTE MENSAL DE SERVICOS";
    stmt.transactions.push_back(t4);

    ofx::core::Transaction t5;
    t5.date = "2023-10-14";
    t5.amount = -250.00;
    t5.description = "Pix Enviado para chave 123.456.789-00 E12345678901234567890123456789012";
    stmt.transactions.push_back(t5);

    stmt.computeStatistics();

    ASSERT_TRUE(stmt.transactions[0].category == "Salario / Proventos", "Salario categorized");
    ASSERT_TRUE(stmt.transactions[1].category == "Alimentacao / Mercado", "iFood categorized");
    ASSERT_TRUE(stmt.transactions[2].category == "Utilidades / Contas", "CPFL Energia categorized");
    ASSERT_TRUE(stmt.transactions[3].category == "Tarifas / Bancarios", "Tarifa categorized");
    ASSERT_TRUE(stmt.transactions[4].category == "Pix Enviado", "Pix categorized");
    ASSERT_TRUE(stmt.transactions[4].pixKey == "123.456.789-00", "Pix key extracted");
    ASSERT_TRUE(stmt.transactions[4].pixE2eId == "E12345678901234567890123456789012", "Pix E2E ID extracted");
}

void testCsvWithMetadataPreHeaders() {
    std::cout << "\n--- TEST: CSV with Pre-Header Metadata Rows ---\n";
    std::string csvWithHeaderInfo =
        "BANCO DO BRASIL S.A. - EXTRATO DE CONTA CORRENTE\n"
        "Agencia: 1234-5  Conta: 98765-4\n"
        "Periodo: 01/10/2023 a 31/10/2023\n"
        "Saldo Anterior: 1.500,00\n"
        "\n"
        "Data;Historico;Documento;Valor;Saldo\n"
        "05/10/2023;PIX TRANSF JOAO;DOC101;-150,00;1.350,00\n"
        "10/10/2023;DEPOSITO DINHEIRO;DOC102;800,00;2.150,00\n";

    std::string tmp = "/tmp/test_preheaders.csv";
    ofx::core::Utils::writeFileString(tmp, csvWithHeaderInfo);

    ofx::parsers::CsvParser parser;
    auto stmt = parser.parse(tmp);
    ASSERT_TRUE(stmt != nullptr, "Parsed CSV with metadata rows");
    ASSERT_TRUE(stmt->transactions.size() == 2, "Found both transactions despite 5 header lines");
    ASSERT_TRUE(stmt->hasInitialBalance, "Parsed initial balance from pre-header");
    ASSERT_TRUE(std::abs(stmt->initialBalance - 1500.00) < 0.001, "Initial balance is 1500.00");
    ASSERT_TRUE(std::abs(stmt->transactions[0].amount - (-150.00)) < 0.001, "First tx amount -150.00");
    ASSERT_TRUE(std::abs(stmt->transactions[1].amount - 800.00) < 0.001, "Second tx amount +800.00");

    std::remove(tmp.c_str());
}

void testPdfUtf16BeHexDecoding() {
    std::cout << "\n--- TEST: PDF UTF-16BE Hex Decoding ---\n";
    ofx::parsers::PdfParser parser;

    // Simulate PDF stream containing UTF-16BE hex string <00420041004E0043004F> (BANCO)
    // and transactions encoded with hex characters
    std::string hexPdf = 
        "%PDF-1.4\n"
        "1 0 obj << /Length 200 >> stream\n"
        "BT\n"
        "/F1 12 Tf\n"
        "<00420041004E0043004F00200044004F002000420052004100530049004C> Tj T*\n"
        "<00300035002F00310030002F003200300032003300200050004900580020005400520041004E005300460020002D003100350030002C00300030> Tj\n"
        "ET\n"
        "endstream\n"
        "endobj\n";

    std::vector<uint8_t> bytes(hexPdf.begin(), hexPdf.end());
    std::string extracted = parser.extractRawTextFromPdf(bytes);
    ASSERT_TRUE(extracted.find("BANCO DO BRASIL") != std::string::npos, "UTF-16BE 'BANCO DO BRASIL' decoded");
    ASSERT_TRUE(extracted.find("05/10/2023") != std::string::npos, "UTF-16BE date decoded");
}

void testIntegratedOcrEngine() {
    std::cout << "\n--- TEST: Integrated Native C++ OCR Engine ---\n";
    auto& ocr = ofx::ocr::OcrEngine::instance();

    std::cout << "[*] System Tesseract available: " << (ocr.isSystemOcrAvailable() ? "SIM" : "NAO (Usando Motor Nativo C++)") << "\n";

    // Test blank image handling
    int w = 240, h = 40;
    std::vector<uint8_t> gray(w * h, 255); // White background
    std::string text = ocr.recognizeGrayscale(gray.data(), w, h);
    ASSERT_TRUE(text.empty(), "Blank page produces empty OCR string");

    // Test domain correction
    std::string rawOcr = "MERCADO PAGO 24/1O/2O23 R $ 15O,OO";
    std::string cleaned = ocr.postProcessFinancialText(rawOcr);
    ASSERT_TRUE(cleaned.find("150,00") != std::string::npos, "Financial domain OCR correction '15O,OO' -> '150,00'");
    ASSERT_TRUE(cleaned.find("24/10/2023") != std::string::npos, "Date domain OCR correction '24/1O/2O23' -> '24/10/2023'");
}

void testUniversalBankAndStackedPdfParsing() {
    std::cout << "\n--- TEST: Universal All-Bank Recognition & Stacked PDF Layouts ---\n";
    ofx::parsers::PdfParser parser;

    // Test 1: Stacked mobile statement layout (Nubank / Inter style)
    std::string stackedMobilePdfText =
        "BANCO INTER S.A.\n"
        "Conta Corrente: 1234567-8   Agencia: 0001-9\n"
        "Extrato de Movimentacao Financeira\n"
        "02/11/2023\n"
        "Pix Enviado - Transferencia para Joao\n"
        "Chave: joao@email.com - ID: E0770000020231102143000\n"
        "- R$ 85,50\n"
        "05/11/2023\n"
        "Pix Recebido de Maria Silva\n"
        "Chave: 123.456.789-00\n"
        "+ R$ 450,00\n"
        "10/11/2023\n"
        "Pagamento de Fatura Cartao Inter Mastercard\n"
        "DOC 991203\n"
        "- R$ 1.250,00\n";

    auto interStmt = parser.parseFromText(stackedMobilePdfText);
    ASSERT_TRUE(interStmt != nullptr, "Parsed Inter stacked mobile statement");
    ASSERT_TRUE(interStmt->account.bankId == "077", "Inter Bank ID matched 077");
    ASSERT_TRUE(interStmt->account.accountId == "1234567-8", "Inter Account ID matches");
    ASSERT_TRUE(interStmt->transactions.size() == 3, "All 3 stacked transactions extracted");
    ASSERT_TRUE(interStmt->transactions[0].isDebit(), "Tx 1 is debit");
    ASSERT_TRUE(std::abs(interStmt->transactions[0].amount - (-85.50)) < 0.001, "Tx 1 amount -85.50");
    ASSERT_TRUE(interStmt->transactions[1].isCredit(), "Tx 2 is credit");
    ASSERT_TRUE(std::abs(interStmt->transactions[1].amount - 450.00) < 0.001, "Tx 2 amount 450.00");
    ASSERT_TRUE(interStmt->transactions[2].isDebit(), "Tx 3 is debit");
    ASSERT_TRUE(std::abs(interStmt->transactions[2].amount - (-1250.00)) < 0.001, "Tx 3 amount -1250.00");

    // Test 2: Santander 4-column statement with running balance
    std::string santanderPdfText =
        "BANCO SANTANDER (BRASIL) S.A.\n"
        "Agencia: 0123    Conta Corrente: 13.001.234-5\n"
        "SALDO ANTERIOR                                          1.500,00\n"
        "Data        Historico                  Docto      Valor (R$)     Saldo (R$)\n"
        "12/10/2023  TARIFA MENSALIDADE PACOTE  000001        -45,00 D    1.455,00\n"
        "15/10/2023  TED RECEBIDA REMUNERACAO   889120      3.500,00 C    4.955,00\n"
        "SALDO FINAL                                             4.955,00\n";

    auto santanderStmt = parser.parseFromText(santanderPdfText);
    ASSERT_TRUE(santanderStmt != nullptr, "Parsed Santander statement");
    ASSERT_TRUE(santanderStmt->account.bankId == "033", "Santander Bank ID matched 033");
    ASSERT_TRUE(santanderStmt->transactions.size() == 2, "Santander 2 transactions extracted");
    ASSERT_TRUE(santanderStmt->hasInitialBalance && std::abs(santanderStmt->initialBalance - 1500.00) < 0.001, "Initial balance parsed");
    ASSERT_TRUE(santanderStmt->hasFinalBalance && std::abs(santanderStmt->finalBalance - 4955.00) < 0.001, "Final balance parsed");
    ASSERT_TRUE(santanderStmt->auditStatus == ofx::core::AuditStatus::VERIFIED, "Santander statement verified by accounting audit engine");

    // Test 3: C6 Bank and BTG Pactual recognition
    std::string c6Text = "BANCO C6 S.A. Extrato Mensal Conta 1234567-8 Agencia 0001\n10/10/2023 PGTO CONDOMINIO -350,00";
    auto c6Stmt = parser.parseFromText(c6Text);
    ASSERT_TRUE(c6Stmt->account.bankId == "336", "C6 Bank ID matched 336");

    std::string btgText = "BANCO BTG PACTUAL S.A. Agencia 0050 Conta 998877-6\n01/10/2023 RENDIMENTO CDB +120,45";
    auto btgStmt = parser.parseFromText(btgText);
    ASSERT_TRUE(btgStmt->account.bankId == "208", "BTG Pactual Bank ID matched 208");

    // Test 4: Fintechs & Institutions (Stone, PagBank, Cora, Asaas, PicPay)
    std::string stoneText = "STONE PAGAMENTOS S.A. Relatorio de Movimentacao\n01/10/2023 VENDAS CARTAO DEBITO +890,00";
    ASSERT_TRUE(parser.parseFromText(stoneText)->account.bankId == "197", "Stone matched 197");

    std::string pagbankText = "PAGBANK - PAGSEGURO INTERNET S.A.\n01/10/2023 RECARGA CELULAR -30,00";
    ASSERT_TRUE(parser.parseFromText(pagbankText)->account.bankId == "290", "PagBank matched 290");

    std::string coraText = "CORA SOCIEDADE DE CREDITO DIRETO\n01/10/2023 RECEBIMENTO PIX CLIENTE +1500,00";
    ASSERT_TRUE(parser.parseFromText(coraText)->account.bankId == "403", "Cora matched 403");

    std::string asaasText = "ASAAS GESTAO FINANCEIRA\n01/10/2023 COBRANCA BOLETO LIQUIDADO +750,00";
    ASSERT_TRUE(parser.parseFromText(asaasText)->account.bankId == "461", "Asaas matched 461");
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " EXECUTANDO BATERIA DE TESTES DE CONVERSAO UNIVERSAL\n";
    std::cout << "========================================================\n";

    testSha256();
    testAmountAndDateParsing();
    testUniversalFormatRoundtrip();
    testJsonRoundtrip();
    testMultiPagePdfGeneration();
    testAuditAndReconciliationEngine();
    testCategorizationAndPixParsing();
    testCsvWithMetadataPreHeaders();
    testPdfUtf16BeHexDecoding();
    testLayoutAgnosticPdfParsing();
    testMercadoLivreStatementParsing();
    testUniversalBankAndStackedPdfParsing();
    testIntegratedOcrEngine();

    std::cout << "\n[================ TODOS OS TESTES PASSARAM! ================]\n";
    return 0;
}
