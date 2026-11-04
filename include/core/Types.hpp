/**
 * =================================================================================================
 * Projeto: BSC (Bank Statement Conversion)
 * Desenvolvido por Matheus Sousa, Laboratório de Comunicação e Segurança da Informação,
 * Departamento de Eletrônica e Sistemas, CTG-UFPE.
 * =================================================================================================
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <optional>
#include <memory>
#include <string_view>

namespace ofx::core {

enum class FileFormat {
    PDF,
    OFX,
    CSV,
    XLSX,
    QIF,
    UNKNOWN
};

inline std::string formatToString(FileFormat fmt) {
    switch (fmt) {
        case FileFormat::PDF:  return "PDF";
        case FileFormat::OFX:  return "OFX";
        case FileFormat::CSV:  return "CSV";
        case FileFormat::XLSX: return "XLSX";
        case FileFormat::QIF:  return "QIF";
        default:               return "UNKNOWN";
    }
}

inline FileFormat stringToFormat(std::string_view str) {
    if (str == "PDF" || str == "pdf") return FileFormat::PDF;
    if (str == "OFX" || str == "ofx") return FileFormat::OFX;
    if (str == "CSV" || str == "csv") return FileFormat::CSV;
    if (str == "XLSX" || str == "xlsx") return FileFormat::XLSX;
    if (str == "QIF" || str == "qif") return FileFormat::QIF;
    return FileFormat::UNKNOWN;
}

inline std::string formatToExtension(FileFormat fmt) {
    switch (fmt) {
        case FileFormat::PDF:  return ".pdf";
        case FileFormat::OFX:  return ".ofx";
        case FileFormat::CSV:  return ".csv";
        case FileFormat::XLSX: return ".xlsx";
        case FileFormat::QIF:  return ".qif";
        default:               return ".dat";
    }
}

enum class TransactionType {
    CREDIT,   // Inflow / Depósito / Recebimento / Pix Recebido
    DEBIT,    // Outflow / Pagamento / Saque / Pix Enviado / Tarifa
    TRANSFER,
    FEE,
    INTEREST,
    OTHER
};

inline std::string transactionTypeToString(TransactionType t) {
    switch (t) {
        case TransactionType::CREDIT:   return "CREDIT";
        case TransactionType::DEBIT:    return "DEBIT";
        case TransactionType::TRANSFER: return "TRANSFER";
        case TransactionType::FEE:      return "FEE";
        case TransactionType::INTEREST: return "INTEREST";
        default:                        return "OTHER";
    }
}

struct Transaction {
    std::string id;              // Unique or FITID
    std::string date;            // ISO 8601: YYYY-MM-DD
    std::string time;            // Optional: HH:MM:SS
    double amount = 0.0;         // Signed: Positive = Credit, Negative = Debit
    TransactionType type = TransactionType::OTHER;
    std::string description;     // Description / Memo / Histórico
    std::string documentNumber;  // Document ID / Cheque / Doc number
    std::optional<double> runningBalance; // Optional running balance
    std::string category;        // Auto-categorized or extracted
    std::string hashSha256;      // Cryptographic hash for non-repudiation & deduplication

    bool isCredit() const { return amount > 0.0; }
    bool isDebit() const { return amount < 0.0; }
};

struct AccountMetadata {
    std::string bankId;          // e.g. "001" (BB), "341" (Itaú), "237" (Bradesco), "260" (Nubank)
    std::string bankName;
    std::string branchId;        // Agência
    std::string accountId;       // Número da Conta
    std::string accountType;     // CHECKING, SAVINGS, CREDITCARD
    std::string currency = "BRL";// BRL, USD, EUR
};

struct Statement {
    std::string sourceFilePath;
    FileFormat sourceFormat = FileFormat::UNKNOWN;
    AccountMetadata account;
    
    std::string periodStart;     // YYYY-MM-DD
    std::string periodEnd;       // YYYY-MM-DD
    
    double initialBalance = 0.0;
    double finalBalance = 0.0;
    bool hasInitialBalance = false;
    bool hasFinalBalance = false;

    std::vector<Transaction> transactions;
    std::string statementHashSha256; // Integrity checksum of the whole statement
    
    // Summary statistics
    double totalCredits = 0.0;
    double totalDebits = 0.0;
    double netFlow = 0.0;
    size_t creditCount = 0;
    size_t debitCount = 0;

    void computeStatistics();
};

struct BatchConversionTask {
    std::string id;
    std::string inputFilePath;
    FileFormat inputFormat = FileFormat::UNKNOWN;
    std::vector<FileFormat> targetFormats;
    std::string outputDirectory;
    
    // Status
    enum class Status {
        PENDING,
        PARSING,
        PARSED,
        CONVERTING,
        COMPLETED,
        FAILED
    } status = Status::PENDING;

    std::string errorMessage;
    std::shared_ptr<Statement> parsedStatement;
    std::vector<std::string> generatedFiles;

    // Telecom / Performance telemetry
    double parseTimeMs = 0.0;
    double encodeTimeMs = 0.0;
    double totalTimeMs = 0.0;
    size_t inputSizeBytes = 0;
    size_t outputSizeBytes = 0;
    double throughputMBs = 0.0;
    double throughputTxSec = 0.0;
    std::string sha256Input;
};

struct PipelineMetrics {
    size_t totalFiles = 0;
    size_t successfulFiles = 0;
    size_t failedFiles = 0;
    size_t totalTransactions = 0;
    size_t totalBytesProcessed = 0;
    double totalDurationMs = 0.0;
    double averageThroughputMBs = 0.0;
    double averageTransactionsPerSec = 0.0;
};

} // namespace ofx::core
